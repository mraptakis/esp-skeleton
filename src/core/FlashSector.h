#ifndef FLASH_SECTOR_H
#define FLASH_SECTOR_H

#include <Arduino.h>

// One raw 4KB flash sector, read/written as a whole blob. No heap: the
// caller owns the buffer (unlike EEPROM.h, whose begin() heap-allocates its
// RAM mirror with `new`). `len` must be a multiple of 4 — the SDK's flash
// read/write calls are word-addressed; callers pad their struct to match.
class FlashSector {
public:
  static constexpr size_t SIZE = 4096;

  // addr is the memory-mapped address (e.g. the linker-provided
  // _EEPROM_start), not a raw flash offset — the conversion happens here.
  explicit FlashSector(uint32_t mappedAddr);

  bool read(void* dst, size_t len) const;
  bool write(const void* src, size_t len) const;

private:
  uint32_t _offset; // byte offset from the start of the flash chip
  uint32_t _sector;  // sector index, for erase
};

#endif // FLASH_SECTOR_H
