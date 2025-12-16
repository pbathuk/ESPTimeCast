#include "NetworkManager.h"

// -----------------------------------------------------------------------------
// WiFi Setup
// -----------------------------------------------------------------------------
void connectWiFi() {

  Serial.println(F("[WIFI] Connecting to WiFi..."));

  bool credentialsExist = (strlen(ssid) > 0);

  if (!credentialsExist) {
    Serial.println(
        F("[WIFI] No saved credentials. Starting AP mode directly."));
    WiFi.mode(WIFI_AP);
    WiFi.disconnect();
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
                  mode == WIFI_OFF      ? "OFF"
                  : mode == WIFI_STA    ? "STA ONLY"
                  : mode == WIFI_AP     ? "AP ONLY"
                  : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                        : "UNKNOWN");

    Serial.println(F("[WIFI] AP Mode Started"));
    return;
  }

  // If credentials exist, attempt STA connection
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(250);

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  const unsigned long timeout = 30000;
  unsigned long animTimer = 0;
  int animFrame = 0;
  bool animating = true;

  while (animating) {
    unsigned long now = millis();

    if (WiFi.status() == WL_CONNECTED) {
      delay(500);
      Serial.println("[WIFI] Connected: " + WiFi.localIP().toString());
      isAPMode = false;

      WiFiMode_t mode = WiFi.getMode();
      Serial.printf("[WIFI] WiFi mode after STA connection: %s\n",
                    mode == WIFI_OFF      ? "OFF"
                    : mode == WIFI_STA    ? "STA ONLY"
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
      ipDisplayCount = 0; // Reset count for IP display
      P.displayClear();
      P.setCharSpacing(1); // Set spacing for IP scroll
      textEffect_t actualScrollDirection =
          getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
      P.displayScroll(pendingIpToShow.c_str(), PA_CENTER, actualScrollDirection,
                      IP_SCROLL_SPEED);
      // --- END IP Display initiation ---

      animating = false; // Exit the connection loop
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
                    mode == WIFI_OFF      ? "OFF"
                    : mode == WIFI_STA    ? "STA ONLY"
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
      case 0:
        P.print(F("WIFI ©"));
        break;
      case 1:
        P.print(F("WIFI ª"));
        break;
      case 2:
        P.print(F("WIFI «"));
        break;
      }
      animFrame++;
    }
    delay(10);
  }
  if (isAPMode) {
    Serial.println(F("[SETUP] WiFi connection failed. Device is in AP Mode."));
  } else if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("[SETUP] WiFi connected successfully to local network."));
  } else {
    Serial.println(
        F("[SETUP] WiFi state is uncertain after connection attempt."));
  }
}

// -----------------------------------------------------------------------------
// mDNS
// -----------------------------------------------------------------------------
void setupMDNS() {
  String mdnsName = String(hostName);
  mdnsName.toLowerCase();
  bool mdnsStarted = MDNS.begin(mdnsName);
  if (mdnsStarted) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WIFI] mDNS started: http://%s.local\n", mdnsName);
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

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[TIME] ERROR: WiFi not connected!"));
    return;
  }

  Serial.print(F("[TIME] WiFi connected, IP: "));
  Serial.println(WiFi.localIP());

  Serial.print(F("[TIME] Configuring NTP servers: "));
  Serial.print(ntpServer1);
  Serial.print(", ");
  Serial.println(ntpServer2);

  P.setTextAlignment(PA_CENTER);
  P.print(F("NTP SYNC"));

  // Create EspSntpClock with default setup first
  sntpClock = new EspSntpClock();
  sntpClock->setup(ntpServer1);

  // NOW override the SNTP servers directly
  sntp_stop(); // Stop the default SNTP

  // Manually configure with your servers
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, ntpServer1);
  sntp_setservername(1, ntpServer2);
  sntp_init(); // Restart with your servers

  // Step 3: Setup timezone for Europe/London (handles GMT/BST automatically)
  localTz = TimeZone::forZoneInfo(&kZoneEurope_London, &zoneProcessor);

  // Verify the servers are set
  Serial.print(F("[TIME] Configured SNTP Server 0: "));
  Serial.println(sntp_getservername(0) ? sntp_getservername(0) : "(null)");
  Serial.print(F("[TIME] Configured SNTP Server 1: "));
  Serial.println(sntp_getservername(1) ? sntp_getservername(1) : "(null)");

  Serial.println(F("[TIME] Waiting for initial NTP sync..."));

  unsigned long startWait = millis();
  bool synced = false;
  int attempts = 0;

  while (!synced && (millis() - startWait < 15000)) {
    P.print(F("NTP WAIT"));
    delay(1000);
    acetime_t nowSeconds = sntpClock->getNow(); // ← Fast! Returns cached time

    attempts++;
    Serial.print(F("[TIME] Attempt "));
    Serial.print(attempts);
    Serial.print(F(", time: "));
    Serial.print(nowSeconds);

    if (nowSeconds > -946080000 && nowSeconds < 946080000) {
      synced = true;
      Serial.println(F(" ✓ SYNCED!"));
      P.print(F("NTP SYNCED"));

      // Convert UTC to Europe/London time (handles DST)
      auto localTime = ZonedDateTime::forEpochSeconds(nowSeconds, localTz);
      Serial.print(F("[TIME] UTC -> Local: "));
      localTime.printTo(Serial);
      Serial.println();
    } else {
      Serial.println();
    }
  }

  if (!synced) {
    Serial.println(
        F("[TIME] ⚠ Initial sync timeout - will retry in background"));
  }
}
