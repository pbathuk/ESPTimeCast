#include "Globals.h"
#include <AceTime.h>

// --- Object Instantiation ---
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
AsyncWebServer server(80);
Audio audio;
DNSServer dnsServer;
WiFiClient client;

EspSntpClock *sntpClock = nullptr;
BasicZoneProcessor zoneProcessor;
TimeZone localTz;

// --- Variable Initialization ---
char ssid[32] = "";
char password[64] = "";
char homeAssistantURL[254] = "";
char homeAssistantApiKey[254] = "";
char haTempSensor[128] = "";
char haHumiditySensor[128] = "";
char openWeatherApiKey[64] = "";
char openWeatherCity[64] = "";
char openWeatherCountry[64] = "";
char weatherUnits[12] = "metric";
char timeZone[64] = "";
unsigned long lastWifiConnectTime = 0;
String mainDesc = "";
String detailedDesc = "";
const char *hostName = "ESPTimeCast";

// Timing and display settings
int brightness = 7;
unsigned long clockDuration = 10000;
unsigned long weatherDuration = 5000;
bool displayOff = false;

bool flipDisplay = false;
bool twelveHourToggle = false;
bool amPMShow = false;
bool showDayOfWeek = true;
bool useHomeAssistant = false;
bool showHumidity = false;
bool colonBlinkEnabled = true;
char ntpServer1[256] =
    "pool.ntp.org"; // Change these before flashing if you want local only
char ntpServer2[256] =
    "time.nist.gov"; // Change these before flashing if you want local only
char customMessage[121] = "";
char lastPersistentMessage[128] = "";
int messageDisplaySeconds;
int messageScrollTimes;
unsigned long messageStartTime = 0;
int currentScrollCount = 0;
int currentDisplayCycleCount = 0;
bool showDate = false;
bool isAlarmPlaying = false;

// Dimming
bool dimmingEnabled = false;
bool displayOffByDimming = false;
bool displayOffByBrightness = false;
int dimStartHour = 18; // 6pm default
int dimStartMinute = 0;
int dimEndHour = 8; // 8am default
int dimEndMinute = 0;
int dimBrightness = 2;           // Dimming level (0-15)
bool autoDimmingEnabled = false; // true if using sunrise/sunset
int sunriseHour = 6;
int sunriseMinute = 0;
int sunsetHour = 18;
int sunsetMinute = 0;

// Wifi constants
const char *DEFAULT_AP_PASSWORD = "12345678";
const char *AP_SSID = "ESPTimeCast";

// Countdown Globals
bool countdownEnabled = false;
time_t countdownTargetTimestamp = 0; // Unix timestamp
char countdownLabel[64] = "";        // Label for the countdown
bool isDramaticCountdown = true;     // Default to the dramatic countdown mode

// Runtime Uptime Tracker
unsigned long bootMillis = 0;                     // Stores millis() at boot
unsigned long lastUptimeLog = 0;                  // Timer for hourly logging
const unsigned long uptimeLogInterval = 600000UL; // 10 minutes in ms
unsigned long totalUptimeSeconds =
    0; // Persistent accumulated uptime in seconds

// --- Global Scroll Speed Settings ---
const int GENERAL_SCROLL_SPEED =
    85; // Default: Adjust this for Weather Description and Countdown Label
        // (e.g., 50 for faster, 200 for slower)
const int IP_SCROLL_SPEED = 115; // Default: Adjust this for the IP Address
                                 // display (slower for readability)
int messageScrollSpeed = 85;     // default fallback

// State management
bool weatherCycleStarted = false;
const byte DNS_PORT = 53;

String currentTemp = "";
String weatherDescription = "";
bool showWeatherDescription = false;
bool weatherAvailable = false;
bool weatherFetched = false;
bool weatherFetchInitiated = false;
bool isAPMode = false;
char tempSymbol = '[';
bool shouldFetchWeatherNow = false;

unsigned long lastSwitch = 0;
unsigned long lastColonBlink = 0;
int displayMode =
    0; // 0: Clock, 1: Weather, 2: Weather Description, 3: Countdown
int prevDisplayMode = -1;
bool clockScrollDone = false;
int currentHumidity = -1;
bool ntpSyncSuccessful = false;

unsigned long ntpStartTime = 0;
const int ntpTimeout = 30000; // 30 seconds
const int maxNtpRetries = 30;
int ntpRetryCount = 0;
unsigned long lastNtpStatusPrintTime = 0;
const unsigned long ntpStatusPrintInterval =
    1000; // Print status every 1 seconds (adjust as needed)

// Non-blocking IP display globals
bool showingIp = false;
int ipDisplayCount = 0;
const int ipDisplayMax = 1; // As per working copy for how long IP shows
String pendingIpToShow = "";

int scrollCount = 0;
const int scrollDisplayMax = 1;
bool dramaticLock = false;

unsigned long hourGlassStartMillis = 0;
const int hourGlassRepeats = 12; // Number of hourglass flips * number of times to flip fully (3 * 4 = 12)
const long hourGlassFlipInterval = 350; // 350ms between flips
int hourGlassFlipCount = 0;
unsigned long segmentStartMillis = 0;

// Countdown display state - NEW
bool countdownScrolling = false;
unsigned long countdownScrollEndTime = 0;
unsigned long countdownStaticStartTime = 0; // For last-day static display


unsigned long customMessageEndTime = 0;
unsigned long customerMessageDuration = 0;

// --- NEW GLOBAL VARIABLES FOR IMMEDIATE COUNTDOWN FINISH ---
bool countdownFinished =
    false; // Tracks if the countdown has permanently finished
bool countdownShowFinishedMessage =
    false; // Flag to indicate "TIMES UP" message is active
unsigned long countdownFinishedMessageStartTime =
    0; // Timer for the 10-second message duration
unsigned long lastFlashToggleTime = 0; // For controlling the flashing speed
bool currentInvertState =
    false; // Current state of display inversion for flashing
bool hourglassPlayed = false;

// Weather Description Mode handling
unsigned long descStartTime = 0; // For static description
bool descScrolling = false;
const unsigned long descriptionDuration = 3000; // 3s for short text
unsigned long descScrollEndTime =
    0; // for post-scroll delay (re-used for scroll timing)
const unsigned long descriptionScrollPause = 300; // 300ms pause after scroll

// Button Objects
Button2 buttonLeft;
Button2 buttonMiddle;
Button2 buttonRight;
