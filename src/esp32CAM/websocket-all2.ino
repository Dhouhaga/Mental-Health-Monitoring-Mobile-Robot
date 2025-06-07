#include "esp_camera.h"
#include "esp_wifi.h"
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <WebSocketsServer.h>
#include <WebSocketsClient.h>  // Add WebSocket client support
#include <ArduinoJson.h>

WebSocketsClient fdClient;  // Face detection WebSocket client
char fd_server_ip[256] = {0};
uint16_t fd_server_port = 0;

// Command WebSocket on port 81
WebSocketsServer commandWebSocket = WebSocketsServer(81); 

// Video WebSocket on port 82
WebSocketsServer videoWebSocket = WebSocketsServer(82); 

// Wifi Crdentials 
/*
const char* ssid = "TT_62F8";
const char* password = "gannjvd19b";
*/

const char* ssid = "dg";
const char* password = "1dhouhafr";

WiFiClient client;

char serverUrl[256] = {0}; // Adjust size based on expected URL length

// Firebase credentials
#define FIREBASE_HOST "iot-projects-6ba0c-default-rtdb.europe-west1.firebasedatabase.app"
#define FIREBASE_AUTH "JiOh1j4PsSfKXEtYJbxubsUVSWpd3UarFalfOjXo"
// Declare Firebase objects
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// Camera Config
#define CAMERA_MODEL_AI_THINKER

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void startCameraServer();

/* Defining motor pins */
const int inh1 = 14;
const int inh2 = 15;
const int inh3 = 13;
const int inh4 = 12;
const int FlashPin = 4; // 4 for flash led or 33 for normal led

const uint8_t motorPatterns[6][4] = {
    {0,0,0,0},     // Stop
    {0,1,1,0},     // Forward 
    {0,0,1,0},     // Left
    {0,1,0,0},     // Right
    {1,0,0,1},     // Backward
    {0,0,0,0}      // Stop
};

bool face_detection_enabled = false;

void initMotors()
{

  /* Attaching the channel to the GPIO to be controlled */
  /* ledcAttach(GPIO, freq, resolution) */
 /* ledcAttach(inh1, 2000, 8);
  ledcAttach(inh2, 2000, 8);
  ledcAttach(inh3, 2000, 8);
  ledcAttach(inh4, 2000, 8);
  //ledcAttach(ENA, 1000, 8);
  //ledcAttach(ENB, 1000, 8);*/

  /*ledcWrite(ENA, speed);  // Enable right motors
  ledcWrite(ENB, speed);  // Enable left motors*/

  pinMode(inh1, OUTPUT);
  pinMode(inh2, OUTPUT);
  pinMode(inh3, OUTPUT);
  pinMode(inh4, OUTPUT);

}


void initFlash() {
  ledcSetup(7, 5000, 8); /* 5000 hz PWM, 8-bit resolution and range from 0 to 255 */
  ledcAttachPin(FlashPin, 7);
}

void commandWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    static int speed = 255;  // Static variable to retain speed value
    
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[CMD][%u] Disconnected!\n", num);
            break;
            
        case WStype_CONNECTED: {
            IPAddress ip = commandWebSocket.remoteIP(num);
            Serial.printf("[CMD][%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
            break;
        }
            
        case WStype_TEXT:
            Serial.printf("[CMD][%u] Received text: %s\n", num, payload);
            
            // Parse command:value
            String message = (char*)payload;
            int separatorIndex = message.indexOf(':');
            if(separatorIndex == -1) return;
            
            String command = message.substring(0, separatorIndex);
            String valueStr = message.substring(separatorIndex + 1);
            int value = valueStr.toInt();

            // Handle commands
            if(command == "car") {
                if(value >= 0 && value <= 5) {
                    digitalWrite(inh1, motorPatterns[value][0]);
                    digitalWrite(inh2, motorPatterns[value][1] ? speed : 0);
                    digitalWrite(inh3, motorPatterns[value][2] ? speed : 0); 
                    digitalWrite(inh4, motorPatterns[value][3]);
                }
            }
            else if(command == "speed") {
                speed = constrain(value, 0, 255);
            }
            else if(command == "flash") {
                ledcWrite(7, constrain(value, 0, 255));
            }
            else if(command == "face_detection") {
                face_detection_enabled = (value == 1);
            }
            break;
    }
}

void videoWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[VID][%u] Disconnected!\n", num);
            break;
            
        case WStype_CONNECTED:
            Serial.printf("[VID][%u] Client connected\n", num);
            break;
            
        case WStype_BIN:
            // Optional: Handle binary messages from client
            break;
    }
}

void sendFrameOverWebSocket() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;

    // Broadcast frame to all video clients
    videoWebSocket.broadcastBIN(fb->buf, fb->len);
    esp_camera_fb_return(fb);
}



void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println();

    camera_config_t config;
      config.ledc_channel = LEDC_CHANNEL_0;
      config.ledc_timer = LEDC_TIMER_0;
      config.pin_d0 = Y2_GPIO_NUM;
      config.pin_d1 = Y3_GPIO_NUM;
      config.pin_d2 = Y4_GPIO_NUM;
      config.pin_d3 = Y5_GPIO_NUM;
      config.pin_d4 = Y6_GPIO_NUM;
      config.pin_d5 = Y7_GPIO_NUM;
      config.pin_d6 = Y8_GPIO_NUM;
      config.pin_d7 = Y9_GPIO_NUM;
      config.pin_xclk = XCLK_GPIO_NUM;
      config.pin_pclk = PCLK_GPIO_NUM;
      config.pin_vsync = VSYNC_GPIO_NUM;
      config.pin_href = HREF_GPIO_NUM;
      config.pin_sscb_sda = SIOD_GPIO_NUM; // Note: Changed from pin_sccb_sda
      config.pin_sscb_scl = SIOC_GPIO_NUM; // Note: Changed from pin_sccb_scl
      config.pin_pwdn = PWDN_GPIO_NUM;
      config.pin_reset = RESET_GPIO_NUM;
      config.xclk_freq_hz = 20000000;
      config.pixel_format = PIXFORMAT_JPEG;
      config.frame_size = FRAMESIZE_HQVGA;
      config.jpeg_quality = 12;
      config.fb_count = 1;

    // if PSRAM is present, give yourself two frame‑buffers:
      if(psramFound()){
        config.fb_count    = 2;
        config.frame_size  = FRAMESIZE_QVGA;   // QVGA is plenty for WebSockets
        config.jpeg_quality= 10;               // dial quality back a notch
      } else {
        config.fb_count    = 1;
        config.frame_size  = FRAMESIZE_QQVGA;  // even smaller if no PSRAM
        config.jpeg_quality= 12;
      }
      
      esp_err_t err = esp_camera_init(&config);
      if(err != ESP_OK){
        Serial.printf("Camera init failed: 0x%x\n", err);
        return;
      }
  /* Initializing motor,servo and led */
  initMotors();
  initFlash();

  /* Connecting to WiFi */
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    //yield(); // Prevent WDT reset
}
  Serial.println("");
  Serial.println("WiFi connected");
  WiFi.setSleep(false);
      // Get ESP32's local IP address
    String espIP = WiFi.localIP().toString();
    Serial.println("ESP32 IP Address: " + espIP);
      
    commandWebSocket.begin();
    commandWebSocket.onEvent(commandWebSocketEvent);
    
    videoWebSocket.begin();
    videoWebSocket.onEvent(videoWebSocketEvent);
    
    // Set Firebase configuration
    firebaseConfig.host = FIREBASE_HOST;
    firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

    // Connect to Firebase
    Firebase.begin(&firebaseConfig, &firebaseAuth);
    Firebase.reconnectWiFi(true);

    if (Firebase.setString(firebaseData, "/server/esp_ip", espIP)) {
        Serial.println("ESP32 IP Address sent to Firebase: " + espIP);
    } else {
        Serial.println("Failed to send ESP32 IP Address to Firebase");
        Serial.println(firebaseData.errorReason());
    }

    // After fetching serverUrl in setup()
    if (Firebase.getString(firebaseData, "/server/fd_ip")) {
        String fd_ip = firebaseData.stringData();
        int colonIndex = fd_ip.indexOf(':');
        if (colonIndex != -1) {
            strncpy(fd_server_ip, fd_ip.substring(0, colonIndex).c_str(), sizeof(fd_server_ip));
            fd_server_port = fd_ip.substring(colonIndex + 1).toInt();
        } else {
            strncpy(fd_server_ip, fd_ip.c_str(), sizeof(fd_server_ip));
            fd_server_port = 83;  // Default port
        }
        Serial.printf("Face Detection Server: %s:%d\n", fd_server_ip, fd_server_port);
    }
    else{
      Serial.print("face detection server ip: ");
      String fd_ip;
        fd_ip = Serial.readStringUntil('\n');  // Blocks until Enter is pressed
        Serial.print("You entered: ");
        Serial.println(fd_ip);
        int colonIndex = fd_ip.indexOf(':');
        if (colonIndex != -1) {
            strncpy(fd_server_ip, fd_ip.substring(0, colonIndex).c_str(), sizeof(fd_server_ip));
            fd_server_port = fd_ip.substring(colonIndex + 1).toInt();
        } else {
            strncpy(fd_server_ip, fd_ip.c_str(), sizeof(fd_server_ip));
            fd_server_port = 83;  // Default port
        }
        Serial.printf("Face Detection Server: %s:%d\n", fd_server_ip, fd_server_port);
      }
  startCameraServer();/* Starting camera server */

  Serial.print("Camera Ready! Use 'http://"); Serial.print(WiFi.localIP());Serial.println("' to connect");

  fdClient.begin("192.168.43.252", 8765, "/");
  fdClient.onEvent([](WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("FD WebSocket Disconnected!");
            break;
        case WStype_CONNECTED:
            Serial.println("FD WebSocket Connected!");
            break;
        case WStype_TEXT: {
            // Parse JSON
            DynamicJsonDocument doc(256);
            deserializeJson(doc, payload, length);
            bool facesDetected = doc["faces_detected"];
            
            // Send status to command WebSocket clients
            String msg = "face_status:" + String(facesDetected ? "1" : "0");
            commandWebSocket.broadcastTXT(msg);
            break;
        }
    }
});
  fdClient.setReconnectInterval(5000);

    /* Flash led */
   int i;
    for ( i= 0; i < 5; i++) {
      ledcWrite(4, 10);
      delay(50);
      ledcWrite(4, 0);
      delay(50);
    }
    
}

// Add this global variable at the top
unsigned long lastFaceDetectTime = 0;

void loop() {
  commandWebSocket.loop();
    videoWebSocket.loop();
    fdClient.loop();

    camera_fb_t *fb = esp_camera_fb_get();
    // Send video frame
    videoWebSocket.broadcastBIN(fb->buf, fb->len);

    // Conditionally send to face detection server
    if (face_detection_enabled && fdClient.isConnected()) {
      if (millis() - lastFaceDetectTime >= 1000) { // ~15 FPS
        fdClient.sendBIN(fb->buf, fb->len);
        lastFaceDetectTime = millis();
      }
    }

    // Release frame buffer
    esp_camera_fb_return(fb);
}
