#pragma once

#include "esp_camera.h"

class GC0308 {
public:
    bool begin();
    bool get();
    bool free();

    camera_fb_t* fb = nullptr;
    sensor_t* sensor = nullptr;
};

