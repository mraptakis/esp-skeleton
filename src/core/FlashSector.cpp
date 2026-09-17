#include "core/FlashSector.h"
#include <flash_hal.h>

namespace {
  constexpr uint32_t FLASH_MEM_BASE = 0x40200000;
}

FlashSector::FlashSector(uint32_t mappedAddr)
  : _offset(mappedAddr - FLASH_MEM_BASE),
    _sector(_offset / SIZE) {}

namespace {
  // The SDK's flash calls require a word-aligned buffer; a caller passing a
  // plain byte array on the stack wouldn't get that for free.
  bool isWordAligned(const void* p) {
    return (reinterpret_cast<uintptr_t>(p) % 4) == 0;
  }
}

bool FlashSector::read(void* dst, size_t len) const {
  if (len == 0 || len > SIZE || (len % 4) != 0 || !isWordAligned(dst)) {
    return false;
  }
  return ESP.flashRead(_offset, reinterpret_cast<uint32_t*>(dst), len);
}

bool FlashSector::write(const void* src, size_t len) const {
  if (len == 0 || len > SIZE || (len % 4) != 0 || !isWordAligned(src)) {
    return false;
  }
  if (!ESP.flashEraseSector(_sector)) {
    return false;
  }
  return ESP.flashWrite(_offset, const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(src)), len);
}
