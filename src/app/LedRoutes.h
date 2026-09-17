#ifndef LED_ROUTES_H
#define LED_ROUTES_H

#include "web/IRouteProvider.h"

class LedController;

class LedRoutes : public IRouteProvider {
public:
  explicit LedRoutes(LedController& led);
  void registerRoutes(HttpServer& server) override;

private:
  void sendState(HttpServer& s);

  LedController& _led;
};

#endif // LED_ROUTES_H
