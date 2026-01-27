    **🌱 My Smart Plant - IoT Irrigation Monitoring System**
Course: Internet of Things

Authors: Tal Adoni, Omri Aviram

📖 Overview
My Smart Plant is an IoT-based system designed to monitor soil moisture, temperature, and humidity for home plants. The system utilizes an ESP32 microcontroller to collect telemetry data and securely transmit it to the AWS Cloud.

The project demonstrates a full IoT pipeline: from edge sensing and local decision-making to cloud serverless computing, database storage, and real-time visualization via Grafana and Telegram alerts.

⚠️ Important Note regarding Live Demo / QR Codes: Please note that the QR codes and live dashboard links referenced in the attached presentation are no longer active. The cloud infrastructure (AWS/Grafana) has been decommissioned to avoid incurring further cloud costs after the project's conclusion.

⚙️ System Architecture & Workflow
The system bridges the physical world (plants) with the digital world (cloud analytics) using the following workflow:

Edge Layer (ESP32): Reads analog soil moisture data and digital temperature/humidity (DHT22). It performs local state analysis (e.g., turning on an LED if the plant is thirsty) regardless of internet connection.

Communication: Data is sent securely via MQTT (TLS 1.2) to AWS IoT Core.

Cloud Logic (AWS):

AWS IoT Core: Ingests messages.

AWS Lambda: Processes data and triggers alerts.

DynamoDB: Stores historical telemetry data.

Visualization & Alerts:

QuestDB & Grafana: Used for time-series visualization and dashboards.

Telegram Bot: Sends push notifications to the user's phone when moisture levels drop below a critical threshold.

(Make sure to upload your workflow image to the repo and name it workflow_picture.png or update this path)

🔌 Hardware Components
Microcontroller: ESP32 Dev Module

Sensors:

Capacitive Soil Moisture Sensor v2.0 (Corrosion resistant)

DHT22 (Temperature & Humidity)

Indicators: Red LED (Status indication)

Misc: Breadboard, Jumper wires

☁️ Tech Stack
Firmware: C++ (Arduino Framework)

Cloud Provider: Amazon Web Services (AWS)

AWS IoT Core

AWS Lambda

Amazon DynamoDB

Amazon S3

Visualization: Grafana

Database: QuestDB (Time-series)

Notifications: Telegram Bot API

🚀 Key Features
Secure Data Ingestion: Uses X.509 certificates and TLS for secure MQTT communication.

Local Decision Making: The ESP32 indicates plant status via LED immediately, even if WiFi is down.

Scalability: The code supports multiple plants ("Plant 1" and "Plant 2") with staggered data transmission.

Cost Efficiency: Designed to run on minimal cloud resources (approx. $0.60/month for active usage).

📂 Project Structure
/src: Contains the ESP32 source code (.ino or .cpp).

/docs: Project presentation and documentation.

/images: System diagrams and photos.

📊 Results
Real-time monitoring of soil moisture decay over time.

Correlation analysis between ambient temperature and soil drying rates.

Instant alerts sent to mobile devices when watering is required.

Created as part of the IoT Course led by Guy Tel-Zur.
