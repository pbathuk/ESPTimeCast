#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <MD_Parola.h> // Needed for textEffect_t
#include <ArduinoJson.h>  // Needed for the getJsonValue template
#include "Globals.h"

// Declare your helpers here
bool isNumber(const char *str);
textEffect_t getEffectiveScrollDirection(textEffect_t desiredDirection, bool isFlipped);
void advanceDisplayMode();
void advanceDisplayModeSafe();
bool isFiveDigitZip(const char *str);
String formatUptime(unsigned long seconds);
// Universal helper for ArduinoJson v7
// 'obj' can be the main doc, or a nested object like doc["countdown"]
template <typename T>
T getJsonValue(JsonVariantConst obj, const char* key, T defaultValue) {
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

#endif