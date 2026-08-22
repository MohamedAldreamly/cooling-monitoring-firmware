# IoT Cooling Monitoring Firmware

ESP-IDF firmware for an ESP32-S3 based IoT cooling monitoring system.

## Target

- ESP32-S3
- ESP-IDF

## Features

- Signal acquisition and processing
- Signal snapshot management
- Alarm detection and transitions
- Local storage management
- Wi-Fi connectivity
- MQTT communication
- AWS IoT Core integration
- Cloud upload pipeline
- Device Shadow support
- Offline buffering and recovery
- Simulation support for development and testing

## Requirements

Install ESP-IDF and make sure the ESP-IDF terminal/environment is active.

Verify installation:

```bash
idf.py --version

Build

Clone the repository and enter the firmware directory:

cd firmware

Set the target:

idf.py set-target esp32s3

Build the project:

idf.py build
Flash

Connect the ESP32-S3 and run:

idf.py -p COM_PORT flash

Example on Windows:

idf.py -p COM5 flash
Serial Monitor
idf.py -p COM_PORT monitor

Or flash and monitor together:

idf.py -p COM_PORT flash monitor

Exit the monitor using:

Ctrl + ]
Clean Build

If a full rebuild is required:

idf.py fullclean
idf.py set-target esp32s3
idf.py build
Project Structure
firmware/
├── components/
├── main/
├── CMakeLists.txt
├── partitions.csv
├── sdkconfig.defaults
├── .clang-format
├── .clangd
└── README.md
Configuration

Default ESP-IDF configuration is stored in:

sdkconfig.defaults

The generated local configuration is stored in:

sdkconfig
Development

The firmware is organized as independent modules for signal processing, alarms, storage, connectivity, cloud communication, and device state management.


هذا README مبدئي، وبعدها لما نخلص المشروع بنخليه أقوى ونضيف Architecture حقيقية.

بعدها خلينا نعمل README رئيسي للمشروع كله.

نفذ:

```powershell
notepad "F:\iot-cooling-system\README.md"

وحط:

# IoT Cooling Monitoring System

An end-to-end IoT cooling monitoring system built with ESP32-S3, ESP-IDF, AWS cloud services, and a React-based monitoring dashboard.

## System Overview

```text
ESP32-S3
    ↓
Signal Processing
    ↓
Alarm Engine
    ↓
Local Storage
    ↓
Wi-Fi / MQTT
    ↓
AWS IoT Core
    ↓
Cloud Services
    ↓
REST API
    ↓
React Dashboard
Repository Structure
iot-cooling-system/
├── firmware/
├── cooling-dashboard/
├── docs/
├── .gitignore
└── README.md
Firmware

The firmware is located in:

firmware/

Target device:

ESP32-S3

To build:

cd firmware
idf.py set-target esp32s3
idf.py build
Web Dashboard

The dashboard is located in:

cooling-dashboard/

Install dependencies:

cd cooling-dashboard
npm install

Create the environment configuration:

.env

Then run:

npm run dev
Technology Stack
Embedded
ESP32-S3
ESP-IDF
FreeRTOS
C
Cloud
AWS IoT Core
MQTT
AWS Lambda
Amazon DynamoDB
Amazon API Gateway
Device Shadow
Frontend
React
TypeScript
Vite
React Router
TanStack Query
Tailwind CSS
Project Status

Active development.