#ifndef DINO_GAME_H
#define DINO_GAME_H

#include <Arduino.h>

#include <cstddef>

#include "esp32_lcd.h"
#include "key_input.h"

class DinoGame {
 public:
  void begin();
  void update(const key_input::KeyEvent *events, size_t eventCount);
  bool needsRender() const;
  void render(esp32_lcd::ST7789Ascii &lcd);

 private:
  static constexpr int kScreenCols = 40;
  static constexpr int kScreenRows = 15;
  static constexpr int kPlayerCol = 5;
  static constexpr int kGroundRow = 10;
  static constexpr int kGroundLineRow = 11;

  void reset();
  void restartIfNeeded();
  void tick();
  void triggerJump();
  void spawnObstacle();
  void buildFrame(char lines[kScreenRows][kScreenCols + 1]) const;

  bool started_ = false;
  bool gameOver_ = false;
  bool jumpQueued_ = false;
  bool jumpHeld_ = false;
  bool ducking_ = false;
  float playerY_ = static_cast<float>(kGroundRow);
  float playerVelocity_ = 0.0f;
  float obstacleX_ = 39.0f;
  int obstacleKind_ = 0;
  float obstacleSpeed_ = 11.0f;
  uint32_t score_ = 0;
  uint32_t lastTickMs_ = 0;
  uint32_t lastRenderMs_ = 0;
  uint32_t jumpBufferUntilMs_ = 0;
  uint32_t coyoteUntilMs_ = 0;
  bool dirty_ = true;
  bool sceneInitialized_ = false;
  bool prevGameOverRendered_ = false;
  int16_t prevDinoX_ = 0;
  int16_t prevDinoY_ = 0;
  uint16_t prevDinoW_ = 0;
  uint16_t prevDinoH_ = 0;
  int16_t prevObstacleX_ = 0;
  int16_t prevObstacleY_ = 0;
  uint16_t prevObstacleW_ = 0;
  uint16_t prevObstacleH_ = 0;
  int16_t prevCloudX_ = 0;
  int16_t prevCloudY_ = 0;
  uint16_t prevCloudW_ = 0;
  uint16_t prevCloudH_ = 0;
  int16_t prevGroundScroll_ = -1;
  bool hudInitialized_ = false;
  uint32_t lastScoreRendered_ = 0;
  char message_[48] = "";
};

#endif
