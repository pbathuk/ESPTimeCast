#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Globals.h" // Access to variables
#include "Utils.h"

unsigned long getTotalRuntimeSeconds();
String formatTotalRuntime();
void loadConfig();
void printConfigToSerial();
void loadUptime();
void saveUptime();
void saveCustomMessageToConfig(const char *msg);
bool saveCountdownConfig(bool enabled, time_t targetTimestamp, const String &label);

#endif