#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>

class LedController {
public:
  explicit LedController(uint8_t pin);

  void begin();
  void tick();

  void setOn(bool on);
  bool isOn() const;

  void overrideRaw(bool lit);
  void releaseOverride();

private:
  void apply();

  const uint8_t _pin;
  bool _on = false;
  bool _overridden = false;
};

#endif // LED_CONTROLLER_H
