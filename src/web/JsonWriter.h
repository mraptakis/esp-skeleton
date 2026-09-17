#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include <Arduino.h>

class JsonWriter {
public:
  JsonWriter(char* buffer, size_t capacity)
    : _buf(buffer), _cap(capacity) {
    _buf[0] = '\0';
  }

  template <size_t N>
  explicit JsonWriter(char (&buffer)[N]) : JsonWriter(buffer, N) {}

  // --- structure ---
  JsonWriter& beginObject() {
    sep();
    put('{');
    _first = true;
    return *this;
  }

  JsonWriter& endObject() {
    put('}');
    _first = false;
    return *this;
  }

  JsonWriter& beginArray() {
    sep();
    put('[');
    _first = true;
    return *this;
  }

  JsonWriter& endArray() {
    put(']');
    _first = false;
    return *this;
  }

  JsonWriter& beginObject(const char* key) {
    key_(key);
    put('{');
    _first = true;
    return *this;
  }

  JsonWriter& beginArray(const char* key) {
    key_(key);
    put('[');
    _first = true;
    return *this;
  }

  JsonWriter& add(const char* key, const char* v) {
    key_(key);
    str(v);
    return *this;
  }

  JsonWriter& add(const char* key, const String& v) {
    key_(key);
    str(v.c_str());
    return *this;
  }

  JsonWriter& add(const char* key, bool v) {
    key_(key);
    raw(v ? "true" : "false");
    return *this;
  }

  JsonWriter& add(const char* key, int32_t v) {
    key_(key);
    num("%ld", (long)v);
    return *this;
  }

  JsonWriter& add(const char* key, uint32_t v) {
    key_(key);
    num("%lu", (unsigned long)v);
    return *this;
  }

  JsonWriter& addNull(const char* key) {
    key_(key);
    raw("null");
    return *this;
  }

  JsonWriter& push(const char* v) {
    sep();
    str(v);
    return *this;
  }

  JsonWriter& push(int32_t v) {
    sep();
    num("%ld", (long)v);
    return *this;
  }

  JsonWriter& push(bool v) {
    sep();
    raw(v ? "true" : "false");
    return *this;
  }

  const char* c_str() const {
    return _buf;
  }

  size_t length() const {
    return _len;
  }

  bool ok() const {
    return !_over;
  }

  void reset() {
    _len = 0;
    _buf[0] = '\0';
    _first = true;
    _over = false;
  }

private:
  void put(char c) {
    if (_len + 1 >= _cap) {
      _over = true;
      return;
    }
    _buf[_len++] = c;
    _buf[_len] = '\0';
  }

  void raw(const char* s) {
    while (*s) {
      put(*s++);
    }
  }

  void num(const char* fmt, ...) {
    char tmp[16];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    raw(tmp);
  }

  void sep() {
    if (!_first) {
      put(',');
    }
    _first = false;
  }

  void key_(const char* key) {
    sep();
    str(key);
    put(':');
  }

  void str(const char* s) {
    put('"');
    if (s) {
      for (; *s; ++s) {
        const char c = *s;
        switch (c) {
          case '"':
            raw("\\\"");
            break;
          case '\\':
            raw("\\\\");
            break;
          case '\n':
            raw("\\n");
            break;
          case '\r':
            raw("\\r");
            break;
          case '\t':
            raw("\\t");
            break;
          case '\b':
            raw("\\b");
            break;
          case '\f':
            raw("\\f");
            break;
          default:
            if ((uint8_t)c < 0x20) {
              char u[7];
              snprintf(u, sizeof(u), "\\u%04x", (uint8_t)c);
              raw(u);
            } else {
              put(c);
            }
        }
      }
    }
    put('"');
  }

  char* _buf;
  size_t _cap;
  size_t _len = 0;
  bool _first = true;
  bool _over = false;
};

#endif // JSON_WRITER_H
