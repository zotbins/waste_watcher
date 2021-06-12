// === wifi credentials ===
// replace with your network credentials
const char* WIFI_SSID = "<YOUR_WIFI_SSID>";
const char* WIFI_PASS = "<YOUR_WIFI_PASSWORD>";

// === http request parameters ===
String serverName = "<YOUR_SERVERNAME>"; // only the serverName, don't add http://
const int serverPort = 80;  // port 80 is default, change if different
String bin_id = "1"; // "1" is default, change if different
WiFiClient client;
