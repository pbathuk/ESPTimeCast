#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include "ConfigManager.h" // Web server needs to save config
#include "Globals.h"
#include "Utils.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

void handleCaptivePortal(AsyncWebServerRequest *request);
void setupWebServer();

#endif