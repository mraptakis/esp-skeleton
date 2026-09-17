#ifndef RESPONSE_BUFFER_H
#define RESPONSE_BUFFER_H

#include <Arduino.h>

namespace ResponseBuffer {
  constexpr size_t SIZE = 768;
  extern char data[SIZE];
}

#endif // RESPONSE_BUFFER_H
