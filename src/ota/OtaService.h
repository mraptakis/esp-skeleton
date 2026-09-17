#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <Arduino.h>
#include "core/Callback.h"

class OtaService {
public:
  void begin(const char* hostname, const char* password);
  void tick();

  template <typename T, void (T::*Method)()>
  void onStart(T* instance) { _onStart.bind<T, Method>(instance); }

  template <typename T, void (T::*Method)(uint8_t)>
  void onProgress(T* instance) { _onProgress.bind<T, Method>(instance); }

  template <typename T, void (T::*Method)()>
  void onEnd(T* instance) { _onEnd.bind<T, Method>(instance); }

private:
  Callback<> _onStart;
  Callback<uint8_t> _onProgress;
  Callback<> _onEnd;
};

#endif // OTA_SERVICE_H
