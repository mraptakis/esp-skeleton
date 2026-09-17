#ifndef ASSET_HANDLER_H
#define ASSET_HANDLER_H

#include "IRouteProvider.h"

class AssetHandler : public IRouteProvider {
public:
  void registerRoutes(HttpServer& server) override;
private:
  static void serve(HttpServer& s, uint8_t index);
};

#endif // ASSET_HANDLER_H
