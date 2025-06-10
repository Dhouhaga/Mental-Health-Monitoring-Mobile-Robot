#include <esp32-hal-ledc.h>
#include <WebSocketsClient.h> 
extern bool face_detection_enabled;
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "Arduino.h"
#include <FirebaseESP32.h>

extern WiFiClient client;
extern char serverUrl[256]; 
extern FirebaseData firebaseData;
extern FirebaseConfig firebaseConfig;
extern FirebaseAuth firebaseAuth;

typedef struct {
  httpd_req_t *req;
  size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

extern WebSocketsClient fdClient;


httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;




static esp_err_t toggle_face_detection_handler(httpd_req_t *req) {
    char* buf;
    size_t buf_len;
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
                face_detection_enabled = (atoi(value) == 1);
                Serial.printf("Face detection %s\n", face_detection_enabled ? "enabled" : "disabled");
            }
        }
        free(buf);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}




static esp_err_t status_handler(httpd_req_t *req) {
  static char json_response[1024];

  sensor_t * s = esp_camera_sensor_get();
  char * p = json_response;
  *p++ = '{';

  p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
  p += sprintf(p, "\"quality\":%u,", s->status.quality);
  *p++ = '}';
  *p++ = 0;
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  return httpd_resp_send(req, json_response, strlen(json_response));
}

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>Surveillance Robot</title>
    <style>
      .button {user-select: none; background-color: #000; border: none; border-radius: 4px; color: #fff; padding: 10px 25px; font-size: 16px; margin: 4px; cursor: pointer;}
      .slider {appearance: none; width: 70%; height: 15px; border-radius: 10px; background: #d3d3d3; outline: none;}
      .slider::-webkit-slider-thumb {appearance: none; width: 30px; height: 30px; border-radius: 50%; background: #000;}
      .label {color: #000; font-size: 18px;}
      .toggle-container {display: flex; align-items: center; justify-content: center; margin: 10px 0;}
      .toggle-switch {position: relative; display: inline-block; width: 60px; height: 34px; margin: 0 10px;}
      .toggle-switch input {opacity: 0; width: 0; height: 0;}
      .slider-toggle {position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px;}
      .slider-toggle:before {position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: #fff; transition: .4s; border-radius: 50%;}
      input:checked + .slider-toggle {background-color: #2196F3;}
      input:checked + .slider-toggle:before {transform: translateX(26px);}
    </style>
    <script src="https://www.gstatic.com/firebasejs/8.10.0/firebase-app.js"></script>
    <script src="https://www.gstatic.com/firebasejs/8.10.0/firebase-database.js"></script>
    <script>
      window.addEventListener('DOMContentLoaded', () => {
        // 1) Initialize Firebase
        const firebaseConfig = {
          apiKey: "",
          authDomain: "",
          databaseURL: "",
          projectId: "",
          storageBucket: "",
          messagingSenderId: "",
          appId: ""
        };
        firebase.initializeApp(firebaseConfig);

        // 2) Grab the status element
        const statusEl = document.getElementById("faceStatus");

        // 3) Listen for face‑detection updates
        const faceRef = firebase.database().ref("/face_detection/status");
        faceRef.on('value',
          snapshot => {
            const data = snapshot.val() || { faces_detected: false };
            statusEl.innerText = data.faces_detected
              ? "Face Detected"
              : "No Faces Detected";
          },
          error => {
            console.error("Firebase read failed:", error);
            statusEl.innerText = "Status unavailable";
          }
        );
      });

      // Command WebSocket (port 81)
      let commandWebSocket;
      function initCommandWebSocket() {
        commandWebSocket = new WebSocket("ws://" + window.location.hostname + ":81/");
        commandWebSocket.onopen    = () => console.log('Command WS connected');
        commandWebSocket.onclose   = () => setTimeout(initCommandWebSocket, 2000);
        commandWebSocket.onerror   = e => console.error('Command WS error', e);
      }

      // Video WebSocket (port 82)
      let videoWebSocket;
      function initVideoWebSocket() {
        videoWebSocket = new WebSocket("ws://" + window.location.hostname + ":82/");
        videoWebSocket.binaryType = 'blob';
        videoWebSocket.onopen    = () => console.log('Video WS connected');
        videoWebSocket.onclose   = () => setTimeout(initVideoWebSocket, 2000);
        videoWebSocket.onmessage = event => {
          const img = document.getElementById('videoStream');
          if (img.src) URL.revokeObjectURL(img.src);
          img.src = URL.createObjectURL(event.data);
        };
      }

      function sendCommand(cmd, value) {
        if (commandWebSocket && commandWebSocket.readyState === WebSocket.OPEN) {
          commandWebSocket.send(cmd + ":" + value);
        }
      }
      function toggleFaceDetection(state) {
        sendCommand("face_detection", state ? 1 : 0);
      }

      window.onload = () => {
        initCommandWebSocket();
        initVideoWebSocket();
      };
    </script>
  </head>
  <body>
    <div align="center">
      <img id="videoStream" style="width:500px;">
    </div>
    <div align="center" style="margin-top:10px;">
      <div id="faceStatus" style="font-size:18px; color:#000;">No faces detected</div>
    </div>
    <br/>
    <div class="toggle-container">
      <label class="label">Face Detection</label>
      <label class="toggle-switch">
        <input type="checkbox" id="faceDetectionToggle" onchange="toggleFaceDetection(this.checked)">
        <span class="slider-toggle"></span>
      </label>
    </div>
    <br/>
    <div align="center">
      <button class="button" onmousedown="sendCommand('car',1)" onmouseup="sendCommand('car',0)">Forward</button>
    </div>
    <br/>
    <div align="center">
      <button class="button" onmousedown="sendCommand('car',2)" onmouseup="sendCommand('car',0)">Turn Left</button>
      <button class="button" onclick="sendCommand('car',0)">Stop</button>
      <button class="button" onmousedown="sendCommand('car',3)" onmouseup="sendCommand('car',0)">Turn Right</button>
    </div>
    <br/>
    <div align="center">
      <button class="button" onmousedown="sendCommand('car',4)" onmouseup="sendCommand('car',0)">Backward</button>
    </div>
    <br/>
    <div align="center">
      <label class="label">Flash</label>
      <input type="range" class="slider" min="0" max="255" value="0" oninput="sendCommand('flash',this.value)">
    </div>
    <br/>
    <div align="center">
      <label class="label">Speed</label>
      <input type="range" class="slider" min="0" max="255" value="255" oninput="sendCommand('speed',this.value)">
    </div>
  </body>
</html>
)rawliteral";


// replace %IP% with actual IP
static esp_err_t index_handler(httpd_req_t *req){
  
    String html = String(INDEX_HTML);
    html.replace("%IP%", WiFi.localIP().toString().c_str());
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html.c_str(), html.length());
}


void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t toggle_fd_uri = {
        .uri       = "/toggle_fd",
        .method    = HTTP_GET,
        .handler   = toggle_face_detection_handler,
        .user_ctx  = NULL
    };

    
    config.server_port = 80;  // Main HTTP server
    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        httpd_register_uri_handler(camera_httpd, &toggle_fd_uri);
    }
}
