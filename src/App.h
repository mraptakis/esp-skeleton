#ifndef APP_H
#define APP_H

#include "Config.h"
#include "core/AppState.h"
#include "core/ITelemetry.h"
#include "core/Health.h"
#include "config/ConfigStore.h"
#include "net/NetworkManager.h"
#include "net/CaptivePortal.h"
#include "web/ConfigRoutes.h"
#include "web/HttpService.h"
#include "web/AssetHandler.h"
#include "ota/OtaService.h"
#include "app/LedController.h"
#include "app/LedRoutes.h"

class App {
public:
  App();
  void begin();
  void tick();

  void setTelemetry(ITelemetry* t);
  void requestProvisioningOnBoot();

private:
  void bootBanner();
  void onNetStateChange(NetState s);
  void startOta();
  void onOtaStart();
  void onOtaProgress(uint8_t percent);
  void onOtaEnd();
  const char* hostname() const;

  // Declaration order is initialisation order: anything holding a reference
  // must come after what it refers to.
  AppState _state;
  Health _health;
  ConfigStore _config;
  NetworkManager _net;
  CaptivePortal _portal;
  ConfigRoutes _configRoutes;
  HttpService _http;
  OtaService _ota;
  AssetHandler _assets;

  ITelemetry* _telemetry = nullptr;
  bool _forceProvisioning = false;
  bool _otaStarted = false;
  bool _otaPending = false;
  uint32_t _otaPendingSince = 0;

  // Example application. Delete src/app and the two addProvider calls to
  // start from a bare skeleton.
  LedController _led;
  LedRoutes _ledRoutes;
};

#endif // APP_H
