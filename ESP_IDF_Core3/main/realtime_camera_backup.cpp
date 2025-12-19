#include <stdio.h>
#include <string.h>
#include <vector>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_camera.h"
#include "LovyanGFX.hpp"
#include "M5Unified.h"

static const char *TAG = "CAM_LGFX";
static constexpr bool USE_COLORBAR = false;

// LovyanGFX init for M5Stack CoreS3 (ILI9342C)
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9342 _panel;
    lgfx::Bus_SPI _bus;
public:
    LGFX() {
        auto bus_cfg = _bus.config();
        bus_cfg.spi_host    = SPI3_HOST;
        bus_cfg.spi_mode    = 0;
        bus_cfg.freq_write  = 40000000;
        bus_cfg.freq_read   = 16000000;
        bus_cfg.spi_3wire   = false;
        bus_cfg.use_lock    = true;
        bus_cfg.dma_channel = SPI_DMA_CH_AUTO;
        bus_cfg.pin_sclk    = 36;
        bus_cfg.pin_mosi    = 37;
        bus_cfg.pin_miso    = 35;
        bus_cfg.pin_dc      = 35;   // CoreS3 DC/MISO shared
        _bus.config(bus_cfg);
        _panel.setBus(&_bus);

        auto panel_cfg = _panel.config();
        panel_cfg.pin_cs         = 3;
        panel_cfg.pin_rst        = -1;
        panel_cfg.pin_busy       = -1;
        panel_cfg.memory_width   = 320;
        panel_cfg.memory_height  = 240;
        panel_cfg.panel_width    = 320;
        panel_cfg.panel_height   = 240;
        panel_cfg.offset_x       = 0;
        panel_cfg.offset_y       = 0;
        panel_cfg.offset_rotation= 3;
        panel_cfg.readable       = true;
        panel_cfg.invert         = true;   // CoreS3 autodetect??
        panel_cfg.rgb_order      = false;  // ILI9342C?RGB?
        panel_cfg.bus_shared     = true;
        _panel.config(panel_cfg);

        setPanel(&_panel);
    }
};

static LGFX lcd;

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

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Start camera preview (ILI9342C)");

    lcd.init();
    lcd.setRotation(1);
    lcd.setSwapBytes(false);
    lcd.fillScreen(TFT_BLACK);

    // M5?????????????I2C???
    M5.In_I2C.release();

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

    while (true) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "fb_get failed");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        lcd.startWrite();
        lcd.pushImage(0, 0, fb->width, fb->height, (lgfx::rgb565_t*)fb->buf);
        lcd.endWrite();

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
