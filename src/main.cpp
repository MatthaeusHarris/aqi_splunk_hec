#include <FS.h>

#include <ESP8266WiFi.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <Arduino.h>
#include <ArduinoJson.h> // This needs to be v5, not v6

#include <TFT_eSPI.h>

#include <ESP8266TrueRandom.h>
#include <qrcode.h>

#include "airquality.h"
#include "splunk.h"

#define HTML_BUF_SIZE 256

const uint16_t aqi_fg_color[] = {TFT_BLACK, TFT_BLACK, TFT_BLACK, TFT_WHITE, TFT_WHITE, TFT_WHITE, TFT_WHITE};
const uint16_t aqi_bg_color[] = {TFT_GREEN, TFT_YELLOW, TFT_ORANGE, TFT_RED, TFT_MAGENTA, TFT_MAROON};

const char* const wifi_input_html[] PROGMEM = {
  "type=\"checkbox\" style=\"width: auto;\" />"\
  "<label for=\"wifi_enabled\">Wifi Enabled</label",
  "type=\"checkbox\" checked style=\"width: auto;\" />"\
  "<label for=\"wifi_enabled\">On</label"
};

const char passwd_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

char splunk_server[1024];
char splunk_port[6] = "8080";
char splunk_auth[2048];
char splunk_index[40];
char splunk_sourcetype[40];
char splunk_wifi_enabled[4];
char device_location[255];
uint8_t wifi_enabled = 0;

bool shouldSaveConfig = false;
bool wifi_failed = true;

uint32_t last_loop_time;
bool blinky_state = true;

Pmsx003 pms(D4, D3);  //Next time, don't use pin D3 because that's the FLASH button

TFT_eSPI tft = TFT_eSPI();

HEC splunk = HEC();

void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void WiFiSetup(bool reset, const char *ssid, const char *passwd) {
  Serial.print("Mounting FS... ");
  if (SPIFFS.begin()) {
    Serial.println("Done!");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(splunk_server, json["splunk_server"]);
          strcpy(splunk_port, json["splunk_port"]);
          strcpy(splunk_auth, json["splunk_auth"]);
          strcpy(splunk_index, json["splunk_index"]);
          strcpy(splunk_sourcetype, json["splunk_sourcetype"]);
          strcpy(device_location, json["device_location"]);
          strcpy(splunk_wifi_enabled, json["wifi_enabled"]);
          if (strcmp(splunk_wifi_enabled,"yes") == 0) {
            wifi_enabled = 1;
          } else {
            wifi_enabled = 0;
          }
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      } else {
        Serial.println("Error opening config file");
      }
    } else {
      Serial.println("Config file doesn't exist.  Moving on...");
    }
  } else {
    Serial.println(" Failed!");
  }

  Serial.println("Done with FS.");

  WiFiManagerParameter custom_splunk_server("server", "splunk server", splunk_server, 1024);
  WiFiManagerParameter custom_splunk_port("port", "8080", splunk_port, 6);
  WiFiManagerParameter custom_splunk_auth("auth", "HEC Token", splunk_auth, 2048);
  WiFiManagerParameter custom_splunk_index("index", "index", splunk_index, 40);
  WiFiManagerParameter custom_splunk_sourcetype("sourcetype", "sourcetype", splunk_sourcetype, 40);
  WiFiManagerParameter custom_device_location("device_location", "Device Location", device_location, 255);
  WiFiManagerParameter custom_wifi_enabled("wifi_enabled", "yes", "yes", 4, wifi_input_html[wifi_enabled]);

  WiFiManager wifiManager;

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_splunk_server);
  wifiManager.addParameter(&custom_splunk_port);
  wifiManager.addParameter(&custom_splunk_auth);
  wifiManager.addParameter(&custom_splunk_index);
  wifiManager.addParameter(&custom_splunk_sourcetype);
  wifiManager.addParameter(&custom_device_location);
  wifiManager.addParameter(&custom_wifi_enabled);

  if (reset) {
    wifiManager.resetSettings();
  }

  if (wifi_enabled || reset) {
    Serial.println("Wifi is enabled, connecting...");
    // tft.println("Connecting to wifi");
    if (!wifiManager.autoConnect(ssid, passwd)) {
      Serial.println("failed to connect and hit timeout");
      tft.println("Failed to connect to wifi");
      wifi_failed = true;
    } else {
      Serial.printf("Connected to %s with IP address %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      wifi_failed = false;
    }
  } else {
    Serial.println("Wifi disabled, skipping wifi connection.");
  }


  strcpy(splunk_server, custom_splunk_server.getValue());
  strcpy(splunk_port, custom_splunk_port.getValue());
  strcpy(splunk_auth, custom_splunk_auth.getValue());
  strcpy(splunk_index, custom_splunk_index.getValue());
  strcpy(splunk_sourcetype, custom_splunk_sourcetype.getValue());
  strcpy(device_location, custom_device_location.getValue());
  strcpy(splunk_wifi_enabled, custom_wifi_enabled.getValue());

  if (shouldSaveConfig) {
    Serial.println("Saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["splunk_server"] = splunk_server;
    json["splunk_port"] = splunk_port;
    json["splunk_auth"] = splunk_auth;
    json["splunk_index"] = splunk_index;
    json["splunk_sourcetype"] = splunk_sourcetype;
    json["device_location"] = device_location;
    if (strcmp(splunk_wifi_enabled, "yes") != 0) {
      strcpy(splunk_wifi_enabled, "no");
    }
    json["wifi_enabled"] = splunk_wifi_enabled;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    Serial.println("Restarting...");
    tft.println("Config saved.\nRestarting...");
    delay(3000);
    ESP.reset();
  }
  if (wifi_failed && wifi_enabled) {
    Serial.println("Wifi failed to connect\nRestarting...");
    tft.println("Wifi failed to connect\nRestarting...");
    delay(3000);
    ESP.reset();
    delay(5000);
  } else if (wifi_enabled) {
    Serial.println("local ip");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Starting in standalone mode");
    WiFi.disconnect();
    WiFi.forceSleepBegin();
  }
}

void setupScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, 128, 128, TFT_WHITE);
  tft.drawLine(0, 10, 128, 10, TFT_WHITE);
  tft.drawLine(0, 70, 128, 70, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("AQI MONITOR", 64, 2, 1);
  tft.drawLine(0,106,128,106, TFT_WHITE);
  if (wifi_enabled) {
    tft.setCursor(2, 108);
    tft.printf("%s", WiFi.SSID().c_str());
    tft.setCursor(2, 118);
    tft.printf("%s", WiFi.localIP().toString().c_str());
  } else {
    tft.setCursor(2, 108);
    tft.printf("RST for WiFi config");
    tft.setCursor(2, 118);
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.printf("#hacktheplanet");
  }
}

void updateScreen(uint16_t aqi, uint16_t pm2dot5, uint16_t pm10dot0) {
  static uint32_t last_update = 0;
  uint32_t current_update;
  static bool blinky = false;
  char buf[5];

  uint16_t aqi_bin = find_bin(epa_aqi_high, aqi);

  uint16_t bg_color = aqi_bg_color[aqi_bin];
  uint16_t fg_color = aqi_fg_color[aqi_bin];
  
  TFT_eSprite m = TFT_eSprite(&tft);
  m.setColorDepth(8);

  m.createSprite(126,59);
  m.fillSprite(bg_color);
  m.setTextSize(1);
  m.setTextColor(fg_color, bg_color);
  m.setTextDatum(TL_DATUM);
  sprintf(buf, "%d", aqi);
  m.drawCentreString(buf, 64, 4, 6);
  m.drawCentreString(epa_aqi_descriptions[aqi_bin], 64, 42, 2);
  m.pushSprite(1,11);
  m.deleteSprite();
  
  m.createSprite(126,32);
  m.setTextColor(TFT_WHITE, TFT_BLACK);
  m.setTextFont(2);
  m.setTextSize(1);
  m.setCursor(1,1);
  m.fillSprite(TFT_BLACK);
  m.setTextDatum(TL_DATUM);
  m.printf("%3d PM2.5", pm2dot5);
  m.setCursor(1,17);
  m.printf("%3d PM10", pm10dot0);
  m.pushSprite(1,72);
  m.deleteSprite();

  current_update = millis();
  if (current_update - last_update > 500) {
    last_update = current_update;
    blinky = !blinky;
    if (blinky) {
      tft.fillRect(1,1,9,9, TFT_GREEN);
    } else {
      tft.fillRect(1,1,9,9, TFT_BLACK);
    }
  }
}

void show_info() {
  uint8_t i;
  TFT_eSprite m = TFT_eSprite(&tft);
  m.setColorDepth(8);
  m.createSprite(126,59);
  m.setTextDatum(TL_DATUM);
  for (i = 0; i < 6; i++) {
    m.fillRect(0,i*10,126,10, aqi_bg_color[i]);
    m.setTextColor(aqi_fg_color[i], aqi_bg_color[i]);
    m.setCursor(1,i*10);
    m.printf("%3d-%3d: %s", epa_aqi_low[i], epa_aqi_high[i], epa_aqi_descriptions[i]);
  }
  m.pushSprite(1,11);
  m.deleteSprite();
}

void mkpasswd(char *buf, uint8_t len, const char *dict) {
  uint8_t i;
  uint8_t dict_len = strlen(dict);
  uint32_t rand;
  for (i = 0; i < len; i++) {
    rand = ESP8266TrueRandom.random(dict_len);
    buf[i] = dict[rand];
  }
  buf[i] = '\0';
}

void displayQRCode(char *ssid, char *passwd) {
  TFT_eSprite m = TFT_eSprite(&tft);
  uint8_t x, y;
  QRCode qrcode;
  char buf[50];
  uint8_t qrcodeBytes[qrcode_getBufferSize(4)];
  sprintf(buf, "WIFI:T:WPA;S:%s;P:%s;;", ssid, passwd);
  Serial.println(buf);

  qrcode_initText(&qrcode, qrcodeBytes, 4, ECC_MEDIUM, buf);

  m.setColorDepth(8);
  m.createSprite(qrcode.size*2, qrcode.size*2);
  for (y = 0; y < qrcode.size*2; y++) {
    for (x = 0; x < qrcode.size*2; x++) {
      m.drawPixel(x, y, qrcode_getModule(&qrcode, x/2, y/2)?TFT_WHITE:TFT_BLACK);
      // Serial.print(qrcode_getModule(&qrcode, x/2, y/2) ? "\u2588\u2588": "  ");
    }
    Serial.print("\n");
  }
  m.pushSprite(31,62);
  m.deleteSprite();
}
 
void setup() {
  char wifipasswd[17];
  char ssid[17];
  char chipID[10];
  sprintf(chipID, "%x", ESP.getChipId());
  Serial.begin(115200);
  Serial.println();
  Serial.printf("Chip ID: %s\n", chipID);

  Serial.println("Initializing LCD...");

  tft.init();
  tft.setRotation(0);
  setupScreen();
  show_info();

  tft.setCursor(2,72);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.printf("Chip ID %x", ESP.getChipId());
  Serial.println("Screen initialized.");
  tft.setCursor(2,82);
  tft.printf("Hold FLASH to setup");
  tft.setCursor(2,92);
  tft.printf("WiFi");

  Serial.println("Hold FLASH to setup wifi");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(0, INPUT_PULLUP);
  last_loop_time = 0;
  
  delay(5000);

  if (digitalRead(0) == 0) {
    Serial.printf("Generating nonce SSID and password\n");
    sprintf(ssid, "AQI %x", ESP.getChipId());
    Serial.printf("Sample SSID %s\n", ssid);
    mkpasswd(wifipasswd, 16, passwd_chars);
    Serial.printf("Sample wifi password %s\n", wifipasswd);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0,0,1);
    tft.println("Resetting wifi");
    tft.println("Connect to");
    tft.printf("SSID %s\n", ssid);
    tft.printf("PASS %s\n", wifipasswd);
    tft.println("and open");
    tft.println("http://192.168.4.1");
    displayQRCode(ssid, wifipasswd);
    WiFiSetup(true, ssid, wifipasswd);    
  } else {
    tft.println("Loading saved WiFi conf");
    WiFiSetup(false, "", "");
  } 

  setupScreen();
  show_info();

  if (wifi_enabled) {
    splunk.init(splunk_server, splunk_port, splunk_auth, chipID, splunk_sourcetype, splunk_index);
  }

  tft.setCursor(2,72);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.printf("Waiting for ");
  tft.setCursor(2,82);
  tft.printf("sensor data");
  Serial.println("Starting PMS Initialization");
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModeActive);

  Serial.printf("PMS initialized.  Waiting for data...");
}

void loop() {
  uint32_t loop_time;

  const auto n = Pmsx003::Reserved;
  Pmsx003::pmsData data[n];
  Pmsx003::PmsStatus status = pms.read(data, n);

  uint16_t aqi;

  String eventString;
  
  switch(status) {
    case Pmsx003::OK:
    {
      aqi = calc_AQI(data[Pmsx003::PM2dot5], data[Pmsx003::PM10dot0]);
      Serial.printf("AQI: %d\tPM2.5: %d\t PM10: %d\n", aqi, data[Pmsx003::PM2dot5], data[Pmsx003::PM10dot0]);
      updateScreen(aqi, data[Pmsx003::PM2dot5], data[Pmsx003::PM10dot0]);

      if (wifi_enabled) {
        eventString = "{\"PM2dot5\": \"" + String(data[Pmsx003::PM2dot5]) + 
          "\", \"PM10dot0\": \"" + String(data[Pmsx003::PM10dot0]) + 
          "\", \"aqi\":\"" + String(aqi) +
          "\",  \"device_location\":\"" + String(device_location) + 
          "\"}";
        splunk.sendEvent(eventString);
      }
    }
    case Pmsx003::noData:
      break;
    default:
    {
      Serial.printf("Error reading from PMSx003: %s\n", Pmsx003::errorMsg[status]);
    }
  }

  loop_time = millis() - last_loop_time;
  if (loop_time > 500) {
    last_loop_time = millis();
    digitalWrite(LED_BUILTIN, blinky_state);
    blinky_state = !blinky_state;
  }
}