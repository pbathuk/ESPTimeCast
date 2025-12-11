#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <ESPmDNS.h>
#include "Globals.h"
#include "Utils.h"
#include "tz_lookup.h"

void connectWiFi();
void setupMDNS();
void setupTime();

#endif