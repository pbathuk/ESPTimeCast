#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
//#include <esp_sntp.h>
//#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "Globals.h"
#include "Globals.h"
#include "Utils.h"
#include "tz_lookup.h"      // Timezone lookup, do not duplicate mapping here!

void connectWiFi();
void setupMDNS();
void setupTime();

#endif