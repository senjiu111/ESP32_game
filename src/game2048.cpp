#include "game2048.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

constexpr int16_t kBoardX = 6;
constexpr int16_t kBoardY = 8;
constexpr uint16_t kBoardSize = 224;
constexpr uint16_t kGap = 4;
constexpr uint16_t kCell = 51;
constexpr int16_t kPanelX = 238;
constexpr uint16_t kPanelW = 82;
constexpr uint16_t kLcdH = 240;

constexpr uint16_t kBg = esp32_lcd::COLOR_WHITE;
constexpr uint16_t kFg = esp32_lcd::COLOR_BLACK;
constexpr uint16_t kHi = esp32_lcd::COLOR_RED;
constexpr uint16_t kDim = 0x8410;
constexpr uint16_t kBoardColor = 0xD69A;
constexpr uint16_t kCellColor = 0xF7BE;
constexpr uint16_t kTileText = esp32_lcd::COLOR_BLACK;

uint16_t gTileBuffer[static_cast<size_t>(kCell) * kCell];

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

void drawCenteredText(esp32_lcd::ST7789Ascii &lcd, int16_t x, int16_t y,
                      uint16_t w, uint16_t h, const char *text, uint16_t fg,
                      uint16_t bg) {
  const int16_t textW = static_cast<int16_t>(std::strlen(text) * 8U);
  const int16_t tx =
      static_cast<int16_t>(x + (static_cast<int16_t>(w) - textW) / 2);
  const int16_t ty =
      static_cast<int16_t>(y + (static_cast<int16_t>(h) - 16) / 2);
  lcd.drawString8x16(static_cast<uint16_t>(tx), static_cast<uint16_t>(ty),
                     text, fg, bg);
}

bool roundedPixel(uint8_t x, uint8_t y, uint8_t radius) {
  if (x >= radius && x < kCell - radius) {
    return true;
  }
  if (y >= radius && y < kCell - radius) {
    return true;
  }

  const int16_t cx = x < radius ? static_cast<int16_t>(radius - 1)
                                : static_cast<int16_t>(kCell - radius);
  const int16_t cy = y < radius ? static_cast<int16_t>(radius - 1)
                                : static_cast<int16_t>(kCell - radius);
  const int16_t dx = static_cast<int16_t>(x) - cx;
  const int16_t dy = static_cast<int16_t>(y) - cy;
  return dx * dx + dy * dy <= radius * radius;
}

uint16_t tileColor(uint16_t value) {
  switch (value) {
    case 2:
      return 0xFFB0;
    case 4:
      return 0xFEE8;
    case 8:
      return 0xFDE0;
    case 16:
      return 0xFCC0;
    case 32:
      return 0xFBA0;
    case 64:
      return 0xFA80;
    case 128:
      return 0xF960;
    case 256:
      return 0xF840;
    case 512:
      return 0xF820;
    case 1024:
      return 0xF000;
    case 2048:
      return 0xC800;
    default:
      return kCellColor;
  }
}

void drawTile(esp32_lcd::ST7789Ascii &lcd, uint8_t row, uint8_t col,
              uint16_t value) {
  const int16_t x =
      static_cast<int16_t>(kBoardX + kGap + col * (kCell + kGap));
  const int16_t y =
      static_cast<int16_t>(kBoardY + kGap + row * (kCell + kGap));
  const uint16_t bg = tileColor(value);

  for (uint8_t py = 0; py < kCell; ++py) {
    for (uint8_t px = 0; px < kCell; ++px) {
      gTileBuffer[static_cast<size_t>(py) * kCell + px] =
          roundedPixel(px, py, 6) ? bg : kBoardColor;
    }
  }

  lcd.pushRgb565(x, y, kCell, kCell, gTileBuffer);
  if (value > 0) {
    char valueText[8];
    std::snprintf(valueText, sizeof(valueText), "%u", value);
    drawCenteredText(lcd, x, y, kCell, kCell, valueText, kTileText, bg);
  }
}

void drawPanel(esp32_lcd::ST7789Ascii &lcd, uint32_t score,
               uint32_t bestScore, bool won, bool gameOver) {
  char scoreText[12];
  lcd.fillRect(kPanelX, 0, kPanelW, kLcdH, kBg);
  lcd.drawString8x16(kPanelX, 8, "2048", kHi, kBg);
  lcd.drawString8x16(kPanelX, 34, "SCORE", kDim, kBg);
  std::snprintf(scoreText, sizeof(scoreText), "%lu",
                static_cast<unsigned long>(score));
  lcd.drawString8x16(kPanelX, 52, scoreText, kFg, kBg);
  lcd.drawString8x16(kPanelX, 82, "BEST", kDim, kBg);
  std::snprintf(scoreText, sizeof(scoreText), "%lu",
                static_cast<unsigned long>(bestScore));
  lcd.drawString8x16(kPanelX, 100, scoreText, kFg, kBg);
  lcd.drawString8x16(kPanelX, 132, "GOAL", kDim, kBg);
  lcd.drawString8x16(kPanelX, 150, "2048", kHi, kBg);

  if (won) {
    lcd.drawString8x16(kPanelX, 184, "WIN!", kHi, kBg);
    lcd.drawString8x16(kPanelX, 214, "OK", kDim, kBg);
  } else if (gameOver) {
    lcd.drawString8x16(kPanelX, 184, "OVER", kHi, kBg);
    lcd.drawString8x16(kPanelX, 214, "OK", kDim, kBg);
  } else {
    lcd.drawString8x16(kPanelX, 184, "READY", kFg, kBg);
    lcd.drawString8x16(kPanelX, 214, "BACK", kDim, kBg);
  }
}

}  // namespace

void Game2048::begin() {
  reset();
}

void Game2048::reset() {
  static bool seeded = false;
  if (!seeded) {
    randomSeed(micros());
    seeded = true;
  }

  for (auto &row : board_) {
    for (uint16_t &cell : row) {
      cell = 0;
    }
  }
  score_ = 0;
  gameOver_ = false;
  won_ = false;
  spawnTile();
  spawnTile();
  fullRedraw_ = true;
  renderedScore_ = UINT32_MAX;
  renderedBestScore_ = UINT32_MAX;
  dirty_ = true;
}

bool Game2048::needsRender() const {
  return dirty_;
}

void Game2048::update(const key_input::KeyEvent *events, size_t eventCount) {
  bool touched = false;
  for (size_t i = 0; i < eventCount; ++i) {
    const auto &event = events[i];
    if (!event.pressed) {
      continue;
    }

    if (event.key == key_input::KeyId::kOk && (gameOver_ || won_)) {
      reset();
      touched = true;
      continue;
    }
    if (gameOver_ || won_) {
      continue;
    }

    bool moved = false;
    switch (event.key) {
      case key_input::KeyId::kUp:
        moved = move(Direction::kUp);
        break;
      case key_input::KeyId::kDown:
        moved = move(Direction::kDown);
        break;
      case key_input::KeyId::kLeft:
        moved = move(Direction::kLeft);
        break;
      case key_input::KeyId::kRight:
        moved = move(Direction::kRight);
        break;
      default:
        break;
    }

    if (moved) {
      won_ = hasWon();
      if (!won_) {
        spawnTile();
        gameOver_ = !canMove();
      }
      touched = true;
    }
  }

  if (touched) {
    dirty_ = true;
  }
}

bool Game2048::processLine(uint16_t line[4]) {
  uint16_t original[4];
  for (uint8_t i = 0; i < 4; ++i) {
    original[i] = line[i];
  }

  uint16_t packed[4] = {};
  uint8_t write = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    if (line[i] != 0) {
      packed[write++] = line[i];
    }
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (packed[i] != 0 && packed[i] == packed[i + 1]) {
      packed[i] = static_cast<uint16_t>(packed[i] * 2U);
      score_ += packed[i];
      packed[i + 1] = 0;
    }
  }

  write = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    line[i] = 0;
  }
  for (uint8_t i = 0; i < 4; ++i) {
    if (packed[i] != 0) {
      line[write++] = packed[i];
    }
  }

  for (uint8_t i = 0; i < 4; ++i) {
    if (line[i] != original[i]) {
      return true;
    }
  }
  return false;
}

bool Game2048::move(Direction dir) {
  bool changed = false;
  uint16_t line[4];

  for (uint8_t index = 0; index < 4; ++index) {
    for (uint8_t i = 0; i < 4; ++i) {
      switch (dir) {
        case Direction::kLeft:
          line[i] = board_[index][i];
          break;
        case Direction::kRight:
          line[i] = board_[index][3 - i];
          break;
        case Direction::kUp:
          line[i] = board_[i][index];
          break;
        case Direction::kDown:
          line[i] = board_[3 - i][index];
          break;
      }
    }

    const bool lineChanged = processLine(line);
    changed = changed || lineChanged;

    for (uint8_t i = 0; i < 4; ++i) {
      switch (dir) {
        case Direction::kLeft:
          board_[index][i] = line[i];
          break;
        case Direction::kRight:
          board_[index][3 - i] = line[i];
          break;
        case Direction::kUp:
          board_[i][index] = line[i];
          break;
        case Direction::kDown:
          board_[3 - i][index] = line[i];
          break;
      }
    }
  }

  if (score_ > bestScore_) {
    bestScore_ = score_;
  }
  return changed;
}

bool Game2048::hasEmptyCell() const {
  for (const auto &row : board_) {
    for (uint16_t cell : row) {
      if (cell == 0) {
        return true;
      }
    }
  }
  return false;
}

void Game2048::spawnTile() {
  uint8_t emptyCount = 0;
  for (const auto &row : board_) {
    for (uint16_t cell : row) {
      if (cell == 0) {
        ++emptyCount;
      }
    }
  }
  if (emptyCount == 0) {
    return;
  }

  const uint8_t target = static_cast<uint8_t>(random(emptyCount));
  uint8_t seen = 0;
  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 4; ++col) {
      if (board_[row][col] != 0) {
        continue;
      }
      if (seen == target) {
        board_[row][col] = (random(10) == 0) ? 4 : 2;
        return;
      }
      ++seen;
    }
  }
}

bool Game2048::hasWon() const {
  for (const auto &row : board_) {
    for (uint16_t cell : row) {
      if (cell >= 2048) {
        return true;
      }
    }
  }
  return false;
}

bool Game2048::canMove() const {
  if (hasEmptyCell()) {
    return true;
  }
  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 4; ++col) {
      const uint16_t value = board_[row][col];
      if (col < 3 && board_[row][col + 1] == value) {
        return true;
      }
      if (row < 3 && board_[row + 1][col] == value) {
        return true;
      }
    }
  }
  return false;
}

void Game2048::render(esp32_lcd::ST7789Ascii &lcd) {
  if (fullRedraw_) {
    lcd.fillScreen(kBg);
    fillRoundRect(lcd, kBoardX, kBoardY, kBoardSize, kBoardSize, 9,
                  kBoardColor);
  }

  for (uint8_t row = 0; row < 4; ++row) {
    for (uint8_t col = 0; col < 4; ++col) {
      if (fullRedraw_ || board_[row][col] != renderedBoard_[row][col]) {
        drawTile(lcd, row, col, board_[row][col]);
        renderedBoard_[row][col] = board_[row][col];
      }
    }
  }

  if (fullRedraw_ || score_ != renderedScore_ ||
      bestScore_ != renderedBestScore_ || won_ != renderedWon_ ||
      gameOver_ != renderedGameOver_) {
    drawPanel(lcd, score_, bestScore_, won_, gameOver_);
    renderedScore_ = score_;
    renderedBestScore_ = bestScore_;
    renderedWon_ = won_;
    renderedGameOver_ = gameOver_;
  }

  fullRedraw_ = false;
  dirty_ = false;
}
