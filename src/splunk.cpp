#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include "splunk.h"

void HEC::init (String l_Host, String l_Port, String l_Token, String l_localHost, String l_sourcetype, String l_index) {
    splunkHost = l_Host;
    splunkPort = l_Port;
    splunkToken = l_Token;
    localHost = l_localHost;
    sourcetype = l_sourcetype;
    index = l_index;

    splunkUrl = "http://" + splunkHost + ":" + splunkPort + "/services/collector";
    tokenValue = "Splunk " + splunkToken;
}

void HEC::init (char *l_Host, char  *l_Port, char *l_Token, char *l_localHost, char *l_sourcetype, char *l_index) {
    init(String(l_Host), String(l_Port), String(l_Token), String(l_localHost), String(l_sourcetype), String(l_index));
}

void HEC::sendEvent(String event) {
    String payload = "{\"host\":\"" + localHost + "\", \"sourcetype\":\"" + sourcetype + "\", \"index\":\""+ index + "\", \"event\": " + event + "}";
    String contentLength;
    
    HTTPClient http;
    if (debug) Serial.println(splunkUrl);
    http.begin(splunkUrl);

    http.addHeader("Content-Type", "application/json");
    if (debug) Serial.println(tokenValue);
    http.addHeader("Authorization", tokenValue);
    contentLength = payload.length();
    http.addHeader("Content-Length", contentLength);
    if (debug) Serial.println(payload);
    http.POST(payload);
    if (debug) http.writeToStream(&Serial);
    http.end();
}