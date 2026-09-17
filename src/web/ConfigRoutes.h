#ifndef CONFIG_ROUTES_H
#define CONFIG_ROUTES_H

#include "IRouteProvider.h"

class NetworkManager;
class ConfigStore;
class Health;
struct AppState;

class ConfigRoutes : public IRouteProvider {
public:
  ConfigRoutes(NetworkManager& net, ConfigStore& config, AppState& state, Health& health);
  void registerRoutes(HttpServer& server) override;

private:
  void handleScan(HttpServer& s);
  void handleStatus(HttpServer& s);
  void handleConfig(HttpServer& s);
  void handleForget(HttpServer& s);
  void handleBrokerGet(HttpServer& s);
  void handleBrokerSet(HttpServer& s);
  void handleSettingsGet(HttpServer& s);
  void handleSettingsSet(HttpServer& s);
  void handleInfo(HttpServer& s);
  void handleLog(HttpServer& s);
  void handleRestart(HttpServer& s);

  NetworkManager& _net;
  ConfigStore& _config;
  AppState& _state;
  Health& _health;
};

#endif // CONFIG_ROUTES_H
