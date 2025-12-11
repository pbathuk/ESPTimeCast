#ifndef GLOBALS_H
#define GLOBALS_H

#include <MD_Parola.h>
#include <Audio.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// --- Constants ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN 15 //12    //D5
#define CS_PIN 16 //10    // D7
#define DATA_PIN 17 //11  //D8

// Audio Pins
#define I2S_LRC       5
#define I2S_BCLK      6
#define I2S_DOUT      7

// --- External Objects ---
extern MD_Parola P;
extern AsyncWebServer server;
extern Audio audio;
extern DNSServer dnsServer;
extern WiFiClient client;

// --- Global Configuration Variables (Declarations) ---
extern char ssid[32];
extern char password[64];
extern char openWeatherApiKey[64];
extern char openWeatherCity[64];
extern char openWeatherCountry[64];
extern char timeZone[64];
extern char homeAssistantURL[254];
extern char homeAssistantApiKey[254];
extern char haTempSensor[128];
extern char haHumiditySensor[128];
extern char weatherUnits[12];
extern char language[8];
extern unsigned long lastWifiConnectTime;
extern String mainDesc;
extern String detailedDesc;


extern int brightness;
extern unsigned long clockDuration;
extern unsigned long weatherDuration;
extern bool displayOf;

extern bool flipDisplay;
extern bool twelveHourToggle;
extern bool amPMShow;
extern bool showDayOfWeek;
extern bool useHomeAssistant;
extern bool showHumidity;
extern bool colonBlinkEnabled;
extern char ntpServer1[64];
extern char ntpServer2[256];
extern char customMessage[121];
extern char lastPersistentMessage[128];
extern int messageDisplaySeconds;
extern int messageScrollTimes;
extern unsigned long messageStartTime;
extern int currentScrollCount;
extern int currentDisplayCycleCount;
extern bool showDate;
extern bool isAlarmPlaying;


// Dimming
extern bool dimmingEnabled;
extern bool displayOff;
extern bool displayOffByDimming;
extern bool displayOffByBrightness;
extern int dimStartHour;
extern int dimStartMinute;
extern int dimEndHour;
extern int dimEndMinute;
extern int dimBrightness;
extern bool autoDimmingEnabled;
extern int sunriseHour;
extern int sunriseMinute;
extern int sunsetHour;
extern int sunsetMinute;

// Wifi constants
extern const char *DEFAULT_AP_PASSWORD;
extern const char *AP_SSID;

//Countdown Globals
extern bool countdownEnabled;
extern time_t countdownTargetTimestamp;
extern char countdownLabel[64];
extern bool isDramaticCountdown;

// Runtime Uptime Tracker
extern unsigned long bootMillis;
extern unsigned long lastUptimeLog;
extern const unsigned long uptimeLogInterval;
extern unsigned long totalUptimeSeconds;


// --- Global Scroll Speed Settings ---
extern const int GENERAL_SCROLL_SPEED;
extern const int IP_SCROLL_SPEED;
extern int messageScrollSpeed;

// --- Nightscout setting ---
extern const unsigned int NIGHTSCOUT_IDLE_THRESHOLD_MIN; 

// State management
extern bool weatherCycleStarted;
extern const byte DNS_PORT;

extern String currentTemp;
extern String weatherDescription;
extern bool showWeatherDescription;
extern bool weatherAvailable;
extern bool weatherFetched;
extern bool weatherFetchInitiated;
extern bool isAPMode;
extern char tempSymbol;
extern bool shouldFetchWeatherNow;
extern int displayMode;
extern unsigned long lastSwitch;
extern unsigned long lastColonBlink;
extern int prevDisplayMode;
extern bool clockScrollDone;
extern int currentHumidity;
extern bool ntpSyncSuccessful;

// NTP Synchronization State Machine
enum NtpState {
  NTP_IDLE,
  NTP_SYNCING,
  NTP_SUCCESS,
  NTP_FAILED
};
extern NtpState ntpState;


extern unsigned long ntpStartTime;
extern const int ntpTimeout;
extern const int maxNtpRetries;
extern int ntpRetryCount;
extern unsigned long lastNtpStatusPrintTime;
extern const unsigned long ntpStatusPrintInterval;

extern bool showingIp;
extern int ipDisplayCount;
extern const int ipDisplayMax;
extern String pendingIpToShow;

extern bool countdownScrolling;
extern unsigned long countdownScrollEndTime;
extern unsigned long countdownStaticStartTime;

extern bool countdownFinished;
extern bool countdownShowFinishedMessage;
extern unsigned long countdownFinishedMessageStartTime;
extern unsigned long lastFlashToggleTime;
extern bool currentInvertState;
extern bool hourglassPlayed;

extern unsigned long descStartTime;
extern bool descScrolling;
extern const unsigned long descriptionDuration;
extern unsigned long descScrollEndTime;
extern const unsigned long descriptionScrollPause;

#endif