#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <DNSServer.h>
#include "web/IRouteProvider.h"

class NetworkManager;

// Answers every DNS query with the AP address and redirects the probe URLs the
// major platforms use to decide whether a network is captive. Runs only while
// NetworkManager is provisioning; starts and stops itself from tick().
class CaptivePortal : public IRouteProvider {
public:
  explicit CaptivePortal(NetworkManager& net);

  void tick();
  void registerRoutes(HttpServer& server) override;
  bool handleNotFound(HttpServer& server) override;

  bool isActive() const;

private:
  void start();
  void stop();
  void redirect(HttpServer& server);

  NetworkManager& _net;
  DNSServer _dns;
  bool _running = false;
};

#endif // CAPTIVE_PORTAL_H
