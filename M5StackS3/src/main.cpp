#include <Arduino.h>

#include <M5Unified.h>
#include <esp_camera.h>

#include <SPI.h>
#include <SD.h>

// ========================= ESP-DL =========================
// Arduino + ESP-IDF ハイブリッドで読み込む
extern "C" {
#include "dl_model_base.h"
}
#include "dl_model_base.hpp"
using namespace dl;

//============================================================
//-------------------- LCD --------------------
#define LCD_WIDTH   (320)
#define LCD_HEIGHT  (240)

// クロップ後の正方形領域
#define CROP_SIZE   (240)

// NN 入力サイズ
#define NN_INPUT_SIZE  (64)

//-------------------- SD Card (SPI) --------------------
#define SD_SPI_SCK_PIN   36
#define SD_SPI_MISO_PIN  35
#define SD_SPI_MOSI_PIN  37
#define SD_SPI_CS_PIN    4

// モデルファイルパス（microSDのルートに配置）
#define MODEL_ESPDL_PATH  "/model.espdl"
#define MODEL_JSON_PATH   "/model.json"

//-------------------- モデル定数 --------------------
#define NUM_ANCHORS  8
#define NUM_CLASSES  80  // COCO classes
#define GRID_SIZE    8   // 8x8 grid
#define CONFIDENCE_THRESHOLD  0.3f

//-------------------- Camera config --------------------
static camera_config_t camera_config = {
    .pin_pwdn     = -1,
    .pin_reset    = -1,
    .pin_xclk     = 2,
    .pin_sscb_sda = 12,
    .pin_sscb_scl = 11,

    .pin_d7 = 47,
    .pin_d6 = 48,
    .pin_d5 = 16,
    .pin_d4 = 15,
    .pin_d3 = 42,
    .pin_d2 = 41,
    .pin_d1 = 40,
    .pin_d0 = 39,

    .pin_vsync = 46,
    .pin_href  = 38,
    .pin_pclk  = 45,

    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565,
    .frame_size   = FRAMESIZE_QVGA,   // 320x240
    .jpeg_quality = 0,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

//-------------------- グローバルバッファ --------------------
static uint16_t g_crop240[CROP_SIZE * CROP_SIZE];
static float g_nn_input[NN_INPUT_SIZE * NN_INPUT_SIZE * 3];

// SD / モデル状態
static bool g_sd_ok        = false;
static bool g_model_loaded = false;

// ESP-DL model instance
static dl::Model *g_model = nullptr;

struct ModelConstants {
    float anchors[NUM_ANCHORS][2];
    float wh_scale[NUM_ANCHORS][2];
};
static ModelConstants g_constants;

// 検出結果構造体
struct Detection {
    float x1, y1, x2, y2;
    float confidence;
    int class_id;
};

//============================================================
//-------------------- Utility --------------------
static inline void rgb565_to_rgb_normalized(uint16_t c, float &r, float &g, float &b)
{
    uint8_t r8 = ((c >> 11) & 0x1F) << 3;
    uint8_t g8 = ((c >> 5)  & 0x3F) << 2;
    uint8_t b8 = (c & 0x1F) << 3;
    
    r = r8 / 255.0f;
    g = g8 / 255.0f;
    b = b8 / 255.0f;
}

void draw_rectangle(uint16_t *img, int width, int height,
                    int x1, int y1, int x2, int y2,
                    uint16_t color, int thickness = 2)
{
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    for (int t = 0; t < thickness; ++t) {
        if (y1+t < height)
            for (int x = x1; x <= x2; ++x)
                img[(y1+t)*width + x] = color;
        if (y2-t >= 0)
            for (int x = x1; x <= x2; ++x)
                img[(y2-t)*width + x] = color;

        if (x1+t < width)
            for (int y = y1; y <= y2; ++y)
                img[y*width + (x1+t)] = color;
        if (x2-t >= 0)
            for (int y = y1; y <= y2; ++y)
                img[y*width + (x2-t)] = color;
    }
}

//============================================================
//-------------------- load model.json --------------------
bool load_model_constants()
{
    File f = SD.open(MODEL_JSON_PATH);
    if (!f) {
        Serial.println("Failed to open model.json");
        return false;
    }

    String json = f.readString();
    f.close();

    auto parse_array = [&](const char *key, float out[][2]) -> bool {
        int pos = json.indexOf(key);
        if (pos < 0) return false;

        int s = json.indexOf("[", pos);
        int e = json.indexOf("]", s);
        if (s < 0 || e < 0) return false;

        String arr = json.substring(s+1, e);
        int idx = 0;
        int p = 0;

        while (p < arr.length() && idx < NUM_ANCHORS * 2) {
            int c = arr.indexOf(",", p);
            if (c < 0) c = arr.length();

            float v = arr.substring(p, c).trim().toFloat();
            out[idx/2][idx%2] = v;

            idx++;
            p = c + 1;
        }
        return true;
    };

    if (!parse_array("anchors", g_constants.anchors)) return false;
    if (!parse_array("wh_scale", g_constants.wh_scale)) return false;

    Serial.println("model.json constants loaded OK");
    return true;
}

//============================================================
//-------------------- init_sd_and_load_model() --------------------
bool init_sd_and_load_model()
{
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("SD init failed!");
        return false;
    }

    Serial.println("SD OK");

    if (!SD.exists(MODEL_ESPDL_PATH)) {
        Serial.println("model.espdl not found");
        return false;
    }
    if (!SD.exists(MODEL_JSON_PATH)) {
        Serial.println("model.json not found");
        return false;
    }

    Serial.println("model.espdl found");
    Serial.println("model.json found");

    if (!load_model_constants()) {
        Serial.println("model.json read failed");
        return false;
    }

    Serial.println("Loading ESP-DL model...");

    g_model = new dl::Model(MODEL_ESPDL_PATH, fbs::MODEL_LOCATION_IN_SDCARD);
    if (!g_model) {
        Serial.println("g_model = nullptr");
        return false;
    }

    if (!g_model->init()) {
        Serial.println("g_model->init() failed!");
        delete g_model;
        g_model = nullptr;
        return false;
    }

    Serial.println("ESP-DL model loaded OK!");
    g_model_loaded = true;
    return true;
}

//============================================================
//-------------------- Camera init --------------------
esp_err_t camera_init()
{
    M5.In_I2C.release();
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) Serial.printf("Camera init error: %x\n", err);
    return err;
}

//============================================================
//-------------------- 前処理 --------------------
void prepare_cropped_and_resized(camera_fb_t *fb)
{
    const uint16_t *src = (const uint16_t *)fb->buf;

    for (int y = 0; y < CROP_SIZE; ++y) {
        int sy = y;
        for (int x = 0; x < CROP_SIZE; ++x) {
            int sx = x + 40;   // center crop
            g_crop240[y*CROP_SIZE + x] = src[sy*320 + sx];
        }
    }

    for (int y = 0; y < NN_INPUT_SIZE; ++y) {
        int sy = y * CROP_SIZE / NN_INPUT_SIZE;
        for (int x = 0; x < NN_INPUT_SIZE; ++x) {
            int sx = x * CROP_SIZE / NN_INPUT_SIZE;
            uint16_t pix = g_crop240[sy*CROP_SIZE + sx];

            float r,g,b;
            rgb565_to_rgb_normalized(pix, r, g, b);

            int idx = y*NN_INPUT_SIZE + x;
            g_nn_input[idx] = r;
            g_nn_input[idx + NN_INPUT_SIZE*NN_INPUT_SIZE] = g;
            g_nn_input[idx + NN_INPUT_SIZE*NN_INPUT_SIZE*2] = b;
        }
    }
}

//============================================================
//-------------------- run_inference (dummy) --------------------
std::vector<Detection> run_inference()
{
    std::vector<Detection> ds;

    if (!g_model_loaded) {
        Detection d{20,20,44,44,0.9f,0};
        ds.push_back(d);
        return ds;
    }

    // ★ ESP-DL 推論は後で実装する（いまはロード確認のみ）
    Detection d{30,30,50,50,0.7f,0};
    ds.push_back(d);

    return ds;
}

//============================================================
//-------------------- drawing --------------------
void camera_capture_and_display()
{
    M5.In_I2C.release();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;

    prepare_cropped_and_resized(fb);

    auto results = run_inference();

    float scale = 240.0f / 64.0f;
    const uint16_t RED = 0xF800;

    for (auto &d : results) {
        int x1 = d.x1 * scale;
        int y1 = d.y1 * scale;
        int x2 = d.x2 * scale;
        int y2 = d.y2 * scale;

        draw_rectangle(g_crop240, 240, 240, x1,y1,x2,y2, RED, 2);
    }

    M5.Display.startWrite();
    M5.Display.setAddrWindow(40, 0, 240, 240);
    M5.Display.writePixels(g_crop240, 240*240);
    M5.Display.endWrite();

    esp_camera_fb_return(fb);
}

//============================================================
//-------------------- setup / loop --------------------
void setup()
{
    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);

    Serial.begin(115200);
    delay(300);

    M5.Display.fillScreen(BLACK);
    M5.Display.drawString("Init...", 160,120);

    if (!init_sd_and_load_model()) {
        Serial.println("Model load failed");
        M5.Display.fillScreen(BLACK);
        M5.Display.drawString("Model load failed",160,120);
        g_model_loaded = false;
    } else {
        Serial.println("Model OK");
    }

    if (camera_init() != ESP_OK) {
        Serial.println("Cam init failed");
        while(1) delay(1000);
    }
}

void loop()
{
    camera_capture_and_display();
}
