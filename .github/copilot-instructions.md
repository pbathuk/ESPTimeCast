<!-- Copilot / AI agent guidance for contributors to ESPTimeCast -->
# ESPTimeCast — Copilot Instructions

Short, actionable notes to help an AI agent be productive in this repo.

- **Big picture:** This is an Arduino/PlatformIO project for an ESP32-S3 LED matrix device that displays clock/weather and exposes a web UI for configuration. Core subsystems:
  - Device runtime: `src/main.cpp` (setup/loop, NTP, display loop, dimming, OTA)
  - Config + storage: `src/ConfigManager.*` (reads/writes `/config.json` on LittleFS)
  - Networking: `src/NetworkManager.*` (WiFi connect, mDNS) and `src/WebHandler.*` (ESPAsyncWebServer handlers)
  - Weather: `src/WeatherManager.*` (fetching/parsing OpenWeather/OpenWeatherCity)
  - Display/audio: `mfactoryfont.h`, `MD_Parola` usage in `main.cpp`, and audio I2S in `main.cpp`

- **Where important patterns live:**
  - Globals: `src/Globals.h` defines many project-wide vars — code relies on mutable globals throughout the repo.
  - Web UI: `src/index_html.h` holds the single-page UI as a `PROGMEM R"rawliteral("...)")` string. At runtime `ensureHtmlFileExists()` copies it into LittleFS as `/index.html` if missing.
  - Config flow: `WebHandler.cpp` uses `deserializeJson`/`serializeJson` and masks sensitive fields with helper functions like `getSafeSsid()` before returning `config.json` to UI.

- **Build / run / debug commands (PlatformIO):**
  - Build: `pio run -e esp32-s3-n16r8` (env name from `platformio.ini`)
  - Upload over USB: `pio run -e esp32-s3-n16r8 -t upload`
  - Monitor serial: `pio device monitor -e esp32-s3-n16r8 -b 115200`
  - OTA: enable `upload_protocol = espota` in `platformio.ini` and set `upload_port` / `upload_flags` as needed; ensure `secrets.ini` contains `ota_password` and `OTA_PASSWORD` is defined.

- **Secrets and configuration:**
  - Sensitive values live in `secrets.ini` (not tracked). Use `secrets.ini_example` as the template. Typical keys: `ota_password`, WiFi credentials used by the `ConfigManager`/`WebHandler`.
  - Runtime config file on the device: LittleFS `/config.json`. The web UI POSTs JSON to `/save`, and `WebHandler.cpp` rebuilds and writes complete JSON to avoid accidental deletions.

- **Project-specific conventions an AI must follow:**
  - Don’t invent new global state — prefer updating existing globals in `Globals.h` to match code style.
  - When adding web endpoints, use `server.on(...)` patterns from `WebHandler.cpp` and follow the sanitization and masking helpers (`getSafeSsid()`, etc.).
  - HTML/UI is embedded in `index_html.h` and expected to be present in LittleFS. Changes to UI should update this file and keep the `ensureHtmlFileExists()` semantics.
  - Use LittleFS APIs exactly as current code (open, rename to `.bak`, verify by deserializing after write) to preserve compatibility.

- **Key function names & examples to reference when coding:**
  - `loadConfig()` — called in `main.cpp` during setup to populate globals
  - `setupWebServer()` — registers routes (see `src/WebHandler.cpp` for patterns)
  - `fetchWeather()` — initiates weather fetch cycle in `WeatherManager.*`
  - `ensureHtmlFileExists()` — copies `index_html` PROGMEM into LittleFS as `/index.html`

- **External dependencies of note (from `platformio.ini`):**
  - `ArduinoJson` (7.x) — JSON handling
  - `ESPAsyncWebServer` + `AsyncTCP` — non-blocking HTTP
  - `MD_Parola` + `MD_MAX72XX` — LED matrix display
  - `Button2` — button handling

- **When editing code, prefer small, targeted changes:**
  - Keep changes in a single subsystem file when possible.
  - Preserve existing style: header/source pairs, no new global variables unless absolutely necessary.

- **Testing / verification steps for changes:**
  - Build locally with `pio run -e esp32-s3-n16r8` to check compilation.
  - For web/UI changes: after flashing, open device IP (or AP mode IP) and verify `/config.json` and `/` serve expected content.
  - For config changes: write via `/save` route and ensure LittleFS `/config.json` correctness by calling `/config.json` and checking masked fields.

If anything above is unclear or you'd like me to expand an example (e.g., add a small sample change implementing a new route or refactor `ConfigManager`), tell me which area to iterate on.
