#include "sd_card.h"

bool initSDCard() {
    if (!SD_MMC.begin()) {
        Serial.println("SD Card Mount Failed");
        return false;
    }
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD Card attached");
        return false;
    }
    return true;
}

bool savePhoto(const char* path, const uint8_t* data, size_t len) {
    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file in writing mode");
        return false;
    }
    if (file.write(data, len) != len) {
        Serial.println("Write failed");
        return false;
    }
    file.close();
    return true;
}
