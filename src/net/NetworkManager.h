#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <IPAddress.h>
#include "core/AppState.h"
#include "core/FixedString.h"
#include "core/Callback.h"
#include "config/ConfigStore.h"

struct ScanResult {
  char ssid[33];
  int32_t rssi;
  uint8_t encryption;
};

class NetworkManager {
public:
  static constexpr uint8_t MAX_SCAN_RESULTS = 12;

  NetworkManager(ConfigStore& config, AppState& state);

  void begin();
  void tick();

  void requestConnect(const char* ssid, const char* password);
  void requestProvisioning();
  void requestForget();
  bool requestScan();

  NetState state() const;
  bool isOnline() const;
  IPAddress ip() const;
  String ssid() const;
  int32_t rssi() const;
  const char* apSsid() const;

  bool scanInProgress() const;
  uint8_t scanCount() const;
  const ScanResult& scanAt(uint8_t i) const;
  uint32_t scanAgeMs() const;

  template <typename T, void (T::*Method)(NetState)>
  void onStateChange(T* instance) {
    _onStateChange.bind<T, Method>(instance);
  }

private:
  void setState(NetState s);
  void enterConnecting(const char* ssid, const char* password);
  void enterConnected();
  void enterReconnecting();
  void enterProvisioning();

  void tickConnecting();
  void tickReconnecting();
  void tickProvisioning();
  void pollScan();

  uint32_t backoffMs() const;
  uint32_t elapsed() const;
  void buildApSsid();

  ConfigStore& _config;
  AppState& _state;

  uint32_t _stateEnteredAt = 0;
  uint32_t _offlineSince = 0;
  uint8_t _retryCount = 0;

  bool _pendingConnect = false;
  bool _pendingProvisioning = false;
  bool _pendingForget = false;
  WifiCredentials _pendingCreds;

  bool _scanRunning = false;
  uint32_t _scanFinishedAt = 0;
  uint8_t _scanCount = 0;
  ScanResult _scan[MAX_SCAN_RESULTS];

  FixedString<39> _apSsid;
  Callback<NetState> _onStateChange;
};

#endif // NETWORK_MANAGER_H
