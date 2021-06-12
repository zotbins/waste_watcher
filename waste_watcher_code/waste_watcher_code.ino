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
// - https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/
// - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html
//- https://stackoverflow.com/questions/63401643/how-to-post-an-image-to-imgbb-com-on-esp32-cam-i-get-empty-upload-source

#include "esp_camera.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "config.h"

// === peripherals pin assignment ===
const int ultrasonicTrigPin = 13;
const int ultrasonicEchoPin = 12;

// this uses the Rx pin on the ESP-32 CAM, make sure to change rtc_hold_function
// parameters below if you change this
const int nmosGate = 15;

// === deep sleep ===
#define uS_TO_S_FACTOR 1000000 //conversion factor for usec to sec
#define TIME_TO_SLEEP 1800   // TIME ESP32 will sleep for 1800 sec

// === other parameters ===
const int binHeight = 50; // units: cm, used to calculate fullness
const int ultraMinRange = 2; //units: cm, min. limit of accurate HC-SR04 reading
const int ultraMaxRange = 400; //units: cm, max. limit of accurate HC-SR04 reading

// === system variables ===
long duration; // units: microseconds, for HC-SR04
int distance; // units:cm, for HC-SR04
int fullness; //units:cm, the bin fullness
String datetimeStamp;
bool debug = true;

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
    if (debug) Serial.println("PSRAM found");
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 20;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    if (debug) Serial.println("PSRAM not found");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 15;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }

  // === Init Camera ===
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    if (debug) Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_UXGA);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
}

void getDateTime() {
  timeClient.begin();
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  datetimeStamp = timeClient.getFormattedDate(); //raw format: 2018-04-30T16:00:13Z
  datetimeStamp.replace("T"," ");
  datetimeStamp.replace("Z","");

  if (debug) Serial.println(datetimeStamp);

  //=== end the client ===
  timeClient.end();
}

String sendPhoto() {
  // === function parameters ====
  String getAll;
  String getBody;

  // === turn on flash ===
  rtc_gpio_hold_dis(GPIO_NUM_4);
  digitalWrite(FLASH_LED_PIN,HIGH);

  // === take the photo ===
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();

  // === turn off flash ===
  digitalWrite(FLASH_LED_PIN,LOW);
  rtc_gpio_hold_en(GPIO_NUM_4);

  // ==== retake if camera capture fails ===
  if(!fb) {
    if (debug) Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  if (debug) Serial.println("Connecting to server: " + serverName);
  if (debug) Serial.println("Connecting to port: " + serverPort);

  if (client.connect(serverName.c_str(), serverPort)) {
    if (debug) Serial.println("Connection successful!");

    // === initialize header and tail of the file ====
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + bin_id + ".jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    // === image and image properties ===
    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    if (debug) {
      Serial.println("POST /image HTTP/1.1");
      Serial.println("Host: " + serverName);
      Serial.println("Content-Length: " + String(totalLen));
      Serial.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
      Serial.println();
      Serial.print(head);
    }
    // === start post request ===
    client.println("POST /image HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);

    // === write the image ===
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        client.write(fbBuf, remainder);
      }
    }
    // === end post request ===
    client.print(tail);

    esp_camera_fb_return(fb);

    int timoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + timoutTimer) > millis()) {
      if (debug) Serial.print(".");
      delay(100);
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length()==0) { state=true; }
          getAll = "";
        }
        else if (c != '\r') { getAll += String(c); }
        if (state==true) { getBody += String(c); }
        startTimer = millis();
      }
      if (getBody.length()>0) { break; }
    }
    if (debug) Serial.println();
    client.stop();
    if (debug) Serial.println(getBody);
  }
  else {
    getBody = "Connection to " + serverName +  " failed.";
    if (debug) Serial.println(getBody);
  }
  return getBody;
}

void fullnessRead(){
  rtc_gpio_hold_dis(GPIO_NUM_15); // disable the rtc hold on nmos gate
  digitalWrite(nmosGate,HIGH);
  delayMicroseconds(500); //units: us

  // === collect ultrasonicc readings ===
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
    if (debug) Serial.print("Raw Distance: ");
    if (debug) Serial.print(distance);
    if (debug) Serial.println();

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

  // Turn off signal pins
  digitalWrite(ultrasonicTrigPin,LOW);
  digitalWrite(ultrasonicTrigPin,LOW);
  digitalWrite(nmosGate,LOW);
  rtc_gpio_hold_en(GPIO_NUM_15); // enable the rtc hold on the nmos gate
}

void post_fullness() {
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;

    if (debug) Serial.println("http://"+serverName+":"+serverPort+"/fullness");
    http.begin("http://"+serverName+":"+serverPort+"/fullness");

    http.addHeader("Content-Type", "application/json");
    if (debug) Serial.println("{\"data\":[{\"datetime\":\"" + datetimeStamp  +"\",\"fullness\":"+ fullness +",\"bin_id\":"+ bin_id +"}]}");
    int httpResponseCode = http.POST("{\"data\":[{\"datetime\":\"" + datetimeStamp  +"\",\"fullness\":"+ fullness +",\"bin_id\":"+ bin_id +"}]}");

    if (debug) Serial.print("HTTP Response code: ");
    if (debug) Serial.println(httpResponseCode);
    http.end(); // Free the resources
  }
  else {
    if (debug) Serial.println("WiFi Disconnected");
  }
}

void setup() {

  if (debug) Serial.begin(115200);
  if (debug) Serial.println("Setup has started...");

  // === deep sleep setup ===
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  if (debug) Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  // === ultrasonic setup ===
  pinMode(ultrasonicTrigPin, OUTPUT);
  pinMode(ultrasonicEchoPin, INPUT);
  pinMode(nmosGate, OUTPUT);

  // === begin wifi connection ===
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (debug) Serial.print("Connecting to ");
  if (debug) Serial.println(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (debug) Serial.print(".");
  }
  // Print local IP address and start web server
  if (debug) {
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // === camera setup ===
  pinMode( FLASH_LED_PIN, OUTPUT); // Setup flash
  configInitCamera(); // Setup camera

  if (debug) Serial.println("Setup finished");
  if (debug) Serial.println();

  // === take a photo ===
  sendPhoto();

  // === get distance reading ===
  fullnessRead();

  //=== update the datetime stamp ===
  getDateTime();

  // === write data to HTTP Server ===
  post_fullness();

  // === sleep timer ===
  if (debug) Serial.println("Going to sleep now");
  delay(1000);
  if (debug) Serial.flush();
  esp_deep_sleep_start();

}

void loop() {

}
