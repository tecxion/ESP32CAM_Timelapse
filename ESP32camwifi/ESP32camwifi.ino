#include "camera.h"
#include "sd_card.h"
#include "web_server.h"
#include <WiFi.h>
#include "time.h"
#include <soc/rtc_cntl_reg.h>  // Desactiva el brownout detector

/**
 * Creado Por TecXarT
 * version 1.0
 * más info en https://github.com/tecxion/ESP32CAM_Timelapse
 */
 
// Configuración de WiFi
const char* ssid = "TU_SSID"; // Pon el nombre de tu red
const char* password = "TU_CONTRASEÑA_WIFI"; // Pon la contraseña de tu wifi
String myTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";

// Inicializa la conexión WiFi
void initWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado");
    Serial.print("Dirección IP de la Cámara: ");
    Serial.println(WiFi.localIP());
}

// Configura la zona horaria
void setTimezone(const String& timezone) {
    setenv("TZ", timezone.c_str(), 1);
    tzset();
}

// Sincroniza la hora usando NTP
void initTime(const String& timezone) {
    configTime(0, 0, "pool.ntp.org");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Error al obtener la Fecha");
        return;
    }
    setTimezone(timezone);
}

// Genera el nombre del archivo para la imagen capturada
String getPictureFilename() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Error con al poner la fecha");
        return "";
    }
    char timeString[25];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d_%H-%M-%S.jpg", &timeinfo);
    return String("/picture_") + timeString;
}

void setup() {
    Serial.begin(115200);
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desactiva el brownout detector

    initWiFi();
    initTime(myTimezone);

    if (!initCamera()) {
        Serial.println("Fallo al iniciar la cámara, inserta sd");
        while (true);  // Detiene el setup si la cámara falla
    }

    if (!initSDCard()) {
        Serial.println("Error al cargar la SD");
        while (true);  // Detiene el setup si la SD falla
    }

    startWebServer();
    Serial.println("Servidor web iniciado");
}

void loop() {
    static unsigned long lastCapture = 0;
    unsigned long now = millis();
    unsigned long interval = getTimelapseInterval();

    if (isTimelapseRunning() && (now - lastCapture >= interval)) {
        lastCapture = now;

        // Captura y guarda la foto
        String path = getPictureFilename();
        if (!path.isEmpty()) {
            camera_fb_t* fb = capturePhoto();
            if (fb) {
                if (savePhoto(path.c_str(), fb->buf, fb->len)) {
                    Serial.print("Captura:  ");
                    Serial.println(path);
                } else {
                    Serial.println("Error guardado de foto");
                }
                releasePhoto(fb);
            } else {
                Serial.println("Error en la captura");
            }
        }
    }
}
