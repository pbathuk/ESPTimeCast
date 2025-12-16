#include "ButtonManager.h"
#include "ConfigManager.h"
#include "Globals.h" // Global variables and constants
#include "NetworkManager.h"
#include "Utils.h" // Utility functions
#include "WeatherManager.h"
#include "WebHandler.h"
#include "mfactoryfont.h" // Custom font
#include <Arduino.h>
#include <ArduinoOTA.h>

void setup() {

  // -----------------------------------------------------------------------------
  // Main setup() and loop()
  // -----------------------------------------------------------------------------
  /*
  DisplayMode key:
    0: Clock
    1: Weather
    2: Weather Description
    3: Countdown
    4: Blank Screen
    5: Date
    6: Custom Message
  */
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println(F("[SETUP] Starting setup..."));

  if (!LittleFS.begin(true)) {
    Serial.println(F("[ERROR] LittleFS mount failed in setup! Halting."));
    while (true) {
      delay(1000);
      yield();
    }
  }
  Serial.println(F("[SETUP] LittleFS file system mounted successfully."));
  loadUptime();
  ensureHtmlFileExists();
  P.begin(); // Initialize Parola library

  P.setCharSpacing(0);
  P.setFont(mFactory);
  loadConfig(); // This function now has internal yields and prints

  P.setIntensity(brightness);
  P.setZoneEffect(0, flipDisplay, PA_FLIP_UD);
  P.setZoneEffect(0, flipDisplay, PA_FLIP_LR);

  Serial.println(F("[SETUP] Parola (LED Matrix) initialized"));

  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    const char *name = nullptr;
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      name = "GOT_IP";
      lastWifiConnectTime = millis();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      name = "DISCONNECTED";
      break;
    default:
      return; // ignore all other events
    }
    Serial.printf("[WIFI EVENT] %s (%d)\n", name, event);
  });

  connectWiFi();
  setupMDNS();
  setupWebServer();
  printConfigToSerial();
  setupTime();

  displayMode = 0;
  lastSwitch = millis() - (clockDuration - 500);
  lastColonBlink = millis();
  bootMillis = millis();
  saveUptime();

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(6); // default 0...21

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(hostName);

  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
        // using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });

  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  static bool colonVisible = true;
  const unsigned long colonBlinkInterval = 800;
  if (millis() - lastColonBlink > colonBlinkInterval) {
    colonVisible = !colonVisible;
    lastColonBlink = millis();
  }

  static unsigned long ntpAnimTimer = 0;
  static int ntpAnimFrame = 0;
  static bool tzSetAfterSync = false;

  static unsigned long lastFetch = 0;
  const unsigned long fetchInterval = 300000; // 5 minutes

  // AP Mode animation
  static unsigned long apAnimTimer = 0;
  static int apAnimFrame = 0;
  if (isAPMode) {
    unsigned long runTimems = millis();
    if (runTimems - apAnimTimer > 750) {
      apAnimTimer = runTimems;
      apAnimFrame++;
    }
    P.setTextAlignment(PA_CENTER);
    switch (apAnimFrame % 3) {
    case 0:
      P.print(F("AP ©"));
      break;
    case 1:
      P.print(F("AP ª"));
      break;
    case 2:
      P.print(F("AP «"));
      break;
    }
    return;
  }

  // -----------------------------
  // TIME ZONE & LOCAL TIME CALCULATION
  // -----------------------------
  // Check if clock is initialized first
  if (!sntpClock) {
    // Clock not ready yet
    return;
  }
  // This is FAST - just reads cached time from ESP32's SNTP client
  // No network calls!
  acetime_t nowSeconds = sntpClock->getNow();

  if (nowSeconds <= -946080000 || nowSeconds >= 946080000) {
    ntpSyncSuccessful = false;
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 5000) {
      Serial.print("[TIME] Waiting for NTP sync, current value: ");
      Serial.println(nowSeconds);
      lastLog = millis();
    }
    return;
  }

  if (!ntpSyncSuccessful) {
    ntpSyncSuccessful = true;
    Serial.println("[TIME] Valid time acquired");
  }

  // Only process time-dependent code if we have valid time
  auto localTime = ZonedDateTime::forEpochSeconds(nowSeconds, localTz);
  // -----------------------------
  // Dimming (auto + manual)
  // -----------------------------
  int curHour = localTime.hour();
  int curMinute = localTime.minute();
  int curTotal = curHour * 60 + curMinute;

  // -----------------------------
  // Determine dimming start/end
  // -----------------------------
  int startTotal, endTotal;
  bool dimActive = false;

  if (autoDimmingEnabled) {
    startTotal = sunsetHour * 60 + sunsetMinute;
    endTotal = sunriseHour * 60 + sunriseMinute;
  } else if (dimmingEnabled) {
    startTotal = dimStartHour * 60 + dimStartMinute;
    endTotal = dimEndHour * 60 + dimEndMinute;
  } else {
    startTotal = endTotal = -1; // not used
  }

  // -----------------------------
  // Check if dimming should be active
  // -----------------------------
  if (autoDimmingEnabled || dimmingEnabled) {
    if (startTotal < endTotal) {
      dimActive = (curTotal >= startTotal && curTotal < endTotal);
    } else {
      dimActive = (curTotal >= startTotal || curTotal < endTotal); // overnight
    }
  }

  // -----------------------------
  // Apply brightness / display on-off
  // -----------------------------
  static bool lastDimActive = false; // remembers last state
  int targetBrightness = dimActive ? dimBrightness : brightness;

  // Log only when transitioning
  if (dimActive != lastDimActive) {
    if (dimActive) {
      if (autoDimmingEnabled)
        Serial.printf("[DISPLAY] Automatic dimming setting brightness to %d\n",
                      targetBrightness);
      else if (dimmingEnabled)
        Serial.printf("[DISPLAY] Custom dimming setting brightness to %d\n",
                      targetBrightness);
    } else {
      Serial.println(F("[DISPLAY] Waking display (dimming end)"));
    }
    lastDimActive = dimActive;
  }

  // Apply brightness or shutdown
  if (targetBrightness == -1) {
    if (!displayOff) {
      Serial.println(F("[DISPLAY] Turning display OFF (dimming -1)"));
      P.displayShutdown(true);
      P.displayClear();
      displayOff = true;
      displayOffByDimming = dimActive;
      displayOffByBrightness = !dimActive;
    }
  } else {
    if (displayOff && ((dimActive && displayOffByBrightness) ||
                       (!dimActive && displayOffByDimming))) {
      P.displayShutdown(false);
      displayOff = false;
      displayOffByDimming = false;
      displayOffByBrightness = false;
    }
    P.setIntensity(targetBrightness);
  }

  // --- IMMEDIATE COUNTDOWN FINISH TRIGGER ---
  if (countdownEnabled && !countdownFinished && ntpSyncSuccessful &&
      countdownTargetTimestamp > 0 &&
      localTime.toUnixSeconds64() >= countdownTargetTimestamp) {
    countdownFinished = true;
    displayMode = 3; // Let main loop handle animation + TIMES UP
    countdownShowFinishedMessage = true;
    hourglassPlayed = false;
    countdownFinishedMessageStartTime = millis();
    Serial.println("[SYSTEM] Countdown target reached! Switching to Mode 3 to "
                   "display finish sequence.");
  }

  // --- IP Display ---
  if (showingIp) {
    if (P.displayAnimate()) {
      ipDisplayCount++;
      if (ipDisplayCount < ipDisplayMax) {
        textEffect_t actualScrollDirection =
            getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(pendingIpToShow.c_str(), PA_CENTER,
                        actualScrollDirection, 120);
      } else {
        showingIp = false;
        P.displayClear();
        displayMode = 0;
        lastSwitch = millis();
      }
    }
    return; // Exit loop early if showing IP
  }

  // --- BRIGHTNESS/OFF CHECK ---
  if (brightness == -1) {
    if (!displayOff) {
      Serial.println(F("[DISPLAY] Turning display OFF"));
      P.displayShutdown(true); // fully off
      P.displayClear();
      displayOff = true;
    }
  }

  // 1. Audio Handling (Highest Priority)
  if (isAlarmPlaying) {
    audio.loop();
    // NO DELAYS HERE!
  }

  // Only advance mode by timer for clock/weather, not description!
  unsigned long displayDuration =
      (displayMode == 0) ? clockDuration : weatherDuration;
  if ((displayMode == 0 || displayMode == 1) &&
      millis() - lastSwitch > displayDuration) {
    advanceDisplayMode();
  }

  // --- MODIFIED WEATHER FETCHING LOGIC ---
  if (WiFi.status() == WL_CONNECTED) {
    if (!weatherFetchInitiated || shouldFetchWeatherNow ||
        (millis() - lastFetch > fetchInterval)) {
      if (shouldFetchWeatherNow) {
        Serial.println(
            F("[LOOP] Immediate weather fetch requested by web server."));
        shouldFetchWeatherNow = false;
      } else if (!weatherFetchInitiated) {
        Serial.println(F("[LOOP] Initial weather fetch."));
      } else {
        Serial.println(F("[LOOP] Regular interval weather fetch."));
      }
      weatherFetchInitiated = true;
      weatherFetched = false;
      fetchWeather();
      lastFetch = millis();
    }
  } else {
    weatherFetchInitiated = false;
    shouldFetchWeatherNow = false;
  }

  // build base HH:MM first ---
  char baseTime[9];
  if (twelveHourToggle) {
    int hour12 = localTime.hour() % 12;
    if (hour12 == 0)
      hour12 = 12;
    if (amPMShow) {
      // Determine AM or PM based on the raw 24-hour value
      const char *ampm = (localTime.hour() < 12) ? " \x80\x82" : " \x81\x82";
      sprintf(baseTime, "%d:%02d%s", hour12, localTime.minute(), ampm);
    } else {
      sprintf(baseTime, "%d:%02d", hour12, localTime.minute());
    }
  } else {
    sprintf(baseTime, "%02d:%02d", localTime.hour(), localTime.minute());
  }

  // add seconds only if colon blink enabled AND weekday hidden ---
  char timeWithSeconds[12];
  if (!showDayOfWeek && colonBlinkEnabled) {
    // Remove any leading space from baseTime
    const char *trimmedBase = baseTime;
    if (baseTime[0] == ' ')
      trimmedBase++; // skip leading space
    sprintf(timeWithSeconds, "%s:%02d", trimmedBase, localTime.second());
  } else {
    strcpy(timeWithSeconds, baseTime); // no seconds
  }

  // keep spacing logic the same ---
  char timeSpacedStr[24];
  int j = 0;
  for (int i = 0; timeWithSeconds[i] != '\0'; i++) {
    timeSpacedStr[j++] = timeWithSeconds[i];
    if (timeWithSeconds[i + 1] != '\0') {
      timeSpacedStr[j++] = ' ';
    }
  }
  timeSpacedStr[j] = '\0';

  String daySymbol = DateStrings().dayOfWeekShortString(localTime.dayOfWeek());
  daySymbol.toLowerCase();

  // build final string ---
  String formattedTime;
  if (showDayOfWeek) {
    formattedTime = String(daySymbol) + "   " + String(timeSpacedStr);
  } else {
    formattedTime = String(timeSpacedStr);
  }

  unsigned long currentDisplayDuration = 0;
  if (displayMode == 0) {
    currentDisplayDuration = clockDuration;
  } else if (displayMode == 1) { // Weather
    currentDisplayDuration = weatherDuration;
  }

  // Only advance mode by timer for clock/weather static (Mode 0 & 1).
  // Other modes (2, 3) have their own internal timers/conditions for
  // advancement.
  if ((displayMode == 0 || displayMode == 1) &&
      (millis() - lastSwitch > currentDisplayDuration)) {
    advanceDisplayMode();
  }

  // --- CLOCK Display Mode ---
  if (displayMode == 0) {
    P.setCharSpacing(0);

    // --- NTP SYNC ---
    if (!ntpSyncSuccessful) {
      P.setTextAlignment(PA_CENTER);
      static unsigned long errorAltTimer = 0;
      static bool showNtpError = true;

      if (!ntpSyncSuccessful && !weatherAvailable) {
        if (millis() - errorAltTimer > 2000) {
          errorAltTimer = millis();
          showNtpError = !showNtpError;
        }
        P.print(showNtpError ? F("(<") : F("(*"));
      } else if (!ntpSyncSuccessful) {
        P.print(F("(<"));
      } else if (!weatherAvailable) {
        P.print(F("(*"));
      }
    }
    // --- DISPLAY CLOCK ---
    else {
      String timeString = formattedTime;
      if (showDayOfWeek && colonBlinkEnabled && !colonVisible) {
        timeString.replace(":", " ");
      }

      // --- SCROLL IN ONLY WHEN COMING FROM SPECIFIC MODES OR FIRST BOOT ---
      bool shouldScrollIn = false;
      if (prevDisplayMode == -1 || prevDisplayMode == 3 ||
          prevDisplayMode == 4) {
        shouldScrollIn = true; // first boot or other special modes
      } else if (prevDisplayMode == 2 && weatherDescription.length() > 8) {
        shouldScrollIn = true; // only scroll in if weather was scrolling
      } else if (prevDisplayMode == 6) {
        shouldScrollIn = true; // scroll in when coming from custom message
      }
      if (!P.displayAnimate()) {
        return; // wait for scroll to finish
      }
      scrollCount++;
      if (shouldScrollIn && !clockScrollDone) {
        if (scrollCount < scrollDisplayMax) {
          textEffect_t inDir =
              getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);

          P.displayText(timeString.c_str(), PA_CENTER, GENERAL_SCROLL_SPEED, 0,
                        inDir, PA_NO_EFFECT);
          return;
        } else {
          // Nothing being displayed, but this is at the end of the scrolling
          clockScrollDone = true; // mark scroll done
          scrollCount = 0;        // reset scroll count
          P.setTextAlignment(PA_CENTER);
          P.print(timeString);
        }
      } else {
        P.setTextAlignment(PA_CENTER);
        P.print(timeString);
      }
    }
  } else {
    // --- leaving clock mode ---
    if (prevDisplayMode == 0) {
      clockScrollDone = false; // reset for next time we enter clock
    }
  }

  // --- WEATHER Display Mode ---
  static bool weatherWasAvailable = false;
  if (displayMode == 1) {
    P.setCharSpacing(1);
    if (weatherAvailable) {
      String weatherDisplay;
      if (showHumidity && currentHumidity != -1) {
        int cappedHumidity = (currentHumidity > 99) ? 99 : currentHumidity;
        weatherDisplay = currentTemp + " " + String(cappedHumidity) + "%";
      } else {
        weatherDisplay = currentTemp + tempSymbol;
      }
      P.print(weatherDisplay.c_str());
      weatherWasAvailable = true;
    } else {
      if (weatherWasAvailable) {
        Serial.println(F("[DISPLAY] Weather not available, showing clock..."));
        weatherWasAvailable = false;
      }
      if (ntpSyncSuccessful) {
        String timeString = formattedTime;
        if (!colonVisible)
          timeString.replace(":", " ");
        P.setCharSpacing(0);
        P.print(timeString);
      } else {
        P.setCharSpacing(0);
        P.setTextAlignment(PA_CENTER);
        P.print(F("(*"));
      }
    }
    return;
  }

  // --- WEATHER DESCRIPTION Display Mode ---
  if (displayMode == 2 && showWeatherDescription && weatherAvailable &&
      weatherDescription.length() > 0) {
    String desc = weatherDescription;

    // --- Check if humidity is actually visible ---
    bool humidityVisible =
        showHumidity && weatherAvailable &&
        ((!useHomeAssistant && strlen(openWeatherApiKey) == 32 &&
          strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0) ||
         (useHomeAssistant && strlen(homeAssistantApiKey) > 32 &&
          strlen(haTempSensor) > 0));

    // --- Conditional padding ---
    bool addPadding = false;
    if (prevDisplayMode == 1 && humidityVisible) {
      addPadding = true;
    }
    if (addPadding) {
      desc = "    " + desc; // 4-space padding before scrolling
    }

    // prepare safe buffer
    static char descBuffer[128]; // large enough for OWM translations
    desc.toCharArray(descBuffer, sizeof(descBuffer));

    if (desc.length() > 8) {
      if (!descScrolling) {
        textEffect_t actualScrollDirection =
            getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(descBuffer, PA_CENTER, actualScrollDirection,
                        GENERAL_SCROLL_SPEED);
        descScrolling = true;
        descScrollEndTime = 0; // reset end time at start
      }
      if (P.displayAnimate()) {
        if (descScrollEndTime == 0) {
          descScrollEndTime = millis(); // mark the time when scroll finishes
        }
        // wait small pause after scroll stops
        if (millis() - descScrollEndTime > descriptionScrollPause) {
          descScrolling = false;
          descScrollEndTime = 0;
          advanceDisplayMode();
        }
      } else {
        descScrollEndTime = 0; // reset if not finished
      }
      return;
    } else {
      if (descStartTime == 0) {
        P.setTextAlignment(PA_CENTER);
        P.setCharSpacing(1);
        P.print(descBuffer);
        descStartTime = millis();
      }
      if (millis() - descStartTime > descriptionDuration) {
        descStartTime = 0;
        advanceDisplayMode();
      }
      return;
    }
  }

  // --- Countdown Display Mode ---
  if (displayMode == 3 && countdownEnabled && ntpSyncSuccessful) {
    static int countdownSegment = 0;
    static unsigned long segmentStartTime = 0;
    const unsigned long SEGMENT_DISPLAY_DURATION =
        1500; // 1.5 seconds for each static segment

    long timeRemaining = countdownTargetTimestamp - localTime.toUnixSeconds64();

    // --- Countdown Finished Logic ---
    // This part of the code remains unchanged.
    if (timeRemaining <= 0 || countdownShowFinishedMessage) {
      // NEW: Only show "TIMES UP" if countdown target timestamp is valid and
      // expired
      auto localTime = ZonedDateTime::forEpochSeconds(nowSeconds, localTz);
      if (countdownTargetTimestamp == 0 ||
          countdownTargetTimestamp > localTime.toUnixSeconds64()) {
        // Target invalid or in the future, don't show "TIMES UP" yet, advance
        // display instead
        countdownShowFinishedMessage = false;
        countdownFinished = false;
        countdownFinishedMessageStartTime = 0;
        hourglassPlayed = false; // Reset if we decide not to show it
        Serial.println("[COUNTDOWN-FINISH] Countdown target invalid or not "
                       "reached yet, skipping 'TIMES UP'. Advancing display.");
        // Disable the countdown after finishing
        // Final cleanup (persisted)
        countdownEnabled = false;
        countdownTargetTimestamp = 0;
        countdownLabel[0] = '\0';
        saveCountdownConfig(false, 0, "");
        advanceDisplayMode();
        return;
      }

      // Define these static variables here if they are not global (or already
      // defined in your loop())
      static const char *flashFrames[] = {"{|", "}~"};
      static unsigned long lastFlashingSwitch = 0;
      static int flashingMessageFrame = 0;

      // --- Initial Combined Sequence: Play Hourglass THEN start Flashing ---
      // This 'if' runs ONLY ONCE when the "finished" sequence begins.
      if (!hourglassPlayed) {     // <-- This is the single entry point for the
                                  // combined sequence
        countdownFinished = true; // Mark as finished overall
        countdownShowFinishedMessage =
            true; // Confirm we are in the finished sequence
        countdownFinishedMessageStartTime =
            millis(); // Start the 15-second timer for the flashing duration

        // 1. Play Hourglass Animation (Blocking)
        const char *hourglassFrames[] = {"¡", "¢", "£", "¤"};
        if (hourGlassStartMillis == 0) {
          hourGlassStartMillis = millis(); // Initialize if not set
        }
        if (hourGlassFlipCount < hourGlassRepeats) {
          if (millis() - hourGlassStartMillis >= hourGlassFlipInterval) {
            hourGlassFlipCount++;
            int i = hourGlassFlipCount % 4;
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(0);
            P.print(hourglassFrames[i]);
            hourGlassStartMillis =
                millis(); // Record start time of hourglass frame
            if (hourGlassFlipCount >= hourGlassRepeats) {
              Serial.println("[COUNTDOWN-FINISH] Played hourglass animation.");
              P.displayClear(); // Clear display after hourglass animation
            }
          }
          return; // Stay in hourglass animation until done
        }
        // 2. Initialize Flashing "TIMES UP" for its very first frame
        flashingMessageFrame = 0;
        lastFlashingSwitch = millis(); // Set initial time for first flash frame
        P.setTextAlignment(PA_CENTER);
        P.setCharSpacing(0);
        P.print(flashFrames[flashingMessageFrame]); // Display the first frame
                                                    // immediately
        flashingMessageFrame =
            (flashingMessageFrame + 1) % 2; // Prepare for the next frame

        hourglassPlayed =
            true; // <-- Mark that this initial combined sequence has completed!
        countdownSegment =
            0; // Reset segment counter after finished sequence initiation
        segmentStartTime =
            0; // Reset segment timer after finished sequence initiation
        hourGlassFlipCount = 0; // Reset hourglass flip count for next time
      }

      // --- Continue Flashing "TIMES UP" for its duration (after initial
      // combined sequence) --- This part runs in subsequent loop iterations
      // after the hourglass has played.
      if (millis() - countdownFinishedMessageStartTime <
          15000) { // Flashing duration
        if (millis() - lastFlashingSwitch >=
            500) { // Check for flashing interval
          lastFlashingSwitch = millis();
          P.displayClear();
          P.setTextAlignment(PA_CENTER);
          P.setCharSpacing(0);
          P.print(flashFrames[flashingMessageFrame]);
          flashingMessageFrame = (flashingMessageFrame + 1) % 2;
        }
        P.displayAnimate(); // Ensure display updates
        return;             // Stay in this mode until the 15 seconds are over
      } else {
        // 15 seconds are over, clean up and advance
        Serial.println(
            "[COUNTDOWN-FINISH] Flashing duration over. Advancing to Clock.");
        countdownShowFinishedMessage = false;
        countdownFinishedMessageStartTime = 0;
        hourglassPlayed =
            false; // <-- RESET this flag for the next countdown cycle!

        // Final cleanup (persisted)
        countdownEnabled = false;
        countdownTargetTimestamp = 0;
        countdownLabel[0] = '\0';
        saveCountdownConfig(false, 0, "");

        P.setInvert(false);
        advanceDisplayMode();
        return; // Exit loop after processing
      }
    } // END of 'if (timeRemaining <= 0 || countdownShowFinishedMessage)'

    // --- NORMAL COUNTDOWN LOGIC ---
    // This 'else' block will only run if `timeRemaining > 0` and
    // `!countdownShowFinishedMessage`
    else {

      // The new variable `isDramaticCountdown` toggles between the two modes
      if (isDramaticCountdown) {
        // --- EXISTING DRAMATIC COUNTDOWN LOGIC ---
        long days = timeRemaining / (24 * 3600);
        long hours = (timeRemaining % (24 * 3600)) / 3600;
        long minutes = (timeRemaining % 3600) / 60;
        long seconds = timeRemaining % 60;
        String currentSegmentText = "";

        if (segmentStartTime == 0 ||
            (millis() - segmentStartTime > SEGMENT_DISPLAY_DURATION)) {
          segmentStartTime = millis();
          P.displayClear();

          switch (countdownSegment) {
          case 0: // Days
            if (days > 0) {
              currentSegmentText =
                  String(days) + " " + (days == 1 ? "DAY" : "DAYS");
              Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n",
                            countdownSegment, currentSegmentText.c_str());
              countdownSegment++;
            } else {
              // Skip days if zero
              countdownSegment++;
              segmentStartTime = 0;
            }
            break;
          case 1: { // Hours
            char buf[10];
            sprintf(buf, "%02ld HRS", hours); // pad hours with 0
            currentSegmentText = String(buf);
            Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n",
                          countdownSegment, currentSegmentText.c_str());
            countdownSegment++;
            break;
          }
          case 2: { // Minutes
            char buf[10];
            sprintf(buf, "%02ld MINS", minutes); // pad minutes with 0
            currentSegmentText = String(buf);
            Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n",
                          countdownSegment, currentSegmentText.c_str());
            countdownSegment++;
            break;
          }
          case 3: {
            // Seconds & Label Scroll
            if (!P.displayAnimate()) {
              return; // Wait until previous animation is done
            }
            char secondsBuf[10];
            if (segmentStartMillis == 0) {
              segmentStartMillis = millis(); // Initialize if not set
              long nowRemaining =
                  countdownTargetTimestamp - localTime.toUnixSeconds64();
              long currentSecond = nowRemaining % 60;
              sprintf(secondsBuf, "%02ld %s", currentSecond,
                      currentSecond == 1 ? "SEC" : "SECS");
              String secondsText = String(secondsBuf);
              Serial.printf("[COUNTDOWN-STATIC] Displaying segment 3: %s\n",
                            secondsText.c_str());
              P.displayClear();
              P.setTextAlignment(PA_CENTER);
              P.setCharSpacing(1);
              P.print(secondsText.c_str());
              return; // Wait for next loop to continue
            } else if (!dramaticLock && (millis() - segmentStartMillis >=
                                         (SEGMENT_DISPLAY_DURATION - 400))) {
              dramaticLock = true; // Lock to prevent re-entry
              unsigned long elapsed = millis() - segmentStartMillis;
              long adjustedSecond =
                  (countdownTargetTimestamp - localTime.toUnixSeconds64() -
                   (elapsed / 1000)) %
                  60;
              sprintf(secondsBuf, "%02ld %s", adjustedSecond,
                      adjustedSecond == 1 ? "SEC" : "SECS");
              String secondsText = String(secondsBuf);
              secondsText = String(secondsBuf);
              P.displayClear();
              P.setTextAlignment(PA_CENTER);
              P.setCharSpacing(1);
              P.print(secondsText.c_str());
              return; // Wait for next loop to continue
            } else if (millis() - segmentStartMillis >=
                       SEGMENT_DISPLAY_DURATION) {
              segmentStartMillis = 0; // Reset for next time
              dramaticLock = false;   // Reset lock for next time
            } else {
              return; // Wait until full duration is over
            }

            String label;
            if (strlen(countdownLabel) == 0) {
              static const char *fallbackLabels[] = {
                  "TO: PARTY TIME!",      "TO: SHOWTIME!",
                  "TO: CLOCKOUT!",        "TO: BLASTOFF!",
                  "TO: GO TIME!",         "TO: LIFTOFF!",
                  "TO: THE BIG REVEAL!",  "TO: ZERO HOUR!",
                  "TO: THE FINAL COUNT!", "TO: MISSION COMPLETE"};
              int randomIndex = random(0, 10);
              strncpy(countdownLabel, fallbackLabels[randomIndex],
                      sizeof(countdownLabel) - 1);
              countdownLabel[sizeof(countdownLabel) - 1] = '\0';
            }
            label = String(countdownLabel);
            label.trim();
            if (!label.startsWith("TO:") && !label.startsWith("to:")) {
              label = "TO: " + label;
            }
            label.replace('.', ',');

            P.setTextAlignment(PA_LEFT);
            P.setCharSpacing(1);
            textEffect_t actualScrollDirection =
                getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
            P.displayScroll(label.c_str(), PA_LEFT, actualScrollDirection,
                            GENERAL_SCROLL_SPEED);
            countdownSegment++;
            return;
          }
          case 4: // Exit countdown
            if (!P.displayAnimate()) {
              return; // Wait until previous animation is done
            }
            Serial.println("[COUNTDOWN-STATIC] All segments and label "
                           "displayed. Advancing to Clock.");
            countdownSegment = 0;
            segmentStartTime = 0;
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(1);
            advanceDisplayMode();
            return;

          default:
            Serial.println(
                "[COUNTDOWN-ERROR] Invalid countdownSegment, resetting.");
            countdownSegment = 0;
            segmentStartTime = 0;
            break;
          }

          if (currentSegmentText.length() > 0) {
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(1);
            P.print(currentSegmentText.c_str());
          }
        }
        P.displayAnimate();
      }

      // --- NEW: SINGLE-LINE COUNTDOWN LOGIC ---
      else {
        // Blocking loop to ensure the full message scrolls
        if (!P.displayAnimate()) {
          return; // Wait until previous animation is done
        }
        if (scrollCount < scrollDisplayMax) {
          scrollCount++;
          long days = timeRemaining / (24 * 3600);
          long hours = (timeRemaining % (24 * 3600)) / 3600;
          long minutes = (timeRemaining % 3600) / 60;
          long seconds = timeRemaining % 60;

          String label;
          if (strlen(countdownLabel) == 0) {
            static const char *fallbackLabels[] = {
                "TO: PARTY TIME!",     "TO: SHOWTIME!",  "TO: CLOCKOUT!",
                "TO: BLASTOFF!",       "TO: GO TIME!",   "TO: LIFTOFF!",
                "TO: THE BIG REVEAL!", "TO: ZERO HOUR!", "TO: THE FINAL COUNT!",
                "TO: MISSION COMPLETE"};
            int randomIndex = random(0, 10);
            strncpy(countdownLabel, fallbackLabels[randomIndex],
                    sizeof(countdownLabel) - 1);
            countdownLabel[sizeof(countdownLabel) - 1] = '\0';
          }
          label = String(countdownLabel);
          label.trim();

          // Replace standard digits 0–9 with your custom font character codes
          for (int i = 0; i < label.length(); i++) {
            if (isDigit(label[i])) {
              int num = label[i] - '0';          // 0–9
              label[i] = 145 + ((num + 9) % 10); // Maps 0→154, 1→145, ... 9→153
            }
          }

          // Format the full string
          char buf[50];
          // Only show days if there are any, otherwise start with hours
          if (days > 0) {
            sprintf(buf, "%s IN: %ldD %02ldH %02ldM %02ldS", label.c_str(),
                    days, hours, minutes, seconds);
          } else {
            sprintf(buf, "%s IN: %02ldH %02ldM %02ldS", label.c_str(), hours,
                    minutes, seconds);
          }

          String fullString = String(buf);
          // Question over this and the test here —is it needed?
          bool addPadding = false;
          bool humidityVisible =
              showHumidity && weatherAvailable &&
              ((!useHomeAssistant && strlen(openWeatherApiKey) == 32 &&
                strlen(openWeatherCity) > 0 &&
                strlen(openWeatherCountry) > 0) ||
               (useHomeAssistant && strlen(homeAssistantApiKey) > 32 &&
                strlen(haTempSensor) > 0));

          // Padding logic
          if (prevDisplayMode == 0 && (showDayOfWeek || colonBlinkEnabled)) {
            addPadding = true;
          } else if (prevDisplayMode == 1 && humidityVisible) {
            addPadding = true;
          }
          if (addPadding) {
            fullString = "    " + fullString; // 4 spaces
          }

          // Display the full string and scroll it
          P.setTextAlignment(PA_LEFT);
          P.setCharSpacing(1);
          textEffect_t actualScrollDirection =
              getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
          P.displayScroll(fullString.c_str(), PA_LEFT, actualScrollDirection,
                          GENERAL_SCROLL_SPEED);
          return;
        } else {
          scrollCount = 0; // reset for next time
          // After scrolling is complete, we're done with this display mode
          // Move to the next mode and exit the function.
          P.setTextAlignment(PA_CENTER);
          advanceDisplayMode();
          return;
        }
      }
    }

    // Keep alignment reset just in case
    P.setTextAlignment(PA_CENTER);
    P.setCharSpacing(1);
    return;
  } // End of if (displayMode == 3 && ...)
  if (displayMode == 4) {
    return; // skip drawing
  }
  // DATE Display Mode
  else if (displayMode == 5 && showDate) {

    // --- VALID DATE CHECK ---
    if ((localTime.year() < 2020)) {
      advanceDisplayMode();
      return; // skip drawing
    }
    // -------------------------

    String monthAbbr = DateStrings().monthShortString(localTime.month());

    monthAbbr.toLowerCase();

    // Add spaces between day digits
    String dayString = String(localTime.day());
    String spacedDay = "";
    for (size_t i = 0; i < dayString.length(); i++) {
      spacedDay += dayString[i];
      if (i < dayString.length() - 1)
        spacedDay += " ";
    }

    String dateString = spacedDay + "   " + monthAbbr;

    P.setTextAlignment(PA_CENTER);
    P.setCharSpacing(0);
    P.print(dateString);

    if (millis() - lastSwitch > weatherDuration) {
      advanceDisplayMode();
    }
  }

  // --- Custom Message Display Mode (displayMode == 6) ---
  if (displayMode == 6) {

    // 1. Initial Check: If message is empty, skip mode 6.
    if (strlen(customMessage) == 0) {
      advanceDisplayMode();
      return;
    }

    if (!P.displayAnimate()) {
      return; // wait for scroll to finish
    }

    // --- CHARACTER REPLACEMENT AND PADDING (Common to both short and long) ---
    const size_t MAX_NON_SCROLLING_CHARS = 8;
    String msg = String(customMessage);

    // Replace standard digits 0–9 with your custom font character codes
    for (int i = 0; i < msg.length(); i++) {
      if (isDigit(msg[i])) {
        int num = msg[i] - '0';
        msg[i] = 145 + ((num + 9) % 10);
      }
    }

    // --- CHECK FOR TIMEOUT (Applies to temporary short & long messages) ---
    bool timedOut = false;
    // Check if a time limit (messageDisplaySeconds > 0) has been exceeded
    if (messageDisplaySeconds > 0 &&
        (millis() - messageStartTime) >= (messageDisplaySeconds * 1000UL)) {
      Serial.printf("[MESSAGE] HA message timed out after %d seconds.\n",
                    messageDisplaySeconds);
      timedOut = true;
    }

    // --- CHECK FOR SCROLL/CYCLE LIMIT BEFORE DISPLAYING ---
    // Scrolls complete applies to long messages.
    bool scrollsComplete = ((messageScrollTimes > 0) &&
                            (currentScrollCount >= messageScrollTimes));

    // Cycles complete applies to short messages.
    // Use the dedicated short message counter
    bool cyclesComplete = ((messageScrollTimes > 0) &&
                           (currentDisplayCycleCount >= messageScrollTimes));

    // --- ADVANCE MODE CHECK (Check if HA parameters are complete) ---
    // If either timer or cycle/scroll count is finished, we clean up the
    // temporary message.
    if (scrollsComplete || cyclesComplete) {
      Serial.println(F("[MESSAGE] HA-controlled message finished."));

      // Reset common counters
      currentScrollCount = 0;
      messageStartTime = 0;
      currentDisplayCycleCount = 0; // Reset the cycle counter

      // CRITICAL LOGIC: RESTORE PERSISTENT MESSAGE (Exit Mode 6 Logic)
      if (strlen(lastPersistentMessage) > 0) {
        // A persistent message exists, restore it
        strncpy(customMessage, lastPersistentMessage, sizeof(customMessage));
        messageScrollSpeed = GENERAL_SCROLL_SPEED;
        messageDisplaySeconds = 0;
        messageScrollTimes = 0;
        Serial.printf(
            "[MESSAGE] Restored persistent message: '%s'. Staying in mode 6.\n",
            customMessage);
      } else {
        // No persistent message to restore. Clear the temporary HA message and
        // Exit mode 6.
        customMessage[0] = '\0';
        Serial.println(F("[MESSAGE] No persistent message to restore. "
                         "Advancing display mode."));
        advanceDisplayMode();
      }
      return;
    }
    // ----------------------------------------------------------------------
    // BRANCH A: NON-SCROLLING (Short Message: strlen <= 8)
    // ----------------------------------------------------------------------
    if (msg.length() <= MAX_NON_SCROLLING_CHARS) {
      if (millis() < customMessageEndTime && customMessageEndTime > 0) {
        // Still within the display duration, continue showing message
        return;
      }
      // Determine the duration: use HA seconds if set, otherwise use
      // weatherDuration.
      if (customerMessageDuration == 0) {
        customerMessageDuration = (messageDisplaySeconds > 0)
                                      ? (messageDisplaySeconds * 1000UL)
                                      : weatherDuration;
        customMessageEndTime = millis() + customerMessageDuration;
        // Block execution for the specified duration (non-HA uses
        // weatherDuration)
      }
      // If HA seconds is set, we use the timedOut check at the top.
      // If only scrollTimes is set, we still display for weatherDuration before
      // incrementing the cycle count.

      Serial.printf("[MESSAGE] Displaying timed short message: '%s' for %lu "
                    "ms. Advancing mode.\n",
                    customMessage, customerMessageDuration);

      P.setTextAlignment(PA_CENTER);
      P.setCharSpacing(1);
      P.print(msg.c_str());

      // --- CYCLE TRACKING FOR SCROLLTIMES ---
      // Increment the counter if the HA message is configured to clear by
      // scroll count.
      if (messageScrollTimes > 0) {
        currentDisplayCycleCount++;
        Serial.printf("[MESSAGE] Short message cycle complete. Count: %d/%d\n",
                      currentDisplayCycleCount, messageScrollTimes);
      }
      if (!scrollsComplete && messageScrollTimes > 0) {
        // If not yet complete by scrolls, reset the timer for display
        customMessageEndTime = millis() + customerMessageDuration;
      }
      if (millis() < customMessageEndTime && customMessageEndTime > 0) {
        // Stops getting to the bottom to reset everything before time is up
        return;
      }
      // After display, the message content must persist, but the display must
      // cycle.
      Serial.println(F("[MESSAGE] Short message duration complete. Advancing "
                       "display mode."));
      customMessageEndTime = 0;
      customerMessageDuration = 0;
      advanceDisplayMode();
      return;
    } else {
      // ----------------------------------------------------------------------
      // BRANCH B: SCROLLING (Long Message: strlen > 8) - (Existing Logic)
      // ----------------------------------------------------------------------

      // --- Determine if we need left padding based on previous mode ---
      bool addPadding = false;
      bool humidityVisible =
          showHumidity && weatherAvailable &&
          ((!useHomeAssistant && strlen(openWeatherApiKey) == 32 &&
            strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0) ||
           (useHomeAssistant && strlen(homeAssistantApiKey) > 32 &&
            strlen(haTempSensor) > 0));

      // If coming from CLOCK mode
      if (prevDisplayMode == 0 && (showDayOfWeek || colonBlinkEnabled)) {
        addPadding = true;
      } else if (prevDisplayMode == 1 && humidityVisible) {
        addPadding = true;
      }
      // Apply padding (4 spaces) if needed
      if (addPadding) {
        msg = "    " + msg;
      }

      // --- Display scrolling message ---
      P.setTextAlignment(PA_LEFT);
      P.setCharSpacing(1);
      textEffect_t actualScrollDirection =
          getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
      extern int messageScrollSpeed;

      // START SCROLL CYCLE
      P.displayScroll(msg.c_str(), PA_LEFT, actualScrollDirection,
                      messageScrollSpeed);

      // SCROLL COUNT INCREMENT
      if (messageScrollTimes > 0) {
        currentScrollCount++;
        Serial.printf("[MESSAGE] Scroll complete. Count: %d/%d\n",
                      currentScrollCount, messageScrollTimes);
      }

      // If no HA parameters are set, this is a persistent/infinite scroll, so
      // advance mode after 1 scroll cycle. If HA parameters ARE set, the mode
      // relies on the check at the top to break out.
      if (messageDisplaySeconds == 0 && messageScrollTimes == 0) {
        P.setTextAlignment(PA_CENTER);
        advanceDisplayMode();
      }
      return;
    }
  }

  unsigned long currentMillis = millis();
  unsigned long runtimeSeconds = (currentMillis - bootMillis) / 1000;
  unsigned long currentTotal = totalUptimeSeconds + runtimeSeconds;

  // --- Log and save uptime every 10 minutes ---
  const unsigned long uptimeLogInterval = 600000UL; // 10 minutes in ms

  if (currentMillis - lastUptimeLog >= uptimeLogInterval) {
    lastUptimeLog = currentMillis;
    Serial.printf("[UPTIME] Runtime: %s (total %.2f hours)\n",
                  formatUptime(currentTotal).c_str(), currentTotal / 3600.0);
    saveUptime(); // Save accumulated uptime every 10 minutes
  }
}