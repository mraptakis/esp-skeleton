#ifndef FIXED_STRING_H
#define FIXED_STRING_H

#include <Arduino.h>
#include <string.h>

// Fixed-capacity, stack/static-storage replacement for Arduino String.
// Never heap-allocates. Assigning a value longer than N leaves the field
// cleared and sets the overflow flag instead of silently truncating, so
// callers can still reject oversized input the same way they rejected an
// over-length String before.
template <size_t N>
class FixedString {
public:
  FixedString() { _buf[0] = '\0'; }

  bool set(const char* s) {
    if (!s) s = "";
    const size_t len = strlen(s);
    if (len > N) {
      _buf[0] = '\0';
      _len = 0;
      _overflowed = true;
      return false;
    }
    memcpy(_buf, s, len + 1);
    _len = len;
    _overflowed = false;
    return true;
  }

  FixedString& operator=(const char* s) { set(s); return *this; }
  FixedString& operator=(const String& s) { set(s.c_str()); return *this; }

  void clear() {
    _buf[0] = '\0';
    _len = 0;
    _overflowed = false;
  }

  // In-place, ASCII-whitespace trim, mirroring String::trim().
  void trim() {
    char* start = _buf;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
      ++start;
    }
    char* end = _buf + _len;
    while (end > start &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) {
      --end;
    }
    _len = (size_t)(end - start);
    if (start != _buf) {
      memmove(_buf, start, _len);
    }
    _buf[_len] = '\0';
  }

  const char* c_str() const { return _buf; }
  size_t length() const { return _len; }
  bool empty() const { return _len == 0; }

  // True after a set()/operator= that was rejected for being too long.
  // Cleared again by the next successful set().
  bool overflowed() const { return _overflowed; }

  static constexpr size_t capacity() { return N; }

private:
  char _buf[N + 1];
  size_t _len = 0;
  bool _overflowed = false;
};

#endif // FIXED_STRING_H
