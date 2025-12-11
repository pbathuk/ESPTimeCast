#include "Utils.h"

// Scroll flipped
textEffect_t getEffectiveScrollDirection(textEffect_t desiredDirection, bool isFlipped) {
  if (isFlipped) {
    // If the display is horizontally flipped, reverse the horizontal scroll direction
    if (desiredDirection == PA_SCROLL_LEFT) {
      return PA_SCROLL_RIGHT;
    } else if (desiredDirection == PA_SCROLL_RIGHT) {
      return PA_SCROLL_LEFT;
    }
  }
  return desiredDirection;
}

void advanceDisplayMode() {
  prevDisplayMode = displayMode;
  int oldMode = displayMode;
  String ntpField = String(ntpServer2);
  bool nightscoutConfigured = ntpField.startsWith("https://");

  if (displayMode == 0) {  // Clock
    if (showDate) {
      displayMode = 5;  // Date mode right after Clock
      Serial.println(F("[DISPLAY] Switching to display mode: DATE (from Clock)"));
    } else if (weatherAvailable && (
          (!useHomeAssistant && strlen(openWeatherApiKey) == 32 && strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0)
          ||
          (useHomeAssistant && strlen(homeAssistantApiKey) > 32 && strlen(haTempSensor) > 0) // Or HomeAssistant setup 
        )
      ) {
      displayMode = 1;
      Serial.println(F("[DISPLAY] Switching to display mode: WEATHER (from Clock)"));
    } else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && countdownTargetTimestamp > time(nullptr)) {
      displayMode = 3;
      Serial.println(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Clock, weather skipped)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;  // Clock -> Nightscout (if weather & countdown are skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Clock, weather & countdown skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Staying in CLOCK (from Clock)"));
    }
  } else if (displayMode == 5) {  // Date mode
    if (weatherAvailable && (
          (!useHomeAssistant && strlen(openWeatherApiKey) == 32 && strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0)
          ||
          (useHomeAssistant && strlen(homeAssistantApiKey) > 32 && strlen(haTempSensor) > 0) // Or HomeAssistant setup 
        )
      ) {
      displayMode = 1;
      Serial.println(F("[DISPLAY] Switching to display mode: WEATHER (from Date)"));
    } else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && countdownTargetTimestamp > time(nullptr)) {
      displayMode = 3;
      Serial.println(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Date, weather skipped)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Date, weather & countdown skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Date)"));
    }
  } else if (displayMode == 1) {  // Weather
    if (showWeatherDescription && weatherAvailable && weatherDescription.length() > 0) {
      displayMode = 2;
      Serial.println(F("[DISPLAY] Switching to display mode: DESCRIPTION (from Weather)"));
    } else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && countdownTargetTimestamp > time(nullptr)) {
      displayMode = 3;
      Serial.println(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Weather)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;  // Weather -> Nightscout (if description & countdown are skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Weather, description & countdown skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Weather)"));
    }
  } else if (displayMode == 2) {  // Weather Description
    if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && countdownTargetTimestamp > time(nullptr)) {
      displayMode = 3;
      Serial.println(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Description)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;  // Description -> Nightscout (if countdown is skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Description, countdown skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Description)"));
    }
  } else if (displayMode == 3) {  // Countdown -> Nightscout
    if (nightscoutConfigured) {
      displayMode = 4;
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Countdown)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Countdown)"));
    }
  } else if (displayMode == 4) {  // Nightscout -> Custom Message
    displayMode = 6;
    Serial.println(F("[DISPLAY] Switching to display mode: CUSTOM MESSAGE (from Nightscout)"));
  } else if (displayMode == 6) {  // Custom Message -> Clock
    displayMode = 0;
    Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Custom Message)"));
  }

  // --- Common cleanup/reset logic remains the same ---
  if ((displayMode == 0) && strlen(customMessage) > 0 && oldMode != 6) {
    displayMode = 6;
    Serial.println(F("[DISPLAY] Custom Message display before returning to CLOCK"));
  }
  lastSwitch = millis();
}

void advanceDisplayModeSafe() {
  int attempts = 0;
  const int MAX_ATTEMPTS = 7;  // Number of possible modes + 1
  int startMode = displayMode;
  bool valid = false;
  do {
    advanceDisplayMode();  // One step advance
    attempts++;
    // Recalculate validity for the new mode
    valid = false;
    String ntpField = String(ntpServer2);
    bool nightscoutConfigured = ntpField.startsWith("https://");

    if (displayMode == 0) valid = true;  // Clock always valid
    else if (displayMode == 5 && showDate) valid = true;
    else if (displayMode == 1 && weatherAvailable && (
        (!useHomeAssistant && strlen(openWeatherApiKey) == 32 && strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0)
        ||
        (useHomeAssistant && strlen(homeAssistantApiKey) > 32 && strlen(haTempSensor) > 0)
      )) valid = true;
    else if (displayMode == 2 && showWeatherDescription && weatherAvailable && weatherDescription.length() > 0) valid = true;
    else if (displayMode == 3 && countdownEnabled && !countdownFinished && ntpSyncSuccessful) valid = true;
    else if (displayMode == 4 && nightscoutConfigured) valid = true;
    else if (displayMode == 6 && strlen(customMessage) > 0) valid = true;

    // If we've looped back to where we started, break to avoid infinite loop
    if (displayMode == startMode) break;

    if (valid) break;
  } while (attempts < MAX_ATTEMPTS);

  // If no valid mode found, fall back to Clock
  if (!valid) {
    displayMode = 0;
    Serial.println(F("[DISPLAY] Safe fallback to CLOCK"));
  }
  lastSwitch = millis();
}

bool isNumber(const char *str) {
  for (int i = 0; str[i]; i++) {
    if (!isdigit(str[i]) && str[i] != '.' && str[i] != '-') return false;
  }
  return true;
}

bool isFiveDigitZip(const char *str) {
  if (strlen(str) != 5) return false;
  for (int i = 0; i < 5; i++) {
    if (!isdigit(str[i])) return false;
  }
  return true;
}

String formatUptime(unsigned long seconds) {
  unsigned long days = seconds / 86400;
  unsigned long hours = (seconds % 86400) / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;

  char buf[64];
  if (days > 0)
    sprintf(buf, "%lud %02lu:%02lu:%02lu", days, hours, minutes, secs);
  else
    sprintf(buf, "%02lu:%02lu:%02lu", hours, minutes, secs);
  return String(buf);
}

void audio_info(const char *info) {
    Serial.print("info        "); Serial.println(info);
}

void audio_eof_mp3(const char *info) {
  Serial.print("EOF (End of File): ");
  Serial.println(info);
  
  isAlarmPlaying = false; // Turn off the switch
  // audio.stopSong(); // Optional: ensures buffers are flushed
}

unsigned long getTotalRuntimeSeconds() {
  return totalUptimeSeconds + (millis() - bootMillis) / 1000;
}

String formatTotalRuntime() {
  unsigned long secs = getTotalRuntimeSeconds();
  unsigned int h = secs / 3600;
  unsigned int m = (secs % 3600) / 60;
  unsigned int s = secs % 60;
  char buf[16];
  sprintf(buf, "%02u:%02u:%02u", h, m, s);
  return String(buf);
}

String getHAJSON(String entityID, JsonDocument &doc) {
  // Debug: Show what we are sending
  Serial.print(F("[HOME ASSISTANT] Entity: "));
  Serial.println(entityID);

  Serial.print(F("[HOME ASSISTANT] URL: "));  // Use F() with Serial.print
  String url = buildHomeAssistantURL(entityID);
  Serial.println(url);
  
  WiFiClientSecure client;  // use secure client for HTTPS
  client.stop();            // ensure previous session closed
  yield();                  // Allow OS to process socket closure
  client.setInsecure();     // no cert validation
  HTTPClient http;          // Create an HTTPClient object
  http.begin(client, url);  // Pass the WiFiClient object and the URL
  http.setTimeout(10000);   // Sets both connection and stream timeout to 10 seconds
  http.addHeader("content-type", "application/json"); // Ensures that the content returned is in json format 
  http.addHeader("User-Agent", "ESPTimeCast"); // Sets the User-Agent to ESPTimeCast
  http.addHeader("Authorization", String("Bearer ") + homeAssistantApiKey); // Add the API key to the header
  Serial.println(F("[HOME ASSISTANT] Sending GET request..."));
  int httpCode = http.GET();  // Send the GET request
  if (httpCode != HTTP_CODE_OK){
    Serial.printf("[HOME ASSISTANT] HTTP GET failed, error code: %d, reason: %s\n",
                  httpCode, http.errorToString(httpCode).c_str());
    Serial.println(F("[HOME ASSISTANT] Home Assistant requested, but not configured"));
    Serial.println(F("[HOME ASSISTANT] Setting Use Home Assistant flag to off"));
    return "Error: No Entity"; // Return text error
  }
  Serial.println(F("[HOME ASSISTANT] HTTP 200 OK. Reading payload..."));

  String payload = http.getString();
  http.end();
  Serial.println(F("[HOME ASSISTANT] Response received."));
  Serial.print(F("[HOME ASSISTANT] Payload: "));  // Use F() with Serial.print
  Serial.println(payload);
  doc.clear();
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print(F("[HOME ASSISTANT] JSON parse error: "));
    Serial.println(error.f_str());
    return "Error: Parsing Error"; // Return text error
  }
  return "200";
}

String getHAEntityState(String entityID) {
  
  JsonDocument doc;
  String status = getHAJSON(entityID, doc);
  if (status == "200") {
    if (doc["state"]) {
      String currentState = doc["state"].as<String>();
      Serial.printf("[HOME ASSISTANT] Temp: %s\n", currentState.c_str());
      return currentState; // Return text error
    }
  }
  Serial.println(F("[HOME ASSISTANT] No State Returned"));
  return "Error: No State"; 
}

String buildWeatherURL() {
  String base = "https://api.openweathermap.org/data/2.5/weather?";

  float lat = atof(openWeatherCity);
  float lon = atof(openWeatherCountry);

  bool latValid = isNumber(openWeatherCity) && isNumber(openWeatherCountry) && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;

  // Create encoded copies
  String cityEncoded = String(openWeatherCity);
  String countryEncoded = String(openWeatherCountry);
  cityEncoded.replace(" ", "%20");
  countryEncoded.replace(" ", "%20");

  if (latValid) {
    base += "lat=" + String(lat, 8) + "&lon=" + String(lon, 8);
  } else if (isFiveDigitZip(openWeatherCity) && String(openWeatherCountry).equalsIgnoreCase("US")) {
    base += "zip=" + String(openWeatherCity) + "," + String(openWeatherCountry);
  } else {
    base += "q=" + cityEncoded + "," + countryEncoded;
  }

  base += "&appid=" + String(openWeatherApiKey);
  base += "&units=" + String(weatherUnits);

  String langForAPI = String(language);
  if (langForAPI == "eo" || langForAPI == "ga" || langForAPI == "sw" || langForAPI == "ja") {
    langForAPI = "en";
  }
  base += "&lang=" + langForAPI;

  return base;
}

String buildHomeAssistantURL(String entityName) {
  // 1. Check if the URL is actually empty (looking at the first character)
  if (homeAssistantURL[0] == '\0') {
    return ""; // Return empty string if no URL is set
  }

  String url = String(homeAssistantURL);

  // 2. Remove ANY trailing slashes (e.g., "http://hass.local///")
  while (url.endsWith("/")) {
    url.remove(url.length() - 1);
  }

  // 3. Build the rest
  url += "/api/states/";
  url += entityName;
  
  return url;
}

const char *getSafeSsid() {
  if (isAPMode && strlen(ssid) == 0) {
    return "";
  } else {
    return isAPMode ? "********" : ssid;
  }
}

const char *getSafePassword() {
  if (strlen(password) == 0) {  // No password set yet — return empty string for fresh install
    return "";
  } else {  // Password exists — mask it in the web UI
    return "********";
  }
}

const char *getSafeApiKey() {
  if (strlen(openWeatherApiKey) == 0) {
    return "";
  } else {
    return "********************************";  // Always masked, even in AP mode
  }
}

const char *getSafeHAApiKey() {
  if (strlen(homeAssistantApiKey) == 0) {
    return "";
  } else {
    return "********************************";  // Always masked, even in AP mode
  }
}

void ensureHtmlFileExists() {
  Serial.println(F("[FS] Checking for /index.html on LittleFS..."));

  // Length of embedded HTML in PROGMEM
  size_t expectedSize = strlen_P(index_html);

  // If the file exists, verify size before deciding to trust it
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");

    if (!f) {
      Serial.println(F("[FS] ERROR: /index.html exists but failed to open! Will rewrite."));
    } else {
      size_t actualSize = f.size();
      f.close();

      if (actualSize == expectedSize) {
        Serial.printf("[FS] /index.html found (size OK: %u bytes). Using file system version.\n", actualSize);
        return;  // STOP HERE — file is good
      }

      Serial.printf(
        "[FS] /index.html size mismatch! Expected %u bytes, found %u. Rewriting...\n",
        expectedSize, actualSize);
    }
  } else {
    Serial.println(F("[FS] /index.html NOT found. Writing embedded content to LittleFS..."));
  }

  // -------------------------------
  // Write embedded HTML to LittleFS
  // -------------------------------

  File f = LittleFS.open("/index.html", "w");
  if (!f) {
    Serial.println(F("[FS] ERROR: Failed to create /index.html for writing!"));
    return;
  }

  size_t htmlLength = expectedSize;
  size_t bytesWritten = 0;

  for (size_t i = 0; i < htmlLength; i++) {
    char c = pgm_read_byte_near(index_html + i);

    if (f.write((uint8_t *)&c, 1) == 1) {
      bytesWritten++;
    } else {
      Serial.printf("[FS] Write failure at character %u. Aborting write.\n", i);
      f.close();
      return;
    }
  }

  f.close();

  if (bytesWritten == htmlLength) {
    Serial.printf("[FS] Successfully wrote %u bytes to /index.html.\n", bytesWritten);
  } else {
    Serial.printf("[FS] WARNING: Only wrote %u of %u bytes to /index.html (might be incomplete).\n",
                  bytesWritten, htmlLength);
  }
}
