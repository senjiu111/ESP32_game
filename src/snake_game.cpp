#include "snake_game.h"

#include <algorithm>
#include <cstdio>

namespace {

constexpr int16_t kFieldX = 6;
constexpr int16_t kFieldY = 8;
constexpr uint16_t kFieldSize = 224;
constexpr uint8_t kGrid = 16;
constexpr uint16_t kCell = 14;
constexpr int16_t kPanelX = 238;
constexpr uint16_t kPanelW = 82;
constexpr uint16_t kLcdH = 240;
constexpr uint32_t kMoveIntervalMs = 220;

constexpr uint16_t kBg = esp32_lcd::COLOR_WHITE;
constexpr uint16_t kFg = esp32_lcd::COLOR_BLACK;
constexpr uint16_t kHi = esp32_lcd::COLOR_RED;
constexpr uint16_t kDim = 0x8410;
constexpr uint16_t kFieldColor = esp32_lcd::COLOR_WHITE;
constexpr uint16_t kGridColor = 0xEF7D;
constexpr uint16_t kSnakeColor = esp32_lcd::COLOR_BLACK;
constexpr uint16_t kSnakeHeadColor = esp32_lcd::COLOR_BLACK;
constexpr uint16_t kFoodColor = 0xF800;
constexpr uint16_t kFoodLeafColor = 0x07E0;

int16_t roundedInset(uint8_t row, uint8_t radius) {
  const int16_t cy = static_cast<int16_t>(radius - 1U - row);
  int16_t dx = 0;
  while (dx < radius && dx * dx + cy * cy <= radius * radius) {
    ++dx;
  }
  return std::max<int16_t>(0, static_cast<int16_t>(radius - dx));
}

void fillRoundRect(esp32_lcd::ST7789Ascii &lcd, int16_t x, int16_t y,
                   uint16_t w, uint16_t h, uint8_t radius, uint16_t color) {
  if (w == 0 || h == 0) {
    return;
  }
  if (radius == 0 || w <= radius * 2U || h <= radius * 2U) {
    lcd.fillRect(x, y, w, h, color);
    return;
  }

  lcd.fillRect(static_cast<int16_t>(x + radius), y,
               static_cast<uint16_t>(w - radius * 2U), h, color);
  lcd.fillRect(x, static_cast<int16_t>(y + radius), w,
               static_cast<uint16_t>(h - radius * 2U), color);

  for (uint8_t row = 0; row < radius; ++row) {
    const int16_t inset = roundedInset(row, radius);
    const uint16_t span = static_cast<uint16_t>(w - inset * 2);
    lcd.fillRect(static_cast<int16_t>(x + inset), static_cast<int16_t>(y + row),
                 span, 1, color);
    lcd.fillRect(static_cast<int16_t>(x + inset),
                 static_cast<int16_t>(y + h - 1 - row), span, 1, color);
  }
}

void drawGrid(esp32_lcd::ST7789Ascii &lcd) {
  fillRoundRect(lcd, kFieldX, kFieldY, kFieldSize, kFieldSize, 8, kFieldColor);
  fillRoundRect(lcd, kFieldX, kFieldY, kFieldSize, kFieldSize, 8, kGridColor);
  fillRoundRect(lcd, static_cast<int16_t>(kFieldX + 1),
                static_cast<int16_t>(kFieldY + 1),
                static_cast<uint16_t>(kFieldSize - 2),
                static_cast<uint16_t>(kFieldSize - 2), 7, kFieldColor);
  for (uint8_t i = 1; i < kGrid; ++i) {
    const int16_t pos = static_cast<int16_t>(kFieldX + i * kCell);
    lcd.fillRect(pos, kFieldY, 1, kFieldSize, kGridColor);
    lcd.fillRect(kFieldX, static_cast<int16_t>(kFieldY + i * kCell),
                 kFieldSize, 1, kGridColor);
  }
}

uint8_t directionBits(int8_t fromX, int8_t fromY, int8_t toX, int8_t toY) {
  if (toY < fromY) {
    return 0x01;
  }
  if (toY > fromY) {
    return 0x02;
  }
  if (toX < fromX) {
    return 0x04;
  }
  if (toX > fromX) {
    return 0x08;
  }
  return 0;
}

void redrawCellBackground(esp32_lcd::ST7789Ascii &lcd, uint8_t col,
                          uint8_t row) {
  const int16_t x = static_cast<int16_t>(kFieldX + col * kCell);
  const int16_t y = static_cast<int16_t>(kFieldY + row * kCell);
  lcd.fillRect(x, y, kCell, kCell, kFieldColor);

  if (col > 0) {
    lcd.fillRect(x, y, 1, kCell, kGridColor);
  }
  if (row > 0) {
    lcd.fillRect(x, y, kCell, 1, kGridColor);
  }
  if (col == kGrid - 1) {
    lcd.fillRect(static_cast<int16_t>(x + kCell - 1), y, 1, kCell,
                 kGridColor);
  }
  if (row == kGrid - 1) {
    lcd.fillRect(x, static_cast<int16_t>(y + kCell - 1), kCell, 1,
                 kGridColor);
  }
}

void fillCenter(esp32_lcd::ST7789Ascii &lcd, int16_t x, int16_t y,
                uint16_t color) {
  fillRoundRect(lcd, static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 3),
                8, 8, 3, color);
}

void drawConnector(esp32_lcd::ST7789Ascii &lcd, int16_t x, int16_t y,
                   uint8_t bits, uint16_t color) {
  if (bits & 0x01) {
    lcd.fillRect(static_cast<int16_t>(x + 5), y, 4, 8, color);
  }
  if (bits & 0x02) {
    lcd.fillRect(static_cast<int16_t>(x + 5), static_cast<int16_t>(y + 6), 4,
                 8, color);
  }
  if (bits & 0x04) {
    lcd.fillRect(x, static_cast<int16_t>(y + 5), 8, 4, color);
  }
  if (bits & 0x08) {
    lcd.fillRect(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 5), 8,
                 4, color);
  }
}

void drawBody(esp32_lcd::ST7789Ascii &lcd, int16_t x, int16_t y,
              uint8_t bits) {
  drawConnector(lcd, x, y, bits, kSnakeColor);
  fillCenter(lcd, x, y, kSnakeColor);
}

void drawTail(esp32_lcd::ST7789Ascii &lcd, int16_t x, int16_t y,
              uint8_t bits) {
  drawConnector(lcd, x, y, bits, kSnakeColor);
  fillRoundRect(lcd, static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 4),
                6, 6, 3, kSnakeColor);

  const uint8_t tailEnd = (bits & 0x01) ? 0x02 : (bits & 0x02) ? 0x01 :
                          (bits & 0x04) ? 0x08 : 0x04;
  if (tailEnd & 0x01) {
    fillRoundRect(lcd, static_cast<int16_t>(x + 5), y, 4, 5, 2, kSnakeColor);
  } else if (tailEnd & 0x02) {
    fillRoundRect(lcd, static_cast<int16_t>(x + 5), static_cast<int16_t>(y + 9),
                  4, 5, 2, kSnakeColor);
  } else if (tailEnd & 0x04) {
    fillRoundRect(lcd, x, static_cast<int16_t>(y + 5), 5, 4, 2, kSnakeColor);
  } else {
    fillRoundRect(lcd, static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 5),
                  5, 4, 2, kSnakeColor);
  }
}

void drawHead(esp32_lcd::ST7789Ascii &lcd, int16_t x, int16_t y,
              SnakeGame::Direction direction) {
  fillRoundRect(lcd, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                12, 12, 5, kSnakeHeadColor);

  if (direction == SnakeGame::Direction::kUp) {
    lcd.fillRect(static_cast<int16_t>(x + 4), y, 6, 3, kSnakeHeadColor);
    lcd.fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 4), 2,
                 2, kBg);
    lcd.fillRect(static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 4), 2,
                 2, kBg);
  } else if (direction == SnakeGame::Direction::kDown) {
    lcd.fillRect(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 11), 6,
                 3, kSnakeHeadColor);
    lcd.fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 8), 2,
                 2, kBg);
    lcd.fillRect(static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 8), 2,
                 2, kBg);
  } else if (direction == SnakeGame::Direction::kLeft) {
    lcd.fillRect(x, static_cast<int16_t>(y + 4), 3, 6, kSnakeHeadColor);
    lcd.fillRect(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 3), 2,
                 2, kBg);
    lcd.fillRect(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 9), 2,
                 2, kBg);
  } else {
    lcd.fillRect(static_cast<int16_t>(x + 11), static_cast<int16_t>(y + 4), 3,
                 6, kSnakeHeadColor);
    lcd.fillRect(static_cast<int16_t>(x + 8), static_cast<int16_t>(y + 3), 2,
                 2, kBg);
    lcd.fillRect(static_cast<int16_t>(x + 8), static_cast<int16_t>(y + 9), 2,
                 2, kBg);
  }
}

void drawApple(esp32_lcd::ST7789Ascii &lcd, int16_t x, int16_t y) {
  fillRoundRect(lcd, static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 3),
                12, 10, 5, kFoodColor);
  lcd.fillRect(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 1), 2, 4,
               kFg);
  fillRoundRect(lcd, static_cast<int16_t>(x + 8), static_cast<int16_t>(y + 1),
                4, 3, 2, kFoodLeafColor);
  lcd.fillRect(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 5), 2, 2,
               kBg);
}

void drawCellSignature(esp32_lcd::ST7789Ascii &lcd, uint8_t col, uint8_t row,
                       uint16_t signature) {
  redrawCellBackground(lcd, col, row);

  const uint8_t type = static_cast<uint8_t>(signature & 0x0F);
  if (type == 0) {
    return;
  }

  const int16_t x = static_cast<int16_t>(kFieldX + col * kCell);
  const int16_t y = static_cast<int16_t>(kFieldY + row * kCell);
  const uint8_t bits = static_cast<uint8_t>((signature >> 4) & 0x0F);
  if (type == 1) {
    drawBody(lcd, x, y, bits);
  } else if (type == 2) {
    const auto direction =
        static_cast<SnakeGame::Direction>((signature >> 8) & 0x03);
    drawHead(lcd, x, y, direction);
  } else if (type == 3) {
    drawApple(lcd, x, y);
  } else if (type == 4) {
    drawTail(lcd, x, y, bits);
  }
}

const char *directionText(SnakeGame::Direction direction) {
  switch (direction) {
    case SnakeGame::Direction::kUp:
      return "UP";
    case SnakeGame::Direction::kDown:
      return "DOWN";
    case SnakeGame::Direction::kLeft:
      return "LEFT";
    case SnakeGame::Direction::kRight:
      return "RIGHT";
  }
  return "RIGHT";
}

void drawPanel(esp32_lcd::ST7789Ascii &lcd, uint32_t score, uint16_t length,
               SnakeGame::Direction direction, bool gameOver) {
  char line[16];
  lcd.fillRect(kPanelX, 0, kPanelW, kLcdH, kBg);
  lcd.drawString8x16(kPanelX, 8, "SNAKE", kHi, kBg);
  lcd.drawString8x16(kPanelX, 34, "SCORE", kDim, kBg);
  std::snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(score));
  lcd.drawString8x16(kPanelX, 52, line, kFg, kBg);
  lcd.drawString8x16(kPanelX, 82, "LEN", kDim, kBg);
  std::snprintf(line, sizeof(line), "%u", length);
  lcd.drawString8x16(kPanelX, 100, line, kFg, kBg);
  lcd.drawString8x16(kPanelX, 132, "DIR", kDim, kBg);
  lcd.drawString8x16(kPanelX, 150, directionText(direction), kFg, kBg);
  lcd.drawString8x16(kPanelX, 184, gameOver ? "OVER" : "READY",
                     gameOver ? kHi : kFg, kBg);
  lcd.drawString8x16(kPanelX, 214, gameOver ? "OK" : "BACK", kDim, kBg);
}

}  // namespace

void SnakeGame::begin() {
  reset();
}

void SnakeGame::reset() {
  static bool seeded = false;
  if (!seeded) {
    randomSeed(micros());
    seeded = true;
  }

  direction_ = Direction::kRight;
  pendingDirection_ = Direction::kRight;
  score_ = 0;
  length_ = 5;
  gameOver_ = false;
  lastMoveMs_ = millis();

  for (uint16_t i = 0; i < kMaxCells; ++i) {
    snake_[i] = {-1, -1};
  }
  for (uint16_t i = 0; i < length_; ++i) {
    snake_[i] = {static_cast<int8_t>(7 - i), 7};
  }
  spawnFood();

  fullRedraw_ = true;
  renderedScore_ = UINT32_MAX;
  renderedLength_ = UINT16_MAX;
  renderedGameOver_ = false;
  dirty_ = true;
}

bool SnakeGame::needsRender() const {
  return dirty_;
}

void SnakeGame::update(const key_input::KeyEvent *events, size_t eventCount) {
  bool touched = false;
  for (size_t i = 0; i < eventCount; ++i) {
    const auto &event = events[i];
    if (!event.pressed) {
      continue;
    }

    switch (event.key) {
      case key_input::KeyId::kUp:
        setDirection(Direction::kUp);
        touched = true;
        break;
      case key_input::KeyId::kDown:
        setDirection(Direction::kDown);
        touched = true;
        break;
      case key_input::KeyId::kLeft:
        setDirection(Direction::kLeft);
        touched = true;
        break;
      case key_input::KeyId::kRight:
        setDirection(Direction::kRight);
        touched = true;
        break;
      case key_input::KeyId::kOk:
        if (gameOver_) {
          reset();
          touched = true;
        }
        break;
      default:
        break;
    }
  }

  tick();

  if (touched) {
    dirty_ = true;
  }
}

bool SnakeGame::isOpposite(Direction a, Direction b) const {
  return (a == Direction::kUp && b == Direction::kDown) ||
         (a == Direction::kDown && b == Direction::kUp) ||
         (a == Direction::kLeft && b == Direction::kRight) ||
         (a == Direction::kRight && b == Direction::kLeft);
}

void SnakeGame::setDirection(Direction direction) {
  if (!isOpposite(direction, direction_)) {
    pendingDirection_ = direction;
  }
}

bool SnakeGame::occupies(int8_t x, int8_t y, uint16_t ignoreTailCount) const {
  const uint16_t checkedLength =
      (ignoreTailCount >= length_) ? 0 : static_cast<uint16_t>(length_ - ignoreTailCount);
  for (uint16_t i = 0; i < checkedLength; ++i) {
    if (snake_[i].x == x && snake_[i].y == y) {
      return true;
    }
  }
  return false;
}

void SnakeGame::spawnFood() {
  uint16_t emptyCount = 0;
  for (int8_t y = 0; y < static_cast<int8_t>(kGridSize); ++y) {
    for (int8_t x = 0; x < static_cast<int8_t>(kGridSize); ++x) {
      if (!occupies(x, y)) {
        ++emptyCount;
      }
    }
  }
  if (emptyCount == 0) {
    food_ = {-1, -1};
    gameOver_ = true;
    return;
  }

  const uint16_t target = static_cast<uint16_t>(random(emptyCount));
  uint16_t seen = 0;
  for (int8_t y = 0; y < static_cast<int8_t>(kGridSize); ++y) {
    for (int8_t x = 0; x < static_cast<int8_t>(kGridSize); ++x) {
      if (occupies(x, y)) {
        continue;
      }
      if (seen == target) {
        food_ = {x, y};
        return;
      }
      ++seen;
    }
  }
}

int16_t SnakeGame::snakeIndexAt(int8_t x, int8_t y) const {
  for (uint16_t i = 0; i < length_; ++i) {
    if (snake_[i].x == x && snake_[i].y == y) {
      return static_cast<int16_t>(i);
    }
  }
  return -1;
}

void SnakeGame::tick() {
  const uint32_t now = millis();
  if (gameOver_ || now - lastMoveMs_ < kMoveIntervalMs) {
    return;
  }
  lastMoveMs_ = now;
  direction_ = pendingDirection_;

  Cell next = snake_[0];
  switch (direction_) {
    case Direction::kUp:
      --next.y;
      break;
    case Direction::kDown:
      ++next.y;
      break;
    case Direction::kLeft:
      --next.x;
      break;
    case Direction::kRight:
      ++next.x;
      break;
  }

  const bool eating = next.x == food_.x && next.y == food_.y;
  if (next.x < 0 || next.y < 0 || next.x >= static_cast<int8_t>(kGridSize) ||
      next.y >= static_cast<int8_t>(kGridSize) ||
      occupies(next.x, next.y, eating ? 0 : 1)) {
    gameOver_ = true;
    dirty_ = true;
    return;
  }

  const uint16_t oldLength = length_;
  if (eating && length_ < kMaxCells) {
    ++length_;
    score_ += 10;
  }
  for (uint16_t i = static_cast<uint16_t>(length_ - 1); i > 0; --i) {
    if (i < oldLength) {
      snake_[i] = snake_[i - 1];
    } else {
      snake_[i] = snake_[oldLength - 1];
    }
  }
  snake_[0] = next;

  if (eating) {
    spawnFood();
  }
  dirty_ = true;
}

uint16_t SnakeGame::cellSignature(uint8_t x, uint8_t y) const {
  if (food_.x == static_cast<int8_t>(x) && food_.y == static_cast<int8_t>(y)) {
    return 3;
  }

  const int16_t index =
      snakeIndexAt(static_cast<int8_t>(x), static_cast<int8_t>(y));
  if (index < 0) {
    return 0;
  }

  if (index == 0) {
    return static_cast<uint16_t>(
        2U | (static_cast<uint16_t>(direction_) << 8));
  }

  uint8_t bits = directionBits(snake_[index].x, snake_[index].y,
                               snake_[index - 1].x, snake_[index - 1].y);
  if (static_cast<uint16_t>(index + 1) < length_) {
    bits |= directionBits(snake_[index].x, snake_[index].y,
                          snake_[index + 1].x, snake_[index + 1].y);
    return static_cast<uint16_t>(1U | (static_cast<uint16_t>(bits) << 4));
  }

  return static_cast<uint16_t>(4U | (static_cast<uint16_t>(bits) << 4));
}

void SnakeGame::render(esp32_lcd::ST7789Ascii &lcd) {
  if (fullRedraw_) {
    lcd.fillScreen(kBg);
    drawGrid(lcd);
  }

  for (uint8_t row = 0; row < kGridSize; ++row) {
    for (uint8_t col = 0; col < kGridSize; ++col) {
      const uint16_t signature = cellSignature(col, row);
      if (fullRedraw_ || renderedCells_[row][col] != signature) {
        drawCellSignature(lcd, col, row, signature);
        renderedCells_[row][col] = signature;
      }
    }
  }

  if (fullRedraw_ || score_ != renderedScore_ ||
      length_ != renderedLength_ || direction_ != renderedDirection_ ||
      gameOver_ != renderedGameOver_) {
    drawPanel(lcd, score_, length_, direction_, gameOver_);
    renderedScore_ = score_;
    renderedLength_ = length_;
    renderedDirection_ = direction_;
    renderedGameOver_ = gameOver_;
  }

  fullRedraw_ = false;
  dirty_ = false;
}
