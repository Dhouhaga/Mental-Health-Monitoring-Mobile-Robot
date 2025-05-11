# ESP32-CAM Mental Health Monitoring Mobile Robot

A mobile four-wheeled robot based on ESP32-CAM that collects environmental and facial data, computes mental health scores, visualizes them in real time with InfluxDB and Grafana, and sends alerts when scores drop below defined thresholds.

---

## Features

* **Environmental Sensing**: Measures temperature, humidity (DHT11), illumination (photoresistor) and air quality (MQ135) via ESP8266 (simulated script).
* **Facial Recognition**: Captures frames on ESP32-CAM and sends them to a local face detection server, then to a face recognition server for per-person mental health scoring.
* **Data Storage & Visualization**: Pushes sensor and mental health data to InfluxDB and visualizes dashboards in Grafana (global and per-person views).
* **Alerts**: Sends email notifications when a person’s mental health score falls below configurable thresholds.
* **Remote Control**: Hosts a custom web interface on the ESP32-CAM for controlling robot movement and live video feed.

---

## Table of Contents

1. [Hardware Components](#hardware-components)
2. [Software Stack](#software-stack)
3. [System Architecture](#system-architecture)
4. [Setup & Installation](#setup--installation)
5. [Usage](#usage)
6. [Configuration](#configuration)
7. [Simulated Sensor Data](#simulated-sensor-data)
8. [License](#license)
9. [Acknowledgements](#acknowledgements)

---

## Hardware Components

* **Robot Platform**: 4-wheel chassis with motor drivers
* **Camera & Controller**: ESP32-CAM module
* **Sensor Node**: ESP8266 (currently simulated)

  * DHT11 (Temperature & Humidity)
  * Photoresistor (Light / Darkness)
  * MQ135 (Air Quality)

---

## Software Stack

* **Firmware**:

  * ESP32-CAM Arduino sketch for camera, web server, and InfluxDB client
  * ESP8266 Arduino sketch (or Python simulation) for sensors
* **Back-End Servers**:

  * Local Face Detection Server (e.g., OpenCV)
  * Local Face Recognition Server (e.g., FaceNet)
* **Data Storage**:

  * InfluxDB (time-series database)
* **Visualization**:

  * Grafana dashboards (global and per-ID panels)
* **Alerts**:

  * Python script using `smtplib` for threshold-based email notifications

---

## System Architecture

```plaintext
+---------------+      HTTP      +---------------------+      MQTT      +----------+
| ESP32-CAM     | <------------> | Custom Web UI       |                | Motors   |
| - Captures    |                | (Control & Video)   |                +----------+
|   Video       |                +---------------------+                    |
| - HTTP POST   |                                                        
|   to Face     |                                                        
|   Detection   |                                                        
+-------+-------+                                                        
        |                                                               
        | Frame                                                         
        v                                                               
+----------------------++         +---------------------+     +-------------------+
| Face Detection Server|| ------> | Face Recognition    | --> | Scoring & Alerts  |
+----------------------++         | Server              |     | (Python Scripts)  |
        | Data                                               |---> Email Alerts      
        v                                                   |                     
+----------------------+                                  |                     
| InfluxDB             | <--------- Sensor & Score Data ---+                     
+----------------------+                                                         
        ^                                                                       
        | Grafana Dashboards                                                   
        +----------------------------------------------------------------------+
```

---

## Setup & Installation

### Prerequisites

1. **Hardware** assembled robot chassis, ESP32-CAM, ESP8266 (or simulation environment).
2. **Software** installed on host machine:

   * Arduino IDE (ESP32 & ESP8266 cores)
   * InfluxDB 2.x
   * Grafana 8.x+
   * Python 3.8+ with dependencies:

     ```bash
     pip install opencv-python influxdb-client flask smtplib
     ```

### Steps

1. **InfluxDB & Grafana**:

   * Install and start InfluxDB. Create `mental_health` bucket and generate an API token.
   * Install and start Grafana. Add InfluxDB as a data source and import provided dashboard JSON (in `dashboards/` folder).

2. **Servers**:

   * Launch face detection server:

     ```bash
     python face_detection_server.py
     ```
   * Launch recognition server:

     ```bash
     python face_recognition_server.py
     ```

3. **Firmware**:

   * Flash `esp32cam/esp32cam.ino` with your bucket URL, token, and thresholds.
   * Flash `esp8266/sensors.ino` (or run `simulate_sensors.py`) to send sensor readings to InfluxDB.

4. **Web UI**:

   * ESP32-CAM hosts the control UI at `http://<ESP32CAM_IP>/`.
   * Use the UI to drive the robot and view live video.

---

## Usage

1. Power on the robot and ensure Wi-Fi connectivity.
2. Access the web UI on ESP32-CAM to control motors and view live feed.
3. Monitor live dashboards in Grafana:

   * **Global Dashboard**: overall environmental and mental health trends.
   * **Per-Person Dashboard**: individual scores and conditions.
4. Email alerts are sent automatically when scores fall below configured thresholds.

---

## Configuration

Edit `config.py` (or `config.h` in Arduino sketches) to set:

* InfluxDB URL, bucket, organization, token
* Thresholds for `warning_score` and `critical_score`
* Email SMTP server settings and recipient lists

---

## Simulated Sensor Data

If using simulation, run:

```bash
python simulate_sensors.py --interval 10
```

This script will publish random but realistic DHT11, photoresistor, and MQ135 readings to InfluxDB every 10 seconds.

---



---

## Acknowledgements

* Inspired by IoT & robotics communities
* Uses open-source libraries: Arduino, OpenCV, InfluxDB, Grafana
