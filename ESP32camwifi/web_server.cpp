#include "web_server.h"
#include "camera.h"
#include <Arduino.h>

httpd_handle_t server = NULL;
bool timelapseRunning = false;
bool streamingEnabled = false;
unsigned long timelapseInterval = 10000; // 10 segundos por defecto
bool hflip = false;
bool vflip = false;

const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32-CAM Control</title>
    <style>
        body { font-family: Arial, sans-serif; padding: 20px; background: #1a1a1a; color: #fff; }
        .container { max-width: 800px; margin: 0 auto; }
        .section { background: #2a2a2a; padding: 20px; margin-bottom: 20px; border-radius: 8px; }
        button { padding: 10px 20px; margin: 5px; border: none; border-radius: 4px; cursor: pointer; }
        .btn-start { background: #28a745; color: white; }
        .btn-stop { background: #dc3545; color: white; }
        #stream { width: 100%; max-width: 640px; margin: 20px 0; border: 2px solid #444; }
    </style>
</head>
<body>
    <div class="container">
        <div class="section">
            <h2>Video en Vivo</h2>
            <button class="btn-start" onclick="startStream()">▶ Iniciar Cámara</button>
            <button class="btn-stop" onclick="stopStream()">⏹ Detener Cámara</button>
            <br>
            <img id="stream" src="" alt="Stream en vivo">
        </div>

        <div class="section">
            <h2>Configuración Timelapse</h2>
            <label>Intervalo (segundos): </label>
            <input type="number" id="interval" min="1" value="10">
            <button onclick="setTimelapseInterval()">Establecer</button>
            <br>
            <button class="btn-start" onclick="startTimelapse()">⏳ Iniciar Timelapse</button>
            <button class="btn-stop" onclick="stopTimelapse()">⏹ Detener Timelapse</button>
        </div>

        <div class="section">
            <h2>Ajustes de Cámara</h2>
            <label>Resolución: </label>
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
            <label>Calidad (0-63): </label>
            <input type="number" id="quality" min="0" max="63" value="10">
            <button onclick="applySettings()">Aplicar</button>
            <br>
            <label><input type="checkbox" id="hflip" onchange="toggleHFlip()"> Volteo Horizontal</label>
            <br>
            <label><input type="checkbox" id="vflip" onchange="toggleVFlip()"> Volteo Vertical</label>
        </div>
    </div>

    <script>
        let streamInterval;
        const streamImg = document.getElementById('stream');

        function startStream() {
            streamImg.src = '/stream?';
            streamImg.style.display = 'block';
        }

        function stopStream() {
            streamImg.src = '';
            streamImg.style.display = 'none';
            fetch('/stop_stream');
        }

        function startTimelapse() {
            fetch('/start_timelapse')
                .then(response => console.log('Timelapse iniciado'))
                .catch(error => console.error('Error:', error));
        }

        function stopTimelapse() {
            fetch('/stop_timelapse')
                .then(response => console.log('Timelapse detenido'))
                .catch(error => console.error('Error:', error));
        }

        function setTimelapseInterval() {
          const intervalInput = document.getElementById('interval');
          const interval = intervalInput.value * 1000;
          console.log('Enviando intervalo:', interval);

          fetch(`/set_interval?interval=${interval}`)
            .then(response => {
            if (response.ok) {
                console.log('Intervalo actualizado');
            } else {
                console.error('Error al actualizar el intervalo');
            }
        })
        .catch(error => console.error('Error:', error));
}


        function applySettings() {
            const resolution = document.getElementById('resolution').value;
            const quality = document.getElementById('quality').value;
            fetch(`/set_camera?resolution=${resolution}&quality=${quality}`)
                .then(response => console.log('Ajustes aplicados'))
                .catch(error => console.error('Error:', error));
        }

        function toggleHFlip() {
            const hflip = document.getElementById('hflip').checked ? 1 : 0;
            fetch(`/set_hflip?hflip=${hflip}`)
                .then(response => console.log('Volteo horizontal actualizado'))
                .catch(error => console.error('Error:', error));
        }

        function toggleVFlip() {
            const vflip = document.getElementById('vflip').checked ? 1 : 0;
            fetch(`/set_vflip?vflip=${vflip}`)
                .then(response => console.log('Volteo vertical actualizado'))
                .catch(error => console.error('Error:', error));
        }
    </script>
</body>
</html>
)rawliteral";

// Manejador para la ruta raíz ("/")
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, strlen(index_html));
}

// Manejador del stream de video
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char *part_buf[64];

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    if(res != ESP_OK) return res;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    streamingEnabled = true;  // Asegúrate de que el streaming esté habilitado

    while(streamingEnabled) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Error en captura de cámara");
            res = ESP_FAIL;
            break;
        }

        size_t hlen = snprintf((char *)part_buf, 64,
            "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            fb->len);

        if(httpd_resp_send_chunk(req, (const char *)part_buf, hlen) != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        if(httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        esp_camera_fb_return(fb);
        delay(1);
    }

    streamingEnabled = false;  // Desactiva el streaming al salir del bucle
    return res;
}

// Manejador para detener el stream
static esp_err_t stop_stream_handler(httpd_req_t *req) {
    streamingEnabled = false;
    return httpd_resp_send(req, "Stream detenido", HTTPD_RESP_USE_STRLEN);
}

// Manejador para iniciar el timelapse
static esp_err_t start_timelapse_handler(httpd_req_t *req) {
    timelapseRunning = true;
    return httpd_resp_send(req, "Timelapse iniciado", HTTPD_RESP_USE_STRLEN);
}

// Manejador para detener el timelapse
static esp_err_t stop_timelapse_handler(httpd_req_t *req) {
    timelapseRunning = false;
    return httpd_resp_send(req, "Timelapse detenido", HTTPD_RESP_USE_STRLEN);
}

// Manejador para establecer el intervalo del timelapse
static esp_err_t set_interval_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
    if (ret == ESP_OK) {
        Serial.printf("Query string: %s\n", buf);
        
        char interval[10];
        if (httpd_query_key_value(buf, "interval", interval, sizeof(interval)) == ESP_OK) {
            timelapseInterval = atol(interval);  // Convertir el valor recibido a entero
            Serial.printf("Nuevo intervalo recibido: %lu ms\n", timelapseInterval);
        } else {
            Serial.println("Error al obtener el valor de 'interval'");
        }
    } else {
        Serial.println("Error al obtener la query string");
    }
    return httpd_resp_send(req, "Intervalo actualizado", HTTPD_RESP_USE_STRLEN);
}

// Manejador para aplicar ajustes de cámara
static esp_err_t set_camera_handler(httpd_req_t *req) {
    char buf[50];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char resolution[10];
        char quality[10];
        if (httpd_query_key_value(buf, "resolution", resolution, sizeof(resolution)) == ESP_OK &&
            httpd_query_key_value(buf, "quality", quality, sizeof(quality)) == ESP_OK) {
            setCameraResolution((framesize_t)atoi(resolution));
            setCameraQuality(atoi(quality));
        }
    }
    return httpd_resp_send(req, "Ajustes aplicados", HTTPD_RESP_USE_STRLEN);
}

// Manejador para volteo horizontal
static esp_err_t set_hflip_handler(httpd_req_t *req) {
    char buf[10];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char hflip_val[2];
        if (httpd_query_key_value(buf, "hflip", hflip_val, sizeof(hflip_val)) == ESP_OK) {
            setHFlip(atoi(hflip_val));
        }
    }
    return httpd_resp_send(req, "Volteo horizontal actualizado", HTTPD_RESP_USE_STRLEN);
}

// Manejador para volteo vertical
static esp_err_t set_vflip_handler(httpd_req_t *req) {
    char buf[10];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char vflip_val[2];
        if (httpd_query_key_value(buf, "vflip", vflip_val, sizeof(vflip_val)) == ESP_OK) {
            setVFlip(atoi(vflip_val));
        }
    }
    return httpd_resp_send(req, "Volteo vertical actualizado", HTTPD_RESP_USE_STRLEN);
}

// Implementación de las funciones faltantes
bool isTimelapseRunning() {
    return timelapseRunning;
}

unsigned long getTimelapseInterval() {
    return timelapseInterval;
}

void setTimelapseInterval(unsigned long interval) {
    timelapseInterval = interval;
}

void setHFlip(bool flip) {
    hflip = flip;
    sensor_t *s = esp_camera_sensor_get();
    s->set_hmirror(s, flip);
}

void setVFlip(bool flip) {
    vflip = flip;
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, flip);
}

void stopCameraStream() {
    streamingEnabled = false;
}

void startWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;

    if (httpd_start(&server, &config) == ESP_OK) {
        Serial.println("Servidor web iniciado correctamente");

        // Registrar todas las rutas
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);

        httpd_uri_t stop_stream_uri = {
            .uri = "/stop_stream",
            .method = HTTP_GET,
            .handler = stop_stream_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stop_stream_uri);

        httpd_uri_t start_timelapse_uri = {
            .uri = "/start_timelapse",
            .method = HTTP_GET,
            .handler = start_timelapse_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &start_timelapse_uri);

        httpd_uri_t stop_timelapse_uri = {
            .uri = "/stop_timelapse",
            .method = HTTP_GET,
            .handler = stop_timelapse_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stop_timelapse_uri);

        httpd_uri_t set_interval_uri = {
            .uri = "/set_interval",
            .method = HTTP_GET,
            .handler = set_interval_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &set_interval_uri);

        httpd_uri_t set_camera_uri = {
            .uri = "/set_camera",
            .method = HTTP_GET,
            .handler = set_camera_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &set_camera_uri);

        httpd_uri_t set_hflip_uri = {
            .uri = "/set_hflip",
            .method = HTTP_GET,
            .handler = set_hflip_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &set_hflip_uri);

        httpd_uri_t set_vflip_uri = {
            .uri = "/set_vflip",
            .method = HTTP_GET,
            .handler = set_vflip_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &set_vflip_uri);
    } else {
        Serial.println("Error al iniciar el servidor web");
    }
}
