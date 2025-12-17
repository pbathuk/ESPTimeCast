#ifndef GLOBALS_H
#define GLOBALS_H

#include <AceTime.h>
#include <AceTimeClock.h>
#include <Audio.h>
#include <Button2.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <SPI.h>

using namespace ace_time;
using ace_time::acetime_t;
using ace_time::BasicZoneProcessor;
using ace_time::TimeZone;
using ace_time::ZonedDateTime;
using ace_time::clock::EspSntpClock;
using ace_time::zonedb2025::kZoneEurope_London;

// --- Time Objects ---
extern BasicZoneProcessor zoneProcessor;
extern EspSntpClock *sntpClock;
extern TimeZone localTz;

// --- Constants ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN 15
#define CS_PIN 16
#define DATA_PIN 17

// Audio Pins
#define I2S_LRC 5
#define I2S_BCLK 6
#define I2S_DOUT 7

// Button Pins
#define BUTTON_PIN_LEFT 12
#define BUTTON_PIN_MID 13
#define BUTTON_PIN_RIGHT 14

extern Button2 buttonLeft;
extern Button2 buttonMiddle;
extern Button2 buttonRight;

#define BUTTON_LONGCLICK_MS 1000

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
extern unsigned long lastWifiConnectTime;
extern String mainDesc;
extern String detailedDesc;
extern const char *hostName;

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
extern char ntpServer1[256];
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

// Countdown Globals
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

extern int scrollCount;
extern const int scrollDisplayMax;
extern bool dramaticLock;

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

extern unsigned long hourGlassStartMillis;
extern const int hourGlassRepeats;
extern const long hourGlassFlipInterval;
extern int hourGlassFlipCount;

extern unsigned long segmentStartMillis;

extern unsigned long customMessageEndTime;
extern unsigned long customerMessageDuration;

extern int currentDisplayCycleCount; 
#endif