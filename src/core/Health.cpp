#include "core/Health.h"
#include "core/Log.h"
#include "Config.h"

namespace {
  // Offset is counted in 4-byte blocks. Block 32 keeps clear of anything the
  // SDK or a future OTA stub might claim at the start of the region.
  constexpr uint32_t RTC_BLOCK = 32;
  constexpr uint32_t MAGIC = 0x48454C31; // "HEL1"

  struct RtcHealth {
    uint32_t magic;
    uint32_t bootCount;
    uint32_t check;
  };

  uint32_t checksum(uint32_t magic, uint32_t count) {
    return (magic ^ 0xA5A5A5A5u) + (count * 2654435761u);
  }
}

bool Health::begin() {
  RtcHealth rec;
  bool valid = false;

  if (ESP.rtcUserMemoryRead(RTC_BLOCK, (uint32_t*)&rec, sizeof(rec))) {
    if (rec.magic == MAGIC && rec.check == checksum(rec.magic, rec.bootCount)) {
      valid = true;
    }
  }

  // No valid record means a cold boot: power was lost, or this is the first
  // run after a serial flash. Either way the count starts again.
  _bootCount = valid ? rec.bootCount + 1 : 1;
  writeCount(_bootCount);

  _safeMode = _bootCount > Config::SAFE_MODE_BOOT_THRESHOLD;
  _stable = false;

  if (_safeMode) {
    LOGF("[health] boot %lu, entering safe mode", (unsigned long)_bootCount);
  } else if (_bootCount > 1) {
    LOGF("[health] boot %lu since last stable run", (unsigned long)_bootCount);
  }

  return _safeMode;
}

void Health::tick() {
  if (_stable) {
    return;
  }
  if (millis() < Config::HEALTHY_UPTIME_MS) {
    return;
  }

  _stable = true;
  writeCount(0);
  LOGS("[health] stable, boot counter cleared");
}

void Health::writeCount(uint32_t value) {
  RtcHealth rec;
  rec.magic = MAGIC;
  rec.bootCount = value;
  rec.check = checksum(rec.magic, rec.bootCount);
  ESP.rtcUserMemoryWrite(RTC_BLOCK, (uint32_t*)&rec, sizeof(rec));
}

bool Health::isSafeMode() const {
  return _safeMode;
}

uint32_t Health::bootCount() const {
  return _bootCount;
}

bool Health::isStable() const {
  return _stable;
}

void Health::clearAndRestart() {
  writeCount(0);
  LOGS("[health] counter cleared, restarting");
  delay(100);
  ESP.restart();
}
