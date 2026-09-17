#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>

enum class NetState : uint8_t {
  Idle,
  Connecting,
  Connected,
  Reconnecting,
  Provisioning
};

enum class AppMode : uint8_t {
  Booting,
  Normal,
  Provisioning,
  Updating,
  SafeMode
};

inline const char* toString(NetState s) {
  switch (s) {
    case NetState::Idle:
      return "idle";
    case NetState::Connecting:
      return "connecting";
    case NetState::Connected:
      return "connected";
    case NetState::Reconnecting:
      return "reconnecting";
    case NetState::Provisioning:
      return "provisioning";
  }
  return "unknown";
}

inline const char* toString(AppMode m) {
  switch (m) {
    case AppMode::Booting:
      return "booting";
    case AppMode::Normal:
      return "normal";
    case AppMode::Provisioning:
      return "provisioning";
    case AppMode::Updating:
      return "updating";
    case AppMode::SafeMode:
      return "safe";
  }
  return "unknown";
}

struct AppState {
  AppMode mode = AppMode::Booting;
  NetState net = NetState::Idle;
  uint8_t otaPercent = 0;
  uint32_t bootMillis = 0;

  bool isOnline() const { return net == NetState::Connected; }
};

#endif // APP_STATE_H
