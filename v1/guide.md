# Waste Watcher v1 Build Guide

Waste Watcher v1 is an IoT-based sensor module unit that collects bin fullness data and takes images of the trash. The main difference between v0 and v1 is the use of an HTTP server to record data instead of a SD card. This build guide will walk you through the steps of setting everything up. If you need additional help feel free to ask for help on our [Discord Server](https://discord.com/invite/mGKVVpxTPr).

![feature photo](https://raw.githubusercontent.com/zotbins/waste_watcher/main/v0/guide_images/feature_photo.jpg)

## Features
- Portable Battery Powered Module
- Sends to an HTTP server (You should set up the [ZBCE API]())
- Modular case design
- Metrics
    - bin fullness
    - waste images

## Steps
1. Supplies
2. Solder Components on Prototyping Board
3. Download the Arduino IDE and Add ESP32 Package
4. Select AI Thinker ESP32-CAM as Board in the Arduino IDE
5. Install the NTP Client Library
6. Preparing to Upload Code to the ESP32-CAM
7. Modify and Upload Code to the ESP32-CAM
8. Understanding the Code
9. Format and Install Your MicroSD Card
10. 3D Printing the Case
11. Start Data Logging
