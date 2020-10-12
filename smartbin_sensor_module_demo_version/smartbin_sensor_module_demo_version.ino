// *Author*: Owen Yang
// *Description*: A simple waste auditing data logger script for the ESP32 CAM.
// *Acknowledgements*: Random Nerd Tutorials 
// *License*: Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files.
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// *Resources*:
// - https://randomnerdtutorials.com/esp32-ntp-client-date-time-arduino-ide/
// - https://randomnerdtutorials.com/telegram-esp32-cam-photo-arduino/
// - https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/
// - https://techtutorialsx.com/2020/09/06/esp32-writing-file-to-sd-card/
// - https://www.instructables.com/Select-SD-Interface-for-ESP32/
// - https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/

#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include <SPI.h>               // SD Card ESP32
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>


// === wifi credentials ===
// replace with your network credentials
const char* WIFI_SSID = "REPLACE_WITH_YOUR_WIFI_SSID";
const char* WIFI_PASS = "REPLACE_WITH_YOUR_WIFI_PASSWORD";

// === peripherals pin assignment === 
const int ultrasonicTrigPin = 13;
const int ultrasonicEchoPin = 12;  

// === deep sleep === 
#define uS_TO_S_FACTOR 1000000 /* conversion factor for usec to sec */
#define TIME_TO_SLEEP 1800 /* TIME ESP32 will go to sleep in sec */ 

// === other parameters ===
const int binHeight = 100; // units: cm, used to calculate fullness
const int ultraMinRange = 2; //units: cm, min. limit of accurate HC-SR04 reading
const int ultraMaxRange = 400; //units: cm, max. limit of accurate HC-SR04 reading
const char* datalogFile = "/data.csv";

// === system variables ===
long duration; // units: microseconds, for HC-SR04
int distance; // units:cm, for HC-SR04
int fullness; //units:cm, the bin fullness
String datetimeStamp;
String path;
String dataMessage;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// === camera pin definitions ===
#define FLASH_LED_PIN 4

//CAMERA_MODEL_AI_THINKER
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

void configInitCamera(){
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //=== init with high specs to pre-allocate larger buffers === 
  if(psramFound()){
    Serial.println("PSRAM found");
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 20;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    Serial.println("PSRAM not found");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 15;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }

  // === Init Camera === 
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_UXGA);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
  
}

void getDateTime() {
  timeClient.begin();
  timeClient.setTimeOffset(-25200); // offset for PST
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  datetimeStamp = timeClient.getFormattedDate(); //format: 2018-04-30T16:00:13Z
  datetimeStamp.replace(":","-"); // get rid of colons

  //=== end the client === 
  timeClient.end();
}

void takePhoto() {
  // === determine datetimestamp === 
  getDateTime();

  // === camera initialize buffer === 
  camera_fb_t *fb = NULL;

  // === take picture with camera === 
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return;
  }
  Serial.println("photo taken!");
  
  // === determine image name === 
  path = "/picture" + datetimeStamp + ".jpg";
  fs::FS &fs = SD_MMC; 
  Serial.printf("Picture file name: %s\n", path.c_str());

  // === save photo to SD-card ===  
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Saved file to path: %s\n", path.c_str());
  }
  file.close();

  //=== return the frame buffer back to be reused ===
  esp_camera_fb_return(fb);
}

void fullnessRead(){
  for (int i = 0; i < 5; ++i )
  {
    // clear trigger pin 
    digitalWrite(ultrasonicTrigPin,LOW);
    delayMicroseconds(2); //units: us
  
    // Toggle trigPin HIGH then LOW 
    digitalWrite(ultrasonicTrigPin,HIGH);
    delayMicroseconds(10); //units: us
    digitalWrite(ultrasonicTrigPin,LOW);
  
    // Reads echoPin and returns sound wave travel time 
    duration = pulseIn(ultrasonicEchoPin, HIGH);
    distance = duration*0.034/2;
    Serial.print("Raw Distance: ");
    Serial.print(distance);
    Serial.println();
    
    // Filters out out of range values
    if (distance<ultraMinRange || distance>ultraMaxRange) {
      distance = -1;
      fullness = -1;
    }
    else {
      // calculate fullness
      fullness = binHeight - distance;
      return; 
    }
  }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
}

void sdSetup() {
  // === sd card setup === 
  // Card Mounting
  Serial.println("Starting SD Card");
  if(!SD_MMC.begin("/sdcard", true)){
    Serial.println("SD Card Mount Failed");
    return;
  }
  
  //Detect Card
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  } 

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD_MMC.open(datalogFile);
  if(!file) {
    file.close();
    Serial.println("File doesn't exist");
    writeFile(SD_MMC, datalogFile, "Datetimestamp,fullness(cm),count");
    return;
  }
  else {
    Serial.println("File already exists");  
    file.close();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup has started...");

  // === deep sleep setup === 
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  // === ultrasonic setup ===
  pinMode(ultrasonicTrigPin, OUTPUT);
  pinMode(ultrasonicEchoPin, INPUT);

  // === sd card setup === 
  sdSetup();
  
  // === wifi setup ===
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  // === camera setup === 
  pinMode( FLASH_LED_PIN, OUTPUT); // Setup flash
  configInitCamera(); // Setup camera 

  Serial.println("Setup finished");
  Serial.println();
  
}

void loop() {
  
  // === take a photo ===
  digitalWrite(FLASH_LED_PIN,HIGH);
  takePhoto(); // will also automatically update the datetimeStamp
  digitalWrite(FLASH_LED_PIN,LOW);

  // === get distance reading ===
  fullnessRead();
  
  // === write data to sd card ===
  //update the datetime stamp 
  getDateTime();
  //create data message
  dataMessage = datetimeStamp + "," + String(fullness) + "\n"; 
  Serial.print("Writing ");
  Serial.print(dataMessage);
  Serial.println();

  // convert String to char*
  int str_len = dataMessage.length()+1;
  char charBuf[str_len];
  dataMessage.toCharArray(charBuf,str_len);

  // write to sd card
  appendFile(SD_MMC, datalogFile, charBuf);
  
  // === sleep timer ===
  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush(); 
  esp_deep_sleep_start();
  
}
