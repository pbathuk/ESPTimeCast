#ifndef UTILS_H
#define UTILS_H

#include "Globals.h"
#include "index_html.h"
#include <ArduinoJson.h> // Needed for the getJsonValue template
#include <HTTPClient.h>
#include <LittleFS.h>

template <typename T>
T getJsonValue(JsonVariantConst obj, const char *key, T defaultValue) {
  // ArduinoJson v7 handles null checks automatically.
  // If 'obj' is null, obj[key] is null.
  // .is<T>() checks if the key exists AND holds the correct type.
  if (obj[key].is<T>()) {
    return obj[key].as<T>();
  }
  Serial.print(F("[CONFIG] Key '"));
  Serial.print(key);
  Serial.print(F("' missing or invalid. Defaulting to: "));
  Serial.println(defaultValue);
  return defaultValue;
}
textEffect_t getEffectiveScrollDirection(textEffect_t desiredDirection,
                                         bool isFlipped);
void advanceDisplayMode();
void advanceDisplayModeSafe();
bool isNumber(const char *str);
bool isFiveDigitZip(const char *str);
String formatUptime(unsigned long seconds);
void audio_info(const char *info);
void audio_eof_mp3(const char *info);
unsigned long getTotalRuntimeSeconds();
String formatTotalRuntime();
String getHAJSON(String entityID, JsonDocument &doc);
String getHAEntityState(String entityID);
String buildWeatherURL();
String buildHomeAssistantURL(String entityName);
const char *getSafeHAApiKey();
const char *getSafeApiKey();
const char *getSafePassword();
const char *getSafeSsid();
void ensureHtmlFileExists();

#endif