Technical Overview

# Multi-ESP32 Distributed Wearable Monitoring System

## Overview

This project implements a **distributed embedded architecture using three ESP32-class microcontrollers**, each responsible for a dedicated sensing and processing role. A central ESP32 node aggregates processed data and communicates with a companion mobile application via Bluetooth.

The modular design improves reliability, scalability, and real-time responsiveness while allowing independent development and testing of subsystems.

---

## System Architecture

The system consists of three ESP devices:

### 1. ESP32 Classic (Central Node)

Responsibilities:

* Receives processed data from peripheral ESP nodes
* Aggregates sensor and event information
* Communicates with the companion mobile application
* Acts as the system communication hub

Interfaces:

* ESP-NOW (inter-ESP communication)
* BLE / Bluetooth Classic (mobile companion app)

---

### 2. ESP32-S3 (Audio Processing Unit)

Responsibilities:

* Performs onboard audio signal processing
* Extracts relevant audio features
* Detects trigger conditions (e.g., emergency cues or anomalies)
* Sends processed results to the central ESP32

Communication:

* ESP-NOW → ESP32 Classic

---

### 3. ESP32-S3 XIAO (Sensor Processing Unit)

Responsibilities:

* Executes fall detection algorithm
* Processes PPG (heart rate monitoring)
* Processes EDA (stress/activity sensing)
* Runs FreeRTOS-based sensor fusion pipeline
* Generates alert flags and confidence scores

Communication:

* ESP-NOW → ESP32 Classic

---

## Communication Architecture

The system uses **ESP-NOW** as the primary inter-device communication protocol.

Advantages:

* Low latency
* Connectionless operation
* No Wi-Fi router required
* Low power consumption
* Reliable peer-to-peer messaging
* Works alongside Bluetooth simultaneously

Communication flow:

Sensor Node → Central Node
Audio Node → Central Node
Central Node → Companion App

---

## Data Flow Summary

```
ESP32-S3 XIAO
   → Fall detection status
   → Heart rate (PPG)
   → EDA metrics
   → Sensor confidence values

ESP32-S3 Audio
   → Audio event triggers
   → Extracted audio features

ESP32 Classic
   → Aggregates incoming data
   → Formats telemetry packets
   → Sends updates to mobile companion app
```

---

## Operating Framework

Each peripheral ESP operates independently using:

* FreeRTOS task scheduling
* Sensor-level preprocessing
* Event-based reporting instead of raw streaming

This ensures:

* Reduced communication bandwidth usage
* Faster decision making
* Improved modular scalability


