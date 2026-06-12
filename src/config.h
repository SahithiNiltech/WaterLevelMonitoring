#pragma once

// ─────────────────────────────────────────────────────────────
//  WiFi credentials
// ─────────────────────────────────────────────────────────────
#define WIFI_SSID     "Niltech_Airtel1_5G"
#define WIFI_PASS     "Niltech12345"

// ─────────────────────────────────────────────────────────────
//  Backend Server Configuration (HTTP API)
// ─────────────────────────────────────────────────────────────
#define BACKEND_URL      "https://your-backend.render.com"  // ← REPLACE with your Render URL (e.g., https://water-level-monitor-api.onrender.com)
#define API_KEY          "waterlevel890890"                  // Must match backend API_KEY
#define DEVICE_ID        "WaterLevelMonitor_001"
#define FIRMWARE_VERSION "1.1"

// Battery voltage range (LiPo — based on measured values)
#define BATT_MIN_V   3.2f   // 0 %  (multimeter: 3.20V → ADC ~1600 mV)
#define BATT_MAX_V   4.2f   // 100 % (multimeter: 4.20V → ADC ~2100 mV)

// ─────────────────────────────────────────────────────────────
//  Sensor pins
// ─────────────────────────────────────────────────────────────
#define TRIG_PIN    32
#define ECHO_PIN    34   // IO34 is input-only; use a voltage divider if sensor runs on 5V
#define BATT_PIN    35   // IO35 input-only; 10K–10K voltage divider → actual voltage = ADC reading × 2

// ─────────────────────────────────────────────────────────────
//  Tank geometry  (adjust to your physical installation)
// ─────────────────────────────────────────────────────────────
// TANK_HEIGHT_CM             : physical tank height (reference only — not used in level calc)
// SENSOR_HEIGHT_ABOVE_TANK   : distance (cm) from sensor face down to the top rim of the tank
// WATER_FULL_HEIGHT_CM       : water column height from tank bottom considered "full" (reference only)
// TANK_EMPTY_CM              : calibrated sensor reading on completely empty tank
// TANK_FULL_CM               : calibrated sensor reading at WATER_FULL_HEIGHT_CM water level
//
// ℹ Sensor mounted flush at the tank rim (no gap above tank).
//   TANK_EMPTY_CM and TANK_FULL_CM are theoretical — replace with actual sensor readings after calibration.
//   TANK_EMPTY_CM ≈ TANK_HEIGHT_CM                          (sensor to empty tank bottom)
//   TANK_FULL_CM  ≈ TANK_HEIGHT_CM - WATER_FULL_HEIGHT_CM   (sensor to full water surface)
#define TANK_HEIGHT_CM            134.62f  // 53 in
#define SENSOR_HEIGHT_ABOVE_TANK    0.0f   // sensor is at the tank rim

#define WATER_FULL_HEIGHT_CM      109.62f  // water column from tank bottom = "full"  (134.62 - 25.0)

#define TANK_EMPTY_CM             134.62f  // sensor reading when tank is empty (≈ tank height)
#define TANK_FULL_CM               25.0f   // sensor reading when tank is full (calibrated)

// ─────────────────────────────────────────────────────────────
//  Alert thresholds  (%)
// ─────────────────────────────────────────────────────────────
#define ALERT_FULL_PCT   95   // fire water_full  when level reaches this or above
#define ALERT_LOW30_PCT  30
#define ALERT_LOW20_PCT  20
#define ALERT_LOW10_PCT  10
#define ALERT_BATT_LOW_PCT 20  // fire battery_low when battery drops below this %

// ─────────────────────────────────────────────────────────────
//  Timing & Connectivity
// ─────────────────────────────────────────────────────────────
#define SLEEP_INTERVAL_US    3600000000ULL  // 1 hour in microseconds
#define WIFI_TIMEOUT_MS              10000    // 10 s WiFi connect timeout
#define HTTP_TIMEOUT_MS              15000    // 15 s HTTP request timeout (handles Render cold-start)
#define WIFI_RETRY_COUNT                 3    // number of WiFi connection attempts
#define SENSOR_NUM_SAMPLES              10    // readings to average per cycle
#define ECHO_TIMEOUT_US              30000    // pulseIn timeout (30 ms → ~5 m max range)
#define ROLLING_AVG_CYCLES               5    // wake cycles to rolling-average

// ─────────────────────────────────────────────────────────────
//  Monitoring mode configuration
// ─────────────────────────────────────────────────────────────
#define MONITOR_CYCLES                   4    // cycles per monitoring block
#define MONITOR_INTERVAL_US       60000000ULL  // 1 minute
