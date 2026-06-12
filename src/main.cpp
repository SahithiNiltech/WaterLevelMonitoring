/*
 * Water Level Monitor — ESP32-WROOM-32E-N4
 *
 * Sensor  : Waterproof ultrasonic  (TRIG → IO32, ECHO → IO34)
 * Backend : Flask REST API (HTTP pull OTA, email alerts)
 * IDE     : PlatformIO / Arduino framework
 *
 * Wake cycle (every 1 hour via deep sleep):
 *   1. Read sensor (no WiFi)
 *   2. Calculate water level (cm) and percentage
 *   3. Connect WiFi (retry up to 3x)
 *   4. Check for firmware updates (HTTP pull OTA)
 *   5. POST sensor data to backend (backend sends email alerts)
 *   6. Decide next sleep interval (monitoring or hourly)
 *   7. Deep sleep
 */

// Configuration
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>

// ─────────────────────────────────────────────────────────────
//  RTC memory — survives deep sleep
// ─────────────────────────────────────────────────────────────
RTC_DATA_ATTR static struct {
    float    buf[ROLLING_AVG_CYCLES]; // ring buffer of recent distance readings
    uint8_t  idx;                     // next write position
    uint8_t  count;                   // how many slots are filled (0 → ROLLING_AVG_CYCLES)
    uint16_t cyclesSinceLastSend;     // wake cycles since last successful HTTP POST
    uint8_t  alertState;              // bitmask for device-side deduplication
    uint8_t  monitoringCycles;        // number of short-interval cycles completed in current block
    uint8_t  isMonitoring;            // 1 = short-interval monitoring mode, 0 = long sleep mode
    float    lastLevelPct;            // last observed level used for increase detection
    uint8_t  increasedInBlock;        // flag set if any increase seen during current MONITOR_CYCLES block
} rtcData;

/**
 * Pushes a new distance reading into the RTC ring buffer and
 * returns the rolling average across all filled slots.
 */
static float rollingAverage(float newReading) {
    rtcData.buf[rtcData.idx] = newReading;
    rtcData.idx = (rtcData.idx + 1) % ROLLING_AVG_CYCLES;
    if (rtcData.count < ROLLING_AVG_CYCLES) rtcData.count++;

    float sum = 0.0f;
    for (uint8_t i = 0; i < rtcData.count; i++) sum += rtcData.buf[i];
    return sum / rtcData.count;
}

// ─────────────────────────────────────────────────────────────
//  Sensor helpers
// ─────────────────────────────────────────────────────────────

/**
 * Triggers the ultrasonic sensor once and returns the measured
 * distance in centimetres, or -1.0 on timeout / out-of-range.
 */
static float singleMeasurementCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
    if (duration == 0) {
        return -1.0f;  // timeout
    }

    float distanceCm = duration * 0.0343f / 2.0f;
    if (distanceCm < 2.0f || distanceCm > 400.0f) {
        return -1.0f;  // out of sensor spec
    }
    return distanceCm;
}

/**
 * Averages SENSOR_NUM_SAMPLES valid readings.
 * Returns -1.0 if fewer than half the samples are valid.
 */
static float measureDistanceCm() {
    float sum   = 0.0f;
    int   valid = 0;

    for (int i = 0; i < SENSOR_NUM_SAMPLES; i++) {
        float d = singleMeasurementCm();
        if (d > 0.0f) {
            sum += d;
            valid++;
        }
        delay(30);  // small gap between pulses
    }

    if (valid < (SENSOR_NUM_SAMPLES / 2 + 1)) {
        return -1.0f;  // too many bad readings
    }
    return sum / valid;
}

// ─────────────────────────────────────────────────────────────
//  Level calculation
// ─────────────────────────────────────────────────────────────

/**
 * Converts a raw sensor distance to a water level percentage.
 *
 *  TANK_EMPTY_CM → 0 %   (sensor reads full tank depth → no water)
 *  TANK_FULL_CM  → 100 % (sensor reads minimum distance → full)
 *
 * Result is clamped to [0, 100].
 */
static float distanceToPct(float distanceCm) {
    float usable = TANK_EMPTY_CM - TANK_FULL_CM;          // cm range representing 0–100 %
    float waterCm = TANK_EMPTY_CM - distanceCm;           // how much water is present
    float pct = (waterCm / usable) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

// ─────────────────────────────────────────────────────────────
//  WiFi helper
// ─────────────────────────────────────────────────────────────
static bool connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timed out");
            return false;
        }
        delay(200);
    }
    Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// ─────────────────────────────────────────────────────────────
//  HTTP POST: Send sensor data to backend
// ─────────────────────────────────────────────────────────────
static bool postSensorData(float waterLevelPct, float waterHeightCm, float batteryV, float batteryPct) {
    HTTPClient http;

    String url = String(BACKEND_URL) + "/api/device/data?api_key=" + String(API_KEY);

    WiFiClientSecure client;
    client.setInsecure();  // For testing; use proper CA cert in production

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    // Create JSON payload
    StaticJsonDocument<256> doc;
    doc["device_id"] = DEVICE_ID;
    doc["water_level_pct"] = waterLevelPct;
    doc["water_height_cm"] = waterHeightCm;
    doc["battery_v"] = batteryV;
    doc["battery_pct"] = (int)batteryPct;

    String payload;
    serializeJson(doc, payload);

    Serial.printf("[HTTP] POST /api/device/data\n");

    int httpCode = http.POST(payload);

    if (httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_OK) {
        Serial.printf("[HTTP] 201 Created\n");
        http.end();
        return true;
    } else {
        Serial.printf("[HTTP] POST failed, code: %d\n", httpCode);
        http.end();
        return false;
    }
}

// Forward declarations
static bool downloadAndUpdateFirmware(const String& downloadUrl, const char* expectedMd5);
static bool checkFirmwareUpdate();
static void postFirmwareApplied(const char* fromVersion, const char* toVersion, bool success);

// ─────────────────────────────────────────────────────────────
//  HTTP GET: Check for firmware updates
// ─────────────────────────────────────────────────────────────
static bool checkFirmwareUpdate() {
    HTTPClient http;

    String url = String(BACKEND_URL) + "/api/firmware/check?current_version=" + String(FIRMWARE_VERSION) + "&api_key=" + String(API_KEY);

    WiFiClientSecure client;
    client.setInsecure();  // For testing

    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    Serial.printf("[FW] Checking for updates...\n");

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();

        // Parse JSON
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, response);

        if (error) {
            Serial.printf("[JSON] Parse error: %s\n", error.c_str());
            http.end();
            return false;
        }

        bool updateAvailable = doc["update_available"] | false;

        if (updateAvailable) {
            String downloadUrl = doc["download_url"] | "";
            const char* md5 = doc["md5"] | "";
            const char* newVersion = doc["current"] | FIRMWARE_VERSION;

            Serial.printf("[FW] Update available: v%s\n", newVersion);
            Serial.printf("[FW] Downloading from: %s\n", downloadUrl.c_str());

            // Download and apply firmware update
            if (downloadAndUpdateFirmware(downloadUrl, md5)) {
                postFirmwareApplied(FIRMWARE_VERSION, newVersion, true);
                Serial.println("[FW] Update successful! Rebooting...");
                delay(3000);
                ESP.restart();
            } else {
                postFirmwareApplied(FIRMWARE_VERSION, newVersion, false);
            }
        } else {
            Serial.println("[FW] No updates available");
        }

        http.end();
        return true;
    } else {
        Serial.printf("[FW] Check failed, code: %d\n", httpCode);
        http.end();
        return false;
    }
}

// ─────────────────────────────────────────────────────────────
//  HTTP POST: Notify backend of firmware applied
// ─────────────────────────────────────────────────────────────
static void postFirmwareApplied(const char* fromVersion, const char* toVersion, bool success) {
    HTTPClient http;

    String url = String(BACKEND_URL) + "/api/firmware/applied?api_key=" + String(API_KEY);

    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    StaticJsonDocument<256> doc;
    doc["device_id"] = DEVICE_ID;
    doc["from_version"] = fromVersion;
    doc["to_version"] = toVersion;
    doc["status"] = success ? "applied" : "failed";

    String payload;
    serializeJson(doc, payload);

    Serial.printf("[FW] Posting firmware applied notification\n");
    http.POST(payload);
    http.end();
}

// ─────────────────────────────────────────────────────────────
//  Download and apply firmware via HTTP pull OTA
// ─────────────────────────────────────────────────────────────
static bool downloadAndUpdateFirmware(const String& downloadUrl, const char* expectedMd5) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, downloadUrl);
    http.setTimeout(HTTP_TIMEOUT_MS);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[FW] Download failed, code: %d\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("[FW] Unknown content length");
        http.end();
        return false;
    }

    Serial.printf("[FW] Downloading: %d bytes\n", contentLength);

    // Start OTA update
    if (!Update.begin(contentLength)) {
        Serial.println("[FW] Update.begin failed");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buffer[1024];

    while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(buffer, min(available, sizeof(buffer)));
            if (bytesRead > 0) {
                Update.write(buffer, bytesRead);
                written += bytesRead;
                Serial.printf(".");
                if (written % (10 * 1024) == 0) {
                    Serial.printf(" %d%%\n", (written * 100) / contentLength);
                }
            }
        }
        delay(10);
    }

    Serial.println();

    if (!Update.end()) {
        Serial.printf("[FW] Update.end failed: %s\n", Update.errorString());
        http.end();
        return false;
    }

    http.end();
    return true;
}

// ─────────────────────────────────────────────────────────────
//  setup() — runs once per deep-sleep wake cycle
// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[Boot] Water Level Monitor starting...");

    // ── Sensor pin setup ───────────────────────────────────
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);  // IO34 is input-only; no pinMode(OUTPUT) needed
    digitalWrite(TRIG_PIN, LOW);

    // ── 1. Read sensor (WiFi not needed yet) ───────────────
    float distanceCm = measureDistanceCm();
    if (distanceCm < 0.0f) {
        Serial.println("[Sensor] Bad reading — skipping this cycle");
        rtcData.cyclesSinceLastSend++;
        esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
        esp_deep_sleep(SLEEP_INTERVAL_US);
    }

    // ── 2. Read battery voltage (10K–10K divider on IO35) ────
    uint32_t rawMv = analogReadMilliVolts(BATT_PIN);
    float batteryV   = (rawMv / 1000.0f) * 2.0f;  // ×2 for voltage divider
    float batteryPct = ((batteryV - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V)) * 100.0f;
    if (batteryPct < 0.0f)   batteryPct = 0.0f;
    if (batteryPct > 100.0f) batteryPct = 100.0f;
    Serial.printf("[Battery] %.2f V | %d %%  (ADC raw: %u mV)\n", batteryV, (int)batteryPct, rawMv);

    // ── 3. Calculate level ─────────────────────────────────────
    float smoothedCm = rollingAverage(distanceCm);
    float percentage = distanceToPct(smoothedCm);
    float waterHeightCm = TANK_EMPTY_CM - smoothedCm;
    Serial.printf("[Sensor] Distance: %.1f cm | Smoothed: %.1f cm | Level: %.1f cm | %.1f %%\n",
                  distanceCm, smoothedCm, waterHeightCm, percentage);

    // ── 4. Info ───────────────────────────────────────────
    Serial.printf("[Boot] Water level: %.1f %%\n", percentage);

    // ── 5. Connect WiFi (with retry) ─────────────────────
    bool wifiConnected = false;
    for (int attempt = 0; attempt < WIFI_RETRY_COUNT && !wifiConnected; attempt++) {
        wifiConnected = connectWiFi();
        if (!wifiConnected && attempt < WIFI_RETRY_COUNT - 1) {
            delay(2000);
        }
    }

    if (!wifiConnected) {
        Serial.println("[Boot] WiFi failed after retries — entering deep sleep");
        rtcData.cyclesSinceLastSend++;
        esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
        esp_deep_sleep(SLEEP_INTERVAL_US);
    }

    // ── 6. Check for firmware updates (HTTP pull OTA) ─────
    checkFirmwareUpdate();

    // ── 7. POST sensor data to backend ──────────────────
    if (postSensorData(percentage, waterHeightCm, batteryV, batteryPct)) {
        rtcData.cyclesSinceLastSend = 0;
    } else {
        rtcData.cyclesSinceLastSend++;
    }

    // ── 8. Decide next sleep interval (monitoring vs long sleep) ─
    uint64_t nextSleepUs = SLEEP_INTERVAL_US;

    // initialize lastLevelPct on first boot
    if (rtcData.count <= 1 && rtcData.cyclesSinceLastSend == 0 && rtcData.lastLevelPct == 0.0f) {
        rtcData.lastLevelPct = percentage;
        rtcData.isMonitoring = 0;
        rtcData.monitoringCycles = 0;
        rtcData.increasedInBlock = 0;
    }

    if (rtcData.isMonitoring) {
        if (percentage > rtcData.lastLevelPct) rtcData.increasedInBlock = 1;
        rtcData.monitoringCycles++;

        if (rtcData.monitoringCycles >= MONITOR_CYCLES) {
            if (rtcData.increasedInBlock) {
                // continue monitoring: reset block and take short sleep
                rtcData.monitoringCycles = 0;
                rtcData.increasedInBlock = 0;
                rtcData.lastLevelPct = percentage;
                nextSleepUs = MONITOR_INTERVAL_US;
            } else {
                // switch to long sleep
                rtcData.isMonitoring = 0;
                rtcData.monitoringCycles = 0;
                rtcData.lastLevelPct = percentage;
                Serial.println("[Monitor] Switched to hourly checks");
                nextSleepUs = SLEEP_INTERVAL_US;
            }
        } else {
            // still within a monitoring block
            rtcData.lastLevelPct = percentage;
            nextSleepUs = MONITOR_INTERVAL_US;
        }
    } else {
        // currently in long-sleep mode
        if (percentage > rtcData.lastLevelPct) {
            // start monitoring mode
            rtcData.isMonitoring = 1;
            rtcData.monitoringCycles = 1;
            rtcData.increasedInBlock = 1;
            rtcData.lastLevelPct = percentage;
            Serial.println("[Monitor] Level increase detected — entering monitoring mode");
            nextSleepUs = MONITOR_INTERVAL_US;
        } else {
            rtcData.lastLevelPct = percentage;
            nextSleepUs = SLEEP_INTERVAL_US;
        }
    }

    Serial.printf("[Sleep] Sleeping for %llu s...\n", nextSleepUs / 1000000ULL);
    Serial.flush();
    esp_sleep_enable_timer_wakeup(nextSleepUs);
    esp_deep_sleep(nextSleepUs);
}

// ─────────────────────────────────────────────────────────────
//  loop() — intentionally empty; all logic is in setup()
// ─────────────────────────────────────────────────────────────
void loop() {}
