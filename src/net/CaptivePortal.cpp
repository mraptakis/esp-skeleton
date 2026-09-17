#include "net/CaptivePortal.h"
#include "net/NetworkManager.h"
#include "core/Log.h"
#include <ESP8266WiFi.h>

namespace {
  constexpr uint16_t DNS_PORT = 53;

  // Probe URLs used by the platforms to detect a captive network. Each vendor
  // checks its own, so missing one means that vendor never shows the popup.
  const char* const PROBE_PATHS[] = {
    "/generate_204",
    "/gen_204",
    "/hotspot-detect.html",
    "/library/test/success.html",
    "/success.txt",
    "/ncsi.txt",
    "/connecttest.txt",
    "/redirect",
    "/fwlink",
    "/canonical.html"
  };
  constexpr uint8_t PROBE_COUNT = sizeof(PROBE_PATHS) / sizeof(PROBE_PATHS[0]);
}

CaptivePortal::CaptivePortal(NetworkManager& net) : _net(net) {}

void CaptivePortal::tick() {
  const bool wanted = (_net.state() == NetState::Provisioning);

  if (wanted && !_running) {
    start();
  } else if (!wanted && _running) {
    stop();
  }

  if (_running) {
    _dns.processNextRequest();
  }
}

void CaptivePortal::start() {
  _dns.setErrorReplyCode(DNSReplyCode::NoError);
  if (!_dns.start(DNS_PORT, "*", WiFi.softAPIP())) {
    LOGS("[portal] DNS server failed to start");
    return;
  }
  _running = true;
  LOGS("[portal] captive DNS active");
}

void CaptivePortal::stop() {
  _dns.stop();
  _running = false;
  LOGS("[portal] captive DNS stopped");
}

bool CaptivePortal::isActive() const {
  return _running;
}

void CaptivePortal::registerRoutes(HttpServer& server) {
  HttpServer* s = &server;
  for (uint8_t i = 0; i < PROBE_COUNT; ++i) {
    s->on(PROBE_PATHS[i], HTTP_GET, [this, s]() { redirect(*s); });
  }
}

bool CaptivePortal::handleNotFound(HttpServer& server) {
  if (!_running) {
    return false;
  }
  redirect(server);
  return true;
}

void CaptivePortal::redirect(HttpServer& server) {
  // Outside provisioning these paths mean nothing, so behave like any other
  // unknown route rather than bouncing a connected client around.
  if (!_running) {
    server.send(404, F("text/plain"), F("Not found"));
    return;
  }

  char target[40];
  snprintf_P(target, sizeof(target), PSTR("http://%s/"), WiFi.softAPIP().toString().c_str());

  server.sendHeader(F("Location"), target, true);
  server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  server.send(302, F("text/plain"), F(""));
}
