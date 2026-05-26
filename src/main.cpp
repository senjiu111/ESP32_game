#include <Arduino.h>
#include <SPI.h>

#include <array>
#include <cstdio>
#include <cstring>

#include "esp32_lcd.h"
#include "dino_game.h"
#include "game2048.h"
#include "key_input.h"
#include "menu_covers.h"
#include "snake_game.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

using esp32_lcd::ST7789Ascii;

namespace {

constexpr int LCD_PIN_MOSI = 11;
constexpr int LCD_PIN_SCLK = 12;
constexpr int LCD_PIN_CS = 10;
constexpr int LCD_PIN_DC = 9;
constexpr int LCD_PIN_RST = 8;

constexpr uint16_t LCD_LOGICAL_WIDTH = 320;
constexpr uint16_t LCD_LOGICAL_HEIGHT = 240;

constexpr uint16_t SCREEN_BG = esp32_lcd::COLOR_WHITE;
constexpr uint16_t SCREEN_FG = esp32_lcd::COLOR_BLACK;
constexpr uint16_t SCREEN_HI = esp32_lcd::COLOR_RED;
constexpr uint16_t SCREEN_DIM = 0x8410;
constexpr uint16_t SCREEN_LINE = 0xD69A;

enum class AppState : uint8_t {
  kMenu = 0,
  kDino,
  kGame2048,
  kSnake,
};

constexpr std::array<const char *, 3> kMenuItems = {{
    "Dino Jump",
    "2048",
    "Snake",
}};

SPIClass lcdSpi(FSPI);
ST7789Ascii lcd(lcdSpi, LCD_PIN_CS, LCD_PIN_DC, LCD_PIN_RST,
                 LCD_LOGICAL_WIDTH, LCD_LOGICAL_HEIGHT);
key_input::KeyInput keys;
DinoGame dinoGame;
Game2048 game2048;
SnakeGame snakeGame;

// RTOS primitives
static QueueHandle_t gEventQueue = nullptr;
static SemaphoreHandle_t gSpiMutex = nullptr;
static TaskHandle_t gRenderTaskHandle = nullptr;



AppState currentState = AppState::kMenu;
size_t menuIndex = 0;
char statusLine[64] = "Ready";
bool menuNeedsRedraw = true;
bool stateNeedsRedraw = true;

void showStatus(const char *text) {
  std::snprintf(statusLine, sizeof(statusLine), "%s", text);
}

void drawHeader(const char *title, const char *subtitle) {
  lcd.fillScreen(SCREEN_BG);
  lcd.drawString8x16(8, 8, title, SCREEN_HI, SCREEN_BG);
  if (subtitle != nullptr) {
    lcd.drawString8x16(8, 28, subtitle, SCREEN_DIM, SCREEN_BG);
  }
}

void drawRectOutline(int16_t x, int16_t y, uint16_t w, uint16_t h,
                     uint16_t color, uint8_t thickness) {
  for (uint8_t i = 0; i < thickness; ++i) {
    lcd.fillRect(static_cast<int16_t>(x + i), static_cast<int16_t>(y + i),
                 static_cast<uint16_t>(w - i * 2U), 1, color);
    lcd.fillRect(static_cast<int16_t>(x + i),
                 static_cast<int16_t>(y + h - 1U - i),
                 static_cast<uint16_t>(w - i * 2U), 1, color);
    lcd.fillRect(static_cast<int16_t>(x + i), static_cast<int16_t>(y + i), 1,
                 static_cast<uint16_t>(h - i * 2U), color);
    lcd.fillRect(static_cast<int16_t>(x + w - 1U - i),
                 static_cast<int16_t>(y + i), 1,
                 static_cast<uint16_t>(h - i * 2U), color);
  }
}

void drawCenteredString(int16_t x, int16_t y, uint16_t w, const char *text,
                        uint16_t fg, uint16_t bg) {
  const uint16_t textW = static_cast<uint16_t>(std::strlen(text) * 8U);
  int16_t tx = static_cast<int16_t>(x + (static_cast<int16_t>(w) -
                                        static_cast<int16_t>(textW)) /
                                           2);
  if (tx < 0) {
    tx = 0;
  }
  lcd.drawString8x16(static_cast<uint16_t>(tx), static_cast<uint16_t>(y), text,
                     fg, bg);
}

void renderMenu() {
  static_assert(kMenuCoversCount == kMenuItems.size(),
                "menu cover count must match menu item count");

  drawHeader("ESP32-S3 Game Menu", "Choose a cover, OK enter");

  constexpr int16_t coverY = 58;
  constexpr uint16_t coverW = 88;
  constexpr uint16_t coverH = 88;
  constexpr int16_t gap = 16;
  constexpr int16_t startX =
      static_cast<int16_t>((LCD_LOGICAL_WIDTH - 3 * coverW - 2 * gap) / 2);
  for (size_t i = 0; i < kMenuItems.size(); ++i) {
    const int16_t x = static_cast<int16_t>(
        startX + static_cast<int16_t>(i) * static_cast<int16_t>(coverW + gap));
    const bool selected = i == menuIndex;
    drawRectOutline(static_cast<int16_t>(x - 4), static_cast<int16_t>(coverY - 4),
                    static_cast<uint16_t>(coverW + 8U),
                    static_cast<uint16_t>(coverH + 8U),
                    selected ? SCREEN_HI : SCREEN_LINE, selected ? 3 : 1);
    lcd.pushRgb565(x, coverY, kMenuCovers[i].w, kMenuCovers[i].h,
                   kMenuCovers[i].pixels);
    drawCenteredString(x, 154, coverW, kMenuItems[i],
                       selected ? SCREEN_HI : SCREEN_FG, SCREEN_BG);
  }

  drawCenteredString(0, 184, LCD_LOGICAL_WIDTH, statusLine, SCREEN_DIM,
                     SCREEN_BG);
  drawCenteredString(0, 208, LCD_LOGICAL_WIDTH, "UP/DOWN select  OK start",
                     SCREEN_DIM, SCREEN_BG);
}

void setState(AppState nextState) {
  currentState = nextState;
  menuNeedsRedraw = true;
  stateNeedsRedraw = true;
}

void handleMenuEvent(const key_input::KeyEvent &event) {
  if (!event.pressed) {
    return;
  }

  switch (event.key) {
    case key_input::KeyId::kUp:
    case key_input::KeyId::kLeft:
      menuIndex = (menuIndex == 0) ? (kMenuItems.size() - 1) : (menuIndex - 1);
      menuNeedsRedraw = true;
      break;
    case key_input::KeyId::kDown:
    case key_input::KeyId::kRight:
      menuIndex = (menuIndex + 1) % kMenuItems.size();
      menuNeedsRedraw = true;
      break;
    case key_input::KeyId::kOk:
      if (menuIndex == 0) {
        showStatus("Entering Dino Jump");
        dinoGame.begin();
        setState(AppState::kDino);
      } else if (menuIndex == 1) {
        showStatus("Entering 2048");
        game2048.begin();
        setState(AppState::kGame2048);
      } else if (menuIndex == 2) {
        showStatus("Entering Snake");
        snakeGame.begin();
        setState(AppState::kSnake);
      }
      break;
    case key_input::KeyId::kBack:
      showStatus("Choose a game with OK");
      menuNeedsRedraw = true;
      break;
    default:
      break;
  }
}

void handleDinoEvent(const key_input::KeyEvent &event) {
  if (event.pressed && event.key == key_input::KeyId::kBack) {
    showStatus("Back to menu");
    setState(AppState::kMenu);
    return;
  }

  dinoGame.update(&event, 1);
}

void handle2048Event(const key_input::KeyEvent &event) {
  if (event.pressed && event.key == key_input::KeyId::kBack) {
    showStatus("Back to menu");
    setState(AppState::kMenu);
    return;
  }

  game2048.update(&event, 1);
}

void handleSnakeEvent(const key_input::KeyEvent &event) {
  if (event.pressed && event.key == key_input::KeyId::kBack) {
    showStatus("Back to menu");
    setState(AppState::kMenu);
    return;
  }

  snakeGame.update(&event, 1);
}

void renderCurrentState() {
  switch (currentState) {
    case AppState::kMenu:
      if (menuNeedsRedraw) {
        renderMenu();
        menuNeedsRedraw = false;
      }
      break;
    case AppState::kDino:
      if (stateNeedsRedraw || dinoGame.needsRender()) {
        dinoGame.render(lcd);
        stateNeedsRedraw = false;
      }
      break;
    case AppState::kGame2048:
      if (stateNeedsRedraw || game2048.needsRender()) {
        game2048.render(lcd);
        stateNeedsRedraw = false;
      }
      break;
    case AppState::kSnake:
      if (stateNeedsRedraw || snakeGame.needsRender()) {
        snakeGame.render(lcd);
        stateNeedsRedraw = false;
      }
      break;
  }
}

void dispatchEvent(const key_input::KeyEvent &event) {
  if (currentState == AppState::kMenu) {
    handleMenuEvent(event);
  } else if (currentState == AppState::kDino) {
    handleDinoEvent(event);
  } else if (currentState == AppState::kGame2048) {
    handle2048Event(event);
  } else if (currentState == AppState::kSnake) {
    handleSnakeEvent(event);
  }
}

}  // namespace

// Forward declarations for RTOS task functions (file-scope)
static void InputTaskFunc(void *pvParameters);
static void GameTaskFunc(void *pvParameters);
static void RenderTaskFunc(void *pvParameters);

void setup() {
  keys.begin();

  lcdSpi.begin(LCD_PIN_SCLK, -1, LCD_PIN_MOSI, LCD_PIN_CS);
  lcd.begin();

  showStatus("Menu ready");
  renderCurrentState();

  // create RTOS primitives
  gEventQueue = xQueueCreate(32, sizeof(key_input::KeyEvent));
  gSpiMutex = xSemaphoreCreateMutex();

  // create tasks
  xTaskCreatePinnedToCore(InputTaskFunc, "InputTask", 2048, nullptr,
                          tskIDLE_PRIORITY + 3, nullptr, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(GameTaskFunc, "GameTask", 4096, nullptr,
                          tskIDLE_PRIORITY + 2, nullptr, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(RenderTaskFunc, "RenderTask", 4096, nullptr,
                          tskIDLE_PRIORITY + 1, &gRenderTaskHandle,
                          tskNO_AFFINITY);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// --- RTOS task implementations ---

static void InputTaskFunc(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    key_input::KeyEvent events[key_input::KeyInput::kKeyCount];
    const size_t n = keys.poll(events, key_input::KeyInput::kKeyCount);
    for (size_t i = 0; i < n; ++i) {
      if (gEventQueue) xQueueSend(gEventQueue, &events[i], 0);
    }
    vTaskDelay(pdMS_TO_TICKS(8));
  }
}

static void GameTaskFunc(void *pvParameters) {
  (void)pvParameters;
  const TickType_t frameTicks = pdMS_TO_TICKS(40); // ~25Hz
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    // drain events
    key_input::KeyEvent ev;
    key_input::KeyEvent dinoBuf[key_input::KeyInput::kKeyCount];
    size_t dinoCount = 0;
    while (gEventQueue && xQueueReceive(gEventQueue, &ev, 0) == pdTRUE) {
      if (currentState == AppState::kDino) {
        // Allow BACK to be handled by main dispatch (to return to menu)
        if (ev.pressed && ev.key == key_input::KeyId::kBack) {
          dispatchEvent(ev);
        } else {
          dinoBuf[dinoCount++] = ev;
        }
      } else {
        dispatchEvent(ev);
      }
    }

    if (currentState == AppState::kDino) {
      if (dinoCount > 0) {
        dinoGame.update(dinoBuf, dinoCount);
      } else {
        dinoGame.update(nullptr, 0);
      }
      if (dinoGame.needsRender() && gRenderTaskHandle) {
        xTaskNotifyGive(gRenderTaskHandle);
      }
    } else if (currentState == AppState::kSnake) {
      snakeGame.update(nullptr, 0);
      if (snakeGame.needsRender() && gRenderTaskHandle) {
        xTaskNotifyGive(gRenderTaskHandle);
      }
    } else {
      if (menuNeedsRedraw || stateNeedsRedraw ||
          (currentState == AppState::kGame2048 && game2048.needsRender()) ||
          (currentState == AppState::kSnake && snakeGame.needsRender())) {
        if (gRenderTaskHandle) xTaskNotifyGive(gRenderTaskHandle);
      }
    }

    vTaskDelayUntil(&lastWake, frameTicks);
  }
}

static void RenderTaskFunc(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (gSpiMutex && xSemaphoreTake(gSpiMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      if (currentState == AppState::kDino) {
        dinoGame.render(lcd);
      } else {
        renderCurrentState();
      }
      xSemaphoreGive(gSpiMutex);
    }
  }
}
