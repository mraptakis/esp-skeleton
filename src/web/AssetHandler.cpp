#include "web/AssetHandler.h"
#include "generated/web_assets.h"

void AssetHandler::registerRoutes(HttpServer& server) {
  HttpServer* s = &server;

  for (uint8_t i = 0; i < WEB_ASSET_COUNT; ++i) {
    WebAsset a;
    memcpy_P(&a, &WEB_ASSETS[i], sizeof(a));
    s->on(a.path, HTTP_GET, [s, i]() { serve(*s, i); });
  }
}

void AssetHandler::serve(HttpServer& s, uint8_t index) {
  WebAsset a;
  memcpy_P(&a, &WEB_ASSETS[index], sizeof(a));

  if (s.hasHeader("If-None-Match")) {
    String inm = s.header("If-None-Match");
    if (inm.indexOf(a.etag) >= 0) {
      s.send(304);
      return;
    }
  }

  s.sendHeader(F("Content-Encoding"), F("gzip"));
  s.sendHeader(F("Cache-Control"), F("public, max-age=31536000, immutable"));
  s.sendHeader(F("ETag"), String('"') + a.etag + '"');

  s.send_P(200, a.contentType, reinterpret_cast<PGM_P>(a.data), a.len);
}
