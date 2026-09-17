#ifndef I_TELEMETRY_H
#define I_TELEMETRY_H

#include "core/AppState.h"

class ITelemetry {
public:
  virtual void onNetState(NetState s) = 0;
  virtual void onMode(AppMode m) = 0;
  virtual void onOtaProgress(uint8_t pct) = 0;
protected:
  ~ITelemetry() = default;
};
#endif // I_TELEMETRY_H
