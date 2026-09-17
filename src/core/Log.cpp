#include "core/Log.h"

namespace {
  // Room for the millis prefix: up to 10 digits plus a space.
  constexpr uint8_t PREFIX_LEN = 11;
  constexpr uint8_t BODY_LEN = Log::LINE_LEN - PREFIX_LEN;

  char _lines[Log::LINES][Log::LINE_LEN];
  uint8_t _head = 0;
  uint8_t _count = 0;
  uint32_t _dropped = 0;

  // Writes the timestamp straight into the ring slot, then hands back where
  // the message body should go. One buffer, no second copy.
  char* claimSlot(size_t& bodyRoom) {
    char* slot = _lines[_head];
    const int used = snprintf(slot, Log::LINE_LEN, "%lu ", (unsigned long)millis());
    _head = (_head + 1) % Log::LINES;
    if (_count < Log::LINES) {
      ++_count;
    } else {
      ++_dropped;
    }
    bodyRoom = (used > 0 && (size_t)used < Log::LINE_LEN) ? Log::LINE_LEN - used : 1;
    return (used > 0) ? slot + used : slot;
  }
}

void Log::begin(uint32_t baud) {
  Serial.begin(baud);
  Serial.println();
  _head = 0;
  _count = 0;
  _dropped = 0;
}

void Log::printf(const char* fmt, ...) {
  size_t room = 0;
  char* body = claimSlot(room);

  va_list ap;
  va_start(ap, fmt);
  vsnprintf_P(body, room, fmt, ap);
  va_end(ap);

  Serial.println(_lines[(_head + LINES - 1) % LINES]);
}

void Log::print(const char* text) {
  size_t room = 0;
  char* body = claimSlot(room);

  strncpy_P(body, text, room);
  body[room - 1] = '\0';

  Serial.println(_lines[(_head + LINES - 1) % LINES]);
}

uint8_t Log::count() {
  return _count;
}

const char* Log::at(uint8_t i) {
  if (i >= _count) {
    return "";
  }
  // _head points at the next slot to write, which is the oldest retained
  // entry once the buffer has wrapped.
  const uint8_t start = (_count == Log::LINES) ? _head : 0;
  return _lines[(start + i) % Log::LINES];
}

uint32_t Log::droppedCount() {
  return _dropped;
}
