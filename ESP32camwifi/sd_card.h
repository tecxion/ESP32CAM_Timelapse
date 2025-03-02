#pragma once
#include "FS.h"
#include "SD_MMC.h"

bool initSDCard();
bool savePhoto(const char* path, const uint8_t* data, size_t len);
