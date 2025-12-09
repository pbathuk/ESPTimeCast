#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <FS.h>
#include "Globals.h"
#include "Utils.h"

void fetchWeather();
String normalizeWeatherDescription(String str);
String buildWeatherURL();
String buildHomeAssistantURL(String entityName);
String getHAJSON(String entityID, JsonDocument &doc);
String getHAEntityState(String entityID);
String getHASun(int &sunriseHour, int &sunriseMinute, int &sunsetHour, int &sunsetMinute);

#endif