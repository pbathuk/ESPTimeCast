#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include "Globals.h"
#include "Utils.h"

void fetchWeather();
String normalizeWeatherDescription(String str);
String getHASun(int &sunriseHour, int &sunriseMinute, int &sunsetHour,
                int &sunsetMinute);

#endif