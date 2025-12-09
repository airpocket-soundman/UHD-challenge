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
)
