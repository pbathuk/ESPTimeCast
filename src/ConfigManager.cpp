#include "ConfigManager.h"


// -----------------------------
// Get total uptime including current session
// -----------------------------
unsigned long getTotalRuntimeSeconds() {
  return totalUptimeSeconds + (millis() - bootMillis) / 1000;
}

// -----------------------------
// Format total uptime as HH:MM:SS
// -----------------------------
String formatTotalRuntime() {
  unsigned long secs = getTotalRuntimeSeconds();
  unsigned int h = secs / 3600;
  unsigned int m = (secs % 3600) / 60;
  unsigned int s = secs % 60;
  char buf[16];
  sprintf(buf, "%02u:%02u:%02u", h, m, s);
  return String(buf);
}


// -----------------------------------------------------------------------------
// Configuration Load & Save
// -----------------------------------------------------------------------------
void loadConfig() {
  Serial.println(F("[CONFIG] Loading configuration..."));

  // Check if config.json exists, if not, create default
  if (!LittleFS.exists("/config.json")) {
    Serial.println(F("[CONFIG] config.json not found, creating with defaults..."));
    JsonDocument doc;
    doc[F("ssid")] = ssid;
    doc[F("password")] = password;
    doc[F("useHomeAssistant")] = useHomeAssistant;
    doc[F("homeAssistantURL")] = homeAssistantURL;
    doc[F("homeAssistantApiKey")] = homeAssistantApiKey;
    doc[F("haTempSensor")] = haTempSensor;
    doc[F("haHumiditySensor")] = haHumiditySensor;
    doc[F("openWeatherApiKey")] = openWeatherApiKey;
    doc[F("openWeatherCity")] = openWeatherCity;
    doc[F("openWeatherCountry")] = openWeatherCountry;
    doc[F("weatherUnits")] = weatherUnits;
    doc[F("clockDuration")] = clockDuration;
    doc[F("weatherDuration")] = weatherDuration;
    doc[F("timeZone")] = timeZone;
    doc[F("language")] = language;
    doc[F("brightness")] = brightness;
    doc[F("flipDisplay")] = flipDisplay;
    doc[F("twelveHourToggle")] = twelveHourToggle;
    doc[F("amPMShow")] = amPMShow;    
    doc[F("showDayOfWeek")] = showDayOfWeek;
    doc[F("showDate")] = showDate;
    doc[F("showHumidity")] = showHumidity;
    doc[F("colonBlinkEnabled")] = colonBlinkEnabled;
    doc[F("ntpServer1")] = ntpServer1;
    doc[F("ntpServer2")] = ntpServer2;
    doc[F("dimmingEnabled")] = dimmingEnabled;
    doc[F("dimStartHour")] = dimStartHour;
    doc[F("dimStartMinute")] = dimStartMinute;
    doc[F("dimEndHour")] = dimEndHour;
    doc[F("dimEndMinute")] = dimEndMinute;
    doc[F("dimBrightness")] = dimBrightness;
    doc[F("showWeatherDescription")] = showWeatherDescription;

    // --- Automatic dimming defaults ---
    doc[F("autoDimmingEnabled")] = autoDimmingEnabled;
    doc[F("sunriseHour")] = sunriseHour;
    doc[F("sunriseMinute")] = sunriseMinute;
    doc[F("sunsetHour")] = sunsetHour;
    doc[F("sunsetMinute")] = sunsetMinute;

    // Add countdown defaults when creating a new config.json
    JsonObject countdownObj = doc["countdown"].to<JsonObject>();
    countdownObj["enabled"] = false;
    countdownObj["targetTimestamp"] = 0;
    countdownObj["label"] = "";
    countdownObj["isDramaticCountdown"] = true;
    
    File f = LittleFS.open("/config.json", "w");
    if (f) {
      serializeJsonPretty(doc, f);
      f.close();
      Serial.println(F("[CONFIG] Default config.json created."));
    } else {
      Serial.println(F("[ERROR] Failed to create default config.json"));
    }
  }

  Serial.println(F("[CONFIG] Attempting to open config.json for reading."));
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println(F("[ERROR] Failed to open config.json for reading. Cannot load config."));
    return;
  }

  JsonDocument doc;  // Size based on ArduinoJson Assistant + buffer
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.print(F("[ERROR] JSON parse failed during load: "));
    Serial.println(error.f_str());
    return;
  }

  strlcpy(ssid, getJsonValue(doc, "ssid", ""), sizeof(ssid));
  strlcpy(password, getJsonValue(doc, "password", ""), sizeof(password));
  strlcpy(homeAssistantURL, getJsonValue(doc, "homeAssistantURL", ""), sizeof(homeAssistantURL));
  strlcpy(homeAssistantApiKey, getJsonValue(doc, "homeAssistantApiKey", ""), sizeof(homeAssistantApiKey));
  strlcpy(haTempSensor, getJsonValue(doc, "haTempSensor", ""), sizeof(haTempSensor));
  strlcpy(haHumiditySensor, getJsonValue(doc, "haHumiditySensor", ""), sizeof(haHumiditySensor));
  strlcpy(openWeatherApiKey, getJsonValue(doc, "openWeatherApiKey", ""), sizeof(openWeatherApiKey));
  strlcpy(openWeatherCity, getJsonValue(doc, "openWeatherCity", ""), sizeof(openWeatherCity));
  strlcpy(openWeatherCountry, getJsonValue(doc, "openWeatherCountry", ""), sizeof(openWeatherCountry));
  strlcpy(weatherUnits, getJsonValue(doc, "weatherUnits", "metric"), sizeof(weatherUnits));
  strlcpy(customMessage, getJsonValue(doc, "customMessage", ""), sizeof(customMessage));
  strlcpy(lastPersistentMessage, customMessage, sizeof(lastPersistentMessage));
  clockDuration = getJsonValue(doc, "clockDuration", 10000);
  weatherDuration = getJsonValue(doc, "weatherDuration", 5000);

  strlcpy(timeZone, getJsonValue(doc, "timeZone", "Etc/UTC"), sizeof(timeZone));
  strlcpy(language, getJsonValue(doc, "language", "en"), sizeof(language));

  brightness = getJsonValue(doc, "brightness", 7);
  flipDisplay = getJsonValue(doc, "flipDisplay", false);
  twelveHourToggle = getJsonValue(doc, "twelveHourToggle", false);
  amPMShow = getJsonValue(doc, "amPMShow", false);
  showDayOfWeek = getJsonValue(doc, "showDayOfWeek", true);
  showDate = getJsonValue(doc, "showDate", false);
  showHumidity = getJsonValue(doc, "showHumidity", false);
  useHomeAssistant = getJsonValue(doc, "useHomeAssistant", false);
  colonBlinkEnabled = getJsonValue(doc, "colonBlinkEnabled", true);
  showWeatherDescription = getJsonValue(doc, "showWeatherDescription", false);
  dimmingEnabled = getJsonValue(doc, "dimmingEnabled", false);
  autoDimmingEnabled = getJsonValue(doc, "autoDimmingEnabled", false);

  dimStartHour = getJsonValue(doc, "dimStartHour", 18);
  dimStartMinute = getJsonValue(doc, "dimStartMinute", 0);
  dimEndHour = getJsonValue(doc, "dimEndHour", 8);
  dimEndMinute = getJsonValue(doc, "dimEndMinute", 0);
  dimBrightness = getJsonValue(doc, "dimBrightness", 2);
  sunriseHour = getJsonValue(doc, "sunriseHour", 6);
  sunriseMinute = getJsonValue(doc, "sunriseMinute", 0);
  sunsetHour = getJsonValue(doc, "sunsetHour", 18);
  sunsetMinute = getJsonValue(doc, "sunsetMinute", 0);

  strlcpy(ntpServer1, getJsonValue(doc, "ntpServer1", "pool.ntp.org"), sizeof(ntpServer1));
  strlcpy(ntpServer2, getJsonValue(doc, "ntpServer2", "time.nist.gov"), sizeof(ntpServer2));

  if (strcmp(weatherUnits, "imperial") == 0)
    tempSymbol = ']';
  else
    tempSymbol = '[';


  // --- COUNTDOWN CONFIG LOADING ---
  if (doc["countdown"]) {
    JsonObject countdownObj = doc["countdown"];

    countdownEnabled = getJsonValue(countdownObj, "enabled", false);
    countdownTargetTimestamp = getJsonValue(countdownObj, "targetTimestamp", 0);
    isDramaticCountdown = getJsonValue(countdownObj, "isDramaticCountdown", true);

    JsonVariant labelVariant = countdownObj["label"];
    if (labelVariant.isNull() || !labelVariant.is<const char *>()) {
      strcpy(countdownLabel, "");
    } else {
      const char *labelTemp = labelVariant.as<const char *>();
      size_t labelLen = strlen(labelTemp);
      if (labelLen >= sizeof(countdownLabel)) {
        Serial.println(F("[CONFIG] label from JSON too long, truncating."));
      }
      strlcpy(countdownLabel, labelTemp, sizeof(countdownLabel));
    }
    countdownFinished = false;
  } else {
    countdownEnabled = false;
    countdownTargetTimestamp = 0;
    strcpy(countdownLabel, "");
    isDramaticCountdown = true;
    Serial.println(F("[CONFIG] Countdown object not found, defaulting to disabled."));
    countdownFinished = false;
  }
  Serial.println(F("[CONFIG] Configuration loaded."));
}


// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------
void printConfigToSerial() {
  Serial.println(F("========= Loaded Configuration ========="));
  Serial.print(F("WiFi SSID: "));
  Serial.println(ssid);
  Serial.print(F("WiFi Password: "));
  Serial.println(password);
  Serial.print(F("OpenWeather City: "));
  Serial.println(openWeatherCity);
  Serial.print(F("OpenWeather Country: "));
  Serial.println(openWeatherCountry);
  Serial.print(F("OpenWeather API Key: "));
  Serial.println(openWeatherApiKey);
  Serial.print(F("Use Home Assistant rather than OpenWeather: "));
  Serial.println(useHomeAssistant ? "Yes" : "No");
  Serial.print(F("HomeAssistant URL: "));
  Serial.println(homeAssistantURL);
  Serial.print(F("HomeAssistant API Key: "));
  Serial.println(homeAssistantApiKey);
  Serial.print(F("HomeAssistant Temperature Entity: "));
  Serial.println(haTempSensor);
  Serial.print(F("HomeAssistant Humidity Entity: "));
  Serial.println(haHumiditySensor);
  Serial.print(F("Temperature Unit: "));
  Serial.println(weatherUnits);
  Serial.print(F("Clock duration: "));
  Serial.println(clockDuration);
  Serial.print(F("Weather duration: "));
  Serial.println(weatherDuration);
  Serial.print(F("TimeZone (IANA): "));
  Serial.println(timeZone);
  Serial.print(F("Days of the Week/Weather description language: "));
  Serial.println(language);
  Serial.print(F("Brightness: "));
  Serial.println(brightness);
  Serial.print(F("Flip Display: "));
  Serial.println(flipDisplay ? "Yes" : "No");
  Serial.print(F("Show 12h Clock: "));
  Serial.println(twelveHourToggle ? "Yes" : "No");
  Serial.print(F("Show A / P on Clock: "));
  Serial.println(amPMShow ? "Yes" : "No");
  Serial.print(F("Show Day of the Week: "));
  Serial.println(showDayOfWeek ? "Yes" : "No");
  Serial.print(F("Show Date: "));
  Serial.println(showDate ? "Yes" : "No");
  Serial.print(F("Show Weather Description: "));
  Serial.println(showWeatherDescription ? "Yes" : "No");
  Serial.print(F("Show Humidity: "));
  Serial.println(showHumidity ? "Yes" : "No");
  Serial.print(F("Blinking colon: "));
  Serial.println(colonBlinkEnabled ? "Yes" : "No");
  Serial.print(F("NTP Server 1: "));
  Serial.println(ntpServer1);
  Serial.print(F("NTP Server 2: "));
  Serial.println(ntpServer2);

  // ---------------------------------------------------------------------------
  // DIMMING SECTION
  // ---------------------------------------------------------------------------
  Serial.print(F("Automatic Dimming: "));
  Serial.println(autoDimmingEnabled ? "Enabled" : "Disabled");
  Serial.print(F("Custom Dimming: "));
  Serial.println(dimmingEnabled ? "Enabled" : "Disabled");

  if (autoDimmingEnabled) {
    // --- Automatic (Sunrise/Sunset) dimming mode ---
    if ((sunriseHour == 6 && sunriseMinute == 0) && (sunsetHour == 18 && sunsetMinute == 0)) {
      Serial.println(F("Automatic Dimming Schedule: Sunrise/Sunset Data not available yet (waiting for weather update)"));
    } else {
      Serial.printf("Automatic Dimming Schedule: Sunrise: %02d:%02d → Sunset: %02d:%02d\n",
                    sunriseHour, sunriseMinute, sunsetHour, sunsetMinute);

      time_t now_time = time(nullptr);
      struct tm localTime;
      localtime_r(&now_time, &localTime);

      int curTotal = localTime.tm_hour * 60 + localTime.tm_min;
      int startTotal = sunsetHour * 60 + sunsetMinute;
      int endTotal = sunriseHour * 60 + sunriseMinute;

      bool autoActive = (startTotal < endTotal)
                          ? (curTotal >= startTotal && curTotal < endTotal)
                          : (curTotal >= startTotal || curTotal < endTotal);

      Serial.printf("Current Auto-Dimming Status: %s\n", autoActive ? "ACTIVE" : "Inactive");
      Serial.printf("Dimming Brightness (night): %d\n", dimBrightness);
    }
  } else {
    // --- Manual (Custom Schedule) dimming mode ---
    Serial.printf("Custom Dimming Schedule: %02d:%02d → %02d:%02d\n",
                  dimStartHour, dimStartMinute, dimEndHour, dimEndMinute);
    Serial.printf("Dimming Brightness: %d\n", dimBrightness);
  }

  Serial.print(F("Countdown Enabled: "));
  Serial.println(countdownEnabled ? "Yes" : "No");
  Serial.print(F("Countdown Target Timestamp: "));
  Serial.println(countdownTargetTimestamp);
  Serial.print(F("Countdown Label: "));
  Serial.println(countdownLabel);
  Serial.print(F("Dramatic Countdown Display: "));
  Serial.println(isDramaticCountdown ? "Yes" : "No");
  Serial.print(F("Custom Message: "));
  Serial.println(customMessage);

  Serial.print(F("Total Runtime: "));
  if (getTotalRuntimeSeconds() > 0) {
    Serial.println(formatTotalRuntime());
  } else {
    Serial.println(F("No runtime recorded yet."));
  }

  Serial.println(F("========================================"));
  Serial.println();
}


// -----------------------------
// Load uptime from LittleFS
// -----------------------------
void loadUptime() {
  if (LittleFS.exists("/uptime.dat")) {
    File f = LittleFS.open("/uptime.dat", "r");
    if (f) {
      totalUptimeSeconds = f.parseInt();
      f.close();
      bootMillis = millis();
      Serial.printf("[UPTIME] Loaded accumulated uptime: %lu seconds (%.2f hours)\n",
                    totalUptimeSeconds, totalUptimeSeconds / 3600.0);
    } else {
      Serial.println(F("[UPTIME] Failed to open /uptime.dat for reading."));
      totalUptimeSeconds = 0;
      bootMillis = millis();
    }
  } else {
    Serial.println(F("[UPTIME] No previous uptime file found. Starting from 0."));
    totalUptimeSeconds = 0;
    bootMillis = millis();
  }
}


// -----------------------------
// Save uptime to LittleFS
// -----------------------------
void saveUptime() {
  // Use getTotalRuntimeSeconds() to include current session
  totalUptimeSeconds = getTotalRuntimeSeconds();
  bootMillis = millis();  // reset session start

  File f = LittleFS.open("/uptime.dat", "w");
  if (f) {
    f.print(totalUptimeSeconds);
    f.close();
    Serial.printf("[UPTIME] Saved accumulated uptime: %s\n", formatTotalRuntime().c_str());
  } else {
    Serial.println(F("[UPTIME] Failed to write /uptime.dat"));
  }
}



void saveCustomMessageToConfig(const char *msg) {
  Serial.println(F("[CONFIG] Updating customMessage in config.json..."));

  JsonDocument doc;

  // Load existing config.json (if present)
  File configFile = LittleFS.open("/config.json", "r");
  if (configFile) {
    DeserializationError err = deserializeJson(doc, configFile);
    configFile.close();
    if (err) {
      Serial.print(F("[CONFIG] Error reading existing config: "));
      Serial.println(err.f_str());
    }
  }

  // Update only customMessage
  doc["customMessage"] = msg;

  // Safely write back to config.json
  if (LittleFS.exists("/config.json")) {
    LittleFS.rename("/config.json", "/config.bak");
  }

  File f = LittleFS.open("/config.json", "w");
  if (!f) {
    Serial.println(F("[CONFIG] ERROR: Failed to open /config.json for writing"));
    return;
  }

  size_t bytesWritten = serializeJson(doc, f);
  f.close();
  Serial.printf("[CONFIG] Saved customMessage='%s' (%u bytes written)\n", msg, bytesWritten);
}

//config save after countdown finishes
bool saveCountdownConfig(bool enabled, time_t targetTimestamp, const String &label) {
  JsonDocument doc;

  File configFile = LittleFS.open("/config.json", "r");
  if (configFile) {
    DeserializationError err = deserializeJson(doc, configFile);
    configFile.close();
    if (err) {
      Serial.print(F("[saveCountdownConfig] Error parsing config.json: "));
      Serial.println(err.f_str());
      return false;
    }
  }

  JsonObject countdownObj = doc["countdown"].is<JsonObject>() ? doc["countdown"].as<JsonObject>() : doc["countdown"].to<JsonObject>();
  countdownObj["enabled"] = enabled;
  countdownObj["targetTimestamp"] = targetTimestamp;
  countdownObj["label"] = label;
  countdownObj["isDramaticCountdown"] = isDramaticCountdown;
  doc.remove("countdownEnabled");
  doc.remove("countdownDate");
  doc.remove("countdownTime");
  doc.remove("countdownLabel");

  if (LittleFS.exists("/config.json")) {
    LittleFS.rename("/config.json", "/config.bak");
  }

  File f = LittleFS.open("/config.json", "w");
  if (!f) {
    Serial.println(F("[saveCountdownConfig] ERROR: Cannot write to /config.json"));
    return false;
  }

  size_t bytesWritten = serializeJson(doc, f);
  f.close();

  Serial.printf("[saveCountdownConfig] Config updated. %u bytes written.\n", bytesWritten);
  return true;
}
