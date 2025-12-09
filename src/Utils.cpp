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


// Returns formatted uptime (for web UI or logs)
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

