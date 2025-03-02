#pragma once
#include "esp_camera.h"

bool initCamera();
camera_fb_t* capturePhoto();
void releasePhoto(camera_fb_t* fb);
void setCameraResolution(framesize_t resolution);
void setCameraQuality(int quality);
