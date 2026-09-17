#include "ota/OtaService.h"
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

namespace {
  OtaService* s_instance = nullptr;
}

void OtaService::begin(const char* hostname, const char* password) {
  s_instance = this;

  ArduinoOTA.setHostname(hostname);
  // Only arm auth when a real password is configured — ArduinoOTA otherwise
  // hashes and requires an empty string, which is not the same as "no auth".
  if (password && password[0] != '\0') {
    ArduinoOTA.setPassword(password);
  }

  ArduinoOTA.onStart([]() {
    Serial.println(F("[OTA] start"));
    if (s_instance) {
      s_instance->_onStart();
    }
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    const uint8_t pct = total ? (uint8_t)((uint32_t)progress * 100 / total) : 0;
    if (s_instance) {
      s_instance->_onProgress(pct);
    }
  });

  ArduinoOTA.onEnd([]() {
    Serial.println(F("[OTA] end"));
    if (s_instance) {
      s_instance->_onEnd();
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf_P(PSTR("[OTA] error %u\n"), error);
  });

  ArduinoOTA.begin();
}

void OtaService::tick() {
  ArduinoOTA.handle();
}
