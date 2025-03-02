#include "camera.h"
#include "sd_card.h"
#include "web_server.h"
#include <WiFi.h>
#include "time.h"
#include <soc/rtc_cntl_reg.h>  // Incluye el registro para desactivar el brownout detector

const char* ssid = "MIWIFI_2G_HJWj";
const char* password = "GcbFXuMP";
String myTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";

void initWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("Camera IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("WiFi connected");
}

void setTimezone(String timezone) {
    setenv("TZ", timezone.c_str(), 1);
    tzset();
}

void initTime(String timezone) {
    configTime(0, 0, "pool.ntp.org");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    setTimezone(timezone);
}

String getPictureFilename() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "";
    }
    char timeString[20];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    return "/picture_" + String(timeString) + ".jpg";
}

void setup() {
    Serial.begin(115200);
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector

    initWiFi();
    initTime(myTimezone);

    if (!initCamera()) {
        Serial.println("Camera initialization failed");
        return;
    }

    if (!initSDCard()) {
        Serial.println("SD Card initialization failed");
        return;
    }

    startWebServer();
    Serial.println("Web server started");
}

void loop() {
    if (isTimelapseRunning()) {
        static unsigned long lastCapture = 0;
        unsigned long now = millis();
        
        if (now - lastCapture >= getTimelapseInterval()) {
            lastCapture = now;
            
            // Capturar y guardar foto
            String path = getPictureFilename();
            if (!path.isEmpty()) {
                camera_fb_t* fb = capturePhoto();
                if (fb) {
                    savePhoto(path.c_str(), fb->buf, fb->len);
                    releasePhoto(fb);
                }
            }
        }
    }
}
