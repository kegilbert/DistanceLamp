#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define SSID_EEPROM_ADDR 0
#define PW_EEPROM_ADDR   128
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 32     // OLED display height, in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define NUMFLAKES 10  // Number of snowflakes in the animation example
#define LOGO_HEIGHT 16
#define LOGO_WIDTH 16

// Replace with your network credentials
const char* ap_ssid = "RemoteLampAP";
const char* ap_password = "penis123";
char wifi_ssid[128] = "";
char wifi_password[128] = "";
bool conn_established = false;
int loop_count = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static const unsigned char PROGMEM logo_bmp[] = { 0b00000000, 0b11000000,
                                                  0b00000001, 0b11000000,
                                                  0b00000001, 0b11000000,
                                                  0b00000011, 0b11100000,
                                                  0b11110011, 0b11100000,
                                                  0b11111110, 0b11111000,
                                                  0b01111110, 0b11111111,
                                                  0b00110011, 0b10011111,
                                                  0b00011111, 0b11111100,
                                                  0b00001101, 0b01110000,
                                                  0b00011011, 0b10100000,
                                                  0b00111111, 0b11100000,
                                                  0b00111111, 0b11110000,
                                                  0b01111100, 0b11110000,
                                                  0b01110000, 0b01110000,
                                                  0b00000000, 0b00110000 };


// Initialize the server on port 80
ESP8266WebServer server(80);
ESP8266WiFiMulti WiFiMulti;

// HTML webpage with a POST form
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>WiFi Setup</title>
</head>
<body>
  <h1 style="font-size: 5em">WiFi Setup</h1>
  <form action="/submit" method="POST">
    <label style="font-size:2em;" for="ssid">SSID:</label>
    <input style="display:block; width:70%; font-size: 2em" type="text" id="ssid" name="ssid"><br><br>
    <label style="font-size:2em;" for="pw">Password:</label> 
    <input style="display:block; width:70%; font-size: 2em" type="text" id="pw" name="pw"><br><br>
    <input style="width:50%; height:3em; font-size: 2em" type="submit" value="Submit">
  </form>
</body>
</html>
)rawliteral";


// Handle the root route (display the form)
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}


// Handle the form submission (POST request)
void handleSubmit() {
  if (server.hasArg("ssid") && server.hasArg("pw")) {
    String ssid = server.arg("ssid");
    String pw = server.arg("pw");
    ssid.trim();
    pw.trim();

    strcpy(wifi_ssid, ssid.c_str());
    strcpy(wifi_password, pw.c_str());

    for (int i = 0; i <= strlen(wifi_ssid); i++) {
      Serial.printf("%c [%02d]\r\n", wifi_ssid[i], wifi_ssid[i]);
      EEPROM.put(SSID_EEPROM_ADDR + i, wifi_ssid[i]);
    }
    for (int i = 0; i <= strlen(wifi_password); i++) {
      EEPROM.put(PW_EEPROM_ADDR + i, wifi_password[i]);
    }

    bool commit_sts = EEPROM.commit();
    if(commit_sts) {
      Serial.println("Write successfully");
    } else {
      Serial.println("Write error");
    }

    // Process the received data (example: print it to the Serial monitor)
    Serial.println("Received POST data:");
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + pw);

    // Respond to the client
    String response = "Received:<br>SSID: " + ssid + "<br>Password: " + pw;
    server.send(200, "text/html", response);
  } else {
    server.send(400, "text/plain", "Invalid Request");
  }
}


void reboot(int delayms) {
  delay(delayms);
  ESP.restart();
  while (1) {}
}


void setup() {
  Serial.begin(115200);
  EEPROM.begin(128*2);
  delay(500);
  Serial.println("");

  if (EEPROM.read(SSID_EEPROM_ADDR) != 255 && EEPROM.read(PW_EEPROM_ADDR) != 255) {
    bool ssid_complete, pw_complete = false;
    int idx  = 0;

    while((!ssid_complete || !pw_complete) && idx < 128 ) {
      if (!ssid_complete) {
        wifi_ssid[idx] = EEPROM.get(SSID_EEPROM_ADDR+idx, wifi_ssid[idx]);
        ssid_complete = (wifi_ssid[idx] == 0);
        Serial.printf("SSID_COMPLETE: %d | wifi_ssid[%02d]: %c [%03d]  |  ", ssid_complete, idx, wifi_ssid[idx], wifi_ssid[idx]);
      }
      if (!pw_complete) {
        wifi_password[idx] = EEPROM.get(PW_EEPROM_ADDR+idx, wifi_password[idx]);
        pw_complete = (wifi_password[idx] == 0); 
        Serial.printf("PW_COMPLETE: %d | wifi_pw[%02d]: %c [%03d] |  ", pw_complete, idx, wifi_password[idx], wifi_password[idx]);
      }
      Serial.println("");
      idx++;
    }

    Serial.printf("WIFI EEPROM SSID: %s\r\n", wifi_ssid);
    Serial.printf("WIFI EEPROM PW  : %s\r\n", wifi_password);
  }


  //test_display();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  WiFi.softAP(ap_ssid, ap_password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  display.clearDisplay();
  display.setTextSize(1);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.print(F("IP: "));
  display.println(myIP);
  display.display();

  // Define routes
  server.on("/", handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);

  // Start the server
  server.begin();
  Serial.println("Server started");
}
unsigned long color = 0x00000000;
int red, green, blue = 0;
#define LED_RED   0
#define LED_GREEN 1
#define LED_BLUE  3
void loop() {
  // color += 50;
  // color &=0x00FFFFFF;

  // red = (color & 0x00FF0000) >> 16;
  // green = (color & 0x0000FF00) >> 8;
  // blue = (color & 0x000000FF);

#if 0
  analogWrite(LED_RED, random(100) + 155);
  analogWrite(LED_GREEN, random(200) + 55);
  analogWrite(LED_BLUE, random(200) + 55);

  delay(1000);

#else
  if (strcmp(wifi_ssid, "") && strcmp(wifi_password, "") && !conn_established) {
    Serial.println("Closing AP server, connecting to wifi");
    server.close();

    if (loop_count == 0) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextSize(1);  // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.println("Connecting to: ");
      display.println(wifi_ssid);
    } else if(loop_count >= 32) {
      display.setCursor(24, 0);
      display.print("Failed to Connect...Rebooting");
      display.display();
      reboot(10000);
    } else {
      display.setCursor(loop_count*2, 16);
      display.print(".");
    } 
    display.display();
    loop_count++;

    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(wifi_ssid, wifi_password);
    if ((WiFiMulti.run(5000) == WL_CONNECTED)) {
      WiFiClient client;
      HTTPClient http;
      conn_established = true;

      display.clearDisplay();
      display.setTextSize(1);  // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.println("CONNECTED");
      display.display();

      Serial.print("[HTTP] begin...\n");

      if (http.begin(client, "http://3.212.51.120:8080/test")) {  // HTTP
        Serial.print("[HTTP] GET...\n");
        // start connection and send HTTP header
        int httpCode = http.GET();

        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTP] GET... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = http.getString();
            Serial.println(payload);
          }
        } else {
          Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
      } else {
        Serial.println("[HTTP] Unable to connect");
      }
    }

    delay(1000);
  } else {
    // Handle client requests
    server.handleClient();
  }
#endif
}
