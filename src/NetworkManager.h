#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "Globals.h"
#include "Utils.h"
#include "esp_sntp.h"
#include <ESPmDNS.h>
#include <WiFi.h>

void connectWiFi();
void setupMDNS();
void setupTime();

#endif