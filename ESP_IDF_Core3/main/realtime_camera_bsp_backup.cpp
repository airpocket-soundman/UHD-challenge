#include <stdio.h>
#include <string.h>
#include <vector>

#include "esp_log.h"
#include "esp_camera.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/m5stack_core_s3.h"
#include "driver/i2c.h"

static const char *TAG = "BSP_CAM";
static constexpr bool USE_COLORBAR = false;

// Camera config (GC0308)
static camera_config_t camera_cfg = {
    .pin_pwdn     = -1,
    .pin_reset    = -1,
    .pin_xclk     = GPIO_NUM_10,
    .pin_sscb_sda = 12,
    .pin_sscb_scl = 11,
    .pin_d7       = 47,
    .pin_d6       = 48,
    .pin_d5       = 16,
    .pin_d4       = 15,
    .pin_d3       = 42,
    .pin_d2       = 41,
    .pin_d1       = 40,
    .pin_d0       = 39,
    .pin_vsync    = 46,
    .pin_href     = 38,
    .pin_pclk     = 45,
    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size   = FRAMESIZE_QVGA,
    .jpeg_quality = 0,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    .sccb_i2c_port= -1,
};

static void ensure_camera_power(void)
{
    // AW9523 P1: enable LCD(1<<1) + CAM(1<<0). Assumes I2C driver already installed by BSP.
    uint8_t data[2];
    esp_err_t err;
    data[0] = 0x02; data[1] = 0x02; // P0 minimal (keep default)
    err = i2c_master_write_to_device((i2c_port_t)BSP_I2C_NUM, 0x58, data, sizeof(data), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "AW9523 P0 write failed: %s", esp_err_to_name(err));
    }
    data[0] = 0x03; data[1] = 0xA3; // P1 base 0b10100000 | LCD | CAM
    err = i2c_master_write_to_device((i2c_port_t)BSP_I2C_NUM, 0x58, data, sizeof(data), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "AW9523 P1 write failed: %s", esp_err_to_name(err));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "BSP camera preview (RGB->BGR + byte-swap) with debug");

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    bsp_display_config_t disp_cfg = {
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(bsp_display_new(&disp_cfg, &panel, &io));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    bsp_display_brightness_init();
    bsp_display_backlight_on();
    ensure_camera_power();
    // esp_camera will install its own I2C driver on the same port; delete existing to avoid install error
    i2c_driver_delete((i2c_port_t)BSP_I2C_NUM);

    esp_err_t err = esp_camera_init(&camera_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return;
    }
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_pixformat(s, PIXFORMAT_RGB565);
        s->set_framesize(s, FRAMESIZE_QVGA);
        s->set_colorbar(s, USE_COLORBAR ? 1 : 0);
    }

    std::vector<uint16_t> conv;

    while (true) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "fb_get failed");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        size_t pixels = fb->width * fb->height;
        conv.resize(pixels);
        const uint16_t* src = reinterpret_cast<const uint16_t*>(fb->buf);
        for (size_t i = 0; i < pixels; ++i) {
            uint16_t c = src[i];
            uint16_t r = (c >> 11) & 0x1F;
            uint16_t g = c & 0x07E0;
            uint16_t b = c & 0x001F;
            uint16_t bgr = static_cast<uint16_t>((b << 11) | g | r);
            conv[i] = __builtin_bswap16(bgr); // panel expects BGR + big-endian
        }

        ESP_LOGI(TAG, "fb w=%u h=%u len=%u src[0]=0x%04X conv[0]=0x%04X",
                 fb->width, fb->height, fb->len, src[0], conv.empty() ? 0 : conv[0]);

        esp_lcd_panel_draw_bitmap(panel, 0, 0, fb->width, fb->height, conv.data());

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
