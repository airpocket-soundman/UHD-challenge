#include <Arduino.h>
#include <M5Unified.h>
#include <esp_camera.h>
#include <SPI.h>
#include <SD.h>

// ========================= ESP-DL =========================
// Arduino IDE 用の include（PlatformIOとは違う）
extern "C" {
#include "dl_model_base.h"
#include "dl_layer.h"
#include "dl_layer_conv2d.h"
#include "dl_layer_softmax.h"
#include "dl_tool.hpp"
}
#include "dl_model_base.hpp"
using namespace dl;

//============================================================
//-------------------- LCD --------------------
#define LCD_WIDTH   (320)
#define LCD_HEIGHT  (240)

#define CROP_SIZE   (240)
#define NN_INPUT_SIZE  (64)

//-------------------- SD --------------------
#define SD_SPI_SCK_PIN   36
#define SD_SPI_MISO_PIN  35
#define SD_SPI_MOSI_PIN  37
#define SD_SPI_CS_PIN    4

#define MODEL_ESPDL_PATH  "/model.espdl"
#define MODEL_JSON_PATH   "/model.json"

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
    .frame_size   = FRAMESIZE_QVGA,
    .jpeg_quality = 0,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

//-------------------- グローバルバッファ --------------------
static uint16_t g_crop240[CROP_SIZE * CROP_SIZE];
static float g_nn_input[NN_INPUT_SIZE * NN_INPUT_SIZE * 3];

static bool g_model_loaded = false;
static dl::Model *g_model = nullptr;

//============================================================
// Utility
static inline void rgb565_to_rgb_normalized(uint16_t c, float &r, float &g, float &b)
{
    uint8_t r8 = ((c >> 11) & 0x1F) << 3;
    uint8_t g8 = ((c >> 5)  & 0x3F) << 2;
    uint8_t b8 = (c & 0x1F) << 3;

    r = r8 / 255.0f;
    g = g8 / 255.0f;
    b = b8 / 255.0f;
}

//============================================================
// モデルロード
bool init_sd_and_load_model()
{
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("SD init failed!");
        return false;
    }

    if (!SD.exists(MODEL_ESPDL_PATH)) {
        Serial.println("model.espdl missing");
        return false;
    }

    Serial.println("Loading ESP-DL model...");

    g_model = new dl::Model(MODEL_ESPDL_PATH, fbs::MODEL_LOCATION_IN_SDCARD);
    if (!g_model) return false;

    if (!g_model->init()) {
        Serial.println("model init failed");
        delete g_model;
        g_model = nullptr;
        return false;
    }

    Serial.println("ESP-DL model loaded!");
    g_model_loaded = true;
    return true;
}

//============================================================
// 前処理（64×64 RGB float）
void prepare_input(camera_fb_t *fb)
{
    const uint16_t *src = (const uint16_t *)fb->buf;

    // 240×240 クロップ
    for (int y = 0; y < CROP_SIZE; y++) {
        int sy = y;
        for (int x = 0; x < CROP_SIZE; x++) {
            int sx = x + 40;
            g_crop240[y*CROP_SIZE + x] = src[sy*320 + sx];
        }
    }

    // 64×64 リサイズ + 正規化
    for (int y = 0; y < NN_INPUT_SIZE; y++) {
        int sy = y * CROP_SIZE / NN_INPUT_SIZE;
        for (int x = 0; x < NN_INPUT_SIZE; x++) {
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
// 推論（esp-dl）
int inference_class_id()
{
    if (!g_model_loaded) return -1;

    dl::Tensor *input  = g_model->get_input();
    dl::Tensor *output = g_model->get_output();

    // 入力にデータをコピー
    memcpy(input->get_data(), g_nn_input,
           NN_INPUT_SIZE * NN_INPUT_SIZE * 3 * sizeof(float));

    // 推論実行
    g_model->forward();

    float *out = output->get_data();
    int out_len = output->shape().dims[1];

    // ArgMax（ESP-DLは未対応なので自作）
    int max_id = 0;
    float max_v = out[0];
    for (int i = 1; i < out_len; i++) {
        if (out[i] > max_v) {
            max_v = out[i];
            max_id = i;
        }
    }
    return max_id;
}

//============================================================
// カメラ描画 + 推論
void camera_capture_and_display()
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;

    prepare_input(fb);

    int cls = inference_class_id();

    // 結果を画面に表示（デバッグ）
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(10,10);
    M5.Display.printf("Class = %d", cls);

    // クロップ表示
    M5.Display.startWrite();
    M5.Display.setAddrWindow(40, 40, 240, 240);
    M5.Display.writePixels(g_crop240, 240*240);
    M5.Display.endWrite();

    esp_camera_fb_return(fb);
}

//============================================================
// setup / loop
void setup()
{
    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);

    Serial.begin(115200);

    M5.Display.fillScreen(BLACK);
    M5.Display.drawString("Init...", 160,120);

    if (!init_sd_and_load_model()) {
        M5.Display.drawString("Model load failed",160,140);
    }

    if (esp_camera_init(&camera_config) != ESP_OK) {
        Serial.println("Camera init failed");
        while(1) delay(1000);
    }
}

void loop()
{
    camera_capture_and_display();
}

