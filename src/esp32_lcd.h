#ifndef ESP32_LCD_H
#define ESP32_LCD_H

#include <Arduino.h>
#include <SPI.h>

namespace esp32_lcd {

constexpr uint16_t LCD_WIDTH = 240;
constexpr uint16_t LCD_HEIGHT = 320;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_RED = 0xF800;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_BLUE = 0x001F;

class ST7789Ascii {
 public:
  ST7789Ascii(SPIClass &spi, int8_t pinCs, int8_t pinDc, int8_t pinRst,
              uint16_t width = LCD_WIDTH, uint16_t height = LCD_HEIGHT)
      : spi_(spi),
        pinCs_(pinCs),
        pinDc_(pinDc),
        pinRst_(pinRst),
        width_(width),
        height_(height) {}

  void begin();
  void fillScreen(uint16_t color);
  void fillRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color);
  void drawChar8x16(uint16_t x, uint16_t y, char ch, uint16_t fgColor,
                    uint16_t bgColor);
  void drawString8x16(uint16_t x, uint16_t y, const char *str, uint16_t fgColor,
                      uint16_t bgColor);
  void drawBitmapMono(int16_t x, int16_t y, const uint8_t *data, uint16_t w,
                      uint16_t h, uint16_t fgColor, uint16_t bgColor);
  void drawBitmapMono(int16_t x, int16_t y, const uint8_t *data, uint16_t w,
                      uint16_t h, uint16_t fgColor, uint16_t bgColor,
                      uint16_t srcX, uint16_t srcY, uint16_t srcW,
                      uint16_t srcH);
  // Draw 1bpp bitmap where '1' bits use fgColor. In the fast sky path, '0'
  // bits are filled with bgColor so sprites can match the scene background.
  void drawBitmapMonoTransparent(int16_t x, int16_t y, const uint8_t *data,
                                 uint16_t w, uint16_t h, uint16_t fgColor,
                                 int16_t groundY = INT16_MIN,
                                 uint16_t bgColor = COLOR_BLACK);
  void drawBitmapMonoTransparent(int16_t x, int16_t y, const uint8_t *data,
                                 uint16_t w, uint16_t h, uint16_t fgColor,
                                 uint16_t srcX, uint16_t srcY, uint16_t srcW,
                                 uint16_t srcH, int16_t groundY = INT16_MIN,
                                 uint16_t bgColor = COLOR_BLACK);
  void pushRgb565(int16_t x, int16_t y, uint16_t w, uint16_t h,
                  const uint16_t *pixels);

 private:
  SPIClass &spi_;
  int8_t pinCs_;
  int8_t pinDc_;
  int8_t pinRst_;
  uint16_t width_;
  uint16_t height_;

  void writeCommand(uint8_t cmd);
  void writeData(const uint8_t *data, size_t len);
  void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
  void resetPanel();
  void select();
  void unselect();
};

}  // namespace esp32_lcd

#endif
