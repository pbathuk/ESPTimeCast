#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "Globals.h"
#include "Utils.h"
#include "ConfigManager.h" // Web server needs to save config

void handleCaptivePortal(AsyncWebServerRequest *request);
void setupWebServer();

#endif