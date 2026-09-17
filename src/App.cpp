#include "App.h"
#include "core/Log.h"

App::App()
  : _net(_config, _state),
    _portal(_net),
    _configRoutes(_net, _config, _state, _health),
    _http(80),
    _led(Config::LED_PIN),
    _ledRoutes(_led) {}

void App::bootBanner() {
  LOGF("[boot] build %s", Config::VERSION);
  LOGF("[boot] reset: %s", ESP.getResetReason().c_str());
  LOGF("[boot] heap %u block %u", ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
}

const char* App::hostname() const {
  const auto& stored = _config.settings().hostname;
  return stored.length() ? stored.c_str() : Config::OTA_HOSTNAME;
}

void App::begin() {
  _state.bootMillis = millis();
  _state.mode = AppMode::Booting;

  Log::begin(Config::SERIAL_BAUD);
  bootBanner();

  // Before anything else: if this device has restarted repeatedly without a
  // stable run, come up in a reduced mode that still accepts an update.
  _health.begin();
  if (_health.isSafeMode()) {
    _state.mode = AppMode::SafeMode;
  }

  _led.begin();

  if (!_config.begin()) {
    LOGF("[cfg] unusable (err %u)", (unsigned)_config.lastError());
  } else if (!_config.isProvisioned()) {
    LOGS("[cfg] no stored credentials");
  } else {
    LOGF("[cfg] ssid: %s", _config.wifi().ssid.c_str());
  }

  _net.onStateChange<App, &App::onNetStateChange>(this);
  if (_forceProvisioning) {
    LOGS("[boot] provisioning requested at boot");
    _net.requestProvisioning();
  }
  _net.begin();

  _http.addProvider(_configRoutes);
  _http.addProvider(_portal);
  _http.addProvider(_assets);
  if (!_health.isSafeMode()) {
    _http.addProvider(_ledRoutes);
  } else {
    LOGS("[boot] safe mode: application routes not mounted");
  }
  _http.begin();

  LOGS("[boot] ready");
}

void App::startOta() {
  _ota.begin(hostname(), Config::OTA_PASSWORD);

  _ota.onStart<App, &App::onOtaStart>(this);
  _ota.onProgress<App, &App::onOtaProgress>(this);
  _ota.onEnd<App, &App::onOtaEnd>(this);

  _otaStarted = true;
  LOGF("[ota] listening as %s", hostname());
}

void App::onOtaStart() {
  _state.mode = AppMode::Updating;
  if (_telemetry) {
    _telemetry->onMode(_state.mode);
  }
  _led.overrideRaw(true);
}

void App::onOtaProgress(uint8_t percent) {
  _state.otaPercent = percent;
  if (_telemetry) {
    _telemetry->onOtaProgress(percent);
  }
  _led.overrideRaw((percent / 5) % 2 == 0);
}

void App::onOtaEnd() {
  _led.releaseOverride();
}

void App::onNetStateChange(NetState s) {
  switch (s) {
    case NetState::Provisioning:
      _state.mode = _health.isSafeMode() ? AppMode::SafeMode : AppMode::Provisioning;
      break;
    case NetState::Connected:
      _state.mode = _health.isSafeMode() ? AppMode::SafeMode : AppMode::Normal;
      break;
    default:
      break;
  }

  if (_telemetry) {
    _telemetry->onNetState(s);
    _telemetry->onMode(_state.mode);
  }

  // ArduinoOTA registers an mDNS service, so it needs an address first.
  // Deferred (see tick()) so its current draw doesn't stack on top of the
  // association burst that just happened.
  if (s == NetState::Connected && !_otaStarted && !_otaPending) {
    _otaPending = true;
    _otaPendingSince = millis();
  }
}

void App::tick() {
  _health.tick();
  _net.tick();
  _portal.tick();

  if (_otaPending && !_otaStarted &&
      millis() - _otaPendingSince >= Config::OTA_START_DELAY_MS) {
    _otaPending = false;
    startOta();
  }

  if (_otaStarted) {
    _ota.tick();
  }
  _http.tick();
  _led.tick();
}

void App::setTelemetry(ITelemetry* t) {
  _telemetry = t;
}

void App::requestProvisioningOnBoot() {
  _forceProvisioning = true;
}
