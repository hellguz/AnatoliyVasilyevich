#include <WebServer.h>
#include <ESPmDNS.h>
#include "HT_lCMEN2R13EFC1.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <vector>  // for dynamic array

// -----------------------------------------------------
// Hard-coded networks
struct KnownNetwork {
  String ssid;
  String password;
};

KnownNetwork knownNetworks[] = {
  // { "wespennest", "Osichky!" },
  { "Egor", "internet2" },
  { "Vodafone-govno", "Universal1" },
  { "FRITZ!Box 7530 DL", "27444065370300070084" },
};
const int knownNetworksCount = sizeof(knownNetworks) / sizeof(knownNetworks[0]);

// A vector of *additional* networks loaded from ANATOLIY_WIFI_BOOK
std::vector<KnownNetwork> dynamicNetworks;

// -----------------------------------------------------
// Endpoints
const char *ANATOLIY_MEME_ID = "https://anatoliy.i-am-hellguz.uk/get_last_md5";
const char *ANATOLIY_MEME_POCKET = "https://anatoliy.i-am-hellguz.uk/get_last_xbm";
const char *ANATOLIY_WIFI_BOOK = "https://anatoliy.i-am-hellguz.uk/get_wifi_book";

// Timing
const unsigned long CHECK_FREQUENCY_MS = 10 * 1000;  // For MD5 check
const unsigned long WIFI_RESCAN_MS = 30 * 1000;      // For Wi-Fi re-scan if disconnected
const unsigned long WIFI_BOOK_MS = 30 * 1000;        // How often to fetch new Wi-Fi Book

// XBM values
#define WiFi_Logo_width 250
#define WiFi_Logo_height 122
 uint8_t WiFi_Logo_bits[10000] PROGMEM = {
};

// E-Paper, Web server, Preferences
Preferences prefs;
HT_ICMEN2R13EFC1 display(6, 5, 4, 7, 3, 2, -1, 6000000);
WebServer server(80);

// State variables
String lastKnownMD5 = "";
unsigned long lastCheckMillis = 0;
unsigned long lastWifiCheckMillis = 0;  // For repeated scans
unsigned long lastBookMillis = 0;       // For repeated Wi-Fi book fetch

// Forward declarations
void connectToWiFi();
bool tryConnect(const char *ssid, const char *password);
void drawImageDemo();
void downloadAndDisplayImage();
String getServerMD5();
int parseWifiBook(const String &jsonData);
void updateWifiBook();                      // <--- NEW
void loadWifiBookFromPrefs();               // <--- NEW
void saveWifiBookToPrefs(const String &s);  // <--- NEW

// -----------------------------------------------------
// Callback for "/set" endpoint (original code).
// -----------------------------------------------------
void Config_Callback() {
  String Payload = server.arg("value");
  const char *buff = Payload.c_str();

  delay(100);
  int i = 0;
  char *token = strtok((char *)buff, ",");

  while (token != NULL) {
    int num = atoi(token);
    WiFi_Logo_bits[i] = num;
    token = strtok(NULL, ",");
    Serial.print(WiFi_Logo_bits[i]);
    i++;
  }
  drawImageDemo();
  Serial.println("dd");
}

// -----------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();

  // Set Wi-Fi to Station Mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);  // Disconnect from any previous connections

  // Initialize Preferences with a namespace
  prefs.begin("my-namespace", false);

  // Retrieve saved MD5
  lastKnownMD5 = prefs.getString("lastMD5", "");

  // Load any previously saved Wi-Fi Book
  loadWifiBookFromPrefs();

  VextON();
  delay(100);
  display.init();

  // Attempt Wi-Fi connection at startup
  connectToWiFi();
}

// -----------------------------------------------------
void loop() {
  server.handleClient();

  unsigned long now = millis();

  // 1) If we're disconnected, re-scan every WIFI_RESCAN_MS
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiCheckMillis >= WIFI_RESCAN_MS) {
      lastWifiCheckMillis = now;
      Serial.println("Wi-Fi is disconnected. Trying to reconnect...");
      // Set Wi-Fi to Station Mode

      connectToWiFi();
    }
  }

  // 2) Check the Wi-Fi book every WIFI_BOOK_MS if we're connected
  if (WiFi.status() == WL_CONNECTED) {
    if (now - lastBookMillis >= WIFI_BOOK_MS) {
      lastBookMillis = now;
      updateWifiBook();
    }
  }

  // 3) Check for new MD5 updates every CHECK_FREQUENCY_MS
  if (now - lastCheckMillis >= CHECK_FREQUENCY_MS) {
    lastCheckMillis = now;
    String currentMD5 = getServerMD5();
    if (currentMD5 != lastKnownMD5 && currentMD5.length() > 0) {
      Serial.println("Image MD5 changed => Download new image.");
      downloadAndDisplayImage();
      lastKnownMD5 = currentMD5;
      prefs.putString("lastMD5", lastKnownMD5);
    } else {
      Serial.println("Image MD5 unchanged, or no Wi-Fi, do nothing.");
    }
  }
}

// -----------------------------------------------------
// Connect to WiFi:
//  1) Try last successful SSID
//  2) Combine knownNetworks + dynamicNetworks
//  3) Do a Wi-Fi scan, see if any appear, try them
// -----------------------------------------------------
void connectToWiFi() {
  String lastSSID = prefs.getString("lastSSID", "");
  bool foundLast = false;

  // Step A: Attempt last successful SSID first
  if (lastSSID.length() > 0) {
    // Check in dynamicNetworks first (reverse order)
    for (int i = dynamicNetworks.size() - 1; i >= 0; i--) {
      if (lastSSID == dynamicNetworks[i].ssid) {
        foundLast = true;
        Serial.print("Trying last known network (dynamic): ");
        Serial.println(lastSSID);
        if (tryConnect(dynamicNetworks[i].ssid.c_str(), dynamicNetworks[i].password.c_str())) {
          Serial.println("Connected to last known dynamic network!");
          return;
        }
        break;  // Stop after trying the last known network in dynamic list
      }
    }

    // If not found or failed, check in knownNetworks
    if (!foundLast) {
      for (int i = 0; i < knownNetworksCount; i++) {
        if (lastSSID == knownNetworks[i].ssid) {
          foundLast = true;
          Serial.print("Trying last known network: ");
          Serial.println(lastSSID);
          if (tryConnect(knownNetworks[i].ssid.c_str(), knownNetworks[i].password.c_str())) {
            Serial.println("Connected to last known network!");
            return;
          }
          break;  // Stop after trying the last known network in knownNetworks
        }
      }
    }
  }

  // Step B: If last successful SSID fails, scan all networks
  Serial.println("Forcibly disconnecting any Wi-Fi connection...");
  WiFi.disconnect(true);  // Force disconnect
  delay(1000);            // Brief pause before scanning

  Serial.println("Scanning for Wi-Fi networks...");
  int numNetworks = WiFi.scanNetworks();
  Serial.print("Found ");
  Serial.print(numNetworks);
  Serial.println(" networks");

  // Check dynamicNetworks first (reverse order)
  for (int i = dynamicNetworks.size() - 1; i >= 0; i--) {
    for (int j = 0; j < numNetworks; j++) {
      String foundSSID = WiFi.SSID(j);
      if (foundSSID == dynamicNetworks[i].ssid) {
        Serial.print("Trying dynamic network: ");
        Serial.println(foundSSID);
        if (tryConnect(dynamicNetworks[i].ssid.c_str(), dynamicNetworks[i].password.c_str())) {
          Serial.print("Connected to: ");
          Serial.println(foundSSID);
          prefs.putString("lastSSID", foundSSID);  // Store as last successful
          return;                                  // success
        }
      }
    }
  }

  // Then check knownNetworks
  for (int i = 0; i < knownNetworksCount; i++) {
    for (int j = 0; j < numNetworks; j++) {
      String foundSSID = WiFi.SSID(j);
      if (foundSSID == knownNetworks[i].ssid) {
        Serial.print("Trying known network: ");
        Serial.println(foundSSID);
        if (tryConnect(knownNetworks[i].ssid.c_str(), knownNetworks[i].password.c_str())) {
          Serial.print("Connected to: ");
          Serial.println(foundSSID);
          prefs.putString("lastSSID", foundSSID);  // Store as last successful
          return;                                  // success
        }
      }
    }
  }

  Serial.println("Could not connect to any known network.");
}

// -----------------------------------------------------
// Attempt to connect to a given SSID/pwd with a short timeout
// Return true if connected, false otherwise
// -----------------------------------------------------
bool tryConnect(const char *ssid, const char *password) {
  WiFi.disconnect();  // ensure fresh
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 5000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    delay(250);
    return true;
  }
  return false;
}

// -----------------------------------------------------
// Parses the Wi-Fi Book JSON data and populates dynamicNetworks
// Expects JSON format: [{"ssid":"Egor","password":"internet2"},...]
// Returns the number of networks parsed
// -----------------------------------------------------
int parseWifiBook(const String &jsonData) {
  dynamicNetworks.clear();  // Clear existing dynamic networks
  int networksParsed = 0;

  int len = jsonData.length();
  if (len < 2 || jsonData[0] != '[' || jsonData[len - 1] != ']') {
    Serial.println("parseWifiBook: Invalid JSON format.");
    return networksParsed;
  }

  // Remove the surrounding square brackets
  String innerJson = jsonData.substring(1, len - 1);
  int pos = 0;
  while (pos < innerJson.length()) {
    // Find the start of the next object
    int objStart = innerJson.indexOf('{', pos);
    if (objStart == -1) break;

    // Find the end of the object
    int objEnd = innerJson.indexOf('}', objStart);
    if (objEnd == -1) break;

    // Extract the object string
    String objStr = innerJson.substring(objStart + 1, objEnd);

    // Parse "ssid" and "password"
    String ssid = "";
    String password = "";

    // Find "ssid":"value"
    int ssidKey = objStr.indexOf("\"ssid\"");
    if (ssidKey != -1) {
      int ssidColon = objStr.indexOf(':', ssidKey);
      if (ssidColon != -1) {
        int ssidStartQuote = objStr.indexOf('"', ssidColon);
        int ssidEndQuote = objStr.indexOf('"', ssidStartQuote + 1);
        if (ssidStartQuote != -1 && ssidEndQuote != -1) {
          ssid = objStr.substring(ssidStartQuote + 1, ssidEndQuote);
        }
      }
    }

    // Find "password":"value"
    int passKey = objStr.indexOf("\"password\"");
    if (passKey != -1) {
      int passColon = objStr.indexOf(':', passKey);
      if (passColon != -1) {
        int passStartQuote = objStr.indexOf('"', passColon);
        int passEndQuote = objStr.indexOf('"', passStartQuote + 1);
        if (passStartQuote != -1 && passEndQuote != -1) {
          password = objStr.substring(passStartQuote + 1, passEndQuote);
        }
      }
    }

    // If both ssid and password are found, add to dynamicNetworks
    if (ssid.length() > 0 && password.length() > 0) {
      dynamicNetworks.push_back({ ssid, password });
      networksParsed++;
      Serial.print("Parsed network: ");
      Serial.print(ssid);
      Serial.print(" / ");
      Serial.println(password);
    }

    // Move position past this object
    pos = objEnd + 1;
  }

  return networksParsed;
}

// -----------------------------------------------------
// Call ANATOLIY_WIFI_BOOK, parse JSON, store in dynamicNetworks
// and persist in Preferences. (Called every 30 secs if Wi-Fi ok.)
// -----------------------------------------------------
void updateWifiBook() {
  Serial.println("Fetching Wi-Fi book...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected, skipping Wi-Fi book fetch...");
    return;
  }

  HTTPClient http;
  http.begin(ANATOLIY_WIFI_BOOK);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Wi-Fi book fetched, storing...");

    // Parse the JSON manually
    int parsedNetworks = parseWifiBook(payload);
    if (parsedNetworks > 0) {
      Serial.print("Successfully parsed ");
      Serial.print(parsedNetworks);
      Serial.println(" networks from Wi-Fi book.");
      // Save the raw JSON to Preferences so we can reload it on next boot
      saveWifiBookToPrefs(payload);
    } else {
      Serial.println("No networks parsed from Wi-Fi book.");
    }

  } else {
    Serial.print("Failed to fetch Wi-Fi book. HTTP code: ");
    Serial.println(httpCode);
  }
  http.end();
}

// -----------------------------------------------------
// Save the JSON string to Preferences
// -----------------------------------------------------
void saveWifiBookToPrefs(const String &s) {
  prefs.putString("wifiBook", s);
  Serial.println("Wi-Fi book saved to Preferences.");
}

// -----------------------------------------------------
// Load the JSON from Preferences into dynamicNetworks
// on startup
// -----------------------------------------------------
void loadWifiBookFromPrefs() {
  String book = prefs.getString("wifiBook", "");
  if (book.length() == 0) {
    Serial.println("No saved Wi-Fi book found in prefs.");
    return;
  }

  Serial.println("Loading Wi-Fi book from Preferences...");
  int parsedNetworks = parseWifiBook(book);
  if (parsedNetworks > 0) {
    Serial.print("Successfully loaded ");
    Serial.print(parsedNetworks);
    Serial.println(" networks from Preferences.");
  } else {
    Serial.println("Failed to parse Wi-Fi book from Preferences.");
  }
}

// -----------------------------------------------------
// Downloads the XBM data, parses it, calls drawImageDemo().
// -----------------------------------------------------
void downloadAndDisplayImage() {
  Serial.println("\nDownloading updated XBM image...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping download...");
    return;
  }

  HTTPClient http;
  http.begin(ANATOLIY_MEME_POCKET);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String xbmData = http.getString();
    Serial.println("XBM data downloaded. Parsing...");

    int bytesParsed = parseXBM(xbmData, WiFi_Logo_bits, sizeof(WiFi_Logo_bits));
    if (bytesParsed > 0) {
      Serial.print("Successfully parsed XBM data. Bytes parsed: ");
      Serial.println(bytesParsed);
      drawImageDemo();
    } else {
      Serial.println("Failed to parse XBM data (or no data).");
    }
  } else {
    Serial.print("Failed to download image. HTTP code: ");
    Serial.println(httpCode);
  }
  http.end();
}

// -----------------------------------------------------
// Gets the current MD5 from your server
// -----------------------------------------------------
String getServerMD5() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping MD5 request...");
    return "";
  }

  HTTPClient http;
  http.begin(ANATOLIY_MEME_ID);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String serverMD5 = http.getString();
    serverMD5.trim();
    Serial.print("Server MD5: ");
    Serial.println(serverMD5);
    return serverMD5;
  } else {
    Serial.print("Failed to get MD5 from server. HTTP code: ");
    Serial.println(httpCode);
    http.end();
    return "";
  }
}

// -----------------------------------------------------
// Basic XBM parsing - extracts 0x?? bytes from {...} region
// -----------------------------------------------------
int parseXBM(const String &xbmData, uint8_t *dest, size_t destSize) {
  int startIndex = xbmData.indexOf('{');
  int endIndex = xbmData.indexOf('}');
  if (startIndex < 0 || endIndex < 0 || endIndex <= startIndex) {
    Serial.println("parseXBM: Could not find curly braces in XBM data.");
    return 0;
  }

  String hexArray = xbmData.substring(startIndex + 1, endIndex);

  int byteCount = 0;
  int searchFrom = 0;
  while (true) {
    int commaIndex = hexArray.indexOf(',', searchFrom);
    String token;
    if (commaIndex == -1) {
      token = hexArray.substring(searchFrom);
    } else {
      token = hexArray.substring(searchFrom, commaIndex);
    }
    token.trim();

    if (token.startsWith("0x") || token.startsWith("0X")) {
      String hexStr = token.substring(2);
      int val = (int)strtol(hexStr.c_str(), NULL, 16);
      if (byteCount < (int)destSize) {
        dest[byteCount++] = (uint8_t)val;
      } else {
        Serial.println("parseXBM: Destination buffer too small!");
        break;
      }
    }

    if (commaIndex == -1) {
      break;
    }
    searchFrom = commaIndex + 1;
  }

  return byteCount;
}

// -----------------------------------------------------
// Draw the content of WiFi_Logo_bits[] on the e-paper
// -----------------------------------------------------
void drawImageDemo() {
  display.clear();  // Clear internal buffer
  display.drawXbm(0, 0, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.update(BLACK_BUFFER);
  display.display();
  delay(1000);  // let it settle
}

// -----------------------------------------------------
void VextON() {
  pinMode(45, OUTPUT);
  digitalWrite(45, LOW);
}

// -----------------------------------------------------
void VextOFF() {
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH);
}
