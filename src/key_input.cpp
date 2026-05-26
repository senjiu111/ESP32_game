#include "key_input.h"

namespace key_input {

size_t KeyInput::toIndex(KeyId key) {
  return static_cast<size_t>(key);
}

void KeyInput::begin() {
  for (const auto &key : kKeys) {
    pinMode(key.pin, INPUT_PULLDOWN);
  }

  const uint32_t now = millis();
  for (size_t i = 0; i < kKeyCount; ++i) {
    const bool pressed = digitalRead(kKeys[i].pin) == HIGH;
    runtime_[i].rawPressed = pressed;
    runtime_[i].stablePressed = pressed;
    runtime_[i].lastRawChangeMs = now;
  }
}

size_t KeyInput::poll(KeyEvent *events, size_t maxEvents) {
  size_t eventCount = 0;
  const uint32_t now = millis();

  for (size_t i = 0; i < kKeyCount; ++i) {
    const bool rawPressed = digitalRead(kKeys[i].pin) == HIGH;
    if (rawPressed != runtime_[i].rawPressed) {
      runtime_[i].rawPressed = rawPressed;
      runtime_[i].lastRawChangeMs = now;
    }

    if (runtime_[i].stablePressed == runtime_[i].rawPressed) {
      continue;
    }

    if (now - runtime_[i].lastRawChangeMs < kDebounceMs) {
      continue;
    }

    runtime_[i].stablePressed = runtime_[i].rawPressed;
    if (events != nullptr && eventCount < maxEvents) {
      events[eventCount++] = {static_cast<KeyId>(i), runtime_[i].stablePressed,
                              now};
    }
  }

  return eventCount;
}

bool KeyInput::isPressed(KeyId key) const {
  return runtime_[toIndex(key)].stablePressed;
}

const KeyInfo &KeyInput::info(KeyId key) const {
  return kKeys[toIndex(key)];
}

}  // namespace key_input