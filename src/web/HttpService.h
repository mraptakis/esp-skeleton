#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#include "IRouteProvider.h"

class HttpService {
public:
  static constexpr uint8_t MAX_PROVIDERS = 8;

  explicit HttpService(uint16_t port);

  bool addProvider(IRouteProvider& provider);
  void begin();
  void tick();

private:
  HttpServer _server;
  IRouteProvider* _providers[MAX_PROVIDERS] = {nullptr};
  uint8_t _count = 0;
};

#endif // HTTP_SERVICE_H
