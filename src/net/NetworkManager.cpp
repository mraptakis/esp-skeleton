#include "net/NetworkManager.h"
#include "core/Log.h"
#include "Config.h"
#include <ESP8266WiFi.h>

NetworkManager::NetworkManager(ConfigStore& config, AppState& state)
  : _config(config), _state(state) {}

void NetworkManager::begin() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setOutputPower(Config::WIFI_TX_POWER_DBM);
  WiFi.mode(WIFI_STA);

  buildApSsid();
  _offlineSince = millis();

  // A forced provisioning request must win before any association is started,
  // otherwise the radio briefly chases the stored network for no reason.
  if (_pendingProvisioning) {
    _pendingProvisioning = false;
    LOGS("[net] provisioning forced");
    enterProvisioning();
    return;
  }

  const WifiCredentials& c = _config.wifi();
  if (c.isValid()) {
    enterConnecting(c.ssid.c_str(), c.password.c_str());
  } else {
    LOGS("[net] not provisioned");
    enterProvisioning();
  }
}

void NetworkManager::tick() {
  if (_pendingForget) {
    _pendingForget = false;
    _config.clearWifi();
    enterProvisioning();
    return;
  }

  if (_pendingProvisioning) {
    _pendingProvisioning = false;
    enterProvisioning();
    return;
  }

  if (_pendingConnect) {
    _pendingConnect = false;
    if (!_config.saveWifi(_pendingCreds)) {
      LOGS("[net] could not persist credentials");
    }
    _retryCount = 0;
    _offlineSince = millis();
    enterConnecting(_pendingCreds.ssid.c_str(), _pendingCreds.password.c_str());
    return;
  }

  pollScan();

  switch (_state.net) {
    case NetState::Connecting:
      tickConnecting();
      break;
    case NetState::Reconnecting:
      tickReconnecting();
      break;
    case NetState::Provisioning:
      tickProvisioning();
      break;
    case NetState::Connected:
      if (WiFi.status() != WL_CONNECTED) {
        LOGS("[net] link lost");
        _offlineSince = millis();
        enterReconnecting();
      }
      break;
    case NetState::Idle:
      break;
  }
}

void NetworkManager::setState(NetState s) {
  if (_state.net == s) {
    return;
  }
  LOGF("[net] %s -> %s", toString(_state.net), toString(s));
  _state.net = s;
  _stateEnteredAt = millis();
  _onStateChange(s);
}

void NetworkManager::enterConnecting(const char* ssid, const char* password) {
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
  LOGF("[net] connecting to %s (attempt %u)", ssid, _retryCount + 1);
  WiFi.begin(ssid, password);
  setState(NetState::Connecting);
}

void NetworkManager::enterConnected() {
  _retryCount = 0;
  LOGF("[net] ip %s", WiFi.localIP().toString().c_str());
  setState(NetState::Connected);
}

void NetworkManager::enterReconnecting() {
  WiFi.disconnect(false);
  setState(NetState::Reconnecting);
}

void NetworkManager::enterProvisioning() {
  WiFi.disconnect(true);
  // Not WIFI_AP_STA: one radio across two channels is unreliable here.
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_apSsid.c_str());
  LOGF("[net] AP up: %s @ %s", _apSsid.c_str(), WiFi.softAPIP().toString().c_str());
  setState(NetState::Provisioning);
}

void NetworkManager::tickConnecting() {
  if (WiFi.status() == WL_CONNECTED) {
    enterConnected();
    return;
  }

  if (elapsed() < Config::WIFI_CONNECT_TIMEOUT_MS) {
    return;
  }

  LOGS("[net] connect timeout");
  ++_retryCount;
  enterReconnecting();
}

void NetworkManager::tickReconnecting() {
  if (millis() - _offlineSince > Config::WIFI_AP_FALLBACK_MS) {
    LOGS("[net] offline too long, falling back to AP");
    enterProvisioning();
    return;
  }

  if (elapsed() < backoffMs()) {
    return;
  }

  const WifiCredentials& c = _config.wifi();
  if (!c.isValid()) {
    enterProvisioning();
    return;
  }
  enterConnecting(c.ssid.c_str(), c.password.c_str());
}

void NetworkManager::tickProvisioning() {
  if (!_config.wifi().isValid()) {
    return;
  }
  // Switching the radio would drop anyone mid-setup, so only retry the stored
  // network while nobody is associated with the AP.
  if (WiFi.softAPgetStationNum() > 0) {
    return;
  }
  if (elapsed() < Config::WIFI_AP_RETRY_MS) {
    return;
  }

  LOGS("[net] retrying stored credentials");
  _retryCount = 0;
  _offlineSince = millis();
  const WifiCredentials& c = _config.wifi();
  enterConnecting(c.ssid.c_str(), c.password.c_str());
}

uint32_t NetworkManager::backoffMs() const {
  const uint8_t shift = _retryCount > 5 ? 5 : _retryCount;
  const uint32_t d = 1000UL << shift;
  return d > 30000UL ? 30000UL : d;
}

uint32_t NetworkManager::elapsed() const {
  return millis() - _stateEnteredAt;
}

void NetworkManager::requestConnect(const char* ssid, const char* password) {
  _pendingCreds.ssid = ssid;
  _pendingCreds.password = password;
  _pendingConnect = true;
}

void NetworkManager::requestProvisioning() {
  _pendingProvisioning = true;
}

void NetworkManager::requestForget() {
  _pendingForget = true;
}

bool NetworkManager::requestScan() {
  if (_scanRunning) {
    return true;
  }
  if (_state.net == NetState::Connecting) {
    return false;
  }
  if (_scanFinishedAt && millis() - _scanFinishedAt < Config::WIFI_SCAN_CACHE_MS) {
    return true;
  }

  WiFi.scanDelete();
  // show_hidden stays false: hidden networks come back with an empty SSID and
  // are useless in a picker.
  if (WiFi.scanNetworks(true, false) == WIFI_SCAN_FAILED) {
    return false;
  }
  _scanRunning = true;
  return true;
}

NetState NetworkManager::state() const {
  return _state.net;
}

void NetworkManager::pollScan() {
  if (!_scanRunning) {
    return;
  }

  const int8_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    return;
  }

  _scanRunning = false;
  _scanFinishedAt = millis();
  _scanCount = 0;

  if (n <= 0) {
    WiFi.scanDelete();
    return;
  }

  for (int8_t i = 0; i < n && _scanCount < MAX_SCAN_RESULTS; ++i) {
    const String found = WiFi.SSID(i);
    if (found.length() == 0) {
      continue;
    }
    // Copy out of the String immediately and let it die.
    strlcpy(_scan[_scanCount].ssid, found.c_str(), sizeof(_scan[_scanCount].ssid));
    _scan[_scanCount].rssi = WiFi.RSSI(i);
    _scan[_scanCount].encryption = WiFi.encryptionType(i);
    ++_scanCount;
  }
  // Results live in SDK heap until deleted.
  WiFi.scanDelete();
  LOGF("[net] scan: %u networks", _scanCount);
}

bool NetworkManager::isOnline() const {
  return WiFi.status() == WL_CONNECTED;
}

IPAddress NetworkManager::ip() const {
  return isOnline() ? WiFi.localIP() : WiFi.softAPIP();
}

String NetworkManager::ssid() const {
  return WiFi.SSID();
}

int32_t NetworkManager::rssi() const {
  return WiFi.RSSI();
}

const char* NetworkManager::apSsid() const {
  return _apSsid.c_str();
}

bool NetworkManager::scanInProgress() const {
  return _scanRunning;
}

uint8_t NetworkManager::scanCount() const {
  return _scanCount;
}

const ScanResult& NetworkManager::scanAt(uint8_t i) const {
  return _scan[i];
}

uint32_t NetworkManager::scanAgeMs() const {
  return _scanFinishedAt ? (millis() - _scanFinishedAt) : UINT32_MAX;
}

void NetworkManager::buildApSsid() {
  char buf[40];
  snprintf_P(buf, sizeof(buf), PSTR("%s%06X"), Config::AP_SSID_PREFIX, ESP.getChipId());
  _apSsid = buf;
}
