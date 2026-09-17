#ifndef I_ROUTE_PROVIDER_H
#define I_ROUTE_PROVIDER_H

#include <ESP8266WebServer.h>

using HttpServer = ESP8266WebServer;

class IRouteProvider {
public:
  virtual void registerRoutes(HttpServer& server) = 0;

  // Optional. HttpService offers unmatched requests to each provider in turn;
  // the first to return true owns the response. Used by the captive portal to
  // redirect anything it does not recognise while in provisioning mode.
  virtual bool handleNotFound(HttpServer& server) {
    (void)server;
    return false;
  }

protected:
  ~IRouteProvider() = default;
};

#endif // I_ROUTE_PROVIDER_H
