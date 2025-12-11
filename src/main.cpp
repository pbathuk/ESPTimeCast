#include <Arduino.h>
#include <ArduinoOTA.h>
#include "Globals.h"       // Global variables and constants
#include "Utils.h"      // Utility functions  
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "WeatherManager.h"
#include "WebHandler.h"


#include "mfactoryfont.h"   // Custom font
#include "tz_lookup.h"      // Timezone lookup, do not duplicate mapping here!
#include "days_lookup.h"    // Languages for the Days of the Week
#include "months_lookup.h"  // Languages for the Months of the Year

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
    4: Nightscout
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
  P.begin();  // Initialize Parola library

  P.setCharSpacing(0);
  P.setFont(mFactory);
  loadConfig();  // This function now has internal yields and prints

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
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: name = "DISCONNECTED"; break;
      default: return;  // ignore all other events
    }
    Serial.printf("[WIFI EVENT] %s (%d)\n", name, event);
  });


  connectWiFi();

  if (isAPMode) {
    Serial.println(F("[SETUP] WiFi connection failed. Device is in AP Mode."));
  } else if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("[SETUP] WiFi connected successfully to local network."));
  } else {
    Serial.println(F("[SETUP] WiFi state is uncertain after connection attempt."));
  }

  setupMDNS();
  setupWebServer();
  Serial.println(F("[SETUP] Webserver setup complete"));
  Serial.println(F("[SETUP] Setup complete"));
  Serial.println();
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
  ArduinoOTA.setHostname("ESPTimeCast");
  
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
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
  const unsigned long fetchInterval = 300000;  // 5 minutes


  // AP Mode animation
  static unsigned long apAnimTimer = 0;
  static int apAnimFrame = 0;
  if (isAPMode) {
    unsigned long now = millis();
    if (now - apAnimTimer > 750) {
      apAnimTimer = now;
      apAnimFrame++;
    }
    P.setTextAlignment(PA_CENTER);
    switch (apAnimFrame % 3) {
      case 0: P.print(F("= ©")); break;
      case 1: P.print(F("= ª")); break;
      case 2: P.print(F("= «")); break;
    }
    yield();
    return;
  }


  // -----------------------------
  // Dimming (auto + manual)
  // -----------------------------
  time_t now_time = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now_time, &timeinfo);
  int curHour = timeinfo.tm_hour;
  int curMinute = timeinfo.tm_min;
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
    startTotal = endTotal = -1;  // not used
  }

  // -----------------------------
  // Check if dimming should be active
  // -----------------------------
  if (autoDimmingEnabled || dimmingEnabled) {
    if (startTotal < endTotal) {
      dimActive = (curTotal >= startTotal && curTotal < endTotal);
    } else {
      dimActive = (curTotal >= startTotal || curTotal < endTotal);  // overnight
    }
  }

  // -----------------------------
  // Apply brightness / display on-off
  // -----------------------------
  static bool lastDimActive = false;  // remembers last state
  int targetBrightness = dimActive ? dimBrightness : brightness;

  // Log only when transitioning
  if (dimActive != lastDimActive) {
    if (dimActive) {
      if (autoDimmingEnabled)
        Serial.printf("[DISPLAY] Automatic dimming setting brightness to %d\n", targetBrightness);
      else if (dimmingEnabled)
        Serial.printf("[DISPLAY] Custom dimming setting brightness to %d\n", targetBrightness);
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
    if (displayOff && ((dimActive && displayOffByBrightness) || (!dimActive && displayOffByDimming))) {
      P.displayShutdown(false);
      displayOff = false;
      displayOffByDimming = false;
      displayOffByBrightness = false;
    }
    P.setIntensity(targetBrightness);
  }


  // --- IMMEDIATE COUNTDOWN FINISH TRIGGER ---
  if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && now_time >= countdownTargetTimestamp) {
    countdownFinished = true;
    displayMode = 3;  // Let main loop handle animation + TIMES UP
    countdownShowFinishedMessage = true;
    hourglassPlayed = false;
    countdownFinishedMessageStartTime = millis();

    Serial.println("[SYSTEM] Countdown target reached! Switching to Mode 3 to display finish sequence.");
    yield();
  }


  // --- IP Display ---
  if (showingIp) {
    if (P.displayAnimate()) {
      ipDisplayCount++;
      if (ipDisplayCount < ipDisplayMax) {
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(pendingIpToShow.c_str(), PA_CENTER, actualScrollDirection, 120);
      } else {
        showingIp = false;
        P.displayClear();
        delay(500);  // Blocking delay as in working copy
        displayMode = 0;
        lastSwitch = millis();
      }
    }
    yield();
    return;  // Exit loop early if showing IP
  }


  // --- BRIGHTNESS/OFF CHECK ---
  if (brightness == -1) {
    if (!displayOff) {
      Serial.println(F("[DISPLAY] Turning display OFF"));
      P.displayShutdown(true);  // fully off
      P.displayClear();
      displayOff = true;
    }
    yield();
  }


  // --- NTP State Machine ---
  switch (ntpState) {
    case NTP_IDLE: break;
    case NTP_SYNCING:
      {
        time_t now = time(nullptr);
        if (now > 1000) {  // NTP sync successful
          Serial.println(F("[TIME] NTP sync successful."));
          ntpSyncSuccessful = true;
          ntpState = NTP_SUCCESS;
        } else if (millis() - ntpStartTime > ntpTimeout || ntpRetryCount >= maxNtpRetries) {
          Serial.println(F("[TIME] NTP sync failed."));
          ntpSyncSuccessful = false;
          ntpState = NTP_FAILED;
        } else {
          // Periodically print a more descriptive status message
          if (millis() - lastNtpStatusPrintTime >= ntpStatusPrintInterval) {
            Serial.printf("[TIME] NTP sync in progress (attempt %d of %d)...\n", ntpRetryCount + 1, maxNtpRetries);
            lastNtpStatusPrintTime = millis();
          }
          // Still increment ntpRetryCount based on your original timing for the timeout logic
          // (even if you don't print a dot for every increment)
          if (millis() - ntpStartTime > ((unsigned long)(ntpRetryCount + 1) * 1000UL)) {
            ntpRetryCount++;
          }
        }
        break;
      }
    case NTP_SUCCESS:
      if (!tzSetAfterSync) {
        const char *posixTz = ianaToPosix(timeZone);
        setenv("TZ", posixTz, 1);
        tzset();
        tzSetAfterSync = true;
      }
      ntpAnimTimer = 0;
      ntpAnimFrame = 0;
      break;

    case NTP_FAILED:
      ntpAnimTimer = 0;
      ntpAnimFrame = 0;

      static unsigned long lastNtpRetryAttempt = 0;
      static bool firstRetry = true;

      if (lastNtpRetryAttempt == 0) {
        lastNtpRetryAttempt = millis();  // set baseline on first fail
      }

      unsigned long ntpRetryInterval = firstRetry ? 30000UL : 300000UL;  // first retry after 30s, after that every 5 minutes

      if (millis() - lastNtpRetryAttempt > ntpRetryInterval) {
        lastNtpRetryAttempt = millis();
        ntpRetryCount = 0;
        ntpStartTime = millis();
        ntpState = NTP_SYNCING;
        Serial.println(F("[TIME] Retrying NTP sync..."));

        firstRetry = false;
      }
      break;
  }

  // 1. Audio Handling (Highest Priority)
  if (isAlarmPlaying) {
      audio.loop(); 
      // NO DELAYS HERE!
  } else {
      // 2. Normal Idle maintenance
      // It is safe to delay here because no music is playing
      vTaskDelay(1); 
  }

  // Only advance mode by timer for clock/weather, not description!
  unsigned long displayDuration = (displayMode == 0) ? clockDuration : weatherDuration;
  if ((displayMode == 0 || displayMode == 1) && millis() - lastSwitch > displayDuration) {
    advanceDisplayMode();
  }


  // --- MODIFIED WEATHER FETCHING LOGIC ---
  if (WiFi.status() == WL_CONNECTED) {
    if (!weatherFetchInitiated || shouldFetchWeatherNow || (millis() - lastFetch > fetchInterval)) {
      if (shouldFetchWeatherNow) {
        Serial.println(F("[LOOP] Immediate weather fetch requested by web server."));
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

  const char *const *daysOfTheWeek = getDaysOfWeek(language);
  const char *daySymbol = daysOfTheWeek[timeinfo.tm_wday];


  // build base HH:MM first ---
  char baseTime[9];
  if (twelveHourToggle) {
    int hour12 = timeinfo.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    if (amPMShow) {
      // Determine AM or PM based on the raw 24-hour value
      const char* ampm = (timeinfo.tm_hour < 12) ? " \x80" : " \x81";
      sprintf(baseTime, "%d:%02d%s", hour12, timeinfo.tm_min, ampm);
    }
    else {
      sprintf(baseTime, "%d:%02d", hour12, timeinfo.tm_min);
    }
  } else {
    sprintf(baseTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }

  // add seconds only if colon blink enabled AND weekday hidden ---
  char timeWithSeconds[12];
  if (!showDayOfWeek && colonBlinkEnabled) {
    // Remove any leading space from baseTime
    const char *trimmedBase = baseTime;
    if (baseTime[0] == ' ') trimmedBase++;  // skip leading space
    sprintf(timeWithSeconds, "%s:%02d", trimmedBase, timeinfo.tm_sec);
  } else {
    strcpy(timeWithSeconds, baseTime);  // no seconds
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
  } else if (displayMode == 1) {  // Weather
    currentDisplayDuration = weatherDuration;
  }

  // Only advance mode by timer for clock/weather static (Mode 0 & 1).
  // Other modes (2, 3) have their own internal timers/conditions for advancement.
  if ((displayMode == 0 || displayMode == 1) && (millis() - lastSwitch > currentDisplayDuration)) {
    advanceDisplayMode();
  }


  // --- CLOCK Display Mode ---
  if (displayMode == 0) {
    P.setCharSpacing(0);

    // --- NTP SYNC ---
    if (ntpState == NTP_SYNCING) {
      if (ntpSyncSuccessful || ntpRetryCount >= maxNtpRetries || millis() - ntpStartTime > ntpTimeout) {
        ntpState = NTP_FAILED;
      } else if (millis() - ntpAnimTimer > 750) {
        ntpAnimTimer = millis();
        switch (ntpAnimFrame % 3) {
          case 0: P.print(F("S Y N C ®")); break;
          case 1: P.print(F("S Y N C ¯")); break;
          case 2: P.print(F("S Y N C º")); break;
        }
        ntpAnimFrame++;
      }
    }
    // --- NTP / WEATHER ERROR ---
    else if (!ntpSyncSuccessful) {
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
      if (prevDisplayMode == -1 || prevDisplayMode == 3 || prevDisplayMode == 4) {
        shouldScrollIn = true;  // first boot or other special modes
      } else if (prevDisplayMode == 2 && weatherDescription.length() > 8) {
        shouldScrollIn = true;  // only scroll in if weather was scrolling
      } else if (prevDisplayMode == 6) {
        shouldScrollIn = true;  // scroll in when coming from custom message
      }

      if (shouldScrollIn && !clockScrollDone) {
        textEffect_t inDir = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);

        P.displayText(
          timeString.c_str(),
          PA_CENTER,
          GENERAL_SCROLL_SPEED,
          0,
          inDir,
          PA_NO_EFFECT);
        while (!P.displayAnimate()) yield();
        clockScrollDone = true;  // mark scroll done
      } else {
        P.setTextAlignment(PA_CENTER);
        P.print(timeString);
      }
    }

    yield();
  } else {
    // --- leaving clock mode ---
    if (prevDisplayMode == 0) {
      clockScrollDone = false;  // reset for next time we enter clock
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
        if (!colonVisible) timeString.replace(":", " ");
        P.setCharSpacing(0);
        P.print(timeString);
      } else {
        P.setCharSpacing(0);
        P.setTextAlignment(PA_CENTER);
        P.print(F("(*"));
      }
    }
    yield();
    return;
  }


  // --- WEATHER DESCRIPTION Display Mode ---
  if (displayMode == 2 && showWeatherDescription && weatherAvailable && weatherDescription.length() > 0) {
    String desc = weatherDescription;

    // --- Check if humidity is actually visible ---
    bool humidityVisible = showHumidity && weatherAvailable && (
      (!useHomeAssistant && strlen(openWeatherApiKey) == 32 && strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0)
      ||
      (useHomeAssistant && strlen(homeAssistantApiKey) > 32 && strlen(haTempSensor) > 0) 
    );

    // --- Conditional padding ---
    bool addPadding = false;
    if (prevDisplayMode == 1 && humidityVisible) {
      addPadding = true;
    }
    if (addPadding) {
      desc = "    " + desc;  // 4-space padding before scrolling
    }

    // prepare safe buffer
    static char descBuffer[128];  // large enough for OWM translations
    desc.toCharArray(descBuffer, sizeof(descBuffer));

    if (desc.length() > 8) {
      if (!descScrolling) {
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(descBuffer, PA_CENTER, actualScrollDirection, GENERAL_SCROLL_SPEED);
        descScrolling = true;
        descScrollEndTime = 0;  // reset end time at start
      }
      if (P.displayAnimate()) {
        if (descScrollEndTime == 0) {
          descScrollEndTime = millis();  // mark the time when scroll finishes
        }
        // wait small pause after scroll stops
        if (millis() - descScrollEndTime > descriptionScrollPause) {
          descScrolling = false;
          descScrollEndTime = 0;
          advanceDisplayMode();
        }
      } else {
        descScrollEndTime = 0;  // reset if not finished
      }
      yield();
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
      yield();
      return;
    }
  }


  // --- Countdown Display Mode ---
  if (displayMode == 3 && countdownEnabled && ntpSyncSuccessful) {
    static int countdownSegment = 0;
    static unsigned long segmentStartTime = 0;
    const unsigned long SEGMENT_DISPLAY_DURATION = 1500;  // 1.5 seconds for each static segment

    long timeRemaining = countdownTargetTimestamp - now_time;

    // --- Countdown Finished Logic ---
    // This part of the code remains unchanged.
    if (timeRemaining <= 0 || countdownShowFinishedMessage) {
      // NEW: Only show "TIMES UP" if countdown target timestamp is valid and expired
      time_t now = time(nullptr);
      if (countdownTargetTimestamp == 0 || countdownTargetTimestamp > now) {
        // Target invalid or in the future, don't show "TIMES UP" yet, advance display instead
        countdownShowFinishedMessage = false;
        countdownFinished = false;
        countdownFinishedMessageStartTime = 0;
        hourglassPlayed = false;  // Reset if we decide not to show it
        Serial.println("[COUNTDOWN-FINISH] Countdown target invalid or not reached yet, skipping 'TIMES UP'. Advancing display.");
        advanceDisplayMode();
        yield();
        return;
      }

      // Define these static variables here if they are not global (or already defined in your loop())
      static const char *flashFrames[] = { "{|", "}~" };
      static unsigned long lastFlashingSwitch = 0;
      static int flashingMessageFrame = 0;

      // --- Initial Combined Sequence: Play Hourglass THEN start Flashing ---
      // This 'if' runs ONLY ONCE when the "finished" sequence begins.
      if (!hourglassPlayed) {                          // <-- This is the single entry point for the combined sequence
        countdownFinished = true;                      // Mark as finished overall
        countdownShowFinishedMessage = true;           // Confirm we are in the finished sequence
        countdownFinishedMessageStartTime = millis();  // Start the 15-second timer for the flashing duration

        // 1. Play Hourglass Animation (Blocking)
        const char *hourglassFrames[] = { "¡", "¢", "£", "¤" };
        for (int repeat = 0; repeat < 3; repeat++) {
          for (int i = 0; i < 4; i++) {
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(0);
            P.print(hourglassFrames[i]);
            delay(350);  // This is blocking! (Total ~4.2 seconds for hourglass)
          }
        }
        Serial.println("[COUNTDOWN-FINISH] Played hourglass animation.");
        P.displayClear();  // Clear display after hourglass animation

        // 2. Initialize Flashing "TIMES UP" for its very first frame
        flashingMessageFrame = 0;
        lastFlashingSwitch = millis();  // Set initial time for first flash frame
        P.setTextAlignment(PA_CENTER);
        P.setCharSpacing(0);
        P.print(flashFrames[flashingMessageFrame]);             // Display the first frame immediately
        flashingMessageFrame = (flashingMessageFrame + 1) % 2;  // Prepare for the next frame

        hourglassPlayed = true;  // <-- Mark that this initial combined sequence has completed!
        countdownSegment = 0;    // Reset segment counter after finished sequence initiation
        segmentStartTime = 0;    // Reset segment timer after finished sequence initiation
      }

      // --- Continue Flashing "TIMES UP" for its duration (after initial combined sequence) ---
      // This part runs in subsequent loop iterations after the hourglass has played.
      if (millis() - countdownFinishedMessageStartTime < 15000) {  // Flashing duration
        if (millis() - lastFlashingSwitch >= 500) {                // Check for flashing interval
          lastFlashingSwitch = millis();
          P.displayClear();
          P.setTextAlignment(PA_CENTER);
          P.setCharSpacing(0);
          P.print(flashFrames[flashingMessageFrame]);
          flashingMessageFrame = (flashingMessageFrame + 1) % 2;
        }
        P.displayAnimate();  // Ensure display updates
        yield();
        return;  // Stay in this mode until the 15 seconds are over
      } else {
        // 15 seconds are over, clean up and advance
        Serial.println("[COUNTDOWN-FINISH] Flashing duration over. Advancing to Clock.");
        countdownShowFinishedMessage = false;
        countdownFinishedMessageStartTime = 0;
        hourglassPlayed = false;  // <-- RESET this flag for the next countdown cycle!

        // Final cleanup (persisted)
        countdownEnabled = false;
        countdownTargetTimestamp = 0;
        countdownLabel[0] = '\0';
        saveCountdownConfig(false, 0, "");

        P.setInvert(false);
        advanceDisplayMode();
        yield();
        return;  // Exit loop after processing
      }
    }  // END of 'if (timeRemaining <= 0 || countdownShowFinishedMessage)'


    // --- NORMAL COUNTDOWN LOGIC ---
    // This 'else' block will only run if `timeRemaining > 0` and `!countdownShowFinishedMessage`
    else {

      // The new variable `isDramaticCountdown` toggles between the two modes
      if (isDramaticCountdown) {
        // --- EXISTING DRAMATIC COUNTDOWN LOGIC ---
        long days = timeRemaining / (24 * 3600);
        long hours = (timeRemaining % (24 * 3600)) / 3600;
        long minutes = (timeRemaining % 3600) / 60;
        long seconds = timeRemaining % 60;
        String currentSegmentText = "";

        if (segmentStartTime == 0 || (millis() - segmentStartTime > SEGMENT_DISPLAY_DURATION)) {
          segmentStartTime = millis();
          P.displayClear();

          switch (countdownSegment) {
            case 0:  // Days
              if (days > 0) {
                currentSegmentText = String(days) + " " + (days == 1 ? "DAY" : "DAYS");
                Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
                countdownSegment++;
              } else {
                // Skip days if zero
                countdownSegment++;
                segmentStartTime = 0;
              }
              break;
            case 1:
              {  // Hours
                char buf[10];
                sprintf(buf, "%02ld HRS", hours);  // pad hours with 0
                currentSegmentText = String(buf);
                Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
                countdownSegment++;
                break;
              }
            case 2:
              {  // Minutes
                char buf[10];
                sprintf(buf, "%02ld MINS", minutes);  // pad minutes with 0
                currentSegmentText = String(buf);
                Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
                countdownSegment++;
                break;
              }
            case 3:
              {  // Seconds & Label Scroll
                time_t segmentStartTime = time(nullptr);
                unsigned long segmentStartMillis = millis();

                long nowRemaining = countdownTargetTimestamp - segmentStartTime;
                long currentSecond = nowRemaining % 60;
                char secondsBuf[10];
                sprintf(secondsBuf, "%02ld %s", currentSecond, currentSecond == 1 ? "SEC" : "SECS");
                String secondsText = String(secondsBuf);
                Serial.printf("[COUNTDOWN-STATIC] Displaying segment 3: %s\n", secondsText.c_str());
                P.displayClear();
                P.setTextAlignment(PA_CENTER);
                P.setCharSpacing(1);
                P.print(secondsText.c_str());
                delay(SEGMENT_DISPLAY_DURATION - 400);

                unsigned long elapsed = millis() - segmentStartMillis;
                long adjustedSecond = (countdownTargetTimestamp - segmentStartTime - (elapsed / 1000)) % 60;
                sprintf(secondsBuf, "%02ld %s", adjustedSecond, adjustedSecond == 1 ? "SEC" : "SECS");
                secondsText = String(secondsBuf);
                P.displayClear();
                P.setTextAlignment(PA_CENTER);
                P.setCharSpacing(1);
                P.print(secondsText.c_str());
                delay(400);

                String label;
                if (strlen(countdownLabel) > 0) {
                  label = String(countdownLabel);
                  label.trim();
                  if (!label.startsWith("TO:") && !label.startsWith("to:")) {
                    label = "TO: " + label;
                  }
                  label.replace('.', ',');
                } else {
                  static const char *fallbackLabels[] = {
                    "TO: PARTY TIME!", "TO: SHOWTIME!", "TO: CLOCKOUT!", "TO: BLASTOFF!",
                    "TO: GO TIME!", "TO: LIFTOFF!", "TO: THE BIG REVEAL!",
                    "TO: ZERO HOUR!", "TO: THE FINAL COUNT!", "TO: MISSION COMPLETE"
                  };
                  int randomIndex = random(0, 10);
                  label = fallbackLabels[randomIndex];
                }

                P.setTextAlignment(PA_LEFT);
                P.setCharSpacing(1);
                textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
                P.displayScroll(label.c_str(), PA_LEFT, actualScrollDirection, GENERAL_SCROLL_SPEED);

                while (!P.displayAnimate()) {
                  yield();
                }
                countdownSegment++;
                segmentStartTime = millis();
                break;
              }
            case 4:  // Exit countdown
              Serial.println("[COUNTDOWN-STATIC] All segments and label displayed. Advancing to Clock.");
              countdownSegment = 0;
              segmentStartTime = 0;
              P.setTextAlignment(PA_CENTER);
              P.setCharSpacing(1);
              advanceDisplayMode();
              yield();
              return;

            default:
              Serial.println("[COUNTDOWN-ERROR] Invalid countdownSegment, resetting.");
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
        long days = timeRemaining / (24 * 3600);
        long hours = (timeRemaining % (24 * 3600)) / 3600;
        long minutes = (timeRemaining % 3600) / 60;
        long seconds = timeRemaining % 60;

        String label;
        // Check if countdownLabel is empty and grab a random one if needed
        if (strlen(countdownLabel) > 0) {
          label = String(countdownLabel);
          label.trim();

          // Replace standard digits 0–9 with your custom font character codes
          for (int i = 0; i < label.length(); i++) {
            if (isDigit(label[i])) {
              int num = label[i] - '0';           // 0–9
              label[i] = 145 + ((num + 9) % 10);  // Maps 0→154, 1→145, ... 9→153
            }
          }

        } else {
          static const char *fallbackLabels[] = {
            "PARTY TIME", "SHOWTIME", "CLOCKOUT", "BLASTOFF",
            "GO TIME", "LIFTOFF", "THE BIG REVEAL",
            "ZERO HOUR", "THE FINAL COUNT", "MISSION COMPLETE"
          };
          int randomIndex = random(0, 10);
          label = fallbackLabels[randomIndex];
        }

        // Format the full string
        char buf[50];
        // Only show days if there are any, otherwise start with hours
        if (days > 0) {
          sprintf(buf, "%s IN: %ldD %02ldH %02ldM %02ldS", label.c_str(), days, hours, minutes, seconds);
        } else {
          sprintf(buf, "%s IN: %02ldH %02ldM %02ldS", label.c_str(), hours, minutes, seconds);
        }

        String fullString = String(buf);
        bool addPadding = false;
        bool humidityVisible = showHumidity && weatherAvailable && (
          (!useHomeAssistant && strlen(openWeatherApiKey) == 32 && strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0)
          ||
          (useHomeAssistant && strlen(homeAssistantApiKey) > 32 && strlen(haTempSensor) > 0)
        );

        // Padding logic
        if (prevDisplayMode == 0 && (showDayOfWeek || colonBlinkEnabled)) {
          addPadding = true;
        } else if (prevDisplayMode == 1 && humidityVisible) {
          addPadding = true;
        }
        if (addPadding) {
          fullString = "    " + fullString;  // 4 spaces
        }

        // Display the full string and scroll it
        P.setTextAlignment(PA_LEFT);
        P.setCharSpacing(1);
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(fullString.c_str(), PA_LEFT, actualScrollDirection, GENERAL_SCROLL_SPEED);

        // Blocking loop to ensure the full message scrolls
        while (!P.displayAnimate()) {
          yield();
        }

        // After scrolling is complete, we're done with this display mode
        // Move to the next mode and exit the function.
        P.setTextAlignment(PA_CENTER);
        advanceDisplayMode();
        yield();
        return;
      }
    }

    // Keep alignment reset just in case
    P.setTextAlignment(PA_CENTER);
    P.setCharSpacing(1);
    yield();
    return;
  }  // End of if (displayMode == 3 && ...)


  // --- NIGHTSCOUT Display Mode ---

  if (displayMode == 4) {
    String ntpField = String(ntpServer2);

    // These static variables will retain their values between calls to this block
    static unsigned long lastNightscoutFetchTime = 0;
    const unsigned long NIGHTSCOUT_FETCH_INTERVAL = 150000;  // 2.5 minutes
    static int currentGlucose = -1;
    static String currentDirection = "?";
    static time_t lastGlucoseTime = 0;  // store timestamp from JSON

    // --- Small helper inside this block ---
    auto makeTimeUTC = [](struct tm *tm) -> time_t {
      // ESP32: timegm() is not implemented — emulate correctly
      struct tm tm_copy = *tm;
      // mktime() interprets tm as local, but system time is UTC already
      // so we can safely assume input is UTC
      return mktime(&tm_copy);
    };
    // --------------------------------------

    // Check if it's time to fetch new data or if we have no data yet
    if (currentGlucose == -1 || millis() - lastNightscoutFetchTime >= NIGHTSCOUT_FETCH_INTERVAL) {
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient https;
      https.begin(client, ntpField);
      https.setTimeout(5000);

      Serial.println("[HTTPS] Nightscout fetch initiated...");
      int httpCode = https.GET();

      if (httpCode == HTTP_CODE_OK) {
        String payload = https.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc.is<JsonArray>() && doc.size() > 0) {
          JsonObject firstReading = doc[0].as<JsonObject>();
          currentGlucose = firstReading["glucose"] | firstReading["sgv"] | -1;
          currentDirection = firstReading["direction"] | "?";
          const char *dateStr = firstReading["dateString"];

          // --- Parse ISO 8601 UTC time ---
          if (dateStr) {
            struct tm tm {};
            if (sscanf(dateStr, "%4d-%2d-%2dT%2d:%2d:%2dZ",
                       &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       &tm.tm_hour, &tm.tm_min, &tm.tm_sec)
                == 6) {
              tm.tm_year -= 1900;
              tm.tm_mon -= 1;
              lastGlucoseTime = makeTimeUTC(&tm);
            }
          }

          Serial.printf("Nightscout data fetched: %d mg/dL %s\n", currentGlucose, currentDirection.c_str());
        } else {
          Serial.println("Failed to parse Nightscout JSON");
        }
      } else {
        Serial.printf("[HTTPS] GET failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
      lastNightscoutFetchTime = millis();
    }

    // --- Display the data ---
    if (currentGlucose != -1) {
      // Calculate age of reading
      // Get current UTC time (avoid local timezone offset)
      time_t nowLocal = time(nullptr);
      struct tm *gmt = gmtime(&nowLocal);
      time_t nowUTC = mktime(gmt);

      bool isOutdated = false;
      int ageMinutes = 0;

      if (lastGlucoseTime > 0) {
        double diffSec = difftime(nowUTC, lastGlucoseTime);
        ageMinutes = (int)(diffSec / 60.0);
        isOutdated = (ageMinutes > NIGHTSCOUT_IDLE_THRESHOLD_MIN);
        Serial.printf("[NIGHTSCOUT] Data age: %d minutes old (threshold: %d)\n", ageMinutes, NIGHTSCOUT_IDLE_THRESHOLD_MIN);
      }

      // Pick arrow character
      char arrow;
      if (currentDirection == "Flat") arrow = 139;
      else if (currentDirection == "SingleUp") arrow = 134;
      else if (currentDirection == "DoubleUp") arrow = 135;
      else if (currentDirection == "SingleDown") arrow = 136;
      else if (currentDirection == "DoubleDown") arrow = 137;
      else if (currentDirection == "FortyFiveUp") arrow = 138;
      else if (currentDirection == "FortyFiveDown") arrow = 140;
      else arrow = '?';

      // Build display text
      String displayText = "";
      // ADD crossed digits
      if (isOutdated) {

        String glucoseStr = String(currentGlucose);

        for (int i = 0; i < glucoseStr.length(); i++) {
          if (isDigit(glucoseStr[i])) {
            int num = glucoseStr[i] - '0';           // 0–9
            glucoseStr[i] = 195 + ((num + 9) % 10);  // Maps 0→204, 1→195, ...
          }
        }

        String separatedStr = "";
        for (int i = 0; i < glucoseStr.length(); i++) {
          separatedStr += glucoseStr[i];
          if (i < glucoseStr.length() - 1) {
            separatedStr += char(255);  // insert separator between digits
          }
        }

        displayText += char(255);
        displayText += char(255);
        displayText += separatedStr;
        displayText += char(255);
        displayText += char(255);
        displayText += " ";  // extra space
        displayText += arrow;
        P.setCharSpacing(0);
      } else {
        displayText += String(currentGlucose) + String(arrow);
        P.setCharSpacing(1);
      }

      P.setTextAlignment(PA_CENTER);
      P.print(displayText.c_str());
      delay(weatherDuration);
      advanceDisplayMode();
      return;
    } else {
      P.setTextAlignment(PA_CENTER);
      P.setCharSpacing(0);
      P.print(F("())"));
      delay(2000);
      advanceDisplayMode();
      return;
    }
  }
  //DATE Display Mode
  else if (displayMode == 5 && showDate) {

    // --- VALID DATE CHECK ---
    if (timeinfo.tm_year < 120 || timeinfo.tm_mday <= 0 || timeinfo.tm_mon < 0 || timeinfo.tm_mon > 11) {
      advanceDisplayMode();
      return;  // skip drawing
    }
    // -------------------------
    String dateString;

    // Get localized month names
    const char *const *months = getMonthsOfYear(language);
    String monthAbbr = String(months[timeinfo.tm_mon]).substring(0, 5);
    monthAbbr.toLowerCase();

    // Add spaces between day digits
    String dayString = String(timeinfo.tm_mday);
    String spacedDay = "";
    for (size_t i = 0; i < dayString.length(); i++) {
      spacedDay += dayString[i];
      if (i < dayString.length() - 1) spacedDay += " ";
    }

    // Function to check if day should come first for given language
    auto isDayFirst = [](const String &lang) {
      // Languages with DD-MM order
      const char *dayFirstLangs[] = {
        "af",  // Afrikaans
        "cs",  // Czech
        "da",  // Danish
        "de",  // German
        "eo",  // Esperanto
        "es",  // Spanish
        "et",  // Estonian
        "fi",  // Finnish
        "fr",  // French
        "ga",  // Irish
        "hr",  // Croatian
        "hu",  // Hungarian
        "it",  // Italian
        "lt",  // Lithuanian
        "lv",  // Latvian
        "nl",  // Dutch
        "no",  // Norwegian
        "pl",  // Polish
        "pt",  // Portuguese
        "ro",  // Romanian
        "ru",  // Russian
        "sk",  // Slovak
        "sl",  // Slovenian
        "sr",  // Serbian
        "sv",  // Swedish
        "sw",  // Swahili
        "tr"   // Turkish
      };
      for (auto lf : dayFirstLangs) {
        if (lang.equalsIgnoreCase(lf)) {
          return true;
        }
      }
      return false;
    };

    String langForDate = String(language);

    if (langForDate == "ja") {
      // Japanese: month number (spaced digits) + day + symbol
      String spacedMonth = "";
      String monthNum = String(timeinfo.tm_mon + 1);
      dateString = monthAbbr + "  " + spacedDay + " ±";

    } else {
      if (isDayFirst(language)) {
        dateString = spacedDay + "   " + monthAbbr;
      } else {
        dateString = monthAbbr + "   " + spacedDay;
      }
    }

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
      yield();
      return;
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
    if (messageDisplaySeconds > 0 && (millis() - messageStartTime) >= (messageDisplaySeconds * 1000UL)) {
      Serial.printf("[MESSAGE] HA message timed out after %d seconds.\n", messageDisplaySeconds);
      timedOut = true;
    }

    // --- CHECK FOR SCROLL/CYCLE LIMIT BEFORE DISPLAYING ---
    // Scrolls complete applies to long messages.
    bool scrollsComplete = (messageScrollTimes > 0) && (currentScrollCount >= messageScrollTimes);

    // Cycles complete applies to short messages.
    extern int currentDisplayCycleCount;  // Use the dedicated short message counter
    bool cyclesComplete = (messageScrollTimes > 0) && (currentDisplayCycleCount >= messageScrollTimes);


    // --- ADVANCE MODE CHECK (Check if HA parameters are complete) ---
    // If either timer or cycle/scroll count is finished, we clean up the temporary message.
    if (scrollsComplete || cyclesComplete) {
      Serial.println(F("[MESSAGE] HA-controlled message finished."));

      // Reset common counters
      currentScrollCount = 0;
      messageStartTime = 0;
      currentDisplayCycleCount = 0;  // Reset the cycle counter

      // CRITICAL LOGIC: RESTORE PERSISTENT MESSAGE (Exit Mode 6 Logic)
      if (strlen(lastPersistentMessage) > 0) {
        // A persistent message exists, restore it
        strncpy(customMessage, lastPersistentMessage, sizeof(customMessage));
        messageScrollSpeed = GENERAL_SCROLL_SPEED;
        messageDisplaySeconds = 0;
        messageScrollTimes = 0;
        Serial.printf("[MESSAGE] Restored persistent message: '%s'. Staying in mode 6.\n", customMessage);
      } else {
        // No persistent message to restore. Clear the temporary HA message and Exit mode 6.
        customMessage[0] = '\0';
        Serial.println(F("[MESSAGE] No persistent message to restore. Advancing display mode."));
        advanceDisplayMode();
      }
      yield();
      return;
    }

    // ----------------------------------------------------------------------
    // BRANCH A: NON-SCROLLING (Short Message: strlen <= 8)
    // ----------------------------------------------------------------------
    if (msg.length() <= MAX_NON_SCROLLING_CHARS) {

      // Determine the duration: use HA seconds if set, otherwise use weatherDuration.
      unsigned long durationMs = (messageDisplaySeconds > 0)
                                   ? (messageDisplaySeconds * 1000UL)
                                   : weatherDuration;

      // If HA seconds is set, we use the timedOut check at the top.
      // If only scrollTimes is set, we still display for weatherDuration before incrementing the cycle count.

      Serial.printf("[MESSAGE] Displaying timed short message: '%s' for %lu ms. Advancing mode.\n", customMessage, durationMs);

      P.setTextAlignment(PA_CENTER);
      P.setCharSpacing(1);
      P.print(msg.c_str());

      // Block execution for the specified duration (non-HA uses weatherDuration)
      unsigned long displayUntil = millis() + durationMs;
      while (millis() < displayUntil) {
        yield();
      }

      // --- CYCLE TRACKING FOR SCROLLTIMES ---
      // Increment the counter if the HA message is configured to clear by scroll count.
      if (messageScrollTimes > 0) {
        currentDisplayCycleCount++;
        Serial.printf("[MESSAGE] Short message cycle complete. Count: %d/%d\n", currentDisplayCycleCount, messageScrollTimes);
      }

      // After display, the message content must persist, but the display must cycle.
      Serial.println(F("[MESSAGE] Short message duration complete. Advancing display mode."));
      advanceDisplayMode();
      yield();
      return;
    }

    // ----------------------------------------------------------------------
    // BRANCH B: SCROLLING (Long Message: strlen > 8) - (Existing Logic)
    // ----------------------------------------------------------------------

    // --- Determine if we need left padding based on previous mode ---
    bool addPadding = false;
    bool humidityVisible = showHumidity && weatherAvailable && (
      (!useHomeAssistant && strlen(openWeatherApiKey) == 32 && strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0)
      ||
      (useHomeAssistant && strlen(homeAssistantApiKey) > 32 && strlen(haTempSensor) > 0)
    );

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
    textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
    extern int messageScrollSpeed;

    // START SCROLL CYCLE
    P.displayScroll(msg.c_str(), PA_LEFT, actualScrollDirection, messageScrollSpeed);

    // BLOCKING WAIT: Completes 1 full scroll
    while (!P.displayAnimate()) yield();

    // SCROLL COUNT INCREMENT
    if (messageScrollTimes > 0) {
      currentScrollCount++;
      Serial.printf("[MESSAGE] Scroll complete. Count: %d/%d\n", currentScrollCount, messageScrollTimes);
    }

    // If no HA parameters are set, this is a persistent/infinite scroll, so advance mode after 1 scroll cycle.
    // If HA parameters ARE set, the mode relies on the check at the top to break out.
    if (messageDisplaySeconds == 0 && messageScrollTimes == 0) {
      P.setTextAlignment(PA_CENTER);
      advanceDisplayMode();
    }

    yield();
    return;
  }

  unsigned long currentMillis = millis();
  unsigned long runtimeSeconds = (currentMillis - bootMillis) / 1000;
  unsigned long currentTotal = totalUptimeSeconds + runtimeSeconds;

  // --- Log and save uptime every 10 minutes ---
  const unsigned long uptimeLogInterval = 600000UL;  // 10 minutes in ms

  if (currentMillis - lastUptimeLog >= uptimeLogInterval) {
    lastUptimeLog = currentMillis;
    Serial.printf("[UPTIME] Runtime: %s (total %.2f hours)\n",
                  formatUptime(currentTotal).c_str(), currentTotal / 3600.0);
    saveUptime();  // Save accumulated uptime every 10 minutes
  }
  yield();
}