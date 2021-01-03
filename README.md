# Introduction
I wanted a way to keep track of how much waste I was generating just to be more environmentally conscious. So I created this project as an automated way to help keep track of waste accumulation through fullness measurements while also taking pictures of the objects thrown away.

To achieve this, I use the HC-SR04 ultrasonic sensor along with the ESP32-CAM and save the measurements to a micro SD card. Doing this type of waste tracking is called a waste audit, which is a common practice used by universities to keep track of waste goals while also looking for opportunities increasing waste diversion rates.

# ZotBins Community Edition

This is the first step of a multi-part project of creating a smart waste ecosystem called [ZotBins](https://zotbins.github.io), which is already deployed at UCI. Currently, I am helping to further expand the concept of ZotBins where any city, business, university, classroom, or home could implement a smart waste system. This open-source version ZotBins is called  ZotBins Community Edition (ZBCE). To learn more about this initiative please visit the [ZotBins Community Edition Blog](https://zotbins.github.io/zbceblog/about/)

# Quick Start
Please follow the appropriate build guide for the waste watcher version of your choosing.
- v0 [Last Updated - 2021 January 2 ]
    - Measures bin fullness and collects waste images
    - Saves data and images locally
    - Syncs with NTP Client to generate UTC datetime stamps
- v1 [In Progress]
    - Measures bin fullness and collects waste images
    - Saves data and images to HTTP server
    - Syncs with NTP Client to generate UTC datetime stamps

# Contributing
For this specific repository feel free to submit any issues regarding bugs. This will really help other users who may be interested in building this project and also help me create a debugging guide for the community.

*Written by Owen Yang*
