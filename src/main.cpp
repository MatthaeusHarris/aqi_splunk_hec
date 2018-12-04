#include <FS.h>

#include <ESP8266WiFi.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <Arduino.h>
#include <ArduinoJson.h>

#include <TFT_eSPI.h>

#include "airquality.h"
#include "splunk.h"

char splunk_server[255];
char splunk_port[6] = "8080";
char splunk_auth[40];
char splunk_index[40];
char splunk_sourcetype[40];

bool shouldSaveConfig = false;

uint32_t last_loop_time;
bool blinky_state = true;
bool switch_state = false;
bool last_switch_state = false;
uint16_t count = 0;

Pmsx003 pms(D4, D3);

TFT_eSPI tft = TFT_eSPI();

HEC splunk = HEC();

void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void WiFiSetup(bool reset) {
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

  WiFiManagerParameter custom_splunk_server("server", "splunk server", splunk_server, 255);
  WiFiManagerParameter custom_splunk_port("port", "8080", splunk_port, 6);
  WiFiManagerParameter custom_splunk_auth("auth", "HEC Token", splunk_auth, 40);
  WiFiManagerParameter custom_splunk_index("index", "index", splunk_index, 40);
  WiFiManagerParameter custom_splunk_sourcetype("sourcetype", "sourcetype", splunk_sourcetype, 40);

  WiFiManager wifiManager;

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_splunk_server);
  wifiManager.addParameter(&custom_splunk_port);
  wifiManager.addParameter(&custom_splunk_auth);
  wifiManager.addParameter(&custom_splunk_index);
  wifiManager.addParameter(&custom_splunk_sourcetype);

  if (reset) {
    wifiManager.resetSettings();
  }
  
  if (!wifiManager.autoConnect("WifiManager")) {
    Serial.println("failed to connect and hit timeout");
    tft.println("Failed to connect to wifi");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  Serial.println("Connected!");

  strcpy(splunk_server, custom_splunk_server.getValue());
  strcpy(splunk_port, custom_splunk_port.getValue());
  strcpy(splunk_auth, custom_splunk_auth.getValue());
  strcpy(splunk_index, custom_splunk_index.getValue());
  strcpy(splunk_sourcetype, custom_splunk_sourcetype.getValue());

  if (shouldSaveConfig) {
    Serial.println("Saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["splunk_server"] = splunk_server;
    json["splunk_port"] = splunk_port;
    json["splunk_auth"] = splunk_auth;
    json["splunk_index"] = splunk_index;
    json["splunk_sourcetype"] = splunk_sourcetype;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("Chip ID: %x\n", ESP.getChipId());

  char chip_id[10];
  sprintf(chip_id, "%x", ESP.getChipId());

  Serial.println("Initializing LCD...");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(4,4,1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Screen initialized.");
  Serial.println("Screen initialized.");
  tft.println("Press FLASH to reset wifi");

  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(0, INPUT_PULLUP);
  last_loop_time = 0;
  
  delay(3000);

  if (digitalRead(0) == 0) {
    Serial.printf("\nButton %d\n", switch_state);
    tft.println("Resetting wifi");
    tft.println("SSID WifiManager");
    tft.println("http://192.168.4.1");
    WiFiSetup(true);    
  } else {
    tft.println("Connecting to wifi");
    WiFiSetup(false);
  }

  splunk.init(splunk_server, splunk_port, splunk_auth, chip_id, splunk_sourcetype, splunk_index);

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,8,1);
  tft.drawCentreString("AQI MONITOR", 64, 0, 1);
  // tft.println("AQI MONITOR");
  tft.printf("SSID %s\n", WiFi.SSID().c_str());
  tft.printf("IP %s\n", WiFi.localIP().toString().c_str());
  tft.printf("Device %x\n", ESP.getChipId());
  tft.printf("Index %s\n", splunk_index);
  tft.printf("%s\n", splunk_server);
  Serial.println("Starting PMS Initialization");
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModeActive);

  Serial.printf("PMS initialized.  Waiting for data...");
  
}

void loop() {
  // put your main code here, to run repeatedly:
  uint32_t loop_time;
  char buf[6];

  const auto n = Pmsx003::Reserved;
  Pmsx003::pmsData data[n];
  Pmsx003::PmsStatus status = pms.read(data, n);

  uint16_t aqi;

  uint16_t cursorY = tft.getCursorY();

  String eventString;
  
  switch(status) {
    case Pmsx003::OK:
    {
      // Serial.println("__________________");
      aqi = calc_AQI(data[Pmsx003::PM2dot5], data[Pmsx003::PM10dot0]);
      Serial.printf("AQI: %d\tPM2.5: %d\t PM10: %d\n", aqi, data[Pmsx003::PM2dot5], data[Pmsx003::PM10dot0]);
      // tft.setCursor(4,cursorY);
      // tft.printf("AQI: %d     ", aqi);
      sprintf(buf, "%d", aqi);
      tft.setTextSize(3);
      tft.drawCentreString("      ", 64, 64, 2);
      tft.drawCentreString(buf, 64, 64, 2);

      eventString = "{\"PM2dot5\": \"" + String(data[Pmsx003::PM2dot5]) + "\", \"PM10dot0\": \"" + String(data[Pmsx003::PM10dot0]) + "\", \"aqi\":\"" + String(aqi) + "\" }";

      // sendEvent(eventString, String(splunk_server), String(splunk_port), String(splunk_auth));
      splunk.sendEvent(eventString);
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
    digitalWrite(BUILTIN_LED, blinky_state);
    blinky_state = !blinky_state;
  }
}