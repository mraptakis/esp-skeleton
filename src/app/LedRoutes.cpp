#include "app/LedRoutes.h"
#include "app/LedController.h"
#include "web/JsonWriter.h"
#include "web/ResponseBuffer.h"

namespace {
  const char CT_JSON[] PROGMEM = "application/json";
}

LedRoutes::LedRoutes(LedController& led) : _led(led) {}

void LedRoutes::registerRoutes(HttpServer& server) {
  HttpServer* s = &server;

  s->on("/api/led", HTTP_GET, [this, s]() {
    sendState(*s);
  });

  s->on("/api/led", HTTP_POST, [this, s]() {
    if (!s->hasArg("on")) {
      s->send(400, FPSTR(CT_JSON), F("{\"error\":\"missing 'on'\"}"));
      return;
    }

    const String arg = s->arg("on");
    bool value;
    if (arg == "1" || arg == "true") {
      value = true;
    } else if (arg == "0" || arg == "false") {
      value = false;
    } else {
      s->send(400, FPSTR(CT_JSON), F("{\"error\":\"invalid 'on'\"}"));
      return;
    }

    _led.setOn(value);
    sendState(*s);
  });
}

void LedRoutes::sendState(HttpServer& s) {
  JsonWriter w(ResponseBuffer::data, ResponseBuffer::SIZE);
  w.beginObject().add("on", _led.isOn()).endObject();
  s.send(200, FPSTR(CT_JSON), w.c_str());
}
