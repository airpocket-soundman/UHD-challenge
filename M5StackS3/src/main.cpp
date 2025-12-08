#include <Arduino.h>

#include <M5Unified.h>
#include <esp_camera.h>

#include <SPI.h>
#include <SD.h>

//-------------------- LCD --------------------
#define LCD_WIDTH   (320)
#define LCD_HEIGHT  (240)

// クロップ後の正方形領域
#define CROP_SIZE   (240)

// NN 入力サイズ
#define NN_INPUT_SIZE  (64)

//-------------------- SD Card (SPI) --------------------
// ※あなたが提示してくれた CoreS3 用ピンをそのまま使用
#define SD_SPI_SCK_PIN   36
#define SD_SPI_MISO_PIN  35
#define SD_SPI_MOSI_PIN  37
#define SD_SPI_CS_PIN    4

// モデルファイルパス（microSDのルートに置く想定）
// 例: /UHD64x64.model
#define MODEL_FILE_PATH  "/UHD64x64.model"

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
// 320x240 → 中央 240x240 にトリミングした RGB565 画像
static uint16_t g_crop240[CROP_SIZE * CROP_SIZE];

// 240x240 → 64x64 に縮小した NN 入力画像（ここではグレースケール1ch）
static uint8_t  g_nn_input[NN_INPUT_SIZE * NN_INPUT_SIZE];

// SD / モデル状態
static bool g_sd_ok        = false;
static bool g_model_loaded = false;

//-------------------- ユーティリティ --------------------

// RGB565 → グレースケール(0-255) 変換
static inline uint8_t rgb565_to_gray(uint16_t c)
{
    uint8_t r = ((c >> 11) & 0x1F) << 3;
    uint8_t g = ((c >> 5)  & 0x3F) << 2;
    uint8_t b = (c & 0x1F) << 3;
    // NTSC比率っぽく
    return (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
}

// g_crop240 上に枠線（BB）を描く
void draw_rectangle(uint16_t *img, int width, int height,
                    int x1, int y1, int x2, int y2,
                    uint16_t color)
{
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= width)  x2 = width - 1;
    if (y2 >= height) y2 = height - 1;

    // 上下の辺
    for (int x = x1; x <= x2; ++x) {
        img[y1 * width + x] = color;
        img[y2 * width + x] = color;
    }
    // 左右の辺
    for (int y = y1; y <= y2; ++y) {
        img[y * width + x1] = color;
        img[y * width + x2] = color;
    }
}

//-------------------- SD & モデル読み込み --------------------

bool init_sd_and_check_model()
{
    // SPI 初期化（I2C には一切触らない）
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        M5.Display.println("SD init failed");
        Serial.println("SD init failed");
        g_sd_ok = false;
        g_model_loaded = false;
        return false;
    }

    g_sd_ok = true;

    // モデルファイルの存在チェック
    if (SD.exists(MODEL_FILE_PATH)) {
        File f = SD.open(MODEL_FILE_PATH, FILE_READ);
        if (!f) {
            M5.Display.println("Model open failed");
            Serial.println("Model open failed");
            g_model_loaded = false;
            return false;
        }

        // 本当はここで ESP-DL 用にモデルデータを読み込む
        // 現段階では「存在確認だけ」でフラグを立てて終わり
        size_t size = f.size();
        Serial.printf("Model file %s found, size = %u bytes\n",
                      MODEL_FILE_PATH, (unsigned int)size);
        f.close();

        g_model_loaded = true;
    } else {
        M5.Display.println("Model file not found");
        Serial.printf("Model file %s not found\n", MODEL_FILE_PATH);
        g_model_loaded = false;
    }

    return g_model_loaded;
}

//-------------------- カメラ初期化 --------------------

esp_err_t camera_init()
{
    // M5Unified が掴んでいる I2C を開放（元コードと同じ）
    M5.In_I2C.release();

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        M5.Display.println("Camera Init Failed");
        Serial.printf("Camera Init Failed: 0x%x\n", err);
        return err;
    }

    return ESP_OK;
}

//-------------------- 前処理：fb → 240x240 → 64x64 --------------------

void prepare_cropped_and_resized(camera_fb_t *fb)
{
    // fb は 320x240 RGB565 前提
    const int src_w = fb->width;   // 320
    const int src_h = fb->height;  // 240

    const uint16_t *src = (const uint16_t *)fb->buf;

    // 320x240 → 中央 240x240 を切り出す
    const int crop_w = CROP_SIZE;
    const int crop_h = CROP_SIZE;
    const int crop_x0 = (src_w - crop_w) / 2; // 40
    const int crop_y0 = 0;                    // 上から

    for (int y = 0; y < crop_h; ++y) {
        int sy = crop_y0 + y;
        int src_row_offset = sy * src_w;
        int dst_row_offset = y * crop_w;

        for (int x = 0; x < crop_w; ++x) {
            int sx = crop_x0 + x;
            g_crop240[dst_row_offset + x] = src[src_row_offset + sx];
        }
    }

    // 240x240 → 64x64 へ最近傍補間で縮小（グレースケール）
    for (int y = 0; y < NN_INPUT_SIZE; ++y) {
        int sy = y * crop_h / NN_INPUT_SIZE;
        for (int x = 0; x < NN_INPUT_SIZE; ++x) {
            int sx = x * crop_w / NN_INPUT_SIZE;
            uint16_t pix = g_crop240[sy * crop_w + sx];
            uint8_t gray = rgb565_to_gray(pix);
            g_nn_input[y * NN_INPUT_SIZE + x] = gray;
        }
    }
}

//-------------------- ダミー推論ロジック --------------------

// 本来はここで ESP-DL に g_nn_input を渡して推論する
// いまは「モデルがあればとりあえず何か返す」「無ければダミーBB」にしておく
void run_dummy_inference(int &out_x1, int &out_y1, int &out_x2, int &out_y2)
{
    if (!g_model_loaded) {
        // モデルファイルが無い場合：指定どおりのダミーBB
        out_x1 = 50;
        out_y1 = 50;
        out_x2 = 100;
        out_y2 = 100;
        return;
    }

    // モデルはあるけど、まだ ESP-DL を繋いでいないので
    // いったん「中央付近を囲う」ダミー挙動にしておく
    // ※後でここを ESP-DL の出力に置き換えれば OK
    out_x1 = CROP_SIZE / 2 - 40;
    out_y1 = CROP_SIZE / 2 - 40;
    out_x2 = CROP_SIZE / 2 + 40;
    out_y2 = CROP_SIZE / 2 + 40;
}

//-------------------- カメラキャプチャ＋描画 --------------------

void camera_capture_and_display()
{
    // M5Unified の I2C を離してからカメラが I2C を掴む（元コードと同じ）
    M5.In_I2C.release();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        M5.Display.println("Camera Capture Failed");
        Serial.println("Camera Capture Failed");
        return;
    }

    // 前処理：fb -> g_crop240(240x240) & g_nn_input(64x64)
    prepare_cropped_and_resized(fb);

    // 推論（いまはダミー）
    int bb_x1, bb_y1, bb_x2, bb_y2;
    run_dummy_inference(bb_x1, bb_y1, bb_x2, bb_y2);

    // g_crop240 上に BB を描く（赤）
    const uint16_t RED = 0xF800;
    draw_rectangle(g_crop240, CROP_SIZE, CROP_SIZE,
                   bb_x1, bb_y1, bb_x2, bb_y2,
                   RED);

    // 240x240 をディスプレイに表示
    // 320x240 の中央に 240x240 を置く
    int dst_x = (LCD_WIDTH  - CROP_SIZE) / 2; // 40
    int dst_y = (LCD_HEIGHT - CROP_SIZE) / 2; // 0

    M5.Display.startWrite();
    M5.Display.setAddrWindow(dst_x, dst_y, CROP_SIZE, CROP_SIZE);
    M5.Display.writePixels(g_crop240, CROP_SIZE * CROP_SIZE);
    M5.Display.endWrite();

    // フレームバッファを返却
    esp_camera_fb_return(fb);
}

//-------------------- setup / loop --------------------

void setup()
{
    Serial.begin(115200);

    auto cfg = M5.config();
    cfg.output_power = true; // 外部 5V 出力
    M5.begin(cfg);

    M5.Display.setRotation(1);  // 必要に応じて
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.setFont(&fonts::efontJA_16);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.fillScreen(BLACK);
    M5.Display.drawString("Init...", LCD_WIDTH / 2, LCD_HEIGHT / 2);

    // SD & モデルチェック
    init_sd_and_check_model();

    // カメラ初期化
    if (camera_init() != ESP_OK) {
        M5.Display.fillScreen(BLACK);
        M5.Display.drawString("Camera init failed", LCD_WIDTH / 2, LCD_HEIGHT / 2);
        // ここで止めるか、リトライを書くかはお好みで
        return;
    }

    M5.Display.fillScreen(BLACK);
    if (g_model_loaded) {
        M5.Display.drawString("Model OK (dummy inference)", LCD_WIDTH / 2, LCD_HEIGHT / 2);
    } else {
        M5.Display.drawString("No model -> dummy BB", LCD_WIDTH / 2, LCD_HEIGHT / 2);
    }

    delay(1000);
}

void loop()
{
    camera_capture_and_display();
    // 必要ならフレームレート調整
    // delay(30);
}
