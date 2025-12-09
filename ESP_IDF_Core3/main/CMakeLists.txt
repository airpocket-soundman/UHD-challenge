# Main component CMakeLists.txt
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES 
        esp32-camera
        esp_lcd
        esp_psram
        esp_timer
        nvs_flash
        fatfs
        espressif__esp-dl
        espressif__esp_jpeg
        espressif__m5stack_core_s3
)
