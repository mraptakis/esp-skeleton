#include "app/LedController.h"

LedController::LedController(uint8_t pin): _pin(pin) {
}

void LedController::begin() {
  pinMode(_pin, OUTPUT);
  apply();
}

void LedController::tick() {
}

void LedController::setOn(bool on) {
  _on = on;
  apply();
}

bool LedController::isOn() const {
  return _on;
}

void LedController::overrideRaw(bool lit) {
  _overridden = true;
  digitalWrite(_pin, lit ? LOW : HIGH);
}

void LedController::releaseOverride() {
  _overridden = false;
  apply();
}

void LedController::apply() {
  if (_overridden) {
    return;
  }

  digitalWrite(_pin, _on ? LOW : HIGH);
}
