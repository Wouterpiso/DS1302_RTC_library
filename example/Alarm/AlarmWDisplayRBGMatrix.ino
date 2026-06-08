#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <DS1302.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// RGB matrix panel configuration
// ─────────────────────────────────────────────────────────────────────────────
#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 17
#define E_PIN 18
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

MatrixPanel_I2S_DMA *dma_display = nullptr;
Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

uint16_t myBLACK, myWHITE, myRED, myGREEN, myBLUE;

uint8_t STATE_YEAR  = 0;
uint8_t STATE_MONTH = 1;
uint8_t STATE_DAY   = 1;
uint8_t STATE_HOUR  = 0;
uint8_t STATE_MINUTE = 0;

unsigned long lastDisplayUpdateTime = 0;
unsigned long lastColonToggleTime   = 0;
bool colonVisible    = true;
uint8_t lastSecond   = 255;
uint8_t lastDisplayedDay = 99;

// ─────────────────────────────────────────────────────────────────────────────
// Alarm state
// ─────────────────────────────────────────────────────────────────────────────
bool alarmRunning        = false;
unsigned long alarmStart = 0;
#define ALARM_DURATION_MS 10000

const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

uint8_t getDaysInMonth(uint8_t month, uint8_t year) {
  if (month == 2 && (year % 4 == 0)) return 29;
  return daysInMonth[month - 1];
}

// ─────────────────────────────────────────────────────────────────────────────
// Alarm animation
// ─────────────────────────────────────────────────────────────────────────────

uint16_t hsvToColor565(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  if (s == 0) {
    r = g = b = v;
  } else {
    uint8_t region   = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    switch (region) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      default: r = v; g = p; b = q; break;
    }
  }
  return dma_display->color565(r, g, b);
}

void drawRainbowFrame(uint8_t offset) {
  const int startX = 0;
  const int startY = 0;
  const int endX   = 63;
  const int endY   = 31;

  for (int y = startY; y <= endY; y++) {
    for (int x = startX; x <= endX; x++) {
      uint8_t hue = (uint8_t)(x + y + offset);

      dma_display->drawPixelRGB888(
        x, y,
        (hue * 6) % 256,
        (hue * 3 + 85) % 256,
        (255 - hue * 6) % 256
      );
    }
  }

  if ((millis() / 250) % 2) {
    dma_display->setTextColor(myWHITE);
  } else {
    dma_display->setTextColor(myRED);
  }

  dma_display->setTextSize(2);

  dma_display->setCursor(8, 1);
  dma_display->print("WAKE");

  dma_display->setCursor(5, 17);
  dma_display->print("UP!!!!");
}

void runAlarmAnimation() {
  static uint8_t rainbowOffset = 0;
  static unsigned long lastFrame = 0;

  unsigned long now = millis();

  if (now - lastFrame >= 30) {
    lastFrame = now;
    drawRainbowFrame(rainbowOffset);
    rainbowOffset += 3;
  }

  if (now - alarmStart >= ALARM_DURATION_MS) {
    alarmRunning = false;
    rtc.clearAlarm();
    dma_display->clearScreen();

    lastDisplayedDay = 99;
    lastDisplayUpdateTime = 0;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Clock display
// ─────────────────────────────────────────────────────────────────────────────

void printTwoDigits(uint8_t value) {
  if (value < 10) dma_display->print('0');
  dma_display->print(value);
}

void updateStates() {
  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  delay(5);

  uint8_t currentSecond = now.sec;

  if (lastSecond != 255 && currentSecond == 0 && lastSecond != 0) {
    STATE_MINUTE++;
    if (STATE_MINUTE >= 60) {
      STATE_MINUTE = 0;
      STATE_HOUR++;
      if (STATE_HOUR >= 24) {
        STATE_HOUR = 0;
        STATE_DAY++;
        uint8_t maxDays = getDaysInMonth(STATE_MONTH, STATE_YEAR);
        if (STATE_DAY > maxDays) {
          STATE_DAY = 1;
          STATE_MONTH++;
          if (STATE_MONTH > 12) {
            STATE_MONTH = 1;
            STATE_YEAR++;
          }
        }
        lastDisplayedDay = 99;
      }
    }
  }

  lastSecond = currentSecond;
}

void displayTime() {
  unsigned long currentTime = millis();

  if (currentTime - lastDisplayUpdateTime < 500) return;
  lastDisplayUpdateTime = currentTime;

  if (currentTime - lastColonToggleTime >= 500) {
    colonVisible = !colonVisible;
    lastColonToggleTime = currentTime;
  }

  dma_display->setTextSize(2, 2);
  dma_display->setTextColor(myGREEN);
  dma_display->fillRect(0, 18, 64, 20, myBLACK);

  dma_display->setCursor(0, 18);
  printTwoDigits(STATE_HOUR);

  dma_display->setCursor(20, 18);
  dma_display->print(colonVisible ? ":" : " ");

  dma_display->setCursor(28, 18);
  printTwoDigits(STATE_MINUTE);

  dma_display->setTextSize(1);
  dma_display->setCursor(53, 25);
  printTwoDigits(lastSecond);
}

void updateDateDisplay() {
  if (lastDisplayedDay != 99 && lastDisplayedDay == STATE_DAY) return;

  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

  dma_display->setTextWrap(false);
  dma_display->setTextSize(1);
  dma_display->setTextColor(myBLUE);
  dma_display->fillRect(2, 2, 60, 8, myBLACK);
  dma_display->setCursor(2, 2);
  char dateStr[11];
  sprintf(dateStr, "20%02d-%02d-%02d", STATE_YEAR, STATE_MONTH, STATE_DAY);
  dma_display->print(dateStr);

  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  delay(5);

  dma_display->setTextColor(myGREEN);
  dma_display->fillRect(2, 10, 60, 8, myBLACK);
  dma_display->setCursor(2, 10);
  if (now.day >= 1 && now.day <= 7) {
    dma_display->print(days[now.day - 1]);
    lastDisplayedDay = now.day;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  rtc.initiate();
  delay(50);

  if (rtc.ishalted()) rtc.start();

  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  delay(10);

  STATE_YEAR   = now.year;
  STATE_MONTH  = now.month;
  STATE_DAY    = now.date;
  STATE_HOUR   = now.hour;
  STATE_MINUTE = now.min;
  lastSecond   = now.sec;
  lastDisplayedDay = now.day;

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.gpio.e = E_PIN;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
  dma_display->clearScreen();

  myBLACK = dma_display->color565(0, 0, 0);
  myWHITE = dma_display->color565(255, 255, 255);
  myRED   = dma_display->color565(0, 0, 255);
  myGREEN = dma_display->color565(255, 0, 0);
  myBLUE  = dma_display->color565(0, 255, 0);

  dma_display->fillScreen(myBLACK);
  dma_display->setTextWrap(false);

  uint8_t alarmSec = (now.sec + 10) % 60;
  uint8_t alarmMin = now.min + ((now.sec + 10) >= 60 ? 1 : 0);
  rtc.setAlarm(now.hour, alarmMin, alarmSec);

  Serial.println("Ready. Alarm in 10 seconds.");
}

void loop() {
  if (alarmRunning) {
    runAlarmAnimation();
    return;
  }

  if (rtc.checkAlarm()) {
    alarmRunning = true;
    alarmStart   = millis();
    return;
  }

  updateStates();
  displayTime();
  updateDateDisplay();
}