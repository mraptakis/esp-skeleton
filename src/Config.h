#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "generated/web_assets.h"

// Set via `-DBUILD_OTA_PASSWORD=\"...\"` (see platformio.ini, sourced from
// the OTA_PASSWORD environment variable). Empty by default — ArduinoOTA
// treats an empty password as "no auth required" (see OtaService::begin()).
#ifndef BUILD_OTA_PASSWORD
#define BUILD_OTA_PASSWORD ""
#endif

namespace Config {
  constexpr uint8_t LED_PIN = 2;
  constexpr uint32_t SERIAL_BAUD = 115200;

  constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
  constexpr uint32_t WIFI_AP_FALLBACK_MS = 300000;
  constexpr uint32_t WIFI_AP_RETRY_MS = 120000;
  constexpr uint32_t WIFI_SCAN_CACHE_MS = 30000;
  constexpr char AP_SSID_PREFIX[] = "ESP-Setup-";

  // Lower than the SDK default (~20.5dBm) to reduce the peak current draw
  // during TX bursts (association, mDNS announce) — cheap regulators without
  // enough bulk capacitance can brown out on those spikes otherwise.
  constexpr float WIFI_TX_POWER_DBM = 17.0f;

  // Gap between "connected" and starting OTA/mDNS, so their current draw
  // doesn't stack on top of the association burst that just happened.
  constexpr uint32_t OTA_START_DELAY_MS = 500;

  constexpr char OTA_HOSTNAME[] = "esp01s";
  constexpr char OTA_PASSWORD[] = BUILD_OTA_PASSWORD;

  // Consecutive boots without a stable run before the device drops to safe
  // mode. Safe mode still joins the network and still accepts OTA; it just
  // does not mount the application routes.
  constexpr uint32_t SAFE_MODE_BOOT_THRESHOLD = 5;
  // Uptime after which a boot is considered successful and the counter clears.
  constexpr uint32_t HEALTHY_UPTIME_MS = 30000;

  constexpr char VERSION[] = FIRMWARE_VERSION;
  constexpr char BUILD_TIME[] = FIRMWARE_BUILD_STAMP;
}

#endif // CONFIG_H
