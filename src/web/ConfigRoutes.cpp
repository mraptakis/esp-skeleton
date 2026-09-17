#include "web/ConfigRoutes.h"
#include "web/JsonWriter.h"
#include "web/ResponseBuffer.h"
#include "net/NetworkManager.h"
#include "config/ConfigStore.h"
#include "core/AppState.h"
#include "core/Health.h"
#include "core/Log.h"
#include "Config.h"

namespace {
  const char CT_JSON[] PROGMEM = "application/json";
}

ConfigRoutes::ConfigRoutes(NetworkManager& net, ConfigStore& config, AppState& state, Health& health)
  : _net(net), _config(config), _state(state), _health(health) {}

void ConfigRoutes::registerRoutes(HttpServer& server) {
  HttpServer* s = &server;
  s->on("/api/wifi/scan", HTTP_GET, [this, s]() { handleScan(*s); });
  s->on("/api/wifi/status", HTTP_GET, [this, s]() { handleStatus(*s); });
  s->on("/api/wifi/config", HTTP_POST, [this, s]() { handleConfig(*s); });
  s->on("/api/wifi/forget", HTTP_POST, [this, s]() { handleForget(*s); });
  s->on("/api/broker", HTTP_GET, [this, s]() { handleBrokerGet(*s); });
  s->on("/api/broker", HTTP_POST, [this, s]() { handleBrokerSet(*s); });
  s->on("/api/settings", HTTP_GET, [this, s]() { handleSettingsGet(*s); });
  s->on("/api/settings", HTTP_POST, [this, s]() { handleSettingsSet(*s); });
  s->on("/api/info", HTTP_GET, [this, s]() { handleInfo(*s); });
  s->on("/api/log", HTTP_GET, [this, s]() { handleLog(*s); });
  s->on("/api/restart", HTTP_POST, [this, s]() { handleRestart(*s); });
}

void ConfigRoutes::handleScan(HttpServer& s) {
  if (!_net.requestScan()) {
    s.send(409, FPSTR(CT_JSON), F("{\"error\":\"scan unavailable in this state\"}"));
    return;
  }

  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);

  if (_net.scanInProgress() && _net.scanCount() == 0) {
    w.beginObject()
      .add("scanning", true)
      .beginArray("networks").endArray()
      .endObject();
    s.send(202, FPSTR(CT_JSON), w.c_str());
    return;
  }

  w.beginObject();
  w.add("scanning", _net.scanInProgress());
  w.add("ageMs", (uint32_t)_net.scanAgeMs());
  w.beginArray("networks");

  for (uint8_t i = 0; i < _net.scanCount(); ++i) {
    // Stop before writing a half object. Results are ordered by signal, so
    // anything dropped here is the weakest.
    if (!w.ok()) {
      break;
    }

    const ScanResult& r = _net.scanAt(i);
    w.beginObject()
      .add("ssid", r.ssid)
      .add("rssi", r.rssi)
      .add("open", r.encryption == ENC_TYPE_NONE)
      .endObject();
  }

  w.endArray();
  const bool truncated = !w.ok();
  w.add("truncated", truncated);
  w.endObject();

  if (!w.ok()) {
    s.send(500, FPSTR(CT_JSON), F("{\"error\":\"response overflow\"}"));
    return;
  }

  s.send(200, FPSTR(CT_JSON), w.c_str());
}

void ConfigRoutes::handleStatus(HttpServer& s) {
  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);

  w.beginObject()
    .add("state", toString(_state.net))
    .add("provisioned", _config.isProvisioned())
    .add("ssid", _net.isOnline() ? _net.ssid().c_str() : _config.wifi().ssid.c_str())
    .add("ip", _net.ip().toString())
    .add("apSsid", _net.apSsid());

  if (_net.isOnline()) {
    w.add("rssi", _net.rssi());
  }
  w.endObject();

  s.send(200, FPSTR(CT_JSON), w.c_str());
}

void ConfigRoutes::handleConfig(HttpServer& s) {
  if (!s.hasArg("ssid")) {
    s.send(400, FPSTR(CT_JSON), F("{\"error\":\"missing 'ssid'\"}"));
    return;
  }

  WifiCredentials creds;
  creds.ssid = s.arg("ssid");
  creds.password = s.hasArg("password") ? s.arg("password") : "";

  if (!creds.isValid()) {
    s.send(400, FPSTR(CT_JSON), F("{\"error\":\"invalid ssid or password length\"}"));
    return;
  }

  // Respond first. The radio switch happens in NetworkManager::tick(), after
  // this response has been flushed: we are about to tear down the very link
  // the client is talking to us over.
  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);
  w.beginObject()
    .add("accepted", true)
    .add("ssid", creds.ssid.c_str())
    .add("hostname", _config.settings().hostname.length() ? _config.settings().hostname.c_str() : Config::OTA_HOSTNAME)
    .endObject();

  s.send(200, FPSTR(CT_JSON), w.c_str());
  s.client().flush();

  _net.requestConnect(creds.ssid.c_str(), creds.password.c_str());
}

void ConfigRoutes::handleForget(HttpServer& s) {
  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);
  w.beginObject()
    .add("accepted", true)
    .add("apSsid", _net.apSsid())
    .endObject();

  s.send(200, FPSTR(CT_JSON), w.c_str());
  s.client().flush();

  _net.requestForget();
}

void ConfigRoutes::handleBrokerGet(HttpServer& s) {
  const BrokerSettings& b = _config.broker();
  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);
  w.beginObject()
    .add("configured", b.isConfigured())
    .add("host", b.host.c_str())
    .add("port", (uint32_t)b.port)
    .add("username", b.username.c_str())
    .add("baseTopic", b.baseTopic.c_str())
    .add("hasPassword", b.password.length() > 0)
    .endObject();
  s.send(200, FPSTR(CT_JSON), w.c_str());
}

void ConfigRoutes::handleBrokerSet(HttpServer& s) {
  if (!s.hasArg("host")) {
    s.send(400, FPSTR(CT_JSON), F("{\"error\":\"missing 'host'\"}"));
    return;
  }

  BrokerSettings b;
  b.host = s.arg("host");
  b.username = s.hasArg("username") ? s.arg("username") : "";
  b.baseTopic = s.hasArg("baseTopic") ? s.arg("baseTopic") : "";
  b.password = s.hasArg("password") ? s.arg("password") : _config.broker().password.c_str();

  if (b.host.overflowed() || b.username.overflowed() ||
      b.password.overflowed() || b.baseTopic.overflowed()) {
    s.send(400, FPSTR(CT_JSON), F("{\"error\":\"field too long\"}"));
    return;
  }

  b.host.trim();
  if (b.host.length() == 0) {
    _config.clearBroker();
    s.send(200, FPSTR(CT_JSON), F("{\"configured\":false}"));
    return;
  }

  if (s.hasArg("port")) {
    const long p = s.arg("port").toInt();
    if (p < 1 || p > 65535) {
      s.send(400, FPSTR(CT_JSON), F("{\"error\":\"invalid 'port'\"}"));
      return;
    }
    b.port = (uint16_t)p;
  }

  if (!_config.saveBroker(b)) {
    s.send(500, FPSTR(CT_JSON), F("{\"error\":\"could not save\"}"));
    return;
  }

  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);
  w.beginObject()
    .add("configured", b.isConfigured())
    .add("host", b.host.c_str())
    .add("port", (uint32_t)b.port)
    .add("username", b.username.c_str())
    .add("baseTopic", b.baseTopic.c_str())
    .add("hasPassword", b.password.length() > 0)
    .endObject();
  s.send(200, FPSTR(CT_JSON), w.c_str());
}

void ConfigRoutes::handleSettingsGet(HttpServer& s) {
  const DeviceSettings& d = _config.settings();
  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);
  w.beginObject()
    .add("hostname", d.hostname.c_str())
    .add("friendlyName", d.friendlyName.c_str())
    .add("defaultHostname", Config::OTA_HOSTNAME)
    .endObject();
  s.send(200, FPSTR(CT_JSON), w.c_str());
}

void ConfigRoutes::handleSettingsSet(HttpServer& s) {
  DeviceSettings d = _config.settings();

  if (s.hasArg("hostname")) {
    d.hostname = s.arg("hostname");
    if (d.hostname.overflowed()) {
      s.send(400, FPSTR(CT_JSON), F("{\"error\":\"'hostname' too long\"}"));
      return;
    }
    d.hostname.trim();
  }
  if (s.hasArg("friendlyName")) {
    d.friendlyName = s.arg("friendlyName");
    if (d.friendlyName.overflowed()) {
      s.send(400, FPSTR(CT_JSON), F("{\"error\":\"'friendlyName' too long\"}"));
      return;
    }
    d.friendlyName.trim();
  }

  if (!_config.saveSettings(d)) {
    s.send(400, FPSTR(CT_JSON), F("{\"error\":\"invalid or unsaveable settings\"}"));
    return;
  }

  // The hostname is handed to ArduinoOTA at startup, so it only takes effect
  // on the next boot.
  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);
  w.beginObject()
    .add("hostname", d.hostname.c_str())
    .add("friendlyName", d.friendlyName.c_str())
    .add("restartRequired", true)
    .endObject();
  s.send(200, FPSTR(CT_JSON), w.c_str());
}

void ConfigRoutes::handleInfo(HttpServer& s) {
  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);

  w.beginObject()
    .add("build", Config::VERSION)
    .add("buildTime", Config::BUILD_TIME)
    .add("mode", toString(_state.mode))
    .add("state", toString(_state.net))
    .add("provisioned", _config.isProvisioned())
    .add("friendlyName", _config.settings().friendlyName.c_str())
    .add("ssid", _net.isOnline() ? _net.ssid().c_str() : _config.wifi().ssid.c_str())
    .add("ip", _net.ip().toString())
    .add("apSsid", _net.apSsid())
    .add("chipId", (uint32_t)ESP.getChipId())
    .add("uptimeMs", (uint32_t)(millis() - _state.bootMillis))
    .add("heap", (uint32_t)ESP.getFreeHeap())
    .add("maxBlock", (uint32_t)ESP.getMaxFreeBlockSize())
    .add("sketchSize", (uint32_t)ESP.getSketchSize())
    .add("freeSketch", (uint32_t)ESP.getFreeSketchSpace())
    .add("bootCount", (uint32_t)_health.bootCount())
    .add("safeMode", _health.isSafeMode())
    .add("stable", _health.isStable());

  if (_net.isOnline()) {
    w.add("rssi", _net.rssi());
  }
  w.add("reset", ESP.getResetReason());
  w.endObject();

  s.send(200, FPSTR(CT_JSON), w.c_str());
}

void ConfigRoutes::handleLog(HttpServer& s) {
  // Sent chunked: the ring buffer is larger than the shared response buffer,
  // and growing that buffer for one endpoint is not worth the RAM.
  s.setContentLength(CONTENT_LENGTH_UNKNOWN);
  s.send(200, FPSTR(CT_JSON), F(""));

  char head[48];
  snprintf_P(head, sizeof(head), PSTR("{\"dropped\":%lu,\"lines\":["),
             (unsigned long)Log::droppedCount());
  s.sendContent(head);

  char item[Log::LINE_LEN * 2 + 8];
  const uint8_t n = Log::count();
  for (uint8_t i = 0; i < n; ++i) {
    JsonWriter w(item, sizeof(item));
    if (i > 0) {
      s.sendContent(F(","));
    }
    w.push(Log::at(i));
    s.sendContent(w.c_str());
  }

  s.sendContent(F("]}"));
  s.sendContent(F(""));
}

void ConfigRoutes::handleRestart(HttpServer& s) {
  const bool clearCounter = s.hasArg("clear") && s.arg("clear") == "1";

  s.send(200, FPSTR(CT_JSON), F("{\"accepted\":true}"));
  s.client().flush();

  delay(200);
  if (clearCounter) {
    // Deliberate way out of safe mode once the bad image has been replaced.
    _health.clearAndRestart();
  } else {
    ESP.restart();
  }
}
