#include "esp32_lcd.h"

#include "LCD_font.h"

namespace esp32_lcd {

namespace {

constexpr uint8_t ST7789_SWRESET = 0x01;
constexpr uint8_t ST7789_SLPOUT = 0x11;
constexpr uint8_t ST7789_COLMOD = 0x3A;
constexpr uint8_t ST7789_MADCTL = 0x36;
constexpr uint8_t ST7789_CASET = 0x2A;
constexpr uint8_t ST7789_RASET = 0x2B;
constexpr uint8_t ST7789_INVON = 0x21;
constexpr uint8_t ST7789_NORON = 0x13;
constexpr uint8_t ST7789_DISPON = 0x29;
constexpr uint8_t ST7789_RAMWR = 0x2C;
constexpr uint8_t ST7789_MADCTL_LANDSCAPE = 0xA0;
constexpr uint32_t ST7789_SPI_FREQ = 40000000UL;
constexpr uint8_t ST7789_SPI_MODE = SPI_MODE3;
constexpr uint16_t LCD_CHAR_W = 8;
constexpr uint16_t LCD_CHAR_H = 16;

}  // namespace

void ST7789Ascii::select() { digitalWrite(pinCs_, LOW); }

void ST7789Ascii::unselect() { digitalWrite(pinCs_, HIGH); }

void ST7789Ascii::writeCommand(uint8_t cmd) {
  digitalWrite(pinDc_, LOW);
  spi_.transfer(cmd);
}

void ST7789Ascii::writeData(const uint8_t *data, size_t len) {
  digitalWrite(pinDc_, HIGH);
  while (len > 0) {
    const size_t chunk = len > 4096 ? 4096 : len;
    spi_.writeBytes(data, chunk);
    data += chunk;
    len -= chunk;
  }
}

void ST7789Ascii::resetPanel() {
  digitalWrite(pinRst_, HIGH);
  delay(5);
  digitalWrite(pinRst_, LOW);
  delay(20);
  digitalWrite(pinRst_, HIGH);
  delay(120);
}

void ST7789Ascii::setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1,
                                   uint16_t y1) {
  uint8_t data[4];

  writeCommand(ST7789_CASET);
  data[0] = static_cast<uint8_t>(x0 >> 8);
  data[1] = static_cast<uint8_t>(x0 & 0xFF);
  data[2] = static_cast<uint8_t>(x1 >> 8);
  data[3] = static_cast<uint8_t>(x1 & 0xFF);
  writeData(data, sizeof(data));

  writeCommand(ST7789_RASET);
  data[0] = static_cast<uint8_t>(y0 >> 8);
  data[1] = static_cast<uint8_t>(y0 & 0xFF);
  data[2] = static_cast<uint8_t>(y1 >> 8);
  data[3] = static_cast<uint8_t>(y1 & 0xFF);
  writeData(data, sizeof(data));

  writeCommand(ST7789_RAMWR);
}

void ST7789Ascii::begin() {
  pinMode(pinCs_, OUTPUT);
  pinMode(pinDc_, OUTPUT);
  pinMode(pinRst_, OUTPUT);

  unselect();
  resetPanel();

  spi_.beginTransaction(SPISettings(ST7789_SPI_FREQ, MSBFIRST, ST7789_SPI_MODE));
  select();

  writeCommand(ST7789_SWRESET);
  delay(150);

  writeCommand(ST7789_SLPOUT);
  delay(120);

  uint8_t data = 0x55;
  writeCommand(ST7789_COLMOD);
  writeData(&data, 1);

  data = ST7789_MADCTL_LANDSCAPE;
  writeCommand(ST7789_MADCTL);
  writeData(&data, 1);

  writeCommand(ST7789_INVON);
  delay(10);

  writeCommand(ST7789_NORON);
  delay(10);

  writeCommand(ST7789_DISPON);
  delay(120);

  unselect();
  spi_.endTransaction();
}

void ST7789Ascii::fillScreen(uint16_t color) {
  uint8_t buf[512];
  for (size_t i = 0; i < sizeof(buf); i += 2) {
    buf[i] = static_cast<uint8_t>(color >> 8);
    buf[i + 1] = static_cast<uint8_t>(color & 0xFF);
  }

  spi_.beginTransaction(SPISettings(ST7789_SPI_FREQ, MSBFIRST, ST7789_SPI_MODE));
  select();
  setAddressWindow(0, 0, width_ - 1, height_ - 1);

  uint32_t totalBytes = static_cast<uint32_t>(width_) * height_ * 2U;
  while (totalBytes > 0) {
    uint16_t chunk = totalBytes > sizeof(buf) ? sizeof(buf)
                                               : static_cast<uint16_t>(totalBytes);
    writeData(buf, chunk);
    totalBytes -= chunk;
  }

  unselect();
  spi_.endTransaction();
}

void ST7789Ascii::fillRect(int16_t x, int16_t y, uint16_t w, uint16_t h,
                           uint16_t color) {
  if (w == 0 || h == 0) {
    return;
  }

  int16_t dstX = x;
  int16_t dstY = y;
  uint16_t drawW = w;
  uint16_t drawH = h;

  if (dstX < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstX);
    if (cut >= drawW) {
      return;
    }
    drawW = static_cast<uint16_t>(drawW - cut);
    dstX = 0;
  }
  if (dstY < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstY);
    if (cut >= drawH) {
      return;
    }
    drawH = static_cast<uint16_t>(drawH - cut);
    dstY = 0;
  }

  if (dstX >= static_cast<int16_t>(width_) || dstY >= static_cast<int16_t>(height_)) {
    return;
  }

  if (static_cast<uint16_t>(dstX) + drawW > width_) {
    drawW = static_cast<uint16_t>(width_ - static_cast<uint16_t>(dstX));
  }
  if (static_cast<uint16_t>(dstY) + drawH > height_) {
    drawH = static_cast<uint16_t>(height_ - static_cast<uint16_t>(dstY));
  }
  if (drawW == 0 || drawH == 0) {
    return;
  }

  uint8_t buf[512];
  for (size_t i = 0; i < sizeof(buf); i += 2) {
    buf[i] = static_cast<uint8_t>(color >> 8);
    buf[i + 1] = static_cast<uint8_t>(color & 0xFF);
  }

  spi_.beginTransaction(SPISettings(ST7789_SPI_FREQ, MSBFIRST, ST7789_SPI_MODE));
  select();
  setAddressWindow(static_cast<uint16_t>(dstX), static_cast<uint16_t>(dstY),
                   static_cast<uint16_t>(dstX + drawW - 1U),
                   static_cast<uint16_t>(dstY + drawH - 1U));

  uint32_t totalBytes = static_cast<uint32_t>(drawW) * drawH * 2U;
  while (totalBytes > 0) {
    const uint16_t chunk = totalBytes > sizeof(buf)
                               ? static_cast<uint16_t>(sizeof(buf))
                               : static_cast<uint16_t>(totalBytes);
    writeData(buf, chunk);
    totalBytes -= chunk;
  }

  unselect();
  spi_.endTransaction();
}

void ST7789Ascii::drawChar8x16(uint16_t x, uint16_t y, char ch, uint16_t fgColor,
                               uint16_t bgColor) {
  if (ch < LCD_ASCII_FIRST || ch > LCD_ASCII_LAST) {
    return;
  }
  if (x > width_ - LCD_CHAR_W || y > height_ - LCD_CHAR_H) {
    return;
  }

  const uint8_t *glyph = LCD_ASCII8x16[static_cast<uint8_t>(ch) - LCD_ASCII_FIRST];
  uint8_t px[2];

  spi_.beginTransaction(SPISettings(ST7789_SPI_FREQ, MSBFIRST, ST7789_SPI_MODE));
  select();
  setAddressWindow(x, y, x + LCD_CHAR_W - 1, y + LCD_CHAR_H - 1);

  for (uint16_t row = 0; row < LCD_CHAR_H; ++row) {
    for (uint16_t col = 0; col < LCD_CHAR_W; ++col) {
      const uint8_t low = glyph[col];
      const uint8_t high = glyph[col + 8];
      const uint8_t bit = (row < 8) ? ((low >> row) & 0x01U)
                                    : ((high >> (row - 8)) & 0x01U);
      const uint16_t color = bit ? fgColor : bgColor;

      px[0] = static_cast<uint8_t>(color >> 8);
      px[1] = static_cast<uint8_t>(color & 0xFF);
      writeData(px, sizeof(px));
    }
  }

  unselect();
  spi_.endTransaction();
}

void ST7789Ascii::drawString8x16(uint16_t x, uint16_t y, const char *str,
                                 uint16_t fgColor, uint16_t bgColor) {
  if (str == nullptr) {
    return;
  }

  uint16_t cursorX = x;
  uint16_t cursorY = y;

  while (*str != '\0') {
    if (*str == '\n') {
      cursorX = x;
      cursorY = static_cast<uint16_t>(cursorY + LCD_CHAR_H);
      if (cursorY > height_ - LCD_CHAR_H) {
        break;
      }
      ++str;
      continue;
    }

    if (cursorX > width_ - LCD_CHAR_W) {
      cursorX = x;
      cursorY = static_cast<uint16_t>(cursorY + LCD_CHAR_H);
    }
    if (cursorY > height_ - LCD_CHAR_H) {
      break;
    }

    drawChar8x16(cursorX, cursorY, *str, fgColor, bgColor);
    cursorX = static_cast<uint16_t>(cursorX + LCD_CHAR_W);
    ++str;
  }
}

void ST7789Ascii::drawBitmapMono(int16_t x, int16_t y, const uint8_t *data,
                                 uint16_t w, uint16_t h, uint16_t fgColor,
                                 uint16_t bgColor) {
  if (data == nullptr || w == 0 || h == 0) {
    return;
  }

  int16_t dstX = x;
  int16_t dstY = y;
  uint16_t srcX = 0;
  uint16_t srcY = 0;
  uint16_t drawW = w;
  uint16_t drawH = h;

  if (dstX < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstX);
    if (cut >= drawW) {
      return;
    }
    srcX = cut;
    drawW = static_cast<uint16_t>(drawW - cut);
    dstX = 0;
  }
  if (dstY < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstY);
    if (cut >= drawH) {
      return;
    }
    srcY = cut;
    drawH = static_cast<uint16_t>(drawH - cut);
    dstY = 0;
  }

  if (dstX >= static_cast<int16_t>(width_) || dstY >= static_cast<int16_t>(height_)) {
    return;
  }

  if (static_cast<uint16_t>(dstX) + drawW > width_) {
    drawW = static_cast<uint16_t>(width_ - static_cast<uint16_t>(dstX));
  }
  if (static_cast<uint16_t>(dstY) + drawH > height_) {
    drawH = static_cast<uint16_t>(height_ - static_cast<uint16_t>(dstY));
  }
  if (drawW == 0 || drawH == 0) {
    return;
  }

  const uint16_t srcRowBytes = static_cast<uint16_t>((w + 7U) / 8U);
  uint8_t outBuf[512];
  size_t outLen = 0;

  spi_.beginTransaction(SPISettings(ST7789_SPI_FREQ, MSBFIRST, ST7789_SPI_MODE));
  select();
  setAddressWindow(static_cast<uint16_t>(dstX), static_cast<uint16_t>(dstY),
                   static_cast<uint16_t>(dstX + drawW - 1U),
                   static_cast<uint16_t>(dstY + drawH - 1U));

  for (uint16_t row = 0; row < drawH; ++row) {
    const uint8_t *src =
        data + static_cast<size_t>(srcY + row) * srcRowBytes;
    for (uint16_t col = 0; col < drawW; ++col) {
      const uint16_t sx = static_cast<uint16_t>(srcX + col);
      const uint8_t byteVal = src[sx / 8U];
      const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x07U));
      const uint16_t color = (byteVal & bitMask) ? fgColor : bgColor;

      outBuf[outLen++] = static_cast<uint8_t>(color >> 8);
      outBuf[outLen++] = static_cast<uint8_t>(color & 0xFF);
      if (outLen >= sizeof(outBuf)) {
        writeData(outBuf, outLen);
        outLen = 0;
      }
    }
  }

  if (outLen > 0) {
    writeData(outBuf, outLen);
  }

  unselect();
  spi_.endTransaction();
}

void ST7789Ascii::drawBitmapMono(int16_t x, int16_t y, const uint8_t *data,
                                 uint16_t w, uint16_t h, uint16_t fgColor,
                                 uint16_t bgColor, uint16_t srcX,
                                 uint16_t srcY, uint16_t srcW,
                                 uint16_t srcH) {
  if (data == nullptr || w == 0 || h == 0) {
    return;
  }

  if (srcX >= w || srcY >= h) {
    return;
  }

  if (srcX + srcW > w) {
    srcW = static_cast<uint16_t>(w - srcX);
  }
  if (srcY + srcH > h) {
    srcH = static_cast<uint16_t>(h - srcY);
  }
  if (srcW == 0 || srcH == 0) {
    return;
  }

  int16_t dstX = x;
  int16_t dstY = y;
  uint16_t drawW = srcW;
  uint16_t drawH = srcH;

  if (dstX < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstX);
    if (cut >= drawW) {
      return;
    }
    srcX = static_cast<uint16_t>(srcX + cut);
    drawW = static_cast<uint16_t>(drawW - cut);
    dstX = 0;
  }
  if (dstY < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstY);
    if (cut >= drawH) {
      return;
    }
    srcY = static_cast<uint16_t>(srcY + cut);
    drawH = static_cast<uint16_t>(drawH - cut);
    dstY = 0;
  }

  if (dstX >= static_cast<int16_t>(width_) || dstY >= static_cast<int16_t>(height_)) {
    return;
  }

  if (static_cast<uint16_t>(dstX) + drawW > width_) {
    drawW = static_cast<uint16_t>(width_ - static_cast<uint16_t>(dstX));
  }
  if (static_cast<uint16_t>(dstY) + drawH > height_) {
    drawH = static_cast<uint16_t>(height_ - static_cast<uint16_t>(dstY));
  }
  if (drawW == 0 || drawH == 0) {
    return;
  }

  const uint16_t srcRowBytes = static_cast<uint16_t>((w + 7U) / 8U);
  uint8_t outBuf[512];
  size_t outLen = 0;

  spi_.beginTransaction(SPISettings(ST7789_SPI_FREQ, MSBFIRST, ST7789_SPI_MODE));
  select();
  setAddressWindow(static_cast<uint16_t>(dstX), static_cast<uint16_t>(dstY),
                   static_cast<uint16_t>(dstX + drawW - 1U),
                   static_cast<uint16_t>(dstY + drawH - 1U));

  for (uint16_t row = 0; row < drawH; ++row) {
    const uint8_t *src = data + static_cast<size_t>(srcY + row) * srcRowBytes;
    outLen = 0;
    for (uint16_t col = 0; col < drawW; ++col) {
      const uint16_t sx = static_cast<uint16_t>(srcX + col);
      const uint8_t byteVal = src[sx / 8U];
      const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x07U));
      const uint16_t color = (byteVal & bitMask) ? fgColor : bgColor;
      outBuf[outLen++] = static_cast<uint8_t>(color >> 8);
      outBuf[outLen++] = static_cast<uint8_t>(color & 0xFF);
      if (outLen >= sizeof(outBuf)) {
        writeData(outBuf, outLen);
        outLen = 0;
      }
    }
    if (outLen > 0) {
      writeData(outBuf, outLen);
      outLen = 0;
    }
  }

  unselect();
  spi_.endTransaction();
}

void ST7789Ascii::drawBitmapMonoTransparent(int16_t x, int16_t y,
                                            const uint8_t *data, uint16_t w,
                                            uint16_t h, uint16_t fgColor,
                                            int16_t groundY,
                                            uint16_t bgColor) {
  drawBitmapMonoTransparent(x, y, data, w, h, fgColor, 0, 0, w, h, groundY,
                            bgColor);
}

void ST7789Ascii::drawBitmapMonoTransparent(int16_t x, int16_t y,
                                            const uint8_t *data, uint16_t w,
                                            uint16_t h, uint16_t fgColor,
                                            uint16_t srcX, uint16_t srcY,
                                            uint16_t srcW, uint16_t srcH,
                                            int16_t groundY,
                                            uint16_t bgColor) {
  if (data == nullptr || w == 0 || h == 0) {
    return;
  }

  if (srcX >= w || srcY >= h) {
    return;
  }

  if (srcX + srcW > w) {
    srcW = static_cast<uint16_t>(w - srcX);
  }
  if (srcY + srcH > h) {
    srcH = static_cast<uint16_t>(h - srcY);
  }
  if (srcW == 0 || srcH == 0) {
    return;
  }

  int16_t dstX = x;
  int16_t dstY = y;
  uint16_t drawW = srcW;
  uint16_t drawH = srcH;

  if (dstX < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstX);
    if (cut >= drawW) {
      return;
    }
    srcX = static_cast<uint16_t>(srcX + cut);
    drawW = static_cast<uint16_t>(drawW - cut);
    dstX = 0;
  }
  if (dstY < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstY);
    if (cut >= drawH) {
      return;
    }
    srcY = static_cast<uint16_t>(srcY + cut);
    drawH = static_cast<uint16_t>(drawH - cut);
    dstY = 0;
  }

  if (dstX >= static_cast<int16_t>(width_) || dstY >= static_cast<int16_t>(height_)) {
    return;
  }

  if (static_cast<uint16_t>(dstX) + drawW > width_) {
    drawW = static_cast<uint16_t>(width_ - static_cast<uint16_t>(dstX));
  }
  if (static_cast<uint16_t>(dstY) + drawH > height_) {
    drawH = static_cast<uint16_t>(height_ - static_cast<uint16_t>(dstY));
  }
  if (drawW == 0 || drawH == 0) {
    return;
  }

  const uint16_t srcRowBytes = static_cast<uint16_t>((w + 7U) / 8U);
  uint8_t outBuf[512];
  size_t outLen = 0;

  spi_.beginTransaction(SPISettings(ST7789_SPI_FREQ, MSBFIRST, ST7789_SPI_MODE));
  select();
  for (uint16_t row = 0; row < drawH; ++row) {
    const uint8_t *src = data + static_cast<size_t>(srcY + row) * srcRowBytes;
    const int16_t targetY = static_cast<int16_t>(dstY + row);
    // Fast path: if groundY provided and this row is completely in sky area,
    // write full row with fg/bg in one address window instead of per-run writes.
    if (groundY != INT16_MIN && targetY < groundY) {
      setAddressWindow(static_cast<uint16_t>(dstX), static_cast<uint16_t>(targetY),
                       static_cast<uint16_t>(dstX + drawW - 1U),
                       static_cast<uint16_t>(targetY));
      outLen = 0;
      for (uint16_t col = 0; col < drawW; ++col) {
        const uint16_t sx = static_cast<uint16_t>(srcX + col);
        const uint8_t byteVal = src[sx / 8U];
        const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x07U));
        const uint16_t color = (byteVal & bitMask) ? fgColor : bgColor;
        outBuf[outLen++] = static_cast<uint8_t>(color >> 8);
        outBuf[outLen++] = static_cast<uint8_t>(color & 0xFF);
        if (outLen >= sizeof(outBuf)) {
          writeData(outBuf, outLen);
          outLen = 0;
        }
      }
      if (outLen > 0) {
        writeData(outBuf, outLen);
        outLen = 0;
      }
      continue;
    }

    // fallback: scan for runs of set bits and write only those spans
    uint16_t col = 0;
    while (col < drawW) {
      // find next set bit
      while (col < drawW) {
        const uint16_t sx = static_cast<uint16_t>(srcX + col);
        const uint8_t byteVal = src[sx / 8U];
        const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x07U));
        if (byteVal & bitMask) break;
        ++col;
      }
      if (col >= drawW) break;
      // start of run
      const uint16_t runStart = col;
      while (col < drawW) {
        const uint16_t sx = static_cast<uint16_t>(srcX + col);
        const uint8_t byteVal = src[sx / 8U];
        const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (sx & 0x07U));
        if (!(byteVal & bitMask)) break;
        ++col;
      }
      const uint16_t runLen = static_cast<uint16_t>(col - runStart);

      // set address window for this run
      setAddressWindow(static_cast<uint16_t>(dstX + runStart),
                       static_cast<uint16_t>(dstY + row),
                       static_cast<uint16_t>(dstX + runStart + runLen - 1U),
                       static_cast<uint16_t>(dstY + row));

      // prepare and write run pixels
      outLen = 0;
      for (uint16_t r = 0; r < runLen; ++r) {
        outBuf[outLen++] = static_cast<uint8_t>(fgColor >> 8);
        outBuf[outLen++] = static_cast<uint8_t>(fgColor & 0xFF);
        if (outLen >= sizeof(outBuf)) {
          writeData(outBuf, outLen);
          outLen = 0;
        }
      }
      if (outLen > 0) {
        writeData(outBuf, outLen);
        outLen = 0;
      }
    }
  }

  unselect();
  spi_.endTransaction();
}

void ST7789Ascii::pushRgb565(int16_t x, int16_t y, uint16_t w, uint16_t h,
                             const uint16_t *pixels) {
  if (pixels == nullptr || w == 0 || h == 0) {
    return;
  }

  int16_t dstX = x;
  int16_t dstY = y;
  uint16_t srcX = 0;
  uint16_t srcY = 0;
  uint16_t drawW = w;
  uint16_t drawH = h;

  if (dstX < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstX);
    if (cut >= drawW) {
      return;
    }
    srcX = cut;
    drawW = static_cast<uint16_t>(drawW - cut);
    dstX = 0;
  }
  if (dstY < 0) {
    const uint16_t cut = static_cast<uint16_t>(-dstY);
    if (cut >= drawH) {
      return;
    }
    srcY = cut;
    drawH = static_cast<uint16_t>(drawH - cut);
    dstY = 0;
  }

  if (dstX >= static_cast<int16_t>(width_) ||
      dstY >= static_cast<int16_t>(height_)) {
    return;
  }

  if (static_cast<uint16_t>(dstX) + drawW > width_) {
    drawW = static_cast<uint16_t>(width_ - static_cast<uint16_t>(dstX));
  }
  if (static_cast<uint16_t>(dstY) + drawH > height_) {
    drawH = static_cast<uint16_t>(height_ - static_cast<uint16_t>(dstY));
  }
  if (drawW == 0 || drawH == 0) {
    return;
  }

  uint8_t outBuf[512];
  size_t outLen = 0;

  spi_.beginTransaction(SPISettings(ST7789_SPI_FREQ, MSBFIRST, ST7789_SPI_MODE));
  select();
  setAddressWindow(static_cast<uint16_t>(dstX), static_cast<uint16_t>(dstY),
                   static_cast<uint16_t>(dstX + drawW - 1U),
                   static_cast<uint16_t>(dstY + drawH - 1U));

  for (uint16_t row = 0; row < drawH; ++row) {
    const uint16_t *src =
        pixels + static_cast<size_t>(srcY + row) * w + srcX;
    for (uint16_t col = 0; col < drawW; ++col) {
      const uint16_t color = src[col];
      outBuf[outLen++] = static_cast<uint8_t>(color >> 8);
      outBuf[outLen++] = static_cast<uint8_t>(color & 0xFF);
      if (outLen >= sizeof(outBuf)) {
        writeData(outBuf, outLen);
        outLen = 0;
      }
    }
  }

  if (outLen > 0) {
    writeData(outBuf, outLen);
  }

  unselect();
  spi_.endTransaction();
}

}  // namespace esp32_lcd
