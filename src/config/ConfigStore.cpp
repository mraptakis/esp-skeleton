#include "config/ConfigStore.h"
#include "core/Log.h"
#include <flash_hal.h>
#include <string.h>

ConfigStore::ConfigStore() : _sector(EEPROM_start) {}

uint16_t ConfigStore::crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; ++b) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
  }
  return crc;
}

void ConfigStore::applyBlob(const ConfigBlob& blob) {
  _wifi.ssid = blob.ssid;
  _wifi.password = blob.password;
  _settings.hostname = blob.hostname;
  _settings.friendlyName = blob.friendlyName;
  _broker.host = blob.brokerHost;
  _broker.port = blob.brokerPort;
  _broker.username = blob.brokerUser;
  _broker.password = blob.brokerPass;
  _broker.baseTopic = blob.brokerTopic;
}

ConfigStore::ConfigBlob ConfigStore::buildBlob() const {
  ConfigBlob blob{};
  blob.magic = BLOB_MAGIC;
  blob.schemaVersion = SCHEMA_VERSION;
  strlcpy(blob.ssid, _wifi.ssid.c_str(), sizeof(blob.ssid));
  strlcpy(blob.password, _wifi.password.c_str(), sizeof(blob.password));
  strlcpy(blob.hostname, _settings.hostname.c_str(), sizeof(blob.hostname));
  strlcpy(blob.friendlyName, _settings.friendlyName.c_str(), sizeof(blob.friendlyName));
  strlcpy(blob.brokerHost, _broker.host.c_str(), sizeof(blob.brokerHost));
  blob.brokerPort = _broker.port;
  strlcpy(blob.brokerUser, _broker.username.c_str(), sizeof(blob.brokerUser));
  strlcpy(blob.brokerPass, _broker.password.c_str(), sizeof(blob.brokerPass));
  strlcpy(blob.brokerTopic, _broker.baseTopic.c_str(), sizeof(blob.brokerTopic));
  blob.crc = crc16(reinterpret_cast<const uint8_t*>(&blob), offsetof(ConfigBlob, crc));
  return blob;
}

bool ConfigStore::persist(uint8_t schemaVersion) {
  ConfigBlob blob = buildBlob();
  blob.schemaVersion = schemaVersion;
  blob.crc = crc16(reinterpret_cast<const uint8_t*>(&blob), offsetof(ConfigBlob, crc));

  // uint32_t, not uint8_t: the SDK's flash calls require a word-aligned
  // buffer, which a plain byte array on the stack doesn't guarantee.
  uint32_t io[BLOB_IO_SIZE / 4] = {0};
  memcpy(io, &blob, sizeof(blob));
  return _sector.write(io, BLOB_IO_SIZE);
}

bool ConfigStore::begin() {
  _ready = false;
  _lastError = ConfigError::None;

  uint32_t io[BLOB_IO_SIZE / 4] = {0};
  if (!_sector.read(io, BLOB_IO_SIZE)) {
    _lastError = ConfigError::MountFailed;
    return false;
  }

  ConfigBlob blob{};
  memcpy(&blob, io, sizeof(blob));

  if (blob.magic != BLOB_MAGIC) {
    LOGS("[cfg] no valid record, initializing blank");
    _wifi.clear();
    _broker.clear();
    _settings = DeviceSettings{};
    _ready = true;
    _lastError = persist(SCHEMA_VERSION) ? ConfigError::NotProvisioned : ConfigError::WriteFailed;
    return true;
  }

  const uint16_t crc = crc16(reinterpret_cast<const uint8_t*>(&blob), offsetof(ConfigBlob, crc));
  if (crc != blob.crc) {
    LOGS("[cfg] checksum mismatch, discarding record");
    _wifi.clear();
    _broker.clear();
    _settings = DeviceSettings{};
    _ready = true;
    persist(SCHEMA_VERSION);
    _lastError = ConfigError::Corrupt;
    return true;
  }

  if (blob.schemaVersion < SCHEMA_VERSION) {
    if (!migrate(blob, blob.schemaVersion)) {
      _lastError = ConfigError::Corrupt;
      return false;
    }
  } else if (blob.schemaVersion > SCHEMA_VERSION) {
    LOGF("[cfg] schema %u newer than %u", blob.schemaVersion, SCHEMA_VERSION);
    _lastError = ConfigError::Corrupt;
    return false;
  }

  applyBlob(blob);
  _ready = true;

  if (!_wifi.isValid()) {
    _lastError = ConfigError::NotProvisioned;
  }
  return true;
}

const WifiCredentials& ConfigStore::wifi() const     { return _wifi; }
const DeviceSettings&  ConfigStore::settings() const { return _settings; }
const BrokerSettings&  ConfigStore::broker() const   { return _broker; }

bool        ConfigStore::isProvisioned() const { return _wifi.isValid(); }
ConfigError ConfigStore::lastError() const     { return _lastError; }

bool ConfigStore::saveWifi(const WifiCredentials& creds) {
  if (!_ready) return false;

  if (!creds.isValid()) {
    _lastError = ConfigError::WriteFailed;
    return false;
  }

  const WifiCredentials prev = _wifi;
  _wifi = creds;
  if (!persist(SCHEMA_VERSION)) {
    _wifi = prev;
    _lastError = ConfigError::WriteFailed;
    return false;
  }

  _lastError = ConfigError::None;
  return true;
}

bool ConfigStore::clearWifi() {
  if (!_ready) return false;

  _wifi.clear();
  persist(SCHEMA_VERSION);
  _lastError = ConfigError::NotProvisioned;
  return true;
}

bool ConfigStore::saveSettings(const DeviceSettings& s) {
  if (!_ready) return false;

  if (!s.isValid()) return false;

  const DeviceSettings prev = _settings;
  _settings = s;
  if (!persist(SCHEMA_VERSION)) {
    _settings = prev;
    _lastError = ConfigError::WriteFailed;
    return false;
  }

  return true;
}

bool ConfigStore::saveBroker(const BrokerSettings& b) {
  if (!_ready) return false;

  if (!b.isValid()) return false;

  const BrokerSettings prev = _broker;
  _broker = b;
  if (!persist(SCHEMA_VERSION)) {
    _broker = prev;
    _lastError = ConfigError::WriteFailed;
    return false;
  }

  return true;
}

bool ConfigStore::clearBroker() {
  if (!_ready) return false;

  _broker.clear();
  persist(SCHEMA_VERSION);
  return true;
}

bool ConfigStore::factoryReset() {
  if (!_ready) return false;

  _wifi.clear();
  _broker.clear();
  _settings = DeviceSettings{};
  persist(SCHEMA_VERSION);

  _lastError = ConfigError::NotProvisioned;
  return true;
}

bool ConfigStore::migrate(ConfigBlob& blob, uint8_t fromVersion) {
  LOGF("[cfg] migrating %u -> %u", fromVersion, SCHEMA_VERSION);

  // Fall through from the oldest version forward:
  // switch (fromVersion) {
  //   case 1: /* v1 -> v2 */  [[fallthrough]];
  //   case 2: /* v2 -> v3 */  break;
  // }

  blob.schemaVersion = SCHEMA_VERSION;
  return true;
}
