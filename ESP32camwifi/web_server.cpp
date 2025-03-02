#include "web_server.h"
#include "camera.h"
#include <Arduino.h>

httpd_handle_t server = NULL;
bool timelapseRunning = false;
unsigned long timelapseInterval = 10000; // 10 segundos por defecto
bool hflip = false;
bool vflip = false;

const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32-CAM Timelapse</title>
    <style>
        body { font-family: Arial, sans-serif; padding: 20px; }
        .settings { margin-bottom: 20px; border: 1px solid #ccc; padding: 15px; border-radius: 5px; }
        label { display: inline-block; width: 200px; margin: 5px 0; }
        button { padding: 8px 15px; margin: 5px; background: #007bff; color: white; border: none; border-radius: 3px; cursor: pointer; }
        button:hover { background: #0056b3; }
    </style>
</head>
<body>
    <h1>ESP32-CAM Timelapse</h1>
    
    <div class="settings">
        <h2>Timelapse Settings</h2>
        <label for="interval">Interval (seconds):</label>
        <input type="number" id="interval" min="1" value="10">
        <button onclick="setInterval()">Set Interval</button>
        <br>
        <button onclick="startTimelapse()">Start Timelapse</button>
        <button onclick="stopTimelapse()">Stop Timelapse</button>
    </div>

    <div class="settings">
        <h2>Camera Settings</h2>
        <label for="resolution">Resolution:</label>
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
        <br>
        <label for="quality">Quality (0-63):</label>
        <input type="number" id="quality" min="0" max="63" value="10">
        <button onclick="applySettings()">Apply Settings</button>
        <br>
        <label><input type="checkbox" id="hflip" onchange="toggleHFlip()"> Horizontal Flip</label>
        <br>
        <label><input type="checkbox" id="vflip" onchange="toggleVFlip()"> Vertical Flip</label>
    </div>

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

        function setInterval() {
            const interval = document.getElementById('interval').value * 1000;
            fetch(`/set_interval?interval=${interval}`)
                .then(response => console.log('Interval updated'))
                .catch(error => console.error('Error:', error));
        }

        function applySettings() {
            const resolution = document.getElementById('resolution').value;
            const quality = document.getElementById('quality').value;
            fetch(`/set_camera?resolution=${resolution}&quality=${quality}`)
                .then(response => console.log('Settings applied'))
                .catch(error => console.error('Error:', error));
        }

        function toggleHFlip() {
            const hflip = document.getElementById('hflip').checked ? 1 : 0;
            fetch(`/set_hflip?hflip=${hflip}`)
                .then(response => console.log('Horizontal flip toggled'))
                .catch(error => console.error('Error:', error));
        }

        function toggleVFlip() {
            const vflip = document.getElementById('vflip').checked ? 1 : 0;
            fetch(`/set_vflip?vflip=${vflip}`)
                .then(response => console.log('Vertical flip toggled'))
                .catch(error => console.error('Error:', error));
        }
    </script>
</body>
</html>
)rawliteral";

// Manejadores HTTP
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

esp_err_t set_interval_handler(httpd_req_t* req) {
    char query[100];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char interval[10];
        if (httpd_query_key_value(query, "interval", interval, sizeof(interval)) == ESP_OK) {
            timelapseInterval = atoi(interval);
        }
    }
    return httpd_resp_send(req, "Interval updated", HTTPD_RESP_USE_STRLEN);
}

esp_err_t set_hflip_handler(httpd_req_t* req) {
    char query[100];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char hflip_value[10];
        if (httpd_query_key_value(query, "hflip", hflip_value, sizeof(hflip_value)) == ESP_OK) {
            hflip = (atoi(hflip_value) == 1);
            setHFlip(hflip);
        }
    }
    return httpd_resp_send(req, "Horizontal flip updated", HTTPD_RESP_USE_STRLEN);
}

esp_err_t set_vflip_handler(httpd_req_t* req) {
    char query[100];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char vflip_value[10];
        if (httpd_query_key_value(query, "vflip", vflip_value, sizeof(vflip_value)) == ESP_OK) {
            vflip = (atoi(vflip_value) == 1);
            setVFlip(vflip);
        }
    }
    return httpd_resp_send(req, "Vertical flip updated", HTTPD_RESP_USE_STRLEN);
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

    httpd_uri_t set_interval_uri = {
        .uri = "/set_interval",
        .method = HTTP_GET,
        .handler = set_interval_handler,
        .user_ctx = NULL
    };

    httpd_uri_t set_hflip_uri = {
        .uri = "/set_hflip",
        .method = HTTP_GET,
        .handler = set_hflip_handler,
        .user_ctx = NULL
    };

    httpd_uri_t set_vflip_uri = {
        .uri = "/set_vflip",
        .method = HTTP_GET,
        .handler = set_vflip_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &start_timelapse_uri);
        httpd_register_uri_handler(server, &stop_timelapse_uri);
        httpd_register_uri_handler(server, &set_camera_uri);
        httpd_register_uri_handler(server, &set_interval_uri);
        httpd_register_uri_handler(server, &set_hflip_uri);
        httpd_register_uri_handler(server, &set_vflip_uri);
    }
}

// Funciones de control del timelapse
void stopTimelapse() { timelapseRunning = false; }
void startTimelapse() { timelapseRunning = true; }
bool isTimelapseRunning() { return timelapseRunning; }
void setTimelapseInterval(unsigned long interval) { timelapseInterval = interval; }
unsigned long getTimelapseInterval() { return timelapseInterval; }

// Funciones de volteo
void setHFlip(bool flip) {
    sensor_t* s = esp_camera_sensor_get();
    if (s) s->set_hmirror(s, flip);
}

void setVFlip(bool flip) {
    sensor_t* s = esp_camera_sensor_get();
    if (s) s->set_vflip(s, flip);
}
