#ifndef WIFISERVICE_H
#define WIFISERVICE_H

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include "WiFiWebRes.h"


#define OTANAME         "Autuino"
#define OTAPW           "12345"
#define HOSTDNSNAME     "autuino"
#define LOCALWIFINAME   "Autuino-WiFi"
#define LOCALWIFIPW     "12345678"

char ssid[32] = "";
char password[64] = "";
char* http_username = "";
char* http_password = "";
bool hide_on_wifi_connected = false;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
/*
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->opcode == WS_TEXT) {
      String message = "";
      for (size_t i = 0; i < len; i++) {
        message += (char)data[i];
      }
      if (message.startsWith("blinky")) {
        int argumento = message.substring(6, message.length()).toInt();
        if (argumento == 1) {
          ws.textAll("Blink Off!");
        }else if(argumento == 0){
          ws.textAll("Blink On!");
        }else{
          ws.textAll("Wrong Argument!");
        }
      }else{
        ws.textAll("Wrong Command!");
      }
    }
  }
}
*/

void setupOTA() {
  ArduinoOTA.setHostname(OTANAME);
  ArduinoOTA.setPassword(OTAPW);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
        
        Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}

void saveCredentials(AsyncWebServerRequest *request) {
  if (request->hasArg("ssid") && request->hasArg("password")) {
    request->arg("ssid").toCharArray(ssid, request->arg("ssid").length() + 1);
    request->arg("password").toCharArray(password, request->arg("password").length() + 1);
    WiFi.disconnect(true);
    File credsFile = LittleFS.open("/credentials.txt", "w");
    if (credsFile) {
      credsFile.println(ssid);
      credsFile.println(password);
      credsFile.close();

      request->send(200, "text/html", "Credentials saved successfully.<br>Restart the device to connect to the WiFi network.");
      ESP.restart();
    }
    request->send(500, "text/html", "Error: Failed to save credentials.");
  } else {
    request->send(400, "text/html", "Error: You must provide valid SSID and password.");
  }
}

void getSaveCredentials() {
  File credsFile = LittleFS.open("/credentials.txt", "r");
  if (credsFile) {
    (credsFile.readStringUntil('\n')).toCharArray(ssid, sizeof(ssid));
    (credsFile.readStringUntil('\n')).toCharArray(password, sizeof(password));
    credsFile.close();
    ssid[strlen(ssid)-1] = '\0';
    password[strlen(password)-1] = '\0'; //Clear end garbage character
  }else{
    if (LittleFS.format()) {
      Serial.println("LittleFS format sucessfull");
    } else {
      Serial.println("LittleFS format error");
    }
  }
}

void serverSetup() {
  server.end();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if((http_username != "" || http_password != "") && !request->authenticate(http_username, http_password)){
      request->requestAuthentication();
    }
    request->send(200, "text/html", homePage);
  });
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", wifiPage);
  });
  server.on("/saveCredentials", HTTP_POST, saveCredentials);
  server.on("/resource", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("n")) {
      String resourceName = request->getParam("n")->value();
      if (resourceName == "favicon") {
        request->send_P(200, "image/png", favicon_data, sizeof(favicon_data));
      } else {
        request->send(404);
      }
    }else{
      request->send(400);
    }
  });
  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin(); 
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  if (hide_on_wifi_connected) {
    WiFi.softAPdisconnect(true);
    serverSetup();
  }
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  if (hide_on_wifi_connected) {
    WiFi.begin(ssid, password);
    WiFi.softAP(LOCALWIFINAME,LOCALWIFIPW);
  }
}

void WiFiSetup(){
  //Serial.begin(115200);
  if (!LittleFS.begin(true)) {
      Serial.print("Error LittleFS");
  }
  getSaveCredentials();
  WiFi.mode(WIFI_AP_STA);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  //WiFi.disconnect(true);
  WiFi.begin(ssid, password);
  WiFi.softAP(LOCALWIFINAME,LOCALWIFIPW);
  MDNS.begin(HOSTDNSNAME);
  MDNS.addService("http", "tcp", 80);
  setupOTA();
  serverSetup();
}

void WiFiLoop(){
  ArduinoOTA.handle();
}

#endif