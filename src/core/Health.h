#ifndef HEALTH_H
#define HEALTH_H

#include <Arduino.h>

// Boot-loop detection. The ESP8266 has no OTA rollback: once a bad image is
// installed, the only automatic way back is for the device to notice it keeps
// restarting and come up in a reduced mode that still accepts an update.
//
// The counter lives in RTC user memory, which survives a reset or a crash but
// not a power cut. That is the behaviour we want: someone pulling the plug a
// few times is not a boot loop.
class Health {
public:
  bool begin();
  void tick();

  bool isSafeMode() const;
  uint32_t bootCount() const;
  // True once the device has stayed up long enough to be considered healthy.
  bool isStable() const;

  // Clears the counter and restarts. Used to leave safe mode deliberately.
  void clearAndRestart();

private:
  void writeCount(uint32_t value);

  uint32_t _bootCount = 0;
  bool _safeMode = false;
  bool _stable = false;
};

#endif // HEALTH_H
