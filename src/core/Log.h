#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

// Writes to Serial and keeps the most recent lines in a fixed ring buffer so
// they can be read back over HTTP after the serial adapter is gone.
namespace Log {
  constexpr uint8_t LINES = 12;
  constexpr uint8_t LINE_LEN = 72;

  void begin(uint32_t baud);

  // Format string lives in flash; wrap call sites in PSTR().
  void printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
  void print(const char* text);

  uint8_t count();
  // 0 is the oldest retained line.
  const char* at(uint8_t i);
  uint32_t droppedCount();
}

#define LOGF(fmt, ...) Log::printf(PSTR(fmt), ##__VA_ARGS__)
#define LOGS(text) Log::print(PSTR(text))

#endif // LOG_H
