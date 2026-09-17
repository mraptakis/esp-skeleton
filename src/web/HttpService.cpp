#include "web/HttpService.h"

HttpService::HttpService(uint16_t port) : _server(port) {
}

bool HttpService::addProvider(IRouteProvider& provider) {
  if (_count >= MAX_PROVIDERS) {
    return false;
  }

  _providers[_count++] = &provider;
  return true;
}

void HttpService::begin() {
  _server.collectHeaders(F("If-None-Match"));

  for (uint8_t i = 0; i < _count; ++i) {
    if (!_providers[i]) {
      continue;
    }
    _providers[i]->registerRoutes(_server);
  }

  _server.onNotFound([this]() {
    for (uint8_t i = 0; i < _count; ++i) {
      if (!_providers[i]) {
        continue;
      }
      if (_providers[i]->handleNotFound(_server)) {
        return;
      }
    }
    _server.send(404, F("text/plain"), F("Not found"));
  });

  _server.begin();
}

void HttpService::tick() {
  _server.handleClient();
}
