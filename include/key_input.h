#ifndef KEY_INPUT_H
#define KEY_INPUT_H

#include <Arduino.h>

#include <array>
#include <cstddef>

namespace key_input {

enum class KeyId : size_t {
  kUp = 0,
  kDown,
  kLeft,
  kRight,
  kOk,
  kBack,
  kCount,
};

struct KeyEvent {
  KeyId key;
  bool pressed;
  uint32_t timeMs;
};

struct KeyInfo {
  const char *name;
  const char *action;
  int pin;
};

class KeyInput {
 public:
  static constexpr size_t kKeyCount = static_cast<size_t>(KeyId::kCount);

  void begin();
  size_t poll(KeyEvent *events, size_t maxEvents);
  bool isPressed(KeyId key) const;
  const KeyInfo &info(KeyId key) const;

 private:
  struct KeyRuntime {
    bool rawPressed = false;
    bool stablePressed = false;
    uint32_t lastRawChangeMs = 0;
  };

  std::array<KeyRuntime, kKeyCount> runtime_{};

  static constexpr uint32_t kDebounceMs = 30;
  inline static constexpr std::array<KeyInfo, kKeyCount> kKeys = {{
      {"key1", "UP", 4},
      {"key2", "DOWN", 5},
      {"key3", "LEFT", 6},
      {"key4", "RIGHT", 7},
      {"key5", "OK", 15},
      {"key6", "BACK", 16},
  }};

  static size_t toIndex(KeyId key);
};

}  // namespace key_input

#endif
