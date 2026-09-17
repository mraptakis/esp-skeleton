#ifndef CALLBACK_H
#define CALLBACK_H

// Fixed-size replacement for std::function<void(Args...)> for the
// "notify one owner object" pattern used across App/NetworkManager/OtaService.
// Stores an instance pointer plus a plain function pointer thunk — no heap,
// no type-erasure allocation, works with -fno-rtti/-fno-exceptions.
template <typename... Args>
class Callback {
public:
  Callback() = default;

  template <typename T, void (T::*Method)(Args...)>
  void bind(T* instance) {
    _instance = instance;
    _thunk = &invoke<T, Method>;
  }

  void clear() {
    _instance = nullptr;
    _thunk = nullptr;
  }

  explicit operator bool() const { return _thunk != nullptr; }

  void operator()(Args... args) const {
    if (_thunk) {
      _thunk(_instance, args...);
    }
  }

private:
  using Thunk = void (*)(void*, Args...);

  template <typename T, void (T::*Method)(Args...)>
  static void invoke(void* instance, Args... args) {
    (static_cast<T*>(instance)->*Method)(args...);
  }

  void* _instance = nullptr;
  Thunk _thunk = nullptr;
};

#endif // CALLBACK_H
