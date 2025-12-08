#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hello from M5Stack CoreS3!");
    ESP_LOGI(TAG, "ESP-IDF Simple Test");
    
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "Running... count=%d", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
