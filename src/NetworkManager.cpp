#include "NetworkManager.h"


// -----------------------------------------------------------------------------
// WiFi Setup
// -----------------------------------------------------------------------------
void connectWiFi() {
  
  Serial.println(F("[WIFI] Connecting to WiFi..."));

  bool credentialsExist = (strlen(ssid) > 0);

  if (!credentialsExist) {
    Serial.println(F("[WIFI] No saved credentials. Starting AP mode directly."));
    WiFi.mode(WIFI_AP);
    WiFi.disconnect(true);
    delay(100);

    if (strlen(DEFAULT_AP_PASSWORD) < 8) {
      WiFi.softAP(AP_SSID);
      Serial.println(F("[WIFI] AP Mode started (no password, too short)."));
    } else {
      WiFi.softAP(AP_SSID, DEFAULT_AP_PASSWORD);
      Serial.println(F("[WIFI] AP Mode started."));
    }

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    Serial.print(F("[WIFI] AP IP address: "));
    Serial.println(WiFi.softAPIP());
    isAPMode = true;

    WiFiMode_t mode = WiFi.getMode();
    Serial.printf("[WIFI] WiFi mode after setting AP: %s\n",
                  mode == WIFI_OFF ? "OFF" : mode == WIFI_STA    ? "STA ONLY"
                                           : mode == WIFI_AP     ? "AP ONLY"
                                           : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                                 : "UNKNOWN");

    Serial.println(F("[WIFI] AP Mode Started"));
    return;
  }

  // If credentials exist, attempt STA connection
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  const unsigned long timeout = 30000;
  unsigned long animTimer = 0;
  int animFrame = 0;
  bool animating = true;

  while (animating) {
    unsigned long now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WIFI] Connected: " + WiFi.localIP().toString());
      isAPMode = false;

      WiFiMode_t mode = WiFi.getMode();
      Serial.printf("[WIFI] WiFi mode after STA connection: %s\n",
                    mode == WIFI_OFF ? "OFF" : mode == WIFI_STA    ? "STA ONLY"
                                             : mode == WIFI_AP     ? "AP ONLY"
                                             : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                                   : "UNKNOWN");

      // --- IP Display initiation ---
      pendingIpToShow = WiFi.localIP().toString();

      // Replace all dots with your custom font code 184
      for (int i = 0; i < pendingIpToShow.length(); i++) {
        if (pendingIpToShow[i] == '.') {
          pendingIpToShow[i] = 184;
        }
      }

      showingIp = true;
      ipDisplayCount = 0;  // Reset count for IP display
      P.displayClear();
      P.setCharSpacing(1);  // Set spacing for IP scroll
      textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
      P.displayScroll(pendingIpToShow.c_str(), PA_CENTER, actualScrollDirection, IP_SCROLL_SPEED);
      // --- END IP Display initiation ---

      animating = false;  // Exit the connection loop
      break;
    } else if (now - startAttemptTime >= timeout) {
      Serial.println(F("[WIFI] Failed. Starting AP mode..."));
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID, DEFAULT_AP_PASSWORD);
      Serial.print(F("[WIFI] AP IP address: "));
      Serial.println(WiFi.softAPIP());
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
      isAPMode = true;

      auto mode = WiFi.getMode();
      Serial.printf("[WIFI] WiFi mode after STA failure and setting AP: %s\n",
                    mode == WIFI_OFF ? "OFF" : mode == WIFI_STA    ? "STA ONLY"
                                             : mode == WIFI_AP     ? "AP ONLY"
                                             : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                                   : "UNKNOWN");

      animating = false;
      Serial.println(F("[WIFI] AP Mode Started"));
      break;
    }
    if (now - animTimer > 750) {
      animTimer = now;
      P.setTextAlignment(PA_CENTER);
      switch (animFrame % 3) {
        case 0: P.print(F("# ©")); break;
        case 1: P.print(F("# ª")); break;
        case 2: P.print(F("# «")); break;
      }
      animFrame++;
    }
    delay(1);
  }
}

// -----------------------------------------------------------------------------
// mDNS
// -----------------------------------------------------------------------------
void setupMDNS() {
  const char *hostName = "esptimecast";  // your device name
  bool mdnsStarted = false;
  mdnsStarted = MDNS.begin(hostName);

  if (mdnsStarted) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WIFI] mDNS started: http://%s.local\n", hostName);
  } else {
    Serial.println("[WIFI] mDNS failed to start");
  }
}

// -----------------------------------------------------------------------------
// Time / NTP Functions
// -----------------------------------------------------------------------------
void setupTime() {
  if (!isAPMode) {
    Serial.println(F("[TIME] Starting NTP sync"));
  }

  configTime(0, 0, ntpServer1, ntpServer2);

  // Set the Time Zone
  setenv("TZ", ianaToPosix(timeZone), 1);
  tzset();

  // Initialize state flags to begin synchronization tracking
  ntpState = NTP_SYNCING;
  ntpStartTime = millis();
  ntpRetryCount = 0;
  ntpSyncSuccessful = false;
}

