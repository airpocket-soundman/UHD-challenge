# Main component CMakeLists.txt
idf_component_register(
    SRCS
        "realtime_camera_bsp.cpp"
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
        LovyanGFX
)

# LovyanGFX の RGBパネル実装は不要なので無効化
idf_component_get_property(LGFX_LIB LovyanGFX COMPONENT_LIB)
target_compile_definitions(${LGFX_LIB} PRIVATE LGFX_USE_RGB_PANEL=0)
