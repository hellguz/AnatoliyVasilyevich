#include <WebServer.h>
#include <ESPmDNS.h>
#include "HT_lCMEN2R13EFC1.h"
#include "images.h"
#include "html.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>


//  ---------- CHANGE THESE CONSTANTS ----------
/** 
 *  The endpoint returning MD5 of the image. 
 *  Example: "http://yourdomain.com/ANATOLIY_MEME_ID"
 */
const char *ANATOLIY_MEME_ID = "https://anatoliy.i-am-hellguz.uk/get_last_md5";

/** 
 *  The endpoint returning the XBM image text.
 *  Example: "http://yourdomain.com/ANATOLIY_MEME_POCKET"
 */
const char *ANATOLIY_MEME_POCKET = "https://anatoliy.i-am-hellguz.uk/get_last_xbm";

/** 
 * Frequency to check for changes (in milliseconds).
 * Example: 300000 ms = 5 minutes 
 */
const unsigned long CHECK_FREQUENCY_MS = 1 * 10 * 1000;
//  --------------------------------------------

Preferences prefs;


HT_ICMEN2R13EFC1 display(6, 5, 4, 7, 3, 2, -1, 6000000);  // rst,dc,cs,busy,sck,mosi,miso,frequency

int width, height;
String HTTP_Payload;
WebServer server(80);
const char *ssid = "wespennest";
const char *password = "Osichky!";

// Keep track of the last known MD5 from the server
String lastKnownMD5 = "";

// Timestamp to decide when to check again
unsigned long lastCheckMillis = 0;

// Forward declarations
void drawImageDemo();
void downloadAndDisplayImage();
String getServerMD5();
int parseXBM(const String &xbmData, uint8_t *dest, size_t destSize);

// -----------------------------------------------------
// This callback handles the old "/set" endpoint from your code
// (unchanged from your example but kept here for reference).
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

  // Initialize Preferences w/ a namespace
  prefs.begin("my-namespace", false);

  // Retrieve saved MD5 (default to "" if none)
  lastKnownMD5 = prefs.getString("lastMD5", "");

  VextON();
  delay(100);
  display.init();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Web server routes
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/set", HTTP_GET, Config_Callback);
  server.begin();
}

// -----------------------------------------------------
// Downloads the XBM data from ANATOLIY_MEME_POCKET,
// parses it into WiFi_Logo_bits[], and calls drawImageDemo().
// -----------------------------------------------------
void downloadAndDisplayImage() {
  Serial.println("\nDownloading updated XBM image...");

  HTTPClient http;
  http.begin(ANATOLIY_MEME_POCKET);
  int httpCode = http.GET();

  if (httpCode == 200) {
    // Read the entire XBM text payload into a String
    String xbmData = http.getString();
    Serial.println("XBM data downloaded. Parsing...");

    // Attempt to parse the XBM text into WiFi_Logo_bits[]
    int bytesParsed = parseXBM(xbmData, WiFi_Logo_bits, sizeof(WiFi_Logo_bits));

    if (bytesParsed > 0) {
      Serial.print("Successfully parsed XBM data. Bytes parsed: ");
      Serial.println(bytesParsed);

      // Now draw it
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
  HTTPClient http;
  http.begin(ANATOLIY_MEME_ID);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String serverMD5 = http.getString();
    serverMD5.trim();  // remove extra whitespace/newline
    return serverMD5;
  } else {
    Serial.println("Failed to get MD5 from server.");
    http.end();
    return "";
  }
}

// -----------------------------------------------------
// Parses XBM-formatted text into a raw byte array (dest).
// This is a simple parser that looks for comma-separated
// hex values of the form 0x?? in curly braces.
// -----------------------------------------------------
int parseXBM(const String &xbmData, uint8_t *dest, size_t destSize) {
  // Example XBM snippet might look like:
  //
  //    #define test_width 250
  //    #define test_height 122
  //    static unsigned char test_bits[] = {
  //    0x00, 0xff, 0x1c, ... 0xC2
  //    };
  //
  // We'll:
  //   1) Find the '{' and '}'
  //   2) Extract all "0x??" tokens (comma-separated)
  //   3) Convert them to integers
  //   4) Store in dest[]

  // 1) Find the curly brace region
  int startIndex = xbmData.indexOf('{');
  int endIndex = xbmData.indexOf('}');
  if (startIndex < 0 || endIndex < 0 || endIndex <= startIndex) {
    Serial.println("parseXBM: Could not find curly braces in XBM data.");
    return 0;
  }

  // We'll extract the substring containing just the hex array
  String hexArray = xbmData.substring(startIndex + 1, endIndex);

  // Now split by commas
  int byteCount = 0;
  int searchFrom = 0;
  while (true) {
    // Find a comma or the end of string
    int commaIndex = hexArray.indexOf(',', searchFrom);
    String token;
    if (commaIndex == -1) {
      // no more commas
      token = hexArray.substring(searchFrom);
    } else {
      token = hexArray.substring(searchFrom, commaIndex);
    }
    token.trim();  // remove extra whitespace

    // Convert something like "0x1C" into an integer
    if (token.startsWith("0x") || token.startsWith("0X")) {
      // parseInt can handle hex if we skip "0x" and pass HEX
      String hexStr = token.substring(2);  // skip "0x"
      int val = (int)strtol(hexStr.c_str(), NULL, 16);

      if (byteCount < (int)destSize) {
        dest[byteCount++] = (uint8_t)val;
      } else {
        Serial.println("parseXBM: Destination buffer too small!");
        break;
      }
    }

    if (commaIndex == -1) {
      // We reached the last token
      break;
    }
    searchFrom = commaIndex + 1;  // move past the comma
  }

  return byteCount;
}

// -----------------------------------------------------
// The function that draws the content of WiFi_Logo_bits[]
// to the screen.
// -----------------------------------------------------
void drawImageDemo() {
  // Clear the internal buffer (RAM), not the screen yet
  display.clear();

  // Draw XBM in the internal buffer
  display.drawXbm(0, 0, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);

  // Force a single full refresh
  display.update(BLACK_BUFFER);

  // Some Heltec libraries require display.display() after update; others do not
  display.display();

  // Optional small delay
  delay(1000);
}

// -----------------------------------------------------
void VextON(void) {
  pinMode(45, OUTPUT);
  digitalWrite(45, LOW);
}

// -----------------------------------------------------
void VextOFF(void)  // Vext default OFF
{
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH);
}

// -----------------------------------------------------
// Main loop checks for new server requests (for the built-in
// web server) and also periodically checks if the image changed.
// -----------------------------------------------------
void loop() {
  server.handleClient();  // Handle requests from clients

  unsigned long currentMillis = millis();
  if (currentMillis - lastCheckMillis >= CHECK_FREQUENCY_MS) {
    lastCheckMillis = currentMillis;

    // 1) Ask the server for the current MD5
    String currentMD5 = getServerMD5();  // the new MD5 from server
    if (currentMD5 != lastKnownMD5) {
      Serial.println("Image MD5 changed => Download new image.");
      downloadAndDisplayImage();

      // Save the new MD5 in NVS
      lastKnownMD5 = currentMD5;
      prefs.putString("lastMD5", lastKnownMD5);
    } else {
      Serial.println("Image MD5 unchanged, do nothing.");
    }
  }
}
