#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <Arduino.h>

#include <cstddef>

#include "esp32_lcd.h"
#include "key_input.h"

class SnakeGame {
 public:
  enum class Direction : uint8_t {
    kUp,
    kDown,
    kLeft,
    kRight,
  };

  void begin();
  void update(const key_input::KeyEvent *events, size_t eventCount);
  bool needsRender() const;
  void render(esp32_lcd::ST7789Ascii &lcd);

 private:
  struct Cell {
    int8_t x;
    int8_t y;
  };

  static constexpr uint8_t kGridSize = 16;
  static constexpr uint16_t kMaxCells = kGridSize * kGridSize;

  void reset();
  void tick();
  void setDirection(Direction direction);
  void spawnFood();
  bool isOpposite(Direction a, Direction b) const;
  bool occupies(int8_t x, int8_t y, uint16_t ignoreTailCount = 0) const;
  int16_t snakeIndexAt(int8_t x, int8_t y) const;
  uint16_t cellSignature(uint8_t x, uint8_t y) const;

  Direction direction_ = Direction::kRight;
  Direction pendingDirection_ = Direction::kRight;
  Cell snake_[kMaxCells]{};
  Cell food_{};
  uint32_t score_ = 0;
  uint32_t lastMoveMs_ = 0;
  uint16_t length_ = 5;
  uint16_t renderedCells_[kGridSize][kGridSize]{};
  uint32_t renderedScore_ = UINT32_MAX;
  uint16_t renderedLength_ = UINT16_MAX;
  Direction renderedDirection_ = Direction::kRight;
  bool renderedGameOver_ = false;
  bool gameOver_ = false;
  bool fullRedraw_ = true;
  bool dirty_ = true;
};

#endif
