#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "Globals.h"
#include "Utils.h"
#include "ConfigManager.h" // Web server needs to save config
#include "index_html.h"

void setupWebServer();
void ensureHtmlFileExists();
void handleCaptivePortal(AsyncWebServerRequest *request);

const char *getSafeHAApiKey();
const char *getSafeApiKey();
const char *getSafePassword();
const char *getSafeSsid();

#endif