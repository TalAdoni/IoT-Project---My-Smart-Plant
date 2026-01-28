# My Smart Plant - IoT Irrigation Monitoring System 🌱
**Course:** Internet of Things

**Authors:**  
Tal Adoni  
Omri Aviram

---

## 📖 Overview

My Smart Plant is an IoT-based system designed to monitor soil moisture, temperature, and humidity for home plants. The system uses an ESP32 microcontroller to collect telemetry data and securely transmit it to the AWS Cloud.

The project demonstrates a complete IoT pipeline: edge sensing, secure cloud ingestion, serverless processing, time-series storage, visualization, and Telegram notifications.

---

## ⚙️ System Architecture & Workflow

**Edge Layer (ESP32):**  
Reads analog soil moisture and DHT22 temperature/humidity data. Performs local state classification and provides immediate LED feedback independent of internet connectivity.

**Communication:**  
Sensor data is sent securely via MQTT (TLS 1.2) to AWS IoT Core using X.509 certificates.

**Cloud Logic (AWS):**
- **AWS IoT Core:** Message ingestion
- **AWS Lambda:** Data processing and alert triggering
- **Amazon DynamoDB:** Telemetry storage
- **Amazon S3 & CloudFront:** Static website hosting for dashboards

**Visualization & Alerts:**
- **QuestDB & Grafana:** Time-series dashboards and analytics
- **Telegram Bot API:** Sends alerts on threshold breaches

> Note: Upload `workflow_picture.png` to the repo and update its path if included.

---

## 🔌 Hardware Components

- **Microcontroller:** ESP32 Dev Module  
- **Soil Sensors:** Capacitive Soil Moisture Sensor v2.0 (corrosion resistant)  
- **Ambient Sensor:** DHT22 (Temperature & Humidity)  
- **Indicator:** Red LED (status)  
- **Miscellaneous:** Breadboard, jumper wires

---

## ☁️ Tech Stack

- **Firmware:** C++ (Arduino Framework)
- **Cloud Provider:** Amazon Web Services (AWS)
  - AWS IoT Core
  - AWS Lambda
  - Amazon DynamoDB
  - Amazon S3 & CloudFront
- **Visualization:** Grafana, QuestDB (time-series)
- **Notification:** Telegram Bot API

---

## 🚀 Key Features

- **Secure Data Ingestion:** X.509 certificates with MQTT over TLS
- **Local State Indication:** Immediate LED indication when WiFi or cloud is unavailable
- **Multi-Plant Support:** Multiple plants handled with staggered telemetry
- **Cost Efficiency:** Designed to run with minimal cloud costs (~$0.60/month)

---

## 📂 Project Structure
/ESP32 code -> ESP32 firmware (.cpp)
/Cloud services codes -> Python and AWS Lambda scripts
/Presentation Files -> Files for demo and documentation
/Site Code and Pictures -> Static dashboards and image assets
README.md -> This overview

## 📊 Results

- Real-time monitoring of soil moisture and environment
- Reliable alerting to Telegram on critical moisture levels
- Visualization via dashboards for individual and combined plants
- Local analytics via QuestDB + Grafana

*Created as part of the IoT Course led by Dr. Guy Tel-Zur.*
