/*
 * Pico Tama - XIAO ESP32C6 + ST7789V2 240x280
 *
 * 5状態アニメーション版:
 * 夜 / 湿潤 / 普通 / 乾燥 / タッチ
 *
 * LCD:  DIN=D10, CLK=D8, CS=D3, DC=D4, RST=D5, BL=D6
 * Soil: A0, Light: A2, Touch: D1
 */

#include <Arduino.h>
#include <SPI.h>
#include <st7789v2.h>

#include "pico_sprites.h"

#define SOIL_PIN A0
#define LIGHT_PIN A2
#define TOUCH_PIN D1

#define LCD_DIN D10
#define LCD_CLK D8
#define LCD_CS D3
#define LCD_DC D4
#define LCD_RST D5
#define LCD_BL D6

constexpr int SOIL_DRY_TH = 2200;
constexpr int SOIL_WET_TH = 1800;
constexpr int SOIL_HYST = 70;
constexpr bool SOIL_DRY_IS_HIGH = true;
constexpr float SOIL_FILTER_ALPHA = 0.18f;
constexpr int LIGHT_NIGHT_TH = 300;
constexpr int LIGHT_HYST = 120;

constexpr unsigned long SENSOR_INTERVAL_MS = 90;
constexpr unsigned long ANIM_INTERVAL_MS = 180;

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 280;
constexpr int PICO_DRAW_W = PICO_W;
constexpr int PICO_DRAW_H = PICO_H;
constexpr int PICO_X = (SCREEN_W - PICO_DRAW_W) / 2;
constexpr int PICO_Y = 96;
constexpr int MODE_LABEL_Y = 224;
constexpr int SOIL_VALUE_Y = 266;

constexpr uint16_t C_BG_DAY = 0xFFFF;
constexpr uint16_t C_BG_NIGHT = 0x39E7;
constexpr uint16_t C_DARK = 0x2104;
constexpr uint16_t C_GRAY = 0x8410;
constexpr uint16_t C_GREEN = 0x07E0;
constexpr uint16_t C_BLUE = 0x001F;
constexpr uint16_t C_ORANGE = 0xFD20;
constexpr uint16_t C_PINK = 0xF9B5;
constexpr uint16_t C_WHITE = 0xFFFF;
constexpr uint16_t C_SLEEP = 0xB7FF;

st7789v2 Display;

enum SoilMode { SOIL_NORMAL, SOIL_DRY, SOIL_WET };
enum PicoMode { MODE_NORMAL, MODE_DRY, MODE_WET, MODE_NIGHT, MODE_TOUCH };

SoilMode soilMode = SOIL_NORMAL;
float filteredSoil = -1.0f;
int shownSoilValue = -1;
bool nightMode = false;
bool touchWasHigh = false;
bool touchLocked = false;
uint8_t touchLoopsRemaining = 0;

PicoMode currentMode = MODE_NORMAL;
PicoMode targetMode = MODE_NORMAL;
uint8_t animFrame = 0;

unsigned long lastSensorAt = 0;
unsigned long lastAnimAt = 0;

uint16_t pixelBuffer[PICO_PIXELS];

int averageAnalog(int pin, int n = 8) {
  long sum = 0;
  for (int i = 0; i < n; i++) {
    sum += analogRead(pin);
    delay(1);
  }
  return sum / n;
}

int smoothSoil(int raw) {
  if (filteredSoil < 0.0f) {
    filteredSoil = raw;
  } else {
    filteredSoil = filteredSoil * (1.0f - SOIL_FILTER_ALPHA) + raw * SOIL_FILTER_ALPHA;
  }
  return static_cast<int>(filteredSoil + 0.5f);
}

void lcdCommand(uint8_t cmd) {
  digitalWrite(LCD_DC, LOW);
  digitalWrite(LCD_CS, LOW);
  SPI.transfer(cmd);
  digitalWrite(LCD_CS, HIGH);
}

void lcdData8(uint8_t data) {
  digitalWrite(LCD_DC, HIGH);
  digitalWrite(LCD_CS, LOW);
  SPI.transfer(data);
  digitalWrite(LCD_CS, HIGH);
}

void lcdSetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  lcdCommand(0x2A);
  lcdData8(x0 >> 8);
  lcdData8(x0);
  lcdData8(x1 >> 8);
  lcdData8(x1);

  lcdCommand(0x2B);
  lcdData8((y0 + 20) >> 8);
  lcdData8(y0 + 20);
  lcdData8((y1 + 20) >> 8);
  lcdData8(y1 + 20);

  lcdCommand(0x2C);
}

void pushPixels(const uint16_t* pixels, int count) {
  digitalWrite(LCD_DC, HIGH);
  digitalWrite(LCD_CS, LOW);
  for (int i = 0; i < count; i++) {
    SPI.transfer16(pixels[i]);
  }
  digitalWrite(LCD_CS, HIGH);
}

void fillRectFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  const int maxRows = PICO_PIXELS / w;
  for (uint16_t row = 0; row < h; row += maxRows) {
    const uint16_t rows = min<uint16_t>(maxRows, h - row);
    const int pixels = w * rows;
    for (int i = 0; i < pixels; i++) {
      pixelBuffer[i] = color;
    }
    lcdSetWindow(x, y + row, x + w - 1, y + row + rows - 1);
    pushPixels(pixelBuffer, pixels);
  }
}

uint16_t bgFor(PicoMode mode) {
  return mode == MODE_NIGHT ? C_BG_NIGHT : C_BG_DAY;
}

const uint16_t* frameData(PicoMode mode, uint8_t frame) {
  switch (mode) {
    case MODE_DRY:
      return pico_dry[frame % PICO_DRY_FRAMES];
    case MODE_WET:
      return pico_wet[frame % PICO_WET_FRAMES];
    case MODE_NIGHT:
      return pico_sleep[frame % PICO_SLEEP_FRAMES];
    case MODE_TOUCH:
      return pico_touch[frame % PICO_TOUCH_FRAMES];
    case MODE_NORMAL:
    default:
      return pico_normal[frame % PICO_NORMAL_FRAMES];
  }
}

uint8_t frameCount(PicoMode mode) {
  switch (mode) {
    case MODE_DRY:
      return PICO_DRY_FRAMES;
    case MODE_WET:
      return PICO_WET_FRAMES;
    case MODE_NIGHT:
      return PICO_SLEEP_FRAMES;
    case MODE_TOUCH:
      return PICO_TOUCH_FRAMES;
    case MODE_NORMAL:
    default:
      return PICO_NORMAL_FRAMES;
  }
}

const char* modeLabel(PicoMode mode) {
  switch (mode) {
    case MODE_DRY:
      return "DRY";
    case MODE_WET:
      return "WET";
    case MODE_NIGHT:
      return "NIGHT";
    case MODE_TOUCH:
      return "TOUCH";
    case MODE_NORMAL:
    default:
      return "OK";
  }
}

uint16_t modeColor(PicoMode mode) {
  switch (mode) {
    case MODE_DRY:
      return C_ORANGE;
    case MODE_WET:
      return C_BLUE;
    case MODE_NIGHT:
      return C_WHITE;
    case MODE_TOUCH:
      return C_PINK;
    case MODE_NORMAL:
    default:
      return C_GREEN;
  }
}

void drawModeLabel(PicoMode active) {
  const uint16_t bg = bgFor(active);
  Display.DrawRectangle(82, MODE_LABEL_Y, 158, MODE_LABEL_Y + 14, bg, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Display.DrawString_EN(96, MODE_LABEL_Y, modeLabel(active), &Font16, bg, modeColor(active));
}

void drawSoilValue(int value, PicoMode mode) {
  const uint16_t bg = bgFor(mode);
  const uint16_t fg = mode == MODE_NIGHT ? C_WHITE : C_DARK;
  char buf[16];
  snprintf(buf, sizeof(buf), "S%4d", value);
  Display.DrawRectangle(92, SOIL_VALUE_Y, 148, SOIL_VALUE_Y + 12, bg, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Display.DrawString_EN(96, SOIL_VALUE_Y, buf, &Font16, bg, fg);
}

void drawSleepZzz(uint8_t frame) {
  const int y = 18 - (frame % 4);
  Display.DrawRectangle(8, 10, 66, 38, C_BG_NIGHT, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Display.DrawString_EN(14, y, "zzz", &Font16, C_BG_NIGHT, C_SLEEP);
}

void clearScreenFor(PicoMode mode) {
  fillRectFast(0, 0, SCREEN_W, SCREEN_H, bgFor(mode));
  drawModeLabel(mode);
  if (shownSoilValue >= 0) {
    drawSoilValue(shownSoilValue, mode);
  }
}

void clearSpriteArea(PicoMode mode) {
  fillRectFast(PICO_X, PICO_Y, PICO_DRAW_W, PICO_DRAW_H, bgFor(mode));
}

void renderFrame(PicoMode mode, uint8_t frame) {
  const uint16_t* src = frameData(mode, frame);
  const uint16_t bg = bgFor(mode);

  for (int i = 0; i < PICO_PIXELS; i++) {
    const uint16_t c = pgm_read_word(src + i);
    pixelBuffer[i] = c == PICO_TRANSPARENT ? bg : c;
  }

  lcdSetWindow(PICO_X, PICO_Y, PICO_X + PICO_W - 1, PICO_Y + PICO_H - 1);
  pushPixels(pixelBuffer, PICO_PIXELS);

  if (mode == MODE_NIGHT) {
    drawSleepZzz(frame);
  }
}

void updateSoilMode(int raw) {
  const int wetEdge = min(SOIL_WET_TH, SOIL_DRY_TH);
  const int dryEdge = max(SOIL_WET_TH, SOIL_DRY_TH);
  const bool dryNow = SOIL_DRY_IS_HIGH ? raw > dryEdge + SOIL_HYST : raw < wetEdge - SOIL_HYST;
  const bool wetNow = SOIL_DRY_IS_HIGH ? raw < wetEdge - SOIL_HYST : raw > dryEdge + SOIL_HYST;
  const bool dryGone = SOIL_DRY_IS_HIGH ? raw < dryEdge - SOIL_HYST : raw > wetEdge + SOIL_HYST;
  const bool wetGone = SOIL_DRY_IS_HIGH ? raw > wetEdge + SOIL_HYST : raw < dryEdge - SOIL_HYST;

  if (soilMode == SOIL_DRY && dryGone) {
    soilMode = SOIL_NORMAL;
  } else if (soilMode == SOIL_WET && wetGone) {
    soilMode = SOIL_NORMAL;
  } else if (soilMode == SOIL_NORMAL) {
    if (dryNow) {
      soilMode = SOIL_DRY;
    } else if (wetNow) {
      soilMode = SOIL_WET;
    }
  }
}

void updateNightMode(int raw) {
  if (nightMode) {
    if (raw > LIGHT_NIGHT_TH + LIGHT_HYST) {
      nightMode = false;
    }
  } else if (raw < LIGHT_NIGHT_TH - LIGHT_HYST) {
    nightMode = true;
  }
}

void handleTouch(int value) {
  const bool high = value == HIGH;
  if (high && !touchWasHigh && !touchLocked && !nightMode) {
    touchLocked = true;
    touchLoopsRemaining = 3;
    requestMode(MODE_TOUCH);
  }
  touchWasHigh = high;
}

PicoMode modeFromSensors() {
  if (nightMode) {
    return MODE_NIGHT;
  }
  if (touchLocked) {
    return MODE_TOUCH;
  }
  if (soilMode == SOIL_DRY) {
    return MODE_DRY;
  }
  if (soilMode == SOIL_WET) {
    return MODE_WET;
  }
  return MODE_NORMAL;
}

void requestMode(PicoMode next) {
  targetMode = next;
}

void switchToTargetMode() {
  if (currentMode == targetMode) {
    return;
  }

  const uint16_t oldBg = bgFor(currentMode);
  currentMode = targetMode;
  animFrame = 0;

  if (oldBg != bgFor(currentMode)) {
    clearScreenFor(currentMode);
  } else {
    drawModeLabel(currentMode);
    if (shownSoilValue >= 0) {
      drawSoilValue(shownSoilValue, currentMode);
    }
    clearSpriteArea(currentMode);
  }

  renderFrame(currentMode, animFrame);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  analogSetPinAttenuation(LIGHT_PIN, ADC_11db);
  pinMode(TOUCH_PIN, INPUT);

  Display.SetRotate(0);
  Display.Init(LCD_CS, LCD_DC, LCD_RST, LCD_BL);

  SPI.begin(LCD_CLK, -1, LCD_DIN, LCD_CS);
  SPI.setFrequency(40000000);
  SPI.setDataMode(SPI_MODE3);
  SPI.setBitOrder(MSBFIRST);

  Display.SetBacklight(100);
  clearScreenFor(MODE_NORMAL);
  renderFrame(MODE_NORMAL, 0);
  lastAnimAt = millis();
}

void loop() {
  const unsigned long now = millis();

  if (now - lastSensorAt >= SENSOR_INTERVAL_MS) {
    lastSensorAt = now;
    const int rawSoil = averageAnalog(SOIL_PIN, 12);
    const int stableSoil = smoothSoil(rawSoil);
    const int rawLight = averageAnalog(LIGHT_PIN);
    const int touch = digitalRead(TOUCH_PIN);

    updateSoilMode(stableSoil);
    updateNightMode(rawLight);
    handleTouch(touch);
    if (!touchLocked || nightMode) {
      if (nightMode) {
        touchLocked = false;
      }
      requestMode(modeFromSensors());
    }

    if (abs(stableSoil - shownSoilValue) >= 8 || shownSoilValue < 0) {
      shownSoilValue = stableSoil;
      drawSoilValue(shownSoilValue, currentMode);
    }

    Serial.print("soil=");
    Serial.print(rawSoil);
    Serial.print(" stable=");
    Serial.print(stableSoil);
    Serial.print(" soilMode=");
    Serial.print(soilMode == SOIL_DRY ? "DRY" : (soilMode == SOIL_WET ? "WET" : "NORMAL"));
    Serial.print(" light=");
    Serial.print(rawLight);
    Serial.print(" mode=");
    Serial.println(modeLabel(targetMode));
  }

  if (now - lastAnimAt < ANIM_INTERVAL_MS) {
    return;
  }
  lastAnimAt = now;

  animFrame = (animFrame + 1) % frameCount(currentMode);
  renderFrame(currentMode, animFrame);

  if (currentMode == MODE_TOUCH && touchLocked && animFrame == frameCount(MODE_TOUCH) - 1) {
    if (touchLoopsRemaining > 0) {
      touchLoopsRemaining--;
    }
    if (touchLoopsRemaining == 0) {
      touchLocked = false;
      requestMode(modeFromSensors());
    }
  }

  if (animFrame == frameCount(currentMode) - 1) {
    switchToTargetMode();
  }
}
