#include "dino_game.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "game_sprites.h"
#include "sprites_meta.h"

namespace {

constexpr float kJumpVelocity = -15.0f;
constexpr float kJumpCutVelocity = -8.0f;
constexpr float kGravityRising = 24.0f;
constexpr float kGravityFalling = 34.0f;
constexpr float kMaxFallVelocity = 16.0f;
constexpr float kMinObstacleSpeed = 11.5f;
constexpr float kSpeedIncreasePerScore = 0.2f;
constexpr uint32_t kFrameIntervalMs = 20;
constexpr uint32_t kJumpBufferMs = 90;
constexpr uint32_t kCoyoteTimeMs = 70;
constexpr float kPixelsPerCol = 8.0f;
constexpr float kPixelsPerRow = 16.0f;
constexpr float kGroundAnchorRow = 11.0f;
constexpr float kLowBirdAnchorRow = 10.25f;
constexpr float kHighBirdAnchorRow = 9.45f;
constexpr int16_t kCloudY = 44;
constexpr uint32_t kCloudScrollMs = 85;

struct HitboxDef {
  float offsetX;
  float offsetY;
  float width;
  float height;
};

struct RectF {
  float x;
  float y;
  float w;
  float h;
};

struct RectI {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

constexpr int16_t kLcdW = 320;
constexpr int16_t kLcdH = 240;
constexpr int16_t kMaxCompositedH = 180;
constexpr uint16_t kDinoBg = esp32_lcd::COLOR_WHITE;
constexpr uint16_t kDinoFg = esp32_lcd::COLOR_BLACK;
constexpr uint16_t kDinoAccent = esp32_lcd::COLOR_RED;
uint16_t gSceneBuffer[static_cast<size_t>(kLcdW) * kMaxCompositedH];

HitboxDef defaultHitbox() {
  return {-0.5f, -1.0f, 1.0f, 1.0f};
}

HitboxDef hitboxFromMeta(const char *name) {
  for (uint16_t i = 0; i < sprites_meta_count; ++i) {
    if (std::strcmp(sprites_meta[i].name, name) == 0) {
      const SpriteMeta &m = sprites_meta[i];
      HitboxDef hb{};
      hb.offsetX = static_cast<float>(m.col_offset_x) / kPixelsPerCol;
      hb.offsetY = static_cast<float>(m.col_offset_y) / kPixelsPerRow;
      hb.width = std::max(0.4f, static_cast<float>(m.col_w) / kPixelsPerCol);
      hb.height = std::max(0.4f, static_cast<float>(m.col_h) / kPixelsPerRow);
      return hb;
    }
  }
  return defaultHitbox();
}

RectF makeRect(float anchorX, float anchorY, const HitboxDef &hb) {
  return {anchorX + hb.offsetX, anchorY + hb.offsetY, hb.width, hb.height};
}

bool intersects(const RectF &a, const RectF &b) {
  return (a.x < (b.x + b.w)) && ((a.x + a.w) > b.x) &&
         (a.y < (b.y + b.h)) && ((a.y + a.h) > b.y);
}

RectI makeRectI(int16_t x, int16_t y, uint16_t w, uint16_t h) {
  return {x, y, static_cast<int16_t>(w), static_cast<int16_t>(h)};
}

bool rectValid(const RectI &r) {
  return r.w > 0 && r.h > 0;
}

RectI clipToScreen(RectI r) {
  const int16_t x0 = std::max<int16_t>(0, r.x);
  const int16_t y0 = std::max<int16_t>(0, r.y);
  const int16_t x1 = std::min<int16_t>(kLcdW, static_cast<int16_t>(r.x + r.w));
  const int16_t y1 = std::min<int16_t>(kLcdH, static_cast<int16_t>(r.y + r.h));
  if (x1 <= x0 || y1 <= y0) {
    return {0, 0, 0, 0};
  }
  return {x0, y0, static_cast<int16_t>(x1 - x0),
          static_cast<int16_t>(y1 - y0)};
}

RectI unionRect(RectI a, RectI b) {
  a = clipToScreen(a);
  b = clipToScreen(b);
  if (!rectValid(a)) return b;
  if (!rectValid(b)) return a;
  const int16_t x0 = std::min(a.x, b.x);
  const int16_t y0 = std::min(a.y, b.y);
  const int16_t x1 = std::max<int16_t>(static_cast<int16_t>(a.x + a.w),
                                       static_cast<int16_t>(b.x + b.w));
  const int16_t y1 = std::max<int16_t>(static_cast<int16_t>(a.y + a.h),
                                       static_cast<int16_t>(b.y + b.h));
  return {x0, y0, static_cast<int16_t>(x1 - x0),
          static_cast<int16_t>(y1 - y0)};
}

bool spriteBitSet(const GameSprite *spr, uint16_t x, uint16_t y) {
  const uint16_t rowBytes = static_cast<uint16_t>((spr->w + 7U) / 8U);
  const uint8_t *row = spr->data + static_cast<size_t>(y) * rowBytes;
  const uint8_t byteVal = row[x / 8U];
  const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (x & 0x07U));
  return (byteVal & bitMask) != 0;
}

const SpriteMeta *metaByName(const char *name);

void drawGroundToBuffer(uint16_t *buf, const RectI &region,
                        const GameSprite *ground, int16_t groundY,
                        int16_t scroll) {
  if (ground == nullptr) {
    return;
  }

  const int16_t y0 = std::max<int16_t>(region.y, groundY);
  const int16_t y1 = std::min<int16_t>(
      static_cast<int16_t>(region.y + region.h),
      static_cast<int16_t>(groundY + ground->h));
  if (y1 <= y0) {
    return;
  }

  for (int16_t sy = y0; sy < y1; ++sy) {
    const uint16_t srcY = static_cast<uint16_t>(sy - groundY);
    uint16_t *dst = buf + static_cast<size_t>(sy - region.y) * region.w;
    for (int16_t sx = region.x; sx < region.x + region.w; ++sx) {
      int32_t gx = static_cast<int32_t>(sx) + scroll;
      gx %= ground->w;
      if (gx < 0) {
        gx += ground->w;
      }
      if (spriteBitSet(ground, static_cast<uint16_t>(gx), srcY)) {
        dst[sx - region.x] = kDinoFg;
      }
    }
  }
}

void drawSpriteToBuffer(uint16_t *buf, const RectI &region, const char *name,
                        int16_t spriteX, int16_t spriteY, uint16_t fg) {
  const GameSprite *spr = FindGameSpriteByName(name);
  const SpriteMeta *meta = metaByName(name);
  if (spr == nullptr || meta == nullptr) {
    return;
  }

  const int16_t x0 = std::max<int16_t>(region.x, spriteX);
  const int16_t y0 = std::max<int16_t>(region.y, spriteY);
  const int16_t x1 = std::min<int16_t>(
      static_cast<int16_t>(region.x + region.w),
      static_cast<int16_t>(spriteX + meta->bbox_w));
  const int16_t y1 = std::min<int16_t>(
      static_cast<int16_t>(region.y + region.h),
      static_cast<int16_t>(spriteY + meta->bbox_h));
  if (x1 <= x0 || y1 <= y0) {
    return;
  }

  for (int16_t sy = y0; sy < y1; ++sy) {
    const uint16_t srcY =
        static_cast<uint16_t>(meta->bbox_y + (sy - spriteY));
    uint16_t *dst = buf + static_cast<size_t>(sy - region.y) * region.w;
    for (int16_t sx = x0; sx < x1; ++sx) {
      const uint16_t srcX =
          static_cast<uint16_t>(meta->bbox_x + (sx - spriteX));
      if (spriteBitSet(spr, srcX, srcY)) {
        dst[sx - region.x] = fg;
      }
    }
  }
}

void pushCompositedRegion(esp32_lcd::ST7789Ascii &lcd, RectI region,
                          const GameSprite *ground, int16_t groundY,
                          int16_t scroll, int16_t cloudX, int16_t cloudY,
                          const char *obstacleName, int16_t obstacleX,
                          int16_t obstacleY,
                          const char *dinoName, int16_t dinoX,
                          int16_t dinoY) {
  region = clipToScreen(region);
  if (!rectValid(region)) {
    return;
  }
  if (region.h > kMaxCompositedH) {
    region.h = kMaxCompositedH;
  }

  const size_t pixelCount = static_cast<size_t>(region.w) * region.h;
  std::fill(gSceneBuffer, gSceneBuffer + pixelCount, kDinoBg);
  drawSpriteToBuffer(gSceneBuffer, region, "cloud.png", cloudX, cloudY,
                     kDinoFg);
  drawGroundToBuffer(gSceneBuffer, region, ground, groundY, scroll);
  drawSpriteToBuffer(gSceneBuffer, region, obstacleName, obstacleX, obstacleY,
                     kDinoFg);
  drawSpriteToBuffer(gSceneBuffer, region, dinoName, dinoX, dinoY,
                     kDinoFg);
  lcd.pushRgb565(region.x, region.y, static_cast<uint16_t>(region.w),
                 static_cast<uint16_t>(region.h), gSceneBuffer);
}

void clearOldMinusNew(esp32_lcd::ST7789Ascii &lcd, int16_t ox, int16_t oy,
                      uint16_t ow, uint16_t oh, int16_t nx, int16_t ny,
                      uint16_t nw, uint16_t nh, int16_t groundY,
                      uint16_t groundH) {
  if (ow == 0 || oh == 0) {
    return;
  }

  const int16_t oRight = static_cast<int16_t>(ox + ow);
  const int16_t oBottom = static_cast<int16_t>(oy + oh);
  const int16_t nRight = static_cast<int16_t>(nx + nw);
  const int16_t nBottom = static_cast<int16_t>(ny + nh);

  const int16_t ix0 = std::max(ox, nx);
  const int16_t iy0 = std::max(oy, ny);
  const int16_t ix1 = std::min(oRight, nRight);
  const int16_t iy1 = std::min(oBottom, nBottom);

  // Clear full old rect; the ground strip will be redrawn in the same frame,
  // and the sprite bbox is now tight enough to avoid wide background wipes.
  if (ix0 >= ix1 || iy0 >= iy1) {
    lcd.fillRect(ox, oy, ow, oh, kDinoBg);
    return;
  }

  // Top strip.
  if (oy < iy0) {
    lcd.fillRect(ox, oy, ow, static_cast<uint16_t>(iy0 - oy),
                 kDinoBg);
  }
  // Bottom strip.
  if (iy1 < oBottom) {
    lcd.fillRect(ox, iy1, ow, static_cast<uint16_t>(oBottom - iy1),
                 kDinoBg);
  }
  // Left strip in overlap Y range.
  if (ox < ix0) {
    lcd.fillRect(ox, iy0, static_cast<uint16_t>(ix0 - ox),
                 static_cast<uint16_t>(iy1 - iy0), kDinoBg);
  }
  // Right strip in overlap Y range.
  if (ix1 < oRight) {
    lcd.fillRect(ix1, iy0, static_cast<uint16_t>(oRight - ix1),
                 static_cast<uint16_t>(iy1 - iy0), kDinoBg);
  }
}

const SpriteMeta *metaByName(const char *name) {
  if (name == nullptr) {
    return nullptr;
  }
  for (uint16_t i = 0; i < sprites_meta_count; ++i) {
    if (std::strcmp(sprites_meta[i].name, name) == 0) {
      return &sprites_meta[i];
    }
  }
  return nullptr;
}

void drawSpriteTopLeft(esp32_lcd::ST7789Ascii &lcd, const char *name,
                       int16_t x, int16_t y, uint16_t fg, uint16_t bg,
                       int16_t groundY = INT16_MIN) {
  const GameSprite *spr = FindGameSpriteByName(name);
  if (spr == nullptr) {
    return;
  }
  lcd.drawBitmapMonoTransparent(x, y, spr->data, spr->w, spr->h, fg, groundY,
                                bg);
}

void drawSpriteByAnchor(esp32_lcd::ST7789Ascii &lcd, const char *name,
                        float anchorCol, float anchorRow, uint16_t fg,
                        uint16_t bg, int16_t groundY = INT16_MIN) {
  const GameSprite *spr = FindGameSpriteByName(name);
  const SpriteMeta *meta = metaByName(name);
  if (spr == nullptr || meta == nullptr) {
    return;
  }

  const int16_t px = static_cast<int16_t>(
      std::lround(anchorCol * kPixelsPerCol) - meta->anchor_x);
  const int16_t py = static_cast<int16_t>(
      std::lround(anchorRow * kPixelsPerRow) - meta->anchor_y);
    lcd.drawBitmapMonoTransparent(
      static_cast<int16_t>(px + meta->bbox_x),
      static_cast<int16_t>(py + meta->bbox_y), spr->data, spr->w, spr->h, fg,
      meta->bbox_x, meta->bbox_y, meta->bbox_w, meta->bbox_h, groundY, bg);
}

int16_t centeredTextX(const char *text) {
  return static_cast<int16_t>((kLcdW - static_cast<int>(std::strlen(text)) * 8) / 2);
}

void drawHudScore(esp32_lcd::ST7789Ascii &lcd, uint32_t score) {
  char buf[5];
  std::snprintf(buf, sizeof(buf), "%04lu",
                static_cast<unsigned long>(score % 10000U));

  int16_t x = 244;
  drawSpriteTopLeft(lcd, "char_H.png", x, 10, kDinoFg, kDinoBg);
  x = static_cast<int16_t>(x + 12);
  drawSpriteTopLeft(lcd, "char_I.png", x, 10, kDinoFg, kDinoBg);
  x = static_cast<int16_t>(x + 16);

  for (size_t i = 0; buf[i] != '\0'; ++i) {
    const char digit = buf[i];
    char name[16];
    std::snprintf(name, sizeof(name), "number_%c.png", digit);
    drawSpriteTopLeft(lcd, name, x, 10, kDinoFg, kDinoBg);
    x = static_cast<int16_t>(x + 10);
  }
}

void fillLine(char *line, char fillChar, int width) {
  for (int i = 0; i < width; ++i) {
    line[i] = fillChar;
  }
  line[width] = '\0';
}

int clampColumn(float value) {
  if (value < 0.0f) {
    return 0;
  }
  if (value > 39.0f) {
    return 39;
  }
  return static_cast<int>(std::lround(value));
}

const HitboxDef kDinoStandingHitbox = hitboxFromMeta("dino_02.png");
const HitboxDef kDinoDuckingHitbox = hitboxFromMeta("dino_05.png");
const HitboxDef kCactusTallHitbox = hitboxFromMeta("cactus_01.png");
const HitboxDef kCactusSmallHitbox = hitboxFromMeta("cactus_02.png");
const HitboxDef kBirdHitbox = hitboxFromMeta("bird_01.png");

HitboxDef highBirdHitbox() {
  HitboxDef hb = kBirdHitbox;
  hb.offsetX += 0.30f;
  hb.offsetY += 0.75f;
  hb.width = std::max(0.4f, hb.width - 0.60f);
  hb.height = std::max(0.4f, hb.height - 0.55f);
  return hb;
}

HitboxDef lowBirdHitbox() {
  HitboxDef hb = kBirdHitbox;
  hb.offsetX += 0.25f;
  hb.offsetY += 0.45f;
  hb.width = std::max(0.4f, hb.width - 0.50f);
  return hb;
}

const char *obstacleSpriteName(int kind, uint32_t now) {
  if (kind >= 2) {
    return ((now / 140U) % 2U == 0U) ? "bird_01.png" : "bird_02.png";
  }
  return (kind == 0) ? "cactus_02.png" : "cactus_01.png";
}

float obstacleAnchorRow(int kind) {
  if (kind == 2) {
    return kLowBirdAnchorRow;
  }
  if (kind == 3) {
    return kHighBirdAnchorRow;
  }
  return kGroundAnchorRow;
}

const HitboxDef &obstacleHitbox(int kind) {
  static const HitboxDef kLowBirdHitbox = lowBirdHitbox();
  static const HitboxDef kHighBirdHitbox = highBirdHitbox();
  if (kind == 2) {
    return kLowBirdHitbox;
  }
  if (kind == 3) {
    return kHighBirdHitbox;
  }
  return (kind == 0) ? kCactusSmallHitbox : kCactusTallHitbox;
}

}  // namespace

void DinoGame::begin() {
  reset();
}

void DinoGame::reset() {
  started_ = true;
  gameOver_ = false;
  jumpQueued_ = false;
  jumpHeld_ = false;
  ducking_ = false;
  playerY_ = static_cast<float>(kGroundRow);
  playerVelocity_ = 0.0f;
  obstacleX_ = 39.0f;
  obstacleKind_ = 0;
  obstacleSpeed_ = kMinObstacleSpeed;
  score_ = 0;
  lastTickMs_ = millis();
  lastRenderMs_ = 0;
  jumpBufferUntilMs_ = 0;
  coyoteUntilMs_ = lastTickMs_ + kCoyoteTimeMs;
  dirty_ = true;
  sceneInitialized_ = false;
  prevGameOverRendered_ = false;
  hudInitialized_ = false;
  lastScoreRendered_ = UINT32_MAX;
  prevDinoW_ = 0;
  prevDinoH_ = 0;
  prevObstacleW_ = 0;
  prevObstacleH_ = 0;
  prevCloudW_ = 0;
  prevCloudH_ = 0;
  prevGroundScroll_ = -1;
  std::snprintf(message_, sizeof(message_), "%s", "UP jump, DOWN duck");
}

void DinoGame::restartIfNeeded() {
  if (gameOver_) {
    reset();
  }
}

void DinoGame::triggerJump() {
  if (gameOver_) {
    restartIfNeeded();
    return;
  }

  jumpHeld_ = true;
  jumpBufferUntilMs_ = millis() + kJumpBufferMs;
  dirty_ = true;
}

void DinoGame::spawnObstacle() {
  obstacleX_ = 39.0f;
  if (score_ < 4U) {
    obstacleKind_ = static_cast<int>(score_ % 2U);
  } else {
    obstacleKind_ = static_cast<int>(score_ % 4U);
  }
}

void DinoGame::tick() {
  const uint32_t now = millis();
  if (gameOver_) {
    return;
  }
  uint32_t deltaMs = now - lastTickMs_;
  if (deltaMs < kFrameIntervalMs) {
    return;
  }
  lastTickMs_ = now;

  const float dt = static_cast<float>(deltaMs) / 1000.0f;

  const bool onGroundAtStart =
      playerY_ >= static_cast<float>(kGroundRow) - 0.01f;
  if (onGroundAtStart) {
    coyoteUntilMs_ = now + kCoyoteTimeMs;
  }

  if (jumpQueued_ ||
      (jumpBufferUntilMs_ >= now && coyoteUntilMs_ >= now)) {
    playerVelocity_ = kJumpVelocity;
    jumpQueued_ = false;
    jumpBufferUntilMs_ = 0;
  }

  const float gravity = (playerVelocity_ < 0.0f) ? kGravityRising : kGravityFalling;
  playerVelocity_ += gravity * dt;
  if (playerVelocity_ > kMaxFallVelocity) {
    playerVelocity_ = kMaxFallVelocity;
  }
  playerY_ += playerVelocity_ * dt;

  if (playerY_ >= static_cast<float>(kGroundRow)) {
    playerY_ = static_cast<float>(kGroundRow);
    playerVelocity_ = 0.0f;
    coyoteUntilMs_ = now + kCoyoteTimeMs;
  }

  obstacleSpeed_ = kMinObstacleSpeed + static_cast<float>(score_) * kSpeedIncreasePerScore;
  obstacleX_ -= obstacleSpeed_ * dt;

  if (obstacleX_ < -2.0f) {
    ++score_;
    spawnObstacle();
  }

    const bool duckingOnGround =
        ducking_ && playerY_ >= static_cast<float>(kGroundRow) - 0.01f;
    const HitboxDef &dinoHb =
        duckingOnGround ? kDinoDuckingHitbox : kDinoStandingHitbox;
    const HitboxDef &obstacleHb = obstacleHitbox(obstacleKind_);
    const RectF dinoRect =
      makeRect(static_cast<float>(kPlayerCol), playerY_ + 1.0f, dinoHb);
    const RectF obstacleRect =
      makeRect(obstacleX_, obstacleAnchorRow(obstacleKind_), obstacleHb);
  const bool hitObstacle = intersects(dinoRect, obstacleRect);

  if (hitObstacle) {
    gameOver_ = true;
    std::snprintf(message_, sizeof(message_), "%s", "Crash! OK/UP restart, BACK menu");
    dirty_ = true;
  } else if (!gameOver_) {
    std::snprintf(message_, sizeof(message_), "%s", "UP jump, DOWN duck");
  }
  // We advanced the simulation, mark frame dirty so render task will update display.
  dirty_ = true;
}

void DinoGame::update(const key_input::KeyEvent *events, size_t eventCount) {
  bool touchedInput = false;
  for (size_t i = 0; i < eventCount; ++i) {
    const auto &event = events[i];

    if (event.key == key_input::KeyId::kDown) {
      if (gameOver_ && event.pressed) {
        restartIfNeeded();
      } else {
        ducking_ = event.pressed;
      }
      touchedInput = true;
    } else if (!event.pressed) {
      if ((event.key == key_input::KeyId::kUp ||
           event.key == key_input::KeyId::kOk) &&
          jumpHeld_) {
        jumpHeld_ = false;
        if (!gameOver_ && playerVelocity_ < kJumpCutVelocity) {
          playerVelocity_ = kJumpCutVelocity;
        }
        touchedInput = true;
      }
      continue;
    } else if (event.key == key_input::KeyId::kUp || event.key == key_input::KeyId::kOk) {
      triggerJump();
      touchedInput = true;
    }
  }

  tick();
  if (touchedInput) {
    dirty_ = true;
  }
}

bool DinoGame::needsRender() const {
  return dirty_;
}

void DinoGame::buildFrame(char lines[kScreenRows][kScreenCols + 1]) const {
  for (int row = 0; row < kScreenRows; ++row) {
    fillLine(lines[row], ' ', kScreenCols);
  }

  std::snprintf(lines[0], kScreenCols + 1, "DINO JUMP   HI %04lu",
                static_cast<unsigned long>(score_ % 10000U));
  std::snprintf(lines[1], kScreenCols + 1, "UP jump  DOWN duck  BACK menu");

  for (int col = 0; col < kScreenCols; ++col) {
    lines[kGroundLineRow][col] = '=';
  }

  const int playerRow = std::clamp(static_cast<int>(std::lround(playerY_)), 8, kGroundRow);
  lines[playerRow][kPlayerCol] = gameOver_ ? 'X' : (ducking_ ? 'd' : 'D');

  const int obstacleCol = clampColumn(obstacleX_);
  if (obstacleCol >= 0 && obstacleCol < kScreenCols) {
    if (obstacleKind_ == 2) {
      lines[kGroundRow][obstacleCol] = 'b';
    } else if (obstacleKind_ == 3) {
      lines[kGroundRow - 1][obstacleCol] = 'B';
    } else {
      lines[kGroundRow][obstacleCol] = obstacleKind_ == 0 ? '^' : '#';
    }
    if (obstacleKind_ == 1 && obstacleCol > 0) {
      lines[kGroundRow - 1][obstacleCol] = '^';
    }
  }

  std::snprintf(lines[13], kScreenCols + 1, "%s", message_);
  std::snprintf(lines[14], kScreenCols + 1, "BACK menu   OK/UP restart");
}

// Forward declaration so render() can call it before the definition below.
void restoreBackgroundRegion(esp32_lcd::ST7789Ascii &lcd, int16_t rx,
                             int16_t ry, uint16_t rw, uint16_t rh,
                             const GameSprite *ground, int16_t groundY,
                             int16_t scroll);

void DinoGame::render(esp32_lcd::ST7789Ascii &lcd) {
  const uint32_t now = millis();
  if (now - lastRenderMs_ < kFrameIntervalMs) {
    return;
  }
  lastRenderMs_ = now;

  if (!sceneInitialized_) {
    lcd.fillScreen(kDinoBg);
    sceneInitialized_ = true;
  }

  const bool inAir = playerY_ < static_cast<float>(kGroundRow) - 0.05f;
  const bool duckingOnGround = ducking_ && !inAir;
  const char *dinoName = "dino_02.png";
  if (gameOver_) {
    dinoName = "dino_04.png";
  } else if (inAir) {
    dinoName = "dino_01.png";
  } else if (duckingOnGround) {
    dinoName = ((now / 120U) % 2U == 0U) ? "dino_05.png" : "dino_06.png";
  } else {
    dinoName = ((now / 120U) % 2U == 0U) ? "dino_02.png" : "dino_03.png";
  }

  const char *obstacleName = obstacleSpriteName(obstacleKind_, now);
  const GameSprite *obstacleSpr = FindGameSpriteByName(obstacleName);
  const SpriteMeta *obstacleMeta = metaByName(obstacleName);
  int16_t obstacleX = 0;
  int16_t obstacleY = 0;
  uint16_t obstacleW = 0;
  uint16_t obstacleH = 0;
  if (obstacleSpr != nullptr && obstacleMeta != nullptr) {
    const int16_t baseX = static_cast<int16_t>(
      std::lround(obstacleX_ * kPixelsPerCol) - obstacleMeta->anchor_x);
    const int16_t baseY = static_cast<int16_t>(
      std::lround(obstacleAnchorRow(obstacleKind_) * kPixelsPerRow) -
      obstacleMeta->anchor_y);
    obstacleX = static_cast<int16_t>(baseX + obstacleMeta->bbox_x);
    obstacleY = static_cast<int16_t>(baseY + obstacleMeta->bbox_y);
    obstacleW = obstacleMeta->bbox_w;
    obstacleH = obstacleMeta->bbox_h;
  }

  const GameSprite *dinoSpr = FindGameSpriteByName(dinoName);
  const SpriteMeta *dinoMeta = metaByName(dinoName);
  int16_t dinoX = 0;
  int16_t dinoY = 0;
  uint16_t dinoW = 0;
  uint16_t dinoH = 0;
  if (dinoSpr != nullptr && dinoMeta != nullptr) {
    const int16_t baseX = static_cast<int16_t>(
      std::lround(static_cast<float>(kPlayerCol) * kPixelsPerCol) -
      dinoMeta->anchor_x);
    const int16_t baseY = static_cast<int16_t>(
      std::lround((playerY_ + 1.0f) * kPixelsPerRow) - dinoMeta->anchor_y);
    dinoX = static_cast<int16_t>(baseX + dinoMeta->bbox_x);
    dinoY = static_cast<int16_t>(baseY + dinoMeta->bbox_y);
    dinoW = dinoMeta->bbox_w;
    dinoH = dinoMeta->bbox_h;
  }

  const GameSprite *cloudSpr = FindGameSpriteByName("cloud.png");
  const SpriteMeta *cloudMeta = metaByName("cloud.png");
  int16_t cloudX = 0;
  int16_t cloudY = kCloudY;
  uint16_t cloudW = 0;
  uint16_t cloudH = 0;
  if (cloudSpr != nullptr && cloudMeta != nullptr) {
    const int16_t cloudSpan =
        static_cast<int16_t>(kLcdW + cloudMeta->bbox_w + 80);
    const int16_t cloudScroll =
        static_cast<int16_t>((now / kCloudScrollMs) % cloudSpan);
    cloudX = static_cast<int16_t>(kLcdW + 40 - cloudScroll);
    cloudY = static_cast<int16_t>(kCloudY + cloudMeta->bbox_y);
    cloudW = cloudMeta->bbox_w;
    cloudH = cloudMeta->bbox_h;
  }

  // Prepare ground info early so we can restore background under previous bboxes
  const GameSprite *ground = FindGameSpriteByName("ground.png");
  const int16_t groundY = static_cast<int16_t>(kGroundRow * 16);
  const int16_t groundScroll = (ground != nullptr && ground->w > 0)
                                   ? static_cast<int16_t>((now / 18U) % ground->w)
                                   : 0;

  const int kClearMargin = 4;
  RectI dirtyRegion = {0, groundY, kLcdW,
                       static_cast<int16_t>(ground != nullptr ? ground->h : 0)};
  if (prevDinoW_ > 0 && prevDinoH_ > 0) {
    dirtyRegion = unionRect(
        dirtyRegion,
        makeRectI(static_cast<int16_t>(prevDinoX_ - kClearMargin),
                  static_cast<int16_t>(prevDinoY_ - kClearMargin),
                  static_cast<uint16_t>(prevDinoW_ + kClearMargin * 2),
                  static_cast<uint16_t>(prevDinoH_ + kClearMargin * 2)));
  }
  if (prevObstacleW_ > 0 && prevObstacleH_ > 0) {
    dirtyRegion = unionRect(
        dirtyRegion,
        makeRectI(static_cast<int16_t>(prevObstacleX_ - kClearMargin),
                  static_cast<int16_t>(prevObstacleY_ - kClearMargin),
                  static_cast<uint16_t>(prevObstacleW_ + kClearMargin * 2),
                  static_cast<uint16_t>(prevObstacleH_ + kClearMargin * 2)));
  }
  if (prevCloudW_ > 0 && prevCloudH_ > 0) {
    dirtyRegion = unionRect(
        dirtyRegion,
        makeRectI(static_cast<int16_t>(prevCloudX_ - kClearMargin),
                  static_cast<int16_t>(prevCloudY_ - kClearMargin),
                  static_cast<uint16_t>(prevCloudW_ + kClearMargin * 2),
                  static_cast<uint16_t>(prevCloudH_ + kClearMargin * 2)));
  }

  dirtyRegion = unionRect(
      dirtyRegion,
      makeRectI(static_cast<int16_t>(dinoX - kClearMargin),
                static_cast<int16_t>(dinoY - kClearMargin),
                static_cast<uint16_t>(dinoW + kClearMargin * 2),
                static_cast<uint16_t>(dinoH + kClearMargin * 2)));
  dirtyRegion = unionRect(
      dirtyRegion,
      makeRectI(static_cast<int16_t>(obstacleX - kClearMargin),
                static_cast<int16_t>(obstacleY - kClearMargin),
                static_cast<uint16_t>(obstacleW + kClearMargin * 2),
                static_cast<uint16_t>(obstacleH + kClearMargin * 2)));
  dirtyRegion = unionRect(
      dirtyRegion,
      makeRectI(static_cast<int16_t>(cloudX - kClearMargin),
                static_cast<int16_t>(cloudY - kClearMargin),
                static_cast<uint16_t>(cloudW + kClearMargin * 2),
                static_cast<uint16_t>(cloudH + kClearMargin * 2)));

  pushCompositedRegion(lcd, dirtyRegion, ground, groundY, groundScroll,
                       cloudX, cloudY, obstacleName, obstacleX, obstacleY,
                       dinoName, dinoX, dinoY);
  prevGroundScroll_ = groundScroll;

  prevObstacleX_ = obstacleX;
  prevObstacleY_ = obstacleY;
  prevObstacleW_ = obstacleW;
  prevObstacleH_ = obstacleH;
  prevDinoX_ = dinoX;
  prevDinoY_ = dinoY;
  prevDinoW_ = dinoW;
  prevDinoH_ = dinoH;
  prevCloudX_ = cloudX;
  prevCloudY_ = cloudY;
  prevCloudW_ = cloudW;
  prevCloudH_ = cloudH;

  // HUD.
  if (!hudInitialized_ || score_ != lastScoreRendered_) {
    lcd.fillRect(4, 4, 312, 22, kDinoBg);
    drawHudScore(lcd, score_);
    hudInitialized_ = true;
    lastScoreRendered_ = score_;
  }

  if (!gameOver_ && prevGameOverRendered_) {
    lcd.fillRect(40, 40, 240, 190, kDinoBg);
  }
  if (gameOver_ && !prevGameOverRendered_) {
    drawSpriteTopLeft(lcd, "game_over.png", 62, 44, kDinoAccent, kDinoBg);
    drawSpriteTopLeft(lcd, "restart.png", 142, 72, kDinoFg, kDinoBg);
    lcd.drawString8x16(centeredTextX("OK/UP restart"), 188, "OK/UP restart",
                       kDinoFg, kDinoBg);
    lcd.drawString8x16(centeredTextX("BACK menu"), 208, "BACK menu",
                       kDinoFg, kDinoBg);
  }
  prevGameOverRendered_ = gameOver_;

  dirty_ = false;
}

// Restore background inside the given region: sky area -> background, ground area ->
// redraw ground tiles. 'scroll' is current ground horizontal offset.
void restoreBackgroundRegion(esp32_lcd::ST7789Ascii &lcd, int16_t rx, int16_t ry,
                             uint16_t rw, uint16_t rh,
                             const GameSprite *ground, int16_t groundY,
                             int16_t scroll) {
  if (rw == 0 || rh == 0) return;

  // top (sky) portion: fill black
  if (ry < groundY) {
    const int16_t topH = static_cast<int16_t>(std::min<int>(rh, std::max<int>(0, groundY - ry)));
    if (topH > 0) {
      lcd.fillRect(rx, ry, rw, static_cast<uint16_t>(topH), kDinoBg);
    }
  }

  if (ground == nullptr) return;
  // ground overlap: redraw ground tiles that intersect the region.
  const int16_t gy0 = groundY;
  const int16_t gy1 = static_cast<int16_t>(groundY + ground->h);
  const int16_t ry1 = static_cast<int16_t>(ry + rh);
  const int16_t oy0 = std::max(ry, gy0);
  const int16_t oy1 = std::min(ry1, gy1);
  if (oy0 >= oy1) return;

  // draw two tiles that cover the scrolling ground
  const int16_t tile0x = -scroll;
  const int16_t tile1x = static_cast<int16_t>(ground->w - scroll);
  // compute overlap with rx..rx+rw and draw only intersecting columns
  const int16_t rx1 = static_cast<int16_t>(rx + rw);
  // helper lambda to draw overlap of a tile
  auto drawTileOverlap = [&](int16_t tileX) {
    const int16_t t0 = tileX;
    const int16_t t1 = static_cast<int16_t>(tileX + ground->w);
    const int16_t ox0 = std::max<int16_t>(rx, t0);
    const int16_t ox1 = std::min<int16_t>(rx1, t1);
    if (ox0 >= ox1) return;
    const uint16_t drawW = static_cast<uint16_t>(ox1 - ox0);
    const uint16_t srcX = static_cast<uint16_t>(ox0 - t0);
    const uint16_t srcY = static_cast<uint16_t>(oy0 - groundY);
    const uint16_t drawH = static_cast<uint16_t>(oy1 - oy0);
    lcd.drawBitmapMono(ox0, oy0, ground->data, ground->w, ground->h,
                       kDinoFg, kDinoBg,
                       srcX, srcY, drawW, drawH);
  };
  drawTileOverlap(tile0x);
  drawTileOverlap(tile1x);
}
