#ifndef GAME2048_H
#define GAME2048_H

#include <Arduino.h>

#include <cstddef>

#include "esp32_lcd.h"
#include "key_input.h"

class Game2048 {
 public:
  void begin();
  void update(const key_input::KeyEvent *events, size_t eventCount);
  bool needsRender() const;
  void render(esp32_lcd::ST7789Ascii &lcd);

 private:
  enum class Direction : uint8_t {
    kUp,
    kDown,
    kLeft,
    kRight,
  };

  void reset();
  bool move(Direction dir);
  bool processLine(uint16_t line[4]);
  void spawnTile();
  bool hasEmptyCell() const;
  bool canMove() const;
  bool hasWon() const;

  uint16_t board_[4][4]{};
  uint16_t renderedBoard_[4][4]{};
  uint32_t score_ = 0;
  uint32_t bestScore_ = 0;
  uint32_t renderedScore_ = UINT32_MAX;
  uint32_t renderedBestScore_ = UINT32_MAX;
  bool gameOver_ = false;
  bool won_ = false;
  bool renderedGameOver_ = false;
  bool renderedWon_ = false;
  bool fullRedraw_ = true;
  bool dirty_ = true;
};

#endif
