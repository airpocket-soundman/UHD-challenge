#include <stdio.h>
#include <string.h>
#include <vector>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "LovyanGFX.hpp"

static const char *TAG = "CAM_LGFX";

// LovyanGFX panel config for M5Stack CoreS3 (ILI9342C)
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI _bus;
public:
    LGFX() {
        auto bus_cfg = _bus.config();
        bus_cfg.spi_host   = SPI3_HOST;
        bus_cfg.spi_mode   = 0;
        bus_cfg.freq_write = 40000000;
        bus_cfg.freq_read  = 16000000;
        bus_cfg.spi_3wire  = false;
        bus_cfg.use_lock   = true;
        bus_cfg.dma_channel= SPI_DMA_CH_AUTO;
        bus_cfg.pin_sclk   = 36;
        bus_cfg.pin_mosi   = 37;
        bus_cfg.pin_miso   = 35;
        bus_cfg.pin_dc     = 35;   // CoreS3 uses DC/MISO shared
        _bus.config(bus_cfg);
        _panel.setBus(&_bus);

        auto panel_cfg = _panel.config();
        panel_cfg.pin_cs      = 3;
        panel_cfg.pin_rst     = -1;
        panel_cfg.pin_busy    = -1;
        panel_cfg.memory_width  = 320;
        panel_cfg.memory_height = 240;
        panel_cfg.panel_width   = 320;
        panel_cfg.panel_height  = 240;
        panel_cfg.offset_x    = 0;
        panel_cfg.offset_y    = 0;
        panel_cfg.offset_rotation = 3; // orientation for CoreS3
        panel_cfg.readable    = true;
        panel_cfg.invert      = true;
        panel_cfg.rgb_order   = true;   // BGR panel
        panel_cfg.bus_shared  = true;
        _panel.config(panel_cfg);

        setPanel(&_panel);
    }
};

static LGFX lcd;

// GC0308 camera config (LovyanGFX path)
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
    .pixel_format = PIXFORMAT_YUV422,  // YUV input -> manual RGB conversion
    .frame_size   = FRAMESIZE_QVGA,
    .jpeg_quality = 0,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    .sccb_i2c_port= -1,
};

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Start camera preview with LovyanGFX (YUV->RGB)");

    lcd.init();
    lcd.setRotation(1);
    lcd.fillScreen(TFT_BLACK);

    esp_err_t err = esp_camera_init(&camera_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return;
    }
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_pixformat(s, PIXFORMAT_YUV422);
        s->set_framesize(s, FRAMESIZE_QVGA);
        s->set_colorbar(s, 0);
        // Disable auto adjustments to see raw colors
        s->set_whitebal(s, 0);
        s->set_awb_gain(s, 0);
        s->set_gain_ctrl(s, 0);
        s->set_exposure_ctrl(s, 0);
        s->set_lenc(s, 0);
    }

    std::vector<uint8_t> rgb888;
    std::vector<uint16_t> conv;

    while (true) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "fb_get failed");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        size_t pixels = fb->width * fb->height;
        rgb888.resize(pixels * 3);
        conv.resize(pixels);

        // YUV422 -> RGB888
        fmt2rgb888(fb->buf, fb->len, PIXFORMAT_YUV422, rgb888.data());
        // RGB888 -> RGB565 (panel rgb_order already BGR, so no manual swap)
        for (size_t i = 0; i < pixels; ++i) {
            uint8_t r = rgb888[i * 3 + 0];
            uint8_t g = rgb888[i * 3 + 1];
            uint8_t b = rgb888[i * 3 + 2];
            conv[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }

        ESP_LOGI(TAG, "fb w=%u h=%u len=%u src[0]=0x%04X conv[0]=0x%04X",
                 fb->width, fb->height, fb->len,
                 ((uint16_t*)fb->buf)[0], conv.empty() ? 0 : conv[0]);

        lcd.startWrite();
        lcd.pushImage(0, 0, fb->width, fb->height, (lgfx::rgb565_t*)conv.data());
        lcd.endWrite();

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
