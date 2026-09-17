#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <Arduino.h>
#include "core/FixedString.h"
#include "core/FlashSector.h"

struct WifiCredentials {
  static constexpr size_t SSID_MAX_LEN = 32;
  static constexpr size_t PASS_MAX_LEN = 63;

  FixedString<SSID_MAX_LEN> ssid;
  FixedString<PASS_MAX_LEN> password;

  bool isValid() const {
    return !ssid.overflowed() && !password.overflowed() && ssid.length() > 0;
  }

  void clear() {
    ssid.clear();
    password.clear();
  }
};

struct BrokerSettings {
  static constexpr size_t FIELD_MAX_LEN = 64;

  FixedString<FIELD_MAX_LEN> host;
  uint16_t port = 1883;
  FixedString<FIELD_MAX_LEN> username;
  FixedString<FIELD_MAX_LEN> password;
  FixedString<FIELD_MAX_LEN> baseTopic;

  bool isConfigured() const {
    return host.length() > 0;
  }

  bool isValid() const {
    return !host.overflowed() && !username.overflowed() &&
           !password.overflowed() && !baseTopic.overflowed() && port != 0;
  }

  void clear() {
    host.clear();
    port = 1883;
    username.clear();
    password.clear();
    baseTopic.clear();
  }
};

struct DeviceSettings {
  static constexpr size_t FIELD_MAX_LEN = 31;

  FixedString<FIELD_MAX_LEN> hostname;
  FixedString<FIELD_MAX_LEN> friendlyName;

  bool isValid() const {
    return !hostname.overflowed() && !friendlyName.overflowed();
  }
};

enum class ConfigError : uint8_t {
  None,
  MountFailed,
  NotProvisioned,
  WriteFailed,
  Corrupt
};

class ConfigStore {
public:
  static constexpr uint8_t SCHEMA_VERSION = 1;

  ConfigStore();

  bool begin();

  const WifiCredentials& wifi() const;
  const DeviceSettings& settings() const;

  bool saveWifi(const WifiCredentials& creds);
  bool clearWifi();
  bool saveSettings(const DeviceSettings& s);

  bool factoryReset();

  bool isProvisioned() const;

  ConfigError lastError() const;

  const BrokerSettings& broker() const;
  bool saveBroker(const BrokerSettings& b);
  bool clearBroker();
private:
  // On-flash layout. Packed and explicitly padded to a multiple of 4 bytes:
  // the SDK's raw flash read/write calls are word-addressed.
  #pragma pack(push, 1)
  struct ConfigBlob {
    uint32_t magic;
    uint8_t  schemaVersion;
    char     ssid[WifiCredentials::SSID_MAX_LEN + 1];
    char     password[WifiCredentials::PASS_MAX_LEN + 1];
    char     hostname[DeviceSettings::FIELD_MAX_LEN + 1];
    char     friendlyName[DeviceSettings::FIELD_MAX_LEN + 1];
    char     brokerHost[BrokerSettings::FIELD_MAX_LEN + 1];
    uint16_t brokerPort;
    char     brokerUser[BrokerSettings::FIELD_MAX_LEN + 1];
    char     brokerPass[BrokerSettings::FIELD_MAX_LEN + 1];
    char     brokerTopic[BrokerSettings::FIELD_MAX_LEN + 1];
    uint16_t crc;
  };
  #pragma pack(pop)

  static constexpr uint32_t BLOB_MAGIC = 0x534b4630; // "SKF0"
  static constexpr size_t BLOB_IO_SIZE = (sizeof(ConfigBlob) + 3) & ~size_t(3);
  static_assert(BLOB_IO_SIZE <= FlashSector::SIZE, "ConfigBlob no longer fits one flash sector");

  bool migrate(ConfigBlob& blob, uint8_t fromVersion);
  void applyBlob(const ConfigBlob& blob);
  ConfigBlob buildBlob() const;
  static uint16_t crc16(const uint8_t* data, size_t len);
  bool persist(uint8_t schemaVersion);

  FlashSector _sector;
  WifiCredentials _wifi;
  DeviceSettings _settings;
  bool _ready = false;
  ConfigError _lastError = ConfigError::None;
  BrokerSettings _broker;
};

#endif // CONFIG_STORE_H
