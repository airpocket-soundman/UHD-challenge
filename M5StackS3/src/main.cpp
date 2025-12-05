/**
 * M5Stack CoreS3 Object Detection Demo
 * 
 * Features:
 * - Camera capture
 * - ESP-DL model inference (from microSD)
 * - BBox drawing
 * - Real-time display
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// M5Stack CoreS3
#include "M5CoreS3.h"

// ESP-DL
#include "dl_image.hpp"
#include "dl_model_base.hpp"

// Camera
#include "esp_camera.h"

static const char *TAG = "M5S3-Detection";

// Single class model - person detection only
const char* COCO_CLASSES[] = {
    "person"
};

// Model configuration
#define MODEL_WIDTH 64
#define MODEL_HEIGHT 64
#define NUM_CLASSES 1  // Single class: person only
#define NUM_DETECTIONS 100
#define CONF_THRESHOLD 0.5f
#define IOU_THRESHOLD 0.45f

// Camera configuration for M5Stack CoreS3
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     2
#define SIOD_GPIO_NUM     12
#define SIOC_GPIO_NUM     11
#define Y9_GPIO_NUM       47
#define Y8_GPIO_NUM       48
#define Y7_GPIO_NUM       16
#define Y6_GPIO_NUM       15
#define Y5_GPIO_NUM       42
#define Y4_GPIO_NUM       41
#define Y3_GPIO_NUM       40
#define Y2_GPIO_NUM       39
#define VSYNC_GPIO_NUM    46
#define HREF_GPIO_NUM     38
#define PCLK_GPIO_NUM     45

// Detection result structure
struct Detection {
    float score;
    int class_id;
    float x1, y1, x2, y2;
    
    bool operator<(const Detection& other) const {
        return score > other.score;
    }
};

// ESP-DL Model wrapper
class ObjectDetector {
private:
    dl::Model *model;
    
public:
    ObjectDetector() : model(nullptr) {}
    
    bool load_model(const char* model_path) {
        ESP_LOGI(TAG, "Loading model from: %s", model_path);
        
        // Check if file exists
        FILE *f = fopen(model_path, "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open model file: %s", model_path);
            return false;
        }
        
        // Get file size
        fseek(f, 0, SEEK_END);
        size_t file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        ESP_LOGI(TAG, "Model file size: %zu bytes", file_size);
        
        fclose(f);
        
        // Load ESP-DL model
        model = new dl::Model();
        if (!model->load(model_path)) {
            ESP_LOGE(TAG, "Failed to load model");
            delete model;
            model = nullptr;
            return false;
        }
        
        ESP_LOGI(TAG, "Model loaded successfully");
        ESP_LOGI(TAG, "Model has %d outputs", model->get_output_count());
        
        return true;
    }
    
    std::vector<Detection> detect(uint8_t* rgb_data, int width, int height) {
        std::vector<Detection> detections;
        
        if (!model) {
            ESP_LOGE(TAG, "Model not loaded");
            return detections;
        }
        
        // Preprocess: resize and normalize
        uint8_t* resized = (uint8_t*)heap_caps_malloc(
            MODEL_WIDTH * MODEL_HEIGHT * 3, MALLOC_CAP_8BIT
        );
        
        dl::image::resize_image(
            rgb_data, resized,
            width, height,
            MODEL_WIDTH, MODEL_HEIGHT,
            3
        );
        
        // Normalize to [0, 1]
        float* input = (float*)heap_caps_malloc(
            MODEL_WIDTH * MODEL_HEIGHT * 3 * sizeof(float),
            MALLOC_CAP_8BIT
        );
        
        for (int i = 0; i < MODEL_WIDTH * MODEL_HEIGHT * 3; i++) {
            input[i] = resized[i] / 255.0f;
        }
        
        heap_caps_free(resized);
        
        // Run inference
        model->run(input);
        
        heap_caps_free(input);
        
        // Get 6 outputs
        float* detection_scores = (float*)model->get_output(0);
        float* class_scores = (float*)model->get_output(1);
        float* bbox_x1 = (float*)model->get_output(2);
        float* bbox_y1 = (float*)model->get_output(3);
        float* bbox_x2 = (float*)model->get_output(4);
        float* bbox_y2 = (float*)model->get_output(5);
        
        // Apply ArgMax on class_scores
        for (int i = 0; i < NUM_DETECTIONS; i++) {
            float score = detection_scores[i];
            
            if (score < CONF_THRESHOLD) continue;
            
            // ArgMax
            int max_class = 0;
            float max_score = class_scores[i * NUM_CLASSES];
            
            for (int c = 1; c < NUM_CLASSES; c++) {
                float class_score = class_scores[i * NUM_CLASSES + c];
                if (class_score > max_score) {
                    max_score = class_score;
                    max_class = c;
                }
            }
            
            Detection det;
            det.score = score;
            det.class_id = max_class;
            det.x1 = bbox_x1[i];
            det.y1 = bbox_y1[i];
            det.x2 = bbox_x2[i];
            det.y2 = bbox_y2[i];
            
            detections.push_back(det);
        }
        
        std::sort(detections.begin(), detections.end());
        detections = apply_nms(detections, IOU_THRESHOLD);
        
        return detections;
    }
    
private:
    float calculate_iou(const Detection& a, const Detection& b) {
        float x1 = std::max(a.x1, b.x1);
        float y1 = std::max(a.y1, b.y1);
        float x2 = std::min(a.x2, b.x2);
        float y2 = std::min(a.y2, b.y2);
        
        float inter_area = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
        float area_a = (a.x2 - a.x1) * (a.y2 - a.y1);
        float area_b = (b.x2 - b.x1) * (b.y2 - b.y1);
        float union_area = area_a + area_b - inter_area;
        
        return (union_area > 0) ? (inter_area / union_area) : 0.0f;
    }
    
    std::vector<Detection> apply_nms(std::vector<Detection>& dets, float iou_thresh) {
        std::vector<Detection> keep;
        std::vector<bool> suppressed(dets.size(), false);
        
        for (size_t i = 0; i < dets.size(); i++) {
            if (suppressed[i]) continue;
            keep.push_back(dets[i]);
            
            for (size_t j = i + 1; j < dets.size(); j++) {
                if (suppressed[j]) continue;
                if (dets[i].class_id == dets[j].class_id) {
                    float iou = calculate_iou(dets[i], dets[j]);
                    if (iou > iou_thresh) {
                        suppressed[j] = true;
                    }
                }
            }
        }
        
        return keep;
    }
};

// Camera initialization
bool init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return false;
    }
    
    ESP_LOGI(TAG, "Camera initialized");
    return true;
}

// Draw detections
void draw_detections(const std::vector<Detection>& dets, int img_width, int img_height) {
    M5.Lcd.setTextSize(2);
    
    for (const auto& det : dets) {
        int x1 = (int)(det.x1 * img_width);
        int y1 = (int)(det.y1 * img_height);
        int x2 = (int)(det.x2 * img_width);
        int y2 = (int)(det.y2 * img_height);
        
        x1 = std::max(0, std::min(x1, img_width - 1));
        y1 = std::max(0, std::min(y1, img_height - 1));
        x2 = std::max(0, std::min(x2, img_width - 1));
        y2 = std::max(0, std::min(y2, img_height - 1));
        
        M5.Lcd.drawRect(x1, y1, x2 - x1, y2 - y1, TFT_GREEN);
        M5.Lcd.drawRect(x1 + 1, y1 + 1, x2 - x1 - 2, y2 - y1 - 2, TFT_GREEN);
        
        char label[64];
        snprintf(label, sizeof(label), "%s %.2f", COCO_CLASSES[det.class_id], det.score);
        
        M5.Lcd.fillRect(x1, y1 - 20, strlen(label) * 12, 20, TFT_GREEN);
        M5.Lcd.setTextColor(TFT_BLACK);
        M5.Lcd.drawString(label, x1 + 2, y1 - 18);
        M5.Lcd.setTextColor(TFT_WHITE);
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "M5Stack CoreS3 Object Detection Demo");
    ESP_LOGI(TAG, "ESP-IDF: %s", esp_get_idf_version());
    
    // Initialize M5Stack
    auto cfg = M5.config();
    M5.begin(cfg);
    
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.drawString("Object Detection", 60, 10);
    M5.Lcd.drawString("Person Detection", 60, 40);
    M5.Lcd.drawString("Initializing...", 80, 100);
    
    // Initialize camera
    M5.Lcd.drawString("Camera...", 100, 130);
    if (!init_camera()) {
        M5.Lcd.fillScreen(RED);
        M5.Lcd.drawString("Camera Failed!", 50, 100);
        return;
    }
    
    // Load model from SD card
    M5.Lcd.drawString("Model...", 100, 190);
    ObjectDetector detector;
    if (!detector.load_model("/sdcard/uhd_n_w64_multi")) {
        M5.Lcd.fillScreen(RED);
        M5.Lcd.drawString("Model Load Failed!", 30, 100);
        M5.Lcd.drawString("Check SD card file:", 20, 130);
        M5.Lcd.drawString("uhd_n_w64_multi", 40, 160);
        return;
    }
    
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.drawString("Ready!", 120, 100);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "Starting detection loop...");
    
    uint32_t frame_count = 0;
    uint32_t start_time = xTaskGetTickCount();
    
    while (true) {
        M5.update();
        
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }
        
        // Convert RGB565 to RGB888
        uint8_t* rgb888 = (uint8_t*)heap_caps_malloc(
            fb->width * fb->height * 3, MALLOC_CAP_8BIT
        );
        
        uint16_t* rgb565 = (uint16_t*)fb->buf;
        for (int i = 0; i < fb->width * fb->height; i++) {
            uint16_t pixel = rgb565[i];
            rgb888[i * 3 + 0] = ((pixel >> 11) & 0x1F) << 3;
            rgb888[i * 3 + 1] = ((pixel >> 5) & 0x3F) << 2;
            rgb888[i * 3 + 2] = (pixel & 0x1F) << 3;
        }
        
        // Display camera image
        M5.Lcd.pushImage(0, 0, fb->width, fb->height, (uint16_t*)fb->buf);
        
        // Run detection
        auto detections = detector.detect(rgb888, fb->width, fb->height);
        heap_caps_free(rgb888);
        
        // Draw results
        draw_detections(detections, fb->width, fb->height);
        
        // Show FPS
        frame_count++;
        uint32_t elapsed = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
        if (elapsed > 1000) {
            float fps = frame_count * 1000.0f / elapsed;
            char fps_str[32];
            snprintf(fps_str, sizeof(fps_str), "FPS: %.1f", fps);
            M5.Lcd.fillRect(0, 0, 100, 20, TFT_BLACK);
            M5.Lcd.drawString(fps_str, 5, 5);
            ESP_LOGI(TAG, "%s, Detections: %d", fps_str, detections.size());
            frame_count = 0;
            start_time = xTaskGetTickCount();
        }
        
        esp_camera_fb_return(fb);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
