#include "WebHandler.h"

void handleCaptivePortal(AsyncWebServerRequest *request) {
  String uri = request->url();

  // Filter out system-generated probe requests
  if (!uri.endsWith("/204") && !uri.endsWith("/ipv6check") &&
      !uri.endsWith("connecttest.txt") && !uri.endsWith("/generate_204") &&
      !uri.endsWith("/fwlink") && !uri.endsWith("/hotspot-detect.html")) {

    Serial.print(F("[WEBSERVER] Captive Portal triggered for URL: "));
    Serial.println(uri);
  }

  if (isAPMode) {
    IPAddress apIP = WiFi.softAPIP();
    String redirectUrl = "http://" + apIP.toString() + "/";
    Serial.print(F("[WEBSERVER] Redirecting to captive portal: "));
    Serial.println(redirectUrl);
    request->redirect(redirectUrl);
  } else {
    Serial.println(F("[WEBSERVER] Not in AP mode — sending 404"));
    request->send(404, "text/plain", "Not found");
  }
}

void setupWebServer() {
  Serial.println(F("[WEBSERVER] Setting up web server..."));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /"));
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/generate_204", HTTP_GET, handleCaptivePortal);        // Android
  server.on("/fwlink", HTTP_GET, handleCaptivePortal);              // Windows
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal); // iOS/macOS
  server.on("/ncsi.txt", HTTP_GET,
            handleCaptivePortal); // Windows NCSI (variation)
  server.on("/cp/success.txt", HTTP_GET,
            handleCaptivePortal); // Android/Generic Success Check
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204); // 204 No Content response
  });
  server.on("/apple-touch-icon.png", HTTP_GET,
            [](AsyncWebServerRequest *request) { // iOS icon check
              request->send(204);
            });
  server.on(
      "/gen_204", HTTP_GET,
      [](AsyncWebServerRequest
             *request) { // Android short probe (already in handleCaptivePortal,
                         // but safe to also silence if somehow missed)
        request->send(204);
      });
  server.on("/library/test/success.html", HTTP_GET,
            [](AsyncWebServerRequest *request) { // iOS/macOS generic check
              request->send(204);
            });
  server.on("/connecttest.txt", HTTP_GET,
            [](AsyncWebServerRequest *request) { // Windows NCSI check
              request->send(204);
            });
  server.on("/msdownload/update/v3/static/trustedr/en/disallowedcertstl.cab",
            HTTP_GET,
            [](AsyncWebServerRequest *request) { request->send(204); });
  server.on("/msdownload/update/v3/static/trustedr/en/authrootstl.cab",
            HTTP_GET,
            [](AsyncWebServerRequest *request) { request->send(204); });
  server.on("/msdownload/update/v3/static/trustedr/en/pinrulesstl.cab",
            HTTP_GET,
            [](AsyncWebServerRequest *request) { request->send(204); });
  server.on("/r/r1.crl", HTTP_GET,
            [](AsyncWebServerRequest *request) { request->send(204); });

  server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /config.json"));
    File f = LittleFS.open("/config.json", "r");
    if (!f) {
      Serial.println(F("[WEBSERVER] Error opening /config.json"));
      request->send(500, "application/json",
                    "{\"error\":\"Failed to open config.json\"}");
      return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
      Serial.print(F("[WEBSERVER] Error parsing /config.json: "));
      Serial.println(err.f_str());
      request->send(500, "application/json",
                    "{\"error\":\"Failed to parse config.json\"}");
      return;
    }

    // Always sanitize before sending to browser
    doc[F("ssid")] = getSafeSsid();
    doc[F("password")] = getSafePassword();
    doc[F("openWeatherApiKey")] = getSafeApiKey();
    doc[F("homeAssistantApiKey")] = getSafeHAApiKey();

    doc[F("mode")] = isAPMode ? "ap" : "sta";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // HANDLER: Save Config (JSON Mode)
  server.on(
      "/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (index + len != total)
          return;

        Serial.println(F("[WEBSERVER] JSON Data received for /save"));

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
          request->send(400, "application/json",
                        "{\"error\":\"Invalid JSON body\"}");
          return;
        }

        // 1. UPDATE GLOBALS FROM JSON (If present in the incoming data)
        // If the webpage didn't send it, getJsonValue returns the current
        // global value (preserving it) or the default if we force it.

        // Integers
        clockDuration = getJsonValue(doc, "clockDuration", 10000);
        weatherDuration = getJsonValue(doc, "weatherDuration", 10000);
        brightness = getJsonValue(doc, "brightness", 5);
        dimBrightness = getJsonValue(doc, "dimBrightness", -1);

        // Dimming Times
        dimStartHour = getJsonValue(doc, "dimStartHour", 22);
        dimStartMinute = getJsonValue(doc, "dimStartMinute", 0);
        dimEndHour = getJsonValue(doc, "dimEndHour", 7);
        dimEndMinute = getJsonValue(doc, "dimEndMinute", 0);

        // Booleans
        flipDisplay = getJsonValue(doc, "flipDisplay", false);
        twelveHourToggle = getJsonValue(doc, "twelveHourToggle", false);
        amPMShow = getJsonValue(doc, "amPMShow", false);
        showDayOfWeek = getJsonValue(doc, "showDayOfWeek", true);
        showDate = getJsonValue(doc, "showDate", true);
        useHomeAssistant = getJsonValue(doc, "useHomeAssistant", false);
        showHumidity = getJsonValue(doc, "showHumidity", false);
        colonBlinkEnabled = getJsonValue(doc, "colonBlinkEnabled", true);
        showWeatherDescription =
            getJsonValue(doc, "showWeatherDescription", false);
        dimmingEnabled = getJsonValue(doc, "dimmingEnabled", false);
        autoDimmingEnabled = getJsonValue(doc, "autoDimmingEnabled", false);

        // Strings
        // Note: For strings, we only update if the JSON contains the key.
        if (doc["timeZone"])
          strlcpy(timeZone, doc["timeZone"], sizeof(timeZone));
        Serial.print("pre-timeZone set to: ");
        Serial.println(timeZone);
        if (doc["weatherUnits"])
          strlcpy(weatherUnits, doc["weatherUnits"], sizeof(weatherUnits));
        if (doc["customMessage"])
          strlcpy(customMessage, doc["customMessage"], sizeof(customMessage));

        if (doc["ntpServer1"])
          strlcpy(ntpServer1, doc["ntpServer1"], sizeof(ntpServer1));
        if (doc["ntpServer2"])
          strlcpy(ntpServer2, doc["ntpServer2"], sizeof(ntpServer2));

        if (doc["homeAssistantURL"]) {
          String tempURL = doc["homeAssistantURL"].as<String>();
          tempURL.trim(); // Remove spaces from start/end

          // Remove trailing slash if present
          if (tempURL.endsWith("/")) {
            tempURL.remove(tempURL.length() - 1);
          }

          strlcpy(homeAssistantURL, tempURL.c_str(), sizeof(homeAssistantURL));
        }
        if (doc["haTempSensor"])
          strlcpy(haTempSensor, doc["haTempSensor"], sizeof(haTempSensor));
        if (doc["haHumiditySensor"])
          strlcpy(haHumiditySensor, doc["haHumiditySensor"],
                  sizeof(haHumiditySensor));

        if (doc["openWeatherCity"])
          strlcpy(openWeatherCity, doc["openWeatherCity"],
                  sizeof(openWeatherCity));
        if (doc["openWeatherCountry"])
          strlcpy(openWeatherCountry, doc["openWeatherCountry"],
                  sizeof(openWeatherCountry));

        // Security Checks (Password & API Keys)
        String newssid = getJsonValue(doc, "ssid", String(""));
        if (newssid != "********" && newssid.length() > 0)
          strlcpy(ssid, newssid.c_str(), sizeof(ssid));

        String newPass = getJsonValue(doc, "password", String(""));
        if (newPass != "********" && newPass.length() > 0)
          strlcpy(password, newPass.c_str(), sizeof(password));

        String newOwKey = getJsonValue(doc, "openWeatherApiKey", String(""));
        if (newOwKey != "********************************" &&
            newOwKey.length() > 0)
          strlcpy(openWeatherApiKey, newOwKey.c_str(),
                  sizeof(openWeatherApiKey));

        String newHaKey = getJsonValue(doc, "homeAssistantApiKey", String(""));
        if (newHaKey != "********************************" &&
            newHaKey.length() > 0)
          strlcpy(homeAssistantApiKey, newHaKey.c_str(),
                  sizeof(homeAssistantApiKey));

        // Countdown Parsing
        bool newCountdownEnabled = getJsonValue(doc, "countdownEnabled", false);
        bool newIsDramatic = getJsonValue(doc, "isDramaticCountdown", false);
        String cDate = getJsonValue(doc, "countdownDate", String(""));
        String cTime = getJsonValue(doc, "countdownTime", String(""));
        String cLabel = getJsonValue(doc, "countdownLabel", String(""));
        time_t newTargetTimestamp = 0;
        if (newCountdownEnabled && cDate.length() > 0 && cTime.length() > 0) {
          struct tm tm = {0};
          tm.tm_year = cDate.substring(0, 4).toInt() - 1900;
          tm.tm_mon = cDate.substring(5, 7).toInt() - 1;
          tm.tm_mday = cDate.substring(8, 10).toInt();
          tm.tm_hour = cTime.substring(0, 2).toInt();
          tm.tm_min = cTime.substring(3, 5).toInt();
          tm.tm_isdst = -1;
          newTargetTimestamp = mktime(&tm);
          if (newTargetTimestamp == (time_t)-1)
            newTargetTimestamp = 0;
        }

        // 2. REBUILD THE DOC FOR SAVING
        // We must clear and add EVERYTHING back, otherwise missing items get
        // deleted from the file.
        doc.clear();

        doc["ssid"] = ssid;
        doc["password"] = password;
        doc["timeZone"] = timeZone;
        Serial.print("post-timeZone set to: ");
        Serial.println(timeZone);
        doc["clockDuration"] = clockDuration;
        doc["weatherDuration"] = weatherDuration;
        doc["brightness"] = brightness;
        doc["dimBrightness"] = dimBrightness;
        doc["flipDisplay"] = flipDisplay;
        doc["twelveHourToggle"] = twelveHourToggle;
        doc["amPMShow"] = amPMShow;
        doc["showDayOfWeek"] = showDayOfWeek;
        doc["showDate"] = showDate;
        doc["useHomeAssistant"] = useHomeAssistant;
        doc["showHumidity"] = showHumidity;
        doc["colonBlinkEnabled"] = colonBlinkEnabled;
        doc["autoDimmingEnabled"] = autoDimmingEnabled;
        doc["dimmingEnabled"] = dimmingEnabled;
        doc["dimStartHour"] = dimStartHour;
        doc["dimStartMinute"] = dimStartMinute;
        doc["dimEndHour"] = dimEndHour;
        doc["dimEndMinute"] = dimEndMinute;
        doc["showWeatherDescription"] = showWeatherDescription;
        doc["weatherUnits"] = weatherUnits;
        doc["openWeatherApiKey"] = openWeatherApiKey;
        doc["homeAssistantApiKey"] = homeAssistantApiKey;
        doc["customMessage"] = customMessage;

        // Restored Sensor/Network Configs
        doc["homeAssistantURL"] = homeAssistantURL;
        doc["haTempSensor"] = haTempSensor;
        doc["haHumiditySensor"] = haHumiditySensor;
        doc["openWeatherCity"] = openWeatherCity;
        doc["openWeatherCountry"] = openWeatherCountry;
        doc["ntpServer1"] = ntpServer1;
        doc["ntpServer2"] = ntpServer2;

        // Countdown Object
        JsonObject cdObj = doc["countdown"].to<JsonObject>();
        cdObj["enabled"] = newCountdownEnabled;
        cdObj["targetTimestamp"] = newTargetTimestamp;
        cdObj["label"] = cLabel;
        cdObj["isDramaticCountdown"] = newIsDramatic;

        // 3. WRITE TO FILE
        if (LittleFS.exists("/config.json")) {
          LittleFS.rename("/config.json", "/config.bak");
        }
        File f = LittleFS.open("/config.json", "w");
        if (!f || serializeJson(doc, f) == 0) {
          request->send(500, "application/json",
                        "{\"error\":\"Failed to write config file\"}");
          if (f)
            f.close();
          return;
        }
        f.close();

        // 4. VERIFY
        File verify = LittleFS.open("/config.json", "r");
        JsonDocument testDoc;
        DeserializationError verifyErr = deserializeJson(testDoc, verify);
        verify.close();

        if (verifyErr) {
          request->send(500, "application/json",
                        "{\"error\":\"Config corrupted\"}");
          return;
        }

        request->send(200, "application/json", "{\"status\":\"ok\"}");

        request->onDisconnect([]() {
          saveUptime();
          delay(100);
          ESP.restart();
        });
      });

  server.on("/restore", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /restore"));
    if (LittleFS.exists("/config.bak")) {
      File src = LittleFS.open("/config.bak", "r");
      if (!src) {
        Serial.println(F("[WEBSERVER] Failed to open /config.bak"));
        JsonDocument errorDoc;
        errorDoc[F("error")] = "Failed to open backup file.";
        String response;
        serializeJson(errorDoc, response);
        request->send(500, "application/json", response);
        return;
      }
      File dst = LittleFS.open("/config.json", "w");
      if (!dst) {
        src.close();
        Serial.println(
            F("[WEBSERVER] Failed to open /config.json for writing"));
        JsonDocument errorDoc;
        errorDoc[F("error")] = "Failed to open config for writing.";
        String response;
        serializeJson(errorDoc, response);
        request->send(500, "application/json", response);
        return;
      }

      while (src.available()) {
        dst.write(src.read());
      }
      src.close();
      dst.close();

      JsonDocument okDoc;
      okDoc[F("message")] = "✅ Backup restored! Device will now reboot.";
      String response;
      serializeJson(okDoc, response);
      request->send(200, "application/json", response);
      request->onDisconnect([]() {
        Serial.println(F("[WEBSERVER] Rebooting after restore..."));
        saveUptime();
        delay(100); // ensure file is written
        ESP.restart();
      });

    } else {
      Serial.println(F("[WEBSERVER] No backup found"));
      JsonDocument errorDoc;
      errorDoc[F("error")] = "No backup found.";
      String response;
      serializeJson(errorDoc, response);
      request->send(404, "application/json", response);
    }
  });

  server.on("/ap_status", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("[WEBSERVER] Request: /ap_status. isAPMode = "));
    Serial.println(isAPMode);
    String json = "{\"isAP\": ";
    json += (isAPMode) ? "true" : "false";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/set_brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("value", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing value\"}");
      return;
    }

    String sourceHeader = request->header("X-Source");
    bool isFromUI = (sourceHeader == "UI");
    bool isFromHA = !isFromUI;

    int newBrightness = request->getParam("value", true)->value().toInt();

    // Handle OFF request
    if (newBrightness == -1) {
      P.displayShutdown(true);
      P.displayClear();
      displayOff = true;

      Serial.printf("[BRIGHTNESS] Display OFF via %s\n",
                    isFromUI ? "UI" : "HA");

      request->send(200, "application/json",
                    "{\"ok\":true, \"display\":\"off\"}");
      return;
    }

    // Clamp brightness range (0–15)
    newBrightness = constrain(newBrightness, 0, 15);

    if (displayOff) {
      // Wake from OFF
      P.setIntensity(newBrightness);
      advanceDisplayModeSafe();
      P.displayShutdown(false);
      brightness = newBrightness;
      displayOff = false;

      Serial.printf("[BRIGHTNESS] Display woke from OFF via %s → %d\n",
                    isFromUI ? "UI" : "HA", newBrightness);
    } else {
      // Display already ON
      brightness = newBrightness;
      P.setIntensity(brightness);

      Serial.printf("[BRIGHTNESS] Set to %d via %s\n", brightness,
                    isFromUI ? "UI" : "HA");
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_flip", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool flip = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      flip = (v == "1" || v == "true" || v == "on");
    }
    flipDisplay = flip;
    P.setZoneEffect(0, flipDisplay, PA_FLIP_UD);
    P.setZoneEffect(0, flipDisplay, PA_FLIP_LR);
    Serial.printf("[WEBSERVER] Set flipDisplay to %d\n", flipDisplay);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_twelvehour", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool twelveHour = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      twelveHour = (v == "1" || v == "true" || v == "on");
    }
    twelveHourToggle = twelveHour;
    Serial.printf("[WEBSERVER] Set twelveHourToggle to %d\n", twelveHourToggle);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_ampm", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool amPM = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      amPM = (v == "1" || v == "true" || v == "on");
    }
    amPMShow = amPM;
    Serial.printf("[WEBSERVER] Set amPMShow to %d\n", amPMShow);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_dayofweek", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showDay = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      showDay = (v == "1" || v == "true" || v == "on");
    }
    showDayOfWeek = showDay;
    Serial.printf("[WEBSERVER] Set showDayOfWeek to %d\n", showDayOfWeek);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_showdate", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showDateVal = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      showDateVal = (v == "1" || v == "true" || v == "on");
    }
    showDate = showDateVal;
    Serial.printf("[WEBSERVER] Set showDate to %d\n", showDate);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_useHomeAssistant", HTTP_POST,
            [](AsyncWebServerRequest *request) {
              bool useHomeAssistantVal = false;
              if (request->hasParam("value", true)) {
                String v = request->getParam("value", true)->value();
                useHomeAssistantVal = (v == "1" || v == "true" || v == "on");
              }
              useHomeAssistant = useHomeAssistantVal;
              Serial.printf("[WEBSERVER] Set useHomeAssistant to %d\n",
                            useHomeAssistant);
              request->send(200, "application/json", "{\"ok\":true}");
            });

  server.on("/set_humidity", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showHumidityNow = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      showHumidityNow = (v == "1" || v == "true" || v == "on");
    }
    showHumidity = showHumidityNow;
    Serial.printf("[WEBSERVER] Set showHumidity to %d\n", showHumidity);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_colon_blink", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool enableBlink = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      enableBlink = (v == "1" || v == "true" || v == "on");
    }
    colonBlinkEnabled = enableBlink;
    Serial.printf("[WEBSERVER] Set colonBlinkEnabled to %d\n",
                  colonBlinkEnabled);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_weatherdesc", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showDesc = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      showDesc = (v == "1" || v == "true" || v == "on");
    }

    if (showWeatherDescription == true && showDesc == false) {
      Serial.println(F("[WEBSERVER] showWeatherDescription toggled OFF. "
                       "Checking display mode..."));
      if (displayMode == 2) {
        Serial.println(F("[WEBSERVER] Currently in Weather Description mode. "
                         "Forcing mode advance/cleanup."));
        advanceDisplayMode();
      }
    }

    showWeatherDescription = showDesc;
    Serial.printf("[WEBSERVER] Set Show Weather Description to %d\n",
                  showWeatherDescription);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_units", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      if (v == "1" || v == "true" || v == "on") {
        strcpy(weatherUnits, "imperial");
        tempSymbol = ']';
      } else {
        strcpy(weatherUnits, "metric");
        tempSymbol = '[';
      }
      Serial.printf("[WEBSERVER] Set weatherUnits to %s\n", weatherUnits);
      shouldFetchWeatherNow = true;
      request->send(200, "application/json", "{\"ok\":true}");
    } else {
      request->send(400, "application/json",
                    "{\"error\":\"Missing value parameter\"}");
    }
  });

  server.on(
      "/set_countdown_enabled", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool enableCountdownNow = false;
        if (request->hasParam("value", true)) {
          String v = request->getParam("value", true)->value();
          enableCountdownNow = (v == "1" || v == "true" || v == "on");
        }

        if (countdownEnabled == enableCountdownNow) {
          Serial.println(
              F("[WEBSERVER] Countdown enable state unchanged, ignoring."));
          request->send(200, "application/json", "{\"ok\":true}");
          return;
        }

        if (countdownEnabled == true && enableCountdownNow == false) {
          Serial.println(
              F("[WEBSERVER] Countdown toggled OFF. Checking display mode..."));
          if (displayMode == 3) {
            Serial.println(F("[WEBSERVER] Currently in Countdown mode. Forcing "
                             "mode advance/cleanup."));
            advanceDisplayMode();
          }
        }

        countdownEnabled = enableCountdownNow;
        Serial.printf("[WEBSERVER] Set Countdown Enabled to %d\n",
                      countdownEnabled);
        request->send(200, "application/json", "{\"ok\":true}");
      });

  server.on(
      "/set_dramatic_countdown", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool enableDramaticNow = false;
        if (request->hasParam("value", true)) {
          String v = request->getParam("value", true)->value();
          enableDramaticNow = (v == "1" || v == "true" || v == "on");
        }

        // Check if the state has changed
        if (isDramaticCountdown == enableDramaticNow) {
          Serial.println(
              F("[WEBSERVER] Dramatic Countdown state unchanged, ignoring."));
          request->send(200, "application/json", "{\"ok\":true}");
          return;
        }

        // Update the global variable
        isDramaticCountdown = enableDramaticNow;

        // Call saveCountdownConfig with only the existing parameters.
        // It will read the updated global variable 'isDramaticCountdown'.
        saveCountdownConfig(countdownEnabled, countdownTargetTimestamp,
                            countdownLabel);

        Serial.printf("[WEBSERVER] Set Dramatic Countdown to %d\n",
                      isDramaticCountdown);
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // --- Custom Message Endpoint ---
  server.on(
      "/set_custom_message", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("message", true)) {
          String msg = request->getParam("message", true)->value();
          msg.trim();

          String sourceHeader = request->header("X-Source");
          bool isFromUI = (sourceHeader == "UI");
          bool isFromHA = !isFromUI;

          messageDisplaySeconds = 0; // Reset
          if (request->hasParam("seconds", true)) {
            messageDisplaySeconds =
                constrain(request->getParam("seconds", true)->value().toInt(),
                          0, 3600); // 1 hour max
          }

          messageScrollTimes = 0; // Reset
          if (request->hasParam("scrolltimes", true)) {
            messageScrollTimes = constrain(
                request->getParam("scrolltimes", true)->value().toInt(), 0,
                100); // 100 max scrolls
          }

          // --- Local speed variable (does not modify global
          // GENERAL_SCROLL_SPEED) ---
          int localSpeed = GENERAL_SCROLL_SPEED; // Default for UI messages
          if (request->hasParam("speed", true)) {
            localSpeed = constrain(
                request->getParam("speed", true)->value().toInt(), 10, 200);
          }

          // --- CLEAR MESSAGE ---
          if (msg.length() == 0) {
            if (isFromUI) {
              // Web UI clear: The "real" clear, resets everything.
              customMessage[0] = '\0';
              lastPersistentMessage[0] = '\0';
              displayMode = 0;
              messageStartTime = 0;
              currentScrollCount = 0;
              messageDisplaySeconds = 0;
              messageScrollTimes = 0;
              Serial.println(F("[MESSAGE] All messages cleared by UI. "
                               "Returning to normal mode."));
              request->send(200, "text/plain", "CLEARED (UI)");

              // --- SAVE CLEAR STATE ---
              saveCustomMessageToConfig("");
            } else {
              // HA clear: remove only temporary message, reset time/scroll
              // variables.
              customMessage[0] = '\0'; // Clear the currently active message

              // Reset the temporary HA timing/scroll limits.
              messageStartTime = 0;
              currentScrollCount = 0;
              messageDisplaySeconds = 0;
              messageScrollTimes = 0;

              if (strlen(lastPersistentMessage) > 0) {
                // Restore the last persistent message
                strncpy(customMessage, lastPersistentMessage,
                        sizeof(customMessage));
                messageScrollSpeed =
                    GENERAL_SCROLL_SPEED; // Use global speed for persistent

                // Ensure displayMode is set to 6 so the restored persistent
                // message is shown immediately.
                displayMode = 6;
                prevDisplayMode = 0;

                Serial.printf("[MESSAGE] Temporary HA message cleared. "
                              "Restored persistent message: '%s' (speed=%d)\n",
                              customMessage, messageScrollSpeed);
                request->send(200, "text/plain",
                              "CLEARED (HA temporary, persistent restored)");
              } else {
                // No persistent message to restore, return to clock mode.
                displayMode = 0;
                Serial.println(F("[MESSAGE] Temporary HA message cleared. No "
                                 "persistent message to restore."));
                request->send(200, "text/plain",
                              "CLEARED (HA temporary, no persistent)");
              }
            }
            return;
          }

          // --- SANITIZE MESSAGE ---
          msg.toUpperCase();
          String filtered = "";
          for (size_t i = 0; i < msg.length(); i++) {
            char c = msg[i];
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' ||
                c == ':' || c == '!' || c == '\'' || c == '-' || c == '.' ||
                c == ',' || c == '_' || c == '+' || c == '%' || c == '/' ||
                c == '?') {
              filtered += c;
            }
            // Check for degree symbol (UTF-8 0xC2 0xB0)
            else if ((unsigned char)c == 0xC2 && i + 1 < msg.length() &&
                     (unsigned char)msg[i + 1] == 0xB0) {
              filtered += "°"; // add single character
              i++;             // skip next byte
            }
          }

          filtered.toCharArray(customMessage, sizeof(customMessage));

          // --- STORE MESSAGE ---
          if (isFromHA) {
            // --- Only backup if lastPersistentMessage exists ---
            if (strlen(lastPersistentMessage) > 0) {
              Serial.printf("[HA] Will preserve persistent message: '%s'\n",
                            lastPersistentMessage);
            } else {
              Serial.println(F("[HA] No persistent message to preserve. HA "
                               "message is temporary only."));
            }

            // --- Overwrite customMessage with new temporary HA message ---
            filtered.toCharArray(customMessage, sizeof(customMessage));
            messageScrollSpeed = localSpeed;

            Serial.printf(
                "[HA] Temporary HA message received: '%s' (persistent: '%s', "
                "duration: %ds, scrolls: %d, speed: %d)\n",
                customMessage,
                strlen(lastPersistentMessage) ? lastPersistentMessage
                                              : "(none)",
                messageDisplaySeconds, // Added seconds
                messageScrollTimes,    // Added scrolltimes
                localSpeed);           // Added speed
          } else {
            // --- UI-originated message: permanent ---
            filtered.toCharArray(customMessage, sizeof(customMessage));
            strlcpy(lastPersistentMessage, customMessage,
                    sizeof(lastPersistentMessage));
            messageScrollSpeed = GENERAL_SCROLL_SPEED; // Always global for UI

            Serial.printf("[UI] Persistent message stored: %s (speed=%d)\n",
                          customMessage, messageScrollSpeed);

            // --- Persist to config.json immediately ---
            saveCustomMessageToConfig(customMessage);
          }

          // --- Activate display ---
          displayMode = 6;
          prevDisplayMode = 0;
          messageStartTime = millis(); // Start the timer
          currentScrollCount = 0;

          String response = String(isFromHA ? "OK (HA message, speed="
                                            : "OK (UI message, speed=") +
                            String(localSpeed);
          response += String(", duration=") + String(messageDisplaySeconds) +
                      "s, scrolls=" + String(messageScrollTimes) + ")";
          request->send(200, "text/plain", response);
        } else {
          Serial.println(
              F("[MESSAGE] Error: missing 'message' parameter in request."));
          request->send(400, "text/plain", "Missing message parameter");
        }
      });

  server.on("/uptime", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!LittleFS.exists("/uptime.dat")) {
      request->send(200, "text/plain", "No uptime recorded yet.");
      return;
    }

    File f = LittleFS.open("/uptime.dat", "r");
    if (!f) {
      request->send(500, "text/plain", "Error reading uptime file.");
      return;
    }

    String content = f.readString();
    f.close();

    unsigned long seconds = content.toInt();
    String formatted = formatUptime(seconds);
    request->send(200, "text/plain", formatted);
  });

  server.on("/export", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /export"));

    File f;
    if (LittleFS.exists("/config.json")) {
      f = LittleFS.open("/config.json", "r");
      Serial.println(F("[EXPORT] Using /config.json"));
    } else if (LittleFS.exists("/config.bak")) {
      f = LittleFS.open("/config.bak", "r");
      Serial.println(F("[EXPORT] /config.json not found, using /config.bak"));
    } else {
      request->send(404, "application/json", "{\"error\":\"No config found\"}");
      return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
      Serial.print(F("[EXPORT] Error parsing config: "));
      Serial.println(err.f_str());
      request->send(500, "application/json",
                    "{\"error\":\"Failed to parse config\"}");
      return;
    }

    // Only sanitize if NOT in AP mode
    if (!isAPMode) {
      doc["ssid"] = "********";
      doc["password"] = "********";
      doc["openWeatherApiKey"] = "********************************";
      doc["homeAssistantApiKey"] = "********************************";
    }

    doc["mode"] = isAPMode ? "ap" : "sta";

    String jsonOut;
    serializeJsonPretty(doc, jsonOut);

    AsyncWebServerResponse *resp =
        request->beginResponse(200, "application/json", jsonOut);
    resp->addHeader("Content-Disposition",
                    "attachment; filename=\"config.json\"");
    request->send(resp);
  });

  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html style="background: radial-gradient(ellipse at 70% 0%, #2b425a 0%, #171e23 100%); height: 100%;">
      <head>
        <meta charset="UTF-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <style>
          body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
            color: #FFFFFF;
            transition: opacity 0.6s cubic-bezier(.4, 0, .2, 1);
            line-height: 1.5;
            max-width: 300px;
            margin: 3rem auto;
            background: linear-gradient(120deg, rgba(45, 65, 90, 0.72) 0%, rgba(53, 133, 183, 0.38) 100%);
            padding: 1.5rem;
            border-radius: 24px;
            box-shadow: 0 10px 36px 0 rgba(40, 170, 255, 0.11), 0 2px 8px 0 rgba(44, 70, 110, 0.08);
            border: 1.5px solid rgba(180, 230, 255, 0.10);
            text-align: center;
            }
          
          h3 {
            margin-top: 0;
            }

          input::file-selector-button {
            background: linear-gradient(90deg, #3e99bc, #47add4 85%);
            color: white;
            padding: 0.9rem;
            font-size: 1rem;
            font-weight: 600;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            text-align: center;
            transition: background 0.25s, transform 0.15s 
            ease-in-out;
            }
        </style>
      </head>
      <body>
        <h3>Upload config.json</h3>
        <form method="POST" action="/upload" enctype="multipart/form-data">
          <input type="file" name="file" accept=".json" id="fileInput" onchange="this.form.submit()">
        </form>
      </body>
    </html>
    )rawliteral";
    request->send(200, "text/html", html);
  });

  server.on(
      "/upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        String html = R"rawliteral(
      <!DOCTYPE html>
      <html style="background: radial-gradient(ellipse at 70% 0%, #2b425a 0%, #171e23 100%); height: 100%;">
        <head>
          <meta charset="UTF-8" />
          <meta name="viewport" content="width=device-width, initial-scale=1" />
          <title>Upload Successful</title>
          <meta http-equiv="refresh" content="1; url=/" />
          <style>
            body {
              font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
              color: #FFFFFF;
              transition: opacity 0.6s cubic-bezier(.4, 0, .2, 1);
              line-height: 1.5;
              max-width: 300px;
              margin: 3rem auto;
              background: linear-gradient(120deg, rgba(45, 65, 90, 0.72) 0%, rgba(53, 133, 183, 0.38) 100%);
              padding: 1.5rem;
              border-radius: 24px;
              box-shadow: 0 10px 36px 0 rgba(40, 170, 255, 0.11), 0 2px 8px 0 rgba(44, 70, 110, 0.08);
              border: 1.5px solid rgba(180, 230, 255, 0.10);
              text-align: center;
              }
            
            h3 {
              margin-top: 0;
              }

            input::file-selector-button {
              background: linear-gradient(90deg, #3e99bc, #47add4 85%);
              color: white;
              padding: 0.9rem;
              font-size: 1rem;
              font-weight: 600;
              border: none;
              border-radius: 8px;
              cursor: pointer;
              text-align: center;
              transition: background 0.25s, transform 0.15s 
              ease-in-out;
              }
          </style>
        </head>
        <body>
          <h3>File uploaded successfully!</h3>
          <p>Returning to main page...</p>
        </body>
      </html>
    )rawliteral";
        request->send(200, "text/html", html);
        // Restart after short delay to let browser handle redirect
        request->onDisconnect([]() {
          delay(500); // ensure response is sent
          ESP.restart();
        });
      },
      [](AsyncWebServerRequest *request, const String &filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        static File f;
        if (index == 0) {
          f = LittleFS.open("/config.json", "w"); // start new file
        }
        if (f)
          f.write(data, len); // write chunk
        if (final)
          f.close(); // finish file
      });

  server.on("/factory_reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    // If not in AP mode, block and return a 403 response
    if (!isAPMode) {
      request->send(403, "text/plain",
                    "Factory reset only allowed in AP mode.");
      Serial.println(
          F("[RESET] Factory reset attempt blocked (not in AP mode)."));
      return;
    }
    const char *FACTORY_RESET_HTML = R"rawliteral(
      <!DOCTYPE html>
      <html style="background: radial-gradient(ellipse at 70% 0%, #2b425a 0%, #171e23 100%); height: 100%;">
        <head>
          <meta charset="UTF-8" />
          <meta name="viewport" content="width=device-width, initial-scale=1" />
          <title>Resetting Device</title>
          <style>
            body {
              font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
              color: #FFFFFF;
              line-height: 1.5;
              max-width: 300px;
              margin: 3rem auto;
              background: linear-gradient(120deg, rgba(144, 45, 45, 0.72) 0%, rgba(183, 53, 53, 0.38) 100%);
              padding: 1.5rem;
              border-radius: 24px;
              box-shadow: 0 10px 36px 0 rgba(255, 40, 40, 0.11), 0 2px 8px 0 rgba(110, 44, 44, 0.08);
              border: 1.5px solid rgba(255, 180, 180, 0.10);
              text-align: center;
            }
            h3 { margin-top: 0; color: #ff9999; }
            p { font-size: 1.1em; }
            .warning { font-size: 1.2em; font-weight: bold; color: #fff; margin-top: 15px; }
          </style>
        </head>
        <body>
          <h3>Factory Reset Initiated</h3>
          <p>All saved configuration and Wi-Fi credentials are now being erased.</p>
          <hr style="margin: 15px 0; border: 0; border-top: 1px solid rgba(255,255,255,0.2);">
          <p class="warning"><span style="color: yellow;">⚠️</span> ACTION REQUIRED</p>
          <p>
            The device is rebooting and will be temporarily offline for about <strong>45 seconds</strong>.
            <br><br>
            <strong>Your browser will disconnect automatically.</strong>
          </p>
          <p>
            <strong>Next steps:</strong>
            <br>1. Wait about 45 seconds for the reboot to finish.<br>
            2. Reconnect your PC or phone to the Wi-Fi network: <strong>ESPTimeCast</strong>.<br>
            3. Open your browser and go to <strong>192.168.4.1</strong> to continue setup.
          </p>
        </body>
      </html>
    )rawliteral";
    request->send(200, "text/html", FACTORY_RESET_HTML);
    Serial.println(F("[RESET] Factory reset requested, initiating cleanup..."));

    // Use onDisconnect() to ensure the HTTP response is fully sent before the
    // disruptive actions
    request->onDisconnect([]() {
      // Small delay to ensure the response buffer is flushed before file ops
      delay(500);

      // --- Remove configuration and uptime files ---
      const char *filesToRemove[] = {"/config.json", "/uptime.dat",
                                     "/index.html"};
      for (auto &file : filesToRemove) {
        if (LittleFS.exists(file)) {
          if (LittleFS.remove(file)) {
            Serial.printf("[RESET] Deleted %s\n", file);
          } else {
            Serial.printf("[RESET] ERROR deleting %s\n", file);
          }
        } else {
          Serial.printf("[RESET] %s not found, skipping delete.\n", file);
        }
      }

      // --- Clear Wi-Fi credentials ---
      WiFi.disconnect(true, true); // (erase=true, wifioff=true)

      Serial.println(F("[RESET] Factory defaults restored. Rebooting..."));
      delay(500);
      ESP.restart();
    });
  });

  server.onNotFound(handleCaptivePortal);
  server.begin();
  Serial.println(F("[WEBSERVER] Web server started"));
}
