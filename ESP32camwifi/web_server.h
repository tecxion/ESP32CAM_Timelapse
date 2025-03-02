#pragma once
#include "esp_http_server.h"

void startWebServer();
void stopTimelapse();
void startTimelapse();
bool isTimelapseRunning();
void setTimelapseInterval(unsigned long interval);
unsigned long getTimelapseInterval();
void setHFlip(bool flip);
void setVFlip(bool flip);
