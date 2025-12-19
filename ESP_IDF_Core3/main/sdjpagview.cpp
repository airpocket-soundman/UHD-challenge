#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "jpeg_decoder.h"
#include "bsp/m5stack_core_s3.h"
#include "lvgl.h"

static const char *TAG = "JPEG_VIEW";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Start JPEG viewer");

    lv_display_t* disp = bsp_display_start();
    if (!disp) {
        ESP_LOGE(TAG, "Display init failed");
        return;
    }

    ESP_LOGI(TAG, "Mount SD card...");
    if (bsp_sdcard_mount() != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed");
        return;
    }

    const char* path = "/sdcard/image.jpg";
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG, "File not found: %s", path);
        return;
    }
    size_t jpg_size = st.st_size;

    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return;
    }

    uint8_t* jpg_buf = (uint8_t*)malloc(jpg_size);
    if (!jpg_buf) {
        ESP_LOGE(TAG, "No memory for JPEG");
        fclose(f);
        return;
    }
    fread(jpg_buf, 1, jpg_size, f);
    fclose(f);

    const int out_w = 320, out_h = 240;
    uint8_t* rgb565 = (uint8_t*)heap_caps_malloc(out_w * out_h * 2, MALLOC_CAP_SPIRAM);
    if (!rgb565) {
        ESP_LOGE(TAG, "No memory for RGB565");
        free(jpg_buf);
        return;
    }

    esp_jpeg_image_cfg_t cfg = {
        .indata      = jpg_buf,
        .indata_size = (uint32_t)jpg_size,
        .outbuf      = rgb565,
        .outbuf_size = (uint32_t)(out_w * out_h * 2),
        .out_format  = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale   = JPEG_IMAGE_SCALE_0,
    };
    esp_jpeg_image_output_t dec = {};

    int64_t t0 = esp_timer_get_time();
    esp_err_t ret = esp_jpeg_decode(&cfg, &dec);
    int64_t t1 = esp_timer_get_time();
    free(jpg_buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
        free(rgb565);
        return;
    }
    ESP_LOGI(TAG, "Decoded %dx%d bytes=%u time=%.2fms",
             dec.width, dec.height, (unsigned)dec.output_len, (t1 - t0) / 1000.0f);

    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Display lock failed");
        free(rgb565);
        return;
    }
    lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_buffer(canvas, rgb565, dec.width, dec.height, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(canvas);
    bsp_display_unlock();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
