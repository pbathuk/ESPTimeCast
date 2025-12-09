#include "WeatherManager.h"


String normalizeWeatherDescription(String str) {
  // Serbian Cyrillic → Latin
  str.replace("а", "a");
  str.replace("б", "b");
  str.replace("в", "v");
  str.replace("г", "g");
  str.replace("д", "d");
  str.replace("ђ", "dj");
  str.replace("е", "e");
  str.replace("ё", "e");  // Russian
  str.replace("ж", "z");
  str.replace("з", "z");
  str.replace("и", "i");
  str.replace("й", "j");  // Russian
  str.replace("ј", "j");  // Serbian
  str.replace("к", "k");
  str.replace("л", "l");
  str.replace("љ", "lj");
  str.replace("м", "m");
  str.replace("н", "n");
  str.replace("њ", "nj");
  str.replace("о", "o");
  str.replace("п", "p");
  str.replace("р", "r");
  str.replace("с", "s");
  str.replace("т", "t");
  str.replace("ћ", "c");
  str.replace("у", "u");
  str.replace("ф", "f");
  str.replace("х", "h");
  str.replace("ц", "c");
  str.replace("ч", "c");
  str.replace("џ", "dz");
  str.replace("ш", "s");
  str.replace("щ", "sh");  // Russian
  str.replace("ы", "y");   // Russian
  str.replace("э", "e");   // Russian
  str.replace("ю", "yu");  // Russian
  str.replace("я", "ya");  // Russian

  // Latin diacritics → ASCII
  str.replace("å", "a");
  str.replace("ä", "a");
  str.replace("à", "a");
  str.replace("á", "a");
  str.replace("â", "a");
  str.replace("ã", "a");
  str.replace("ā", "a");
  str.replace("ă", "a");
  str.replace("ą", "a");

  str.replace("æ", "ae");

  str.replace("ç", "c");
  str.replace("č", "c");
  str.replace("ć", "c");

  str.replace("ď", "d");

  str.replace("é", "e");
  str.replace("è", "e");
  str.replace("ê", "e");
  str.replace("ë", "e");
  str.replace("ē", "e");
  str.replace("ė", "e");
  str.replace("ę", "e");

  str.replace("ğ", "g");
  str.replace("ģ", "g");

  str.replace("ĥ", "h");

  str.replace("í", "i");
  str.replace("ì", "i");
  str.replace("î", "i");
  str.replace("ï", "i");
  str.replace("ī", "i");
  str.replace("į", "i");

  str.replace("ĵ", "j");

  str.replace("ķ", "k");

  str.replace("ľ", "l");
  str.replace("ł", "l");

  str.replace("ñ", "n");
  str.replace("ń", "n");
  str.replace("ņ", "n");

  str.replace("ó", "o");
  str.replace("ò", "o");
  str.replace("ô", "o");
  str.replace("ö", "o");
  str.replace("õ", "o");
  str.replace("ø", "o");
  str.replace("ō", "o");
  str.replace("ő", "o");

  str.replace("œ", "oe");

  str.replace("ŕ", "r");

  str.replace("ś", "s");
  str.replace("š", "s");
  str.replace("ș", "s");
  str.replace("ŝ", "s");

  str.replace("ß", "ss");

  str.replace("ť", "t");
  str.replace("ț", "t");

  str.replace("ú", "u");
  str.replace("ù", "u");
  str.replace("û", "u");
  str.replace("ü", "u");
  str.replace("ū", "u");
  str.replace("ů", "u");
  str.replace("ű", "u");

  str.replace("ŵ", "w");

  str.replace("ý", "y");
  str.replace("ÿ", "y");
  str.replace("ŷ", "y");

  str.replace("ž", "z");
  str.replace("ź", "z");
  str.replace("ż", "z");

  str.toUpperCase();

  String result = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if ((c >= 'A' && c <= 'Z') || c == ' ') {
      result += c;
    }
  }
  return result;
}



void fetchWeather() {
  if (millis() - lastWifiConnectTime < 5000) {
    Serial.println(F("[WEATHER] Skipped: Network just reconnected. Letting it stabilize..."));
    return;  // Stop execution if connection is less than 5 seconds old
  }

  Serial.println(F("[WEATHER] Fetching weather data..."));
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WEATHER] Skipped: WiFi not connected"));
    weatherAvailable = false;
    weatherFetched = false;
    return;
  }

  if (useHomeAssistant && (!homeAssistantURL || !homeAssistantApiKey || !haTempSensor)) {
    Serial.println(F("[WEATHER] Home Assistant requested, but not configured"));
    Serial.println(F("[WEATHER] Setting Use Home Assistant flag to off"));
    useHomeAssistant = false;
    // Not returning as going to fall back to OpenAPI
  }

  if (!useHomeAssistant && (!openWeatherApiKey || strlen(openWeatherApiKey) != 32)) {
    Serial.println(F("[WEATHER] Skipped: Invalid API key (must be exactly 32 characters)"));
    weatherAvailable = false;
    weatherFetched = false;
    return;
  }
  if (!useHomeAssistant && (!(strlen(openWeatherCity) > 0 && strlen(openWeatherCountry) > 0))) {
    Serial.println(F("[WEATHER] Skipped: City or Country is empty."));
    weatherAvailable = false;
    return;
  }
  if (useHomeAssistant) {
    // --- 1. TEMPERATURE ---
    String rawTempStr = getHAEntityState(haTempSensor); // Returns e.g. "4.9°" or "unknown°"
    String lowerTemp = rawTempStr;
    lowerTemp.toLowerCase();

    // Check for invalid HA states
    if (rawTempStr.startsWith("Error:") || 
        lowerTemp.indexOf("unknown") >= 0 || 
        lowerTemp.indexOf("unavailable") >= 0) {
      
      Serial.println(F("[HOME ASSISTANT] Temp sensor is unknown/unavailable. Skipping weather display."));
      weatherAvailable = false;
    } 
    else {
      // Valid data found! 
      // .toFloat() parses "4.9" and ignores the trailing "°"
      float tempVal = rawTempStr.toFloat(); 
      
      // Round to nearest int, convert to String, and add symbol back manually
      currentTemp = String((int)round(tempVal)) + "°";
      
      weatherAvailable = true;
      Serial.printf("[HOME ASSISTANT] Temp Rounded: %s (Raw: %s)\n", currentTemp.c_str(), rawTempStr.c_str());
    }

    // --- 2. HUMIDITY ---
    if (useHomeAssistant && weatherAvailable && haHumiditySensor && showHumidity) {
      String rawHumStr = getHAEntityState(haHumiditySensor); 
      String lowerHum = rawHumStr;
      lowerHum.toLowerCase();

      if (rawHumStr.startsWith("Error:") || 
          lowerHum.indexOf("unknown") >= 0 || 
          lowerHum.indexOf("unavailable") >= 0) {
        
        Serial.println(F("[HOME ASSISTANT] Humidity unavailable. Hiding humidity."));
        currentHumidity = -1;
      } 
      else {
        // .toFloat() parses "55.4" and ignores "°"
        currentHumidity = (int)round(rawHumStr.toFloat());
        Serial.printf("[HOME ASSISTANT] Humidity Parsed: %d%% (Raw: %s)\n", currentHumidity, rawHumStr.c_str());
      }
    }

    // --- 3. SUNRISE/SUNSET (Auto Dimming) ---
    if (useHomeAssistant && autoDimmingEnabled) {
      String status = getHASun(sunriseHour, sunriseMinute, sunsetHour, sunsetMinute);
      
      if (status.startsWith("Error:")) {
        Serial.println(F("[HOME ASSISTANT] Home Assistant sun.sun not available"));
      }
      else {
        if (sunriseHour >= 0 && sunsetHour >= 0) {
             Serial.printf("[HOME ASSISTANT] Adjusted Sunrise/Sunset (local): %02d:%02d | %02d:%02d\n",
                          sunriseHour, sunriseMinute, sunsetHour, sunsetMinute);
        } else {
          Serial.println(F("[HOME ASSISTANT] Sunrise/Sunset not found"));
        }
      }
    }
  }
  if (!useHomeAssistant) {
    Serial.println(F("[WEATHER] Connecting to OpenWeatherMap..."));
    String url = buildWeatherURL();
    Serial.print(F("[WEATHER] URL: "));  // Use F() with Serial.print
    Serial.println(url);

    WiFiClientSecure client;  // use secure client for HTTPS
    client.stop();            // ensure previous session closed
    yield();                  // Allow OS to process socket closure
    client.setInsecure();     // no cert validation
    HTTPClient http;          // Create an HTTPClient object
    http.begin(client, url);  // Pass the WiFiClient object and the URL
    http.setTimeout(10000);   // Sets both connection and stream timeout to 10 seconds
    Serial.println(F("[WEATHER] Sending GET request..."));
    int httpCode = http.GET();  // Send the GET request
    if (httpCode != HTTP_CODE_OK){
      Serial.printf("[WEATHER] HTTP GET failed, error code: %d, reason: %s\n",
                    httpCode, http.errorToString(httpCode).c_str());
      weatherAvailable = false;
      weatherFetched = false;
    }
    if (httpCode == HTTP_CODE_OK) {  // Check if HTTP response code is 200 (OK)
    Serial.println(F("[WEATHER] HTTP 200 OK. Reading payload..."));

    String payload = http.getString();
    http.end();
    Serial.println(F("[WEATHER] Response received."));
    Serial.print(F("[WEATHER] Payload: "));  // Use F() with Serial.print
    Serial.println(payload);

    JsonDocument doc;  // Adjust size as needed, use ArduinoJson Assistant
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("[WEATHER] JSON parse error: "));
      Serial.println(error.f_str());
      weatherAvailable = false;
      return;
    }

    if (doc["main"] && doc[F("main")][F("temp")]) {
      float temp = doc[F("main")][F("temp")];
      currentTemp = String((int)round(temp)) + "°";
      Serial.printf("[WEATHER] Temp: %s\n", currentTemp.c_str());
      weatherAvailable = true;
    } else {
      Serial.println(F("[WEATHER] Temperature not found in JSON payload"));
      weatherAvailable = false;
      return;
    }
    
    if (doc["main"] && doc["main"][F("humidity")]) {
      currentHumidity = doc[F("main")][F("humidity")];
      Serial.printf("[WEATHER] Humidity: %d%%\n", currentHumidity);
    } else {
      currentHumidity = -1;
    }

    if (doc[F("weather")] && doc[F("weather")].is<JsonArray>()) {
      JsonObject weatherObj = doc[F("weather")][0];
      if (weatherObj["main"]) {
        mainDesc = weatherObj[F("main")].as<String>();
      }
      if (weatherObj["description"]) {
        detailedDesc = weatherObj[F("description")].as<String>();
      }
    } else {
      Serial.println(F("[WEATHER] Weather description not found in JSON payload"));
    }

    weatherDescription = normalizeWeatherDescription(detailedDesc);
    Serial.printf("[WEATHER] Description used: %s\n", weatherDescription.c_str());

    // -----------------------------------------
    // Sunrise/Sunset for Auto Dimming (local time)
    // -----------------------------------------
    if ( doc[F("sys")]) {
      JsonObject sys = doc[F("sys")];
      if (sys[F("sunrise")] && sys[F("sunset")]) {
        // OWM gives UTC timestamps
        time_t sunriseUtc = sys[F("sunrise")].as<time_t>();
        time_t sunsetUtc = sys[F("sunset")].as<time_t>();

        // Get local timezone offset (in seconds)
        long tzOffset = 0;
        struct tm local_tm;
        time_t now = time(nullptr);
        if (localtime_r(&now, &local_tm)) {
          tzOffset = mktime(&local_tm) - now;
        }

        // Convert UTC → local
        time_t sunriseLocal = sunriseUtc + tzOffset;
        time_t sunsetLocal = sunsetUtc + tzOffset;

        // Break into hour/minute
        struct tm tmSunrise, tmSunset;
        localtime_r(&sunriseLocal, &tmSunrise);
        localtime_r(&sunsetLocal, &tmSunset);

        sunriseHour = tmSunrise.tm_hour;
        sunriseMinute = tmSunrise.tm_min;
        sunsetHour = tmSunset.tm_hour;
        sunsetMinute = tmSunset.tm_min;

        Serial.printf("[WEATHER] Adjusted Sunrise/Sunset (local): %02d:%02d | %02d:%02d\n",
                      sunriseHour, sunriseMinute, sunsetHour, sunsetMinute);
      } else {
        Serial.println(F("[WEATHER] Sunrise/Sunset not found in JSON."));
      }
    } else {
      Serial.println(F("[WEATHER] 'sys' object not found in JSON payload."));
    }
  }

  

    weatherFetched = true;

    // -----------------------------------------
    // Save updated sunrise/sunset to config.json
    // -----------------------------------------
    if (autoDimmingEnabled && sunriseHour >= 0 && sunsetHour >= 0) {
      File configFile = LittleFS.open("/config.json", "r");
      JsonDocument doc;

      if (configFile) {
        DeserializationError error = deserializeJson(doc, configFile);
        configFile.close();

        if (!error) {
          // Check if ANY value has changed
          bool valuesChanged =
            (doc["sunriseHour"].as<int>() != sunriseHour || doc["sunriseMinute"].as<int>() != sunriseMinute || doc["sunsetHour"].as<int>() != sunsetHour || doc["sunsetMinute"].as<int>() != sunsetMinute);

          if (valuesChanged) {  // Only write if a change occurred
            doc["sunriseHour"] = sunriseHour;
            doc["sunriseMinute"] = sunriseMinute;
            doc["sunsetHour"] = sunsetHour;
            doc["sunsetMinute"] = sunsetMinute;

            File f = LittleFS.open("/config.json", "w");
            if (f) {
              serializeJsonPretty(doc, f);
              f.close();
              Serial.println(F("[WEATHER] SAVED NEW sunrise/sunset to config.json (Values changed)"));
            } else {
              Serial.println(F("[WEATHER] Failed to write updated sunrise/sunset to config.json"));
            }
          } else {
            Serial.println(F("[WEATHER] Sunrise/Sunset unchanged, skipping config save."));
          }
          // --- END MODIFIED COMPARISON LOGIC ---

        } else {
          Serial.println(F("[WEATHER] JSON parse error when saving updated sunrise/sunset"));
        }
      }
    }

  } 
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

// -----------------------------------------------------------------------------
// Weather Fetching and API settings
// -----------------------------------------------------------------------------
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

// Function to grab the Entity state
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

String getHASun(int &sunriseHour, int &sunriseMinute, int &sunsetHour, int &sunsetMinute) {
  JsonDocument doc;
  String status = getHAJSON("sun.sun", doc);

  if (status == "200") {
    // 1. Check for attributes
    if (doc[F("attributes")]) {
        JsonObject sunAttributes = doc[F("attributes")];
        
        if (sunAttributes[F("next_rising")] && sunAttributes[F("next_setting")]) {
          const char* riseStr = sunAttributes[F("next_rising")];
          const char* setStr = sunAttributes[F("next_setting")];

          // 2. Helper Lambda: Parse ISO String -> UTC time_t
          auto parseIsoToUtc = [](const char* str) -> time_t {
            int y, M, d, h, m, s;
            // Parse "2025-12-03T08:04:34"
            if (sscanf(str, "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s) == 6) {
              struct tm tm = {0};
              tm.tm_year = y - 1900; 
              tm.tm_mon = M - 1; 
              tm.tm_mday = d;
              tm.tm_hour = h; 
              tm.tm_min = m; 
              tm.tm_sec = s;
              tm.tm_isdst = 0;
              
              // FIX for missing timegm: Swap TZ to UTC, mktime, then swap back
              char *oldTz = getenv("TZ"); // Save current TZ
              setenv("TZ", "UTC0", 1);    // Force UTC
              tzset();
              
              time_t t = mktime(&tm);     // Convert
              
              if (oldTz) setenv("TZ", oldTz, 1); // Restore TZ
              else unsetenv("TZ");
              tzset();
              
              return t;
            }
            return 0;
          };

          // 3. Convert Strings to UTC Epochs
          time_t riseUtc = parseIsoToUtc(riseStr);
          time_t setUtc = parseIsoToUtc(setStr);

          // 4. Convert UTC -> Local Time (Using restored system TZ)
          struct tm riseLocal, setLocal;
          localtime_r(&riseUtc, &riseLocal);
          localtime_r(&setUtc, &setLocal);

          // 5. Update the variables
          sunriseHour = riseLocal.tm_hour;
          sunriseMinute = riseLocal.tm_min;
          sunsetHour = setLocal.tm_hour;
          sunsetMinute = setLocal.tm_min;

          Serial.printf("[HOME ASSISTANT] Adjusted Sunrise/Sunset (local): %02d:%02d | %02d:%02d\n",
                        sunriseHour, sunriseMinute, sunsetHour, sunsetMinute);
                        
          return "OK"; // Successfully parsed
        }
    }
    
    // Fallback: If attributes missing, try state
    if (doc[F("state")]) {
       String s = doc[F("state")].as<String>();
       Serial.printf("[HOME ASSISTANT] Sun State: %s\n", s.c_str());
       return s; 
    }
  }
  
  Serial.println(F("[HOME ASSISTANT] No valid sun object found"));
  return "Error: No Sun"; 
}

