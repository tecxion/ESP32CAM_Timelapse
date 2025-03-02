#include "web_server.h"
#include "camera.h"
#include <Arduino.h>

httpd_handle_t server = NULL;
bool timelapseRunning = false;

const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32-CAM Timelapse</title>
</head>
<body>
    <h1>ESP32-CAM Timelapse</h1>
    <button onclick="startTimelapse()">Iniciar Timelapse</button>
    <button onclick="stopTimelapse()">Parar Timelapse</button>
    <br><br>
    <label for="resolution">Resolución:</label>
    <select id="resolution">
        <option value="10">UXGA (1600x1200)</option>
        <option value="9">SXGA (1280x1024)</option>
        <option value="8">XGA (1024x768)</option>
        <option value="7">SVGA (800x600)</option>
        <option value="6">VGA (640x480)</option>
        <option value="5">CIF (400x296)</option>
        <option value="4">QVGA (320x240)</option>
        <option value="3">HQVGA (240x176)</option>
        <option value="0">QQVGA (160x120)</option>
    </select>
    <br><br>
    <label for="quality">Calidad (0-63):</label>
    <input type="number" id="quality" min="0" max="63" value="10">
    <br><br>
    <button onclick="applySettings()">Aplicar Opciones</button>

    <script>
        function startTimelapse() {
            fetch('/start_timelapse')
                .then(response => console.log('Timelapse started'))
                .catch(error => console.error('Error:', error));
        }

        function stopTimelapse() {
            fetch('/stop_timelapse')
                .then(response => console.log('Timelapse stopped'))
                .catch(error => console.error('Error:', error));
        }

        function applySettings() {
            const resolution = document.getElementById('resolution').value;
            const quality = document.getElementById('quality').value;
            fetch(`/set_camera?resolution=${resolution}&quality=${quality}`)
                .then(response => console.log('Settings applied'))
                .catch(error => console.error('Error:', error));
        }
    </script>
</body>
</html>
)rawliteral";

esp_err_t index_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, strlen(index_html));
}

esp_err_t start_timelapse_handler(httpd_req_t* req) {
    startTimelapse();
    return httpd_resp_send(req, "Timelapse started", HTTPD_RESP_USE_STRLEN);
}

esp_err_t stop_timelapse_handler(httpd_req_t* req) {
    stopTimelapse();
    return httpd_resp_send(req, "Timelapse stopped", HTTPD_RESP_USE_STRLEN);
}

esp_err_t set_camera_handler(httpd_req_t* req) {
    char query[100];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char resolution[10], quality[10];
        if (httpd_query_key_value(query, "resolution", resolution, sizeof(resolution)) == ESP_OK &&
            httpd_query_key_value(query, "quality", quality, sizeof(quality)) == ESP_OK) {
            setCameraResolution((framesize_t)atoi(resolution));
            setCameraQuality(atoi(quality));
        }
    }
    return httpd_resp_send(req, "Settings applied", HTTPD_RESP_USE_STRLEN);
}

void startWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };

    httpd_uri_t start_timelapse_uri = {
        .uri = "/start_timelapse",
        .method = HTTP_GET,
        .handler = start_timelapse_handler,
        .user_ctx = NULL
    };

    httpd_uri_t stop_timelapse_uri = {
        .uri = "/stop_timelapse",
        .method = HTTP_GET,
        .handler = stop_timelapse_handler,
        .user_ctx = NULL
    };

    httpd_uri_t set_camera_uri = {
        .uri = "/set_camera",
        .method = HTTP_GET,
        .handler = set_camera_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &start_timelapse_uri);
        httpd_register_uri_handler(server, &stop_timelapse_uri);
        httpd_register_uri_handler(server, &set_camera_uri);
    }
}

void stopTimelapse() {
    timelapseRunning = false;
}

void startTimelapse() {
    timelapseRunning = true;
}

bool isTimelapseRunning() {
    return timelapseRunning;
}
