#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <DS1302.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Pin definitions
// ─────────────────────────────────────────────────────────────────────────────
#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_LED 12
#define B2_PIN 13
#define A_PIN  23
#define B_PIN  19
#define C_PIN   5
#define D_PIN  17
#define E_PIN  18
#define LAT_PIN 4
#define OE_PIN  15
#define CLK_PIN 16

#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

#define BUTTON_PIN 34

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────
MatrixPanel_I2S_DMA *dma_display = nullptr;
Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

uint16_t myBLACK, myWHITE, myRED, myGREEN, myBLUE, myYELLOW;

// Clock state
uint8_t STATE_YEAR   = 0;
uint8_t STATE_MONTH  = 1;
uint8_t STATE_DAY    = 1;
uint8_t STATE_HOUR   = 0;
uint8_t STATE_MINUTE = 0;

unsigned long lastDisplayUpdateTime = 0;
bool    colonVisible     = true;
uint8_t lastSecond       = 255;
uint8_t lastDisplayedDay = 99;
uint8_t currentSecond    = 0;

// Alarm state
uint8_t ALARM_HOUR   = 0;
uint8_t ALARM_MINUTE = 0;
bool    alarmEnabled     = false;
bool    alarmRunning     = false;
unsigned long alarmStart = 0;
#define ALARM_DURATION_MS 10000

const uint8_t daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};

// ─────────────────────────────────────────────────────────────────────────────
// Button / mode state
// ─────────────────────────────────────────────────────────────────────────────
enum Button { NONE, SW1_LEFT, SW2_OK, SW3_ALARM, SW4_RIGHT, SW5_ENTER };
enum Mode   { CLOCK, EDIT_TIME, EDIT_ALARM };
Mode currentMode = CLOCK;

bool editScreenDirty = false;
bool buttonWasDown   = false;

enum Field { YEAR, MONTH, DAY, HOUR, MINUTE };
Field field = YEAR;

// ── Arrow auto-clear ──────────────────────────────────────────────────────────
unsigned long arrowShownAt = 0;
bool arrowVisible = false;

// ── Konami / special mode state ───────────────────────────────────────────────
const Button KONAMI_SEQ[] = {
  SW2_OK, SW2_OK, SW3_ALARM, SW3_ALARM,
  SW1_LEFT, SW4_RIGHT, SW1_LEFT, SW4_RIGHT,
  SW5_ENTER
};
const uint8_t KONAMI_LEN = 9;
uint8_t  konamiPos       = 0;

enum SpecialMode { SPECIAL_NONE, ARROW_TEST, NYAN_CAT };
SpecialMode specialMode = SPECIAL_NONE;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
uint8_t getDaysInMonth(uint8_t m, uint8_t y) {
  if (m == 2 && (y % 4 == 0)) return 29;
  return daysInMonth[m - 1];
}

void printTwoDigits(uint8_t value) {
  if (value < 10) dma_display->print('0');
  dma_display->print(value);
}

// ─────────────────────────────────────────────────────────────────────────────
// Arrow drawing — all within 64×32, with directional tail, auto-clears 1s
//
// Layout: tail (line) + arrowhead, all drawn with pixels.
// The usable area is 64 wide × 32 tall (rows 0–31).
// We draw a big arrowhead using filled rects/lines and a shaft.
// ─────────────────────────────────────────────────────────────────────────────
void drawArrow(Button dir) {
  dma_display->fillRect(0, 0, 64, 32, myBLACK);

  // Centre of the 64×32 area
  const int cx = 32, cy = 16;

  // Shaft half-length and head size
  const int shaftLen = 14;
  const int headW    = 8;   // half-width of arrowhead base
  const int headLen  = 10;  // length of arrowhead

  uint16_t col = myWHITE;

  switch (dir) {

    case SW4_RIGHT: {
      // Shaft: horizontal left of centre
      dma_display->fillRect(cx - shaftLen, cy - 1, shaftLen, 3, col);
      // Arrowhead: triangle pointing right
      for (int i = 0; i < headLen; i++) {
        int half = headW * (headLen - i) / headLen;
        dma_display->drawFastVLine(cx + i, cy - half, half * 2 + 1, col);
      }
      break;
    }

    case SW1_LEFT: {
      // Shaft: horizontal right of centre
      dma_display->fillRect(cx, cy - 1, shaftLen, 3, col);
      // Arrowhead: triangle pointing left
      for (int i = 0; i < headLen; i++) {
        int half = headW * (headLen - i) / headLen;
        dma_display->drawFastVLine(cx - i, cy - half, half * 2 + 1, col);
      }
      break;
    }

    case SW2_OK: {
      // Shaft: vertical below centre
      dma_display->fillRect(cx - 1, cy, 3, shaftLen, col);
      // Arrowhead: triangle pointing up
      for (int i = 0; i < headLen; i++) {
        int half = headW * (headLen - i) / headLen;
        dma_display->drawFastHLine(cx - half, cy - i, half * 2 + 1, col);
      }
      break;
    }

    case SW3_ALARM: {
      // Shaft: vertical above centre
      dma_display->fillRect(cx - 1, cy - shaftLen, 3, shaftLen, col);
      // Arrowhead: triangle pointing down
      for (int i = 0; i < headLen; i++) {
        int half = headW * (headLen - i) / headLen;
        dma_display->drawFastHLine(cx - half, cy + i, half * 2 + 1, col);
      }
      break;
    }

    case SW5_ENTER: {
      // Circle for OK/Enter — fits in 32px height easily
      dma_display->drawCircle(cx, cy, 10, col);
      dma_display->drawCircle(cx, cy,  9, col);  // thicker
      break;
    }

    default: break;
  }

  arrowShownAt = millis();
  arrowVisible = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Alarm animation — constrained to rows 0–31
// ─────────────────────────────────────────────────────────────────────────────
void drawRainbowFrame(uint8_t offset) {
  // Fill only rows 0–31
  for (int y = 0; y <= 31; y++) {
    for (int x = 0; x <= 63; x++) {
      uint8_t hue = (uint8_t)(x + y + offset);
      dma_display->drawPixelRGB888(x, y,
        (hue * 6) % 256,
        (hue * 3 + 85) % 256,
        (255 - hue * 6) % 256
      );
    }
  }

  dma_display->setTextSize(2);
  dma_display->setCursor(4, 1);
  if ((millis() / 250) % 2) dma_display->setTextColor(myWHITE);
  else                       dma_display->setTextColor(myRED);
  dma_display->print("WAKE");

  dma_display->setCursor(1, 17);
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
}

// ─────────────────────────────────────────────────────────────────────────────
// Nyan Cat — fully constrained to rows 0–31, 64 wide
// ─────────────────────────────────────────────────────────────────────────────
static int nyanTrailOffset = 0;

void drawNyanFrame() {
  dma_display->fillRect(0, 0, 64, 32, myBLACK);

  // Rainbow trail stripes — 6 stripes × 2px = 12px tall, vertically centred
  const uint16_t rainbowStripes[6] = {
    dma_display->color565(255, 0,   0),
    dma_display->color565(255, 140, 0),
    dma_display->color565(255, 255, 0),
    dma_display->color565(0,   255, 0),
    dma_display->color565(0,   100, 255),
    dma_display->color565(180, 0,   255),
  };

  // Cat body top-left: cx=34, cy=10 → body is 13×9, bottom at y=19 — fits in 32
  int cx = 34, cy = 10;
  int trailY   = cy + 1;   // align trail with body middle
  int trailLen = 30 + (nyanTrailOffset % 4);

  for (int s = 0; s < 6; s++) {
    dma_display->fillRect(0, trailY + s * 2, trailLen, 2, rainbowStripes[s]);
  }

  // Pop-tart body
  dma_display->fillRect(cx, cy, 13, 11, dma_display->color565(180, 180, 180));
  dma_display->fillRect(cx+1, cy+1, 11, 9, dma_display->color565(255, 100, 150));

  // Sprinkles
  dma_display->drawPixel(cx+2, cy+2, dma_display->color565(255,0,0));
  dma_display->drawPixel(cx+5, cy+4, dma_display->color565(0,255,0));
  dma_display->drawPixel(cx+8, cy+2, dma_display->color565(255,255,0));
  dma_display->drawPixel(cx+3, cy+7, dma_display->color565(0,200,255));
  dma_display->drawPixel(cx+9, cy+6, dma_display->color565(255,0,255));

  // Cat head — sits above body, must stay >= row 0
  int hx = cx + 10, hy = cy - 5;
  if (hy < 0) hy = 0;
  dma_display->fillRect(hx, hy, 8, 7, dma_display->color565(80,80,80));
  dma_display->drawPixel(hx,     hy,     dma_display->color565(80,80,80));
  dma_display->drawPixel(hx + 6, hy,     dma_display->color565(80,80,80));
  dma_display->drawPixel(hx + 2, hy + 2, myWHITE);
  dma_display->drawPixel(hx + 5, hy + 2, myWHITE);
  dma_display->drawPixel(hx + 2, hy + 5, dma_display->color565(80,80,80));
  dma_display->drawPixel(hx + 5, hy + 5, dma_display->color565(80,80,80));

  // Legs — stay within row 31
  bool legPhase = (nyanTrailOffset / 3) % 2;
  uint16_t legCol = dma_display->color565(80,80,80);
  if (legPhase) {
    dma_display->fillRect(cx+2,  cy+11, 2, 3, legCol);
    dma_display->fillRect(cx+6,  cy+11, 2, 3, legCol);
    dma_display->fillRect(cx+10, cy+10, 2, 3, legCol);
  } else {
    dma_display->fillRect(cx+2,  cy+10, 2, 3, legCol);
    dma_display->fillRect(cx+6,  cy+11, 2, 3, legCol);
    dma_display->fillRect(cx+10, cy+11, 2, 3, legCol);
  }

  // Tail
  bool tailUp = legPhase;
  int tx = cx - 2, ty = tailUp ? cy + 3 : cy + 5;
  dma_display->fillRect(tx, ty, 2, 4, legCol);

  // Stars — kept within rows 0–31
  uint16_t starCol = myWHITE;
  auto drawStar = [&](int x, int y) {
    if (x < 1 || x >= 63 || y < 1 || y >= 31) return;
    dma_display->drawPixel(x,   y,   starCol);
    dma_display->drawPixel(x-1, y,   starCol);
    dma_display->drawPixel(x+1, y,   starCol);
    dma_display->drawPixel(x,   y-1, starCol);
    dma_display->drawPixel(x,   y+1, starCol);
  };

  int o = nyanTrailOffset;
  drawStar(((10 + o) % 58) + 2,  3);
  drawStar(((30 + o) % 58) + 2,  8);
  drawStar(((50 + o) % 58) + 2,  1);
  drawStar(((20 + o) % 58) + 2, 28);
  drawStar(((45 + o) % 58) + 2, 25);

  nyanTrailOffset++;
}

void runNyanCat() {
  static unsigned long lastFrame = 0;
  unsigned long now = millis();
  if (now - lastFrame >= 80) {
    lastFrame = now;
    drawNyanFrame();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Edit: adjust helpers
// ─────────────────────────────────────────────────────────────────────────────
void adjustTime(int dir) {
  switch (field) {
    case YEAR:   STATE_YEAR   = constrain((int)STATE_YEAR   + dir, 0,  99); break;
    case MONTH:  STATE_MONTH  = constrain((int)STATE_MONTH  + dir, 1,  12); break;
    case DAY:    STATE_DAY    = constrain((int)STATE_DAY    + dir, 1,  31); break;
    case HOUR:   STATE_HOUR   = constrain((int)STATE_HOUR   + dir, 0,  23); break;
    case MINUTE: STATE_MINUTE = constrain((int)STATE_MINUTE + dir, 0,  59); break;
    default: break;
  }
  editScreenDirty = true;
}

void adjustAlarm(int dir) {
  switch (field) {
    case HOUR:   ALARM_HOUR   = constrain((int)ALARM_HOUR   + dir, 0, 23); break;
    case MINUTE: ALARM_MINUTE = constrain((int)ALARM_MINUTE + dir, 0, 59); break;
    default: break;
  }
  editScreenDirty = true;
}

void applyTime() {
  Ds1302::dateTime dt;
  dt.year  = STATE_YEAR;
  dt.month = STATE_MONTH;
  dt.date  = STATE_DAY;
  dt.hour  = STATE_HOUR;
  dt.min   = STATE_MINUTE;
  dt.sec   = 0;
  dt.day   = 1;
  currentSecond = 0;
  lastSecond    = 255;
  rtc.setDateTime(&dt);
}

void applyAlarm() {
  if (alarmEnabled) {
    rtc.setAlarm(ALARM_HOUR, ALARM_MINUTE, 0);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Button processing
// ─────────────────────────────────────────────────────────────────────────────
void processButton(Button b) {

  if (alarmRunning) {
    alarmRunning = false;
    rtc.clearAlarm();
    dma_display->clearScreen();
    lastDisplayedDay      = 99;
    lastDisplayUpdateTime = 0;
    return;
  }

  if (specialMode == NYAN_CAT) {
    specialMode = SPECIAL_NONE;
    dma_display->fillScreen(myBLACK);
    lastDisplayedDay      = 99;
    lastDisplayUpdateTime = 0;
    return;
  }

  if (specialMode == ARROW_TEST) {
    drawArrow(b);

    if (b == KONAMI_SEQ[konamiPos]) {
      konamiPos++;
      if (konamiPos == KONAMI_LEN) {
        konamiPos   = 0;
        specialMode = NYAN_CAT;
        nyanTrailOffset = 0;
        dma_display->fillRect(0, 0, 64, 32, myGREEN);
        delay(400);
      }
    } else {
      konamiPos   = 0;
      specialMode = SPECIAL_NONE;
      arrowVisible = false;
      dma_display->fillRect(0, 0, 64, 32, myRED);
      delay(400);
      dma_display->fillScreen(myBLACK);
      lastDisplayedDay      = 99;
      lastDisplayUpdateTime = 0;
    }
    return;
  }

  switch (currentMode) {

    case CLOCK:
      if (b == SW5_ENTER) {
        konamiPos   = 0;
        specialMode = ARROW_TEST;
        dma_display->fillScreen(myBLACK);
        return;
      }
      if (b == SW2_OK) {
        currentMode     = EDIT_TIME;
        field           = YEAR;
        editScreenDirty = true;
        dma_display->fillScreen(myBLACK);
      } else if (b == SW3_ALARM) {
        currentMode     = EDIT_ALARM;
        field           = HOUR;
        editScreenDirty = true;
        dma_display->fillScreen(myBLACK);
      }
      break;

    case EDIT_TIME:
      if (b == SW2_OK) {
        if (field == MINUTE) {
          applyTime();
          currentMode           = CLOCK;
          lastDisplayedDay      = 99;
          lastDisplayUpdateTime = 0;
          dma_display->fillScreen(myBLACK);
        } else {
          field = (Field)(field + 1);
          editScreenDirty = true;
        }
      } else if (b == SW1_LEFT)  { adjustTime(-1); }
        else if (b == SW4_RIGHT) { adjustTime(+1); }
      break;

    case EDIT_ALARM:
      if (b == SW1_LEFT)       { adjustAlarm(-1); }
      else if (b == SW4_RIGHT) { adjustAlarm(+1); }
      else if (b == SW3_ALARM) {
        if (field == HOUR) {
          field           = MINUTE;
          editScreenDirty = true;
        } else {
          alarmEnabled          = true;
          applyAlarm();
          currentMode           = CLOCK;
          lastDisplayedDay      = 99;
          lastDisplayUpdateTime = 0;
          dma_display->fillScreen(myBLACK);
        }
      }
      break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Display: clock-time edit
// ─────────────────────────────────────────────────────────────────────────────
void displayEditTime() {
  if (!editScreenDirty) return;
  editScreenDirty = false;

  dma_display->fillScreen(myBLACK);
  dma_display->setTextWrap(false);

  dma_display->setTextSize(1);
  dma_display->setTextColor(myRED);
  dma_display->setCursor(2, 2);
  dma_display->print("EDIT TIME");

  const char* fieldNames[] = {"YEAR", "MON", "DAY", "HOUR", "MIN"};

  dma_display->setTextSize(2);
  dma_display->setTextColor(myWHITE);
  dma_display->setCursor(2, 18);
  dma_display->print(fieldNames[field]);

  dma_display->setTextSize(1);
  dma_display->setTextColor(myGREEN);
  dma_display->setCursor(53, 25);
  switch (field) {
    case YEAR:   printTwoDigits(STATE_YEAR);   break;
    case MONTH:  printTwoDigits(STATE_MONTH);  break;
    case DAY:    printTwoDigits(STATE_DAY);    break;
    case HOUR:   printTwoDigits(STATE_HOUR);   break;
    case MINUTE: printTwoDigits(STATE_MINUTE); break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Display: alarm edit
// ─────────────────────────────────────────────────────────────────────────────
void displayEditAlarm() {
  if (!editScreenDirty) return;
  editScreenDirty = false;

  dma_display->fillScreen(myBLACK);
  dma_display->setTextWrap(false);

  dma_display->setTextSize(1);
  dma_display->setTextColor(myYELLOW);
  dma_display->setCursor(2, 2);
  dma_display->print("SET ALARM");

  dma_display->setTextSize(2);
  dma_display->setTextColor(myWHITE);
  dma_display->setCursor(0, 18);
  dma_display->print(field == HOUR ? "HOUR" : "MIN");

  dma_display->setTextSize(1);
  dma_display->setTextColor(myYELLOW);
  dma_display->setCursor(53, 25);
  printTwoDigits(field == HOUR ? ALARM_HOUR : ALARM_MINUTE);
}

// ─────────────────────────────────────────────────────────────────────────────
// Display: normal clock
// ─────────────────────────────────────────────────────────────────────────────
void updateStates() {
  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  delay(5);

  currentSecond = now.sec;

  if (lastSecond != 255 && currentSecond == 0 && lastSecond != 0) {
    STATE_MINUTE++;
    if (STATE_MINUTE >= 60) {
      STATE_MINUTE = 0;
      STATE_HOUR++;
      if (STATE_HOUR >= 24) {
        STATE_HOUR = 0;
        STATE_DAY++;
        if (STATE_DAY > getDaysInMonth(STATE_MONTH, STATE_YEAR)) {
          STATE_DAY = 1;
          STATE_MONTH++;
          if (STATE_MONTH > 12) { STATE_MONTH = 1; STATE_YEAR++; }
        }
        lastDisplayedDay = 99;
      }
    }
  }
  lastSecond = currentSecond;
}

void displayTime() {
  unsigned long now = millis();
  if (now - lastDisplayUpdateTime < 500) return;
  lastDisplayUpdateTime = now;
  colonVisible = !colonVisible;

  dma_display->setTextSize(2);
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
  printTwoDigits(currentSecond);

  if (alarmEnabled) {
    dma_display->setTextSize(1);
    dma_display->setTextColor(myYELLOW);
    dma_display->setCursor(48, 40);
    dma_display->print('\x0F');
    dma_display->setCursor(54, 40);
    printTwoDigits(ALARM_HOUR);
    dma_display->print(':');
    printTwoDigits(ALARM_MINUTE);
  }
}

void updateDateDisplay() {
  if (lastDisplayedDay != 99 && lastDisplayedDay == STATE_DAY) return;

  const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

  dma_display->setTextWrap(false);
  dma_display->setTextSize(1);
  dma_display->setTextColor(myBLUE);
  dma_display->fillRect(0, 0, 64, 8, myBLACK);
  dma_display->setCursor(2, 2);

  char dateStr[11];
  sprintf(dateStr, "20%02d-%02d-%02d", STATE_YEAR, STATE_MONTH, STATE_DAY);
  dma_display->print(dateStr);

  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  delay(5);

  dma_display->setTextColor(myGREEN);
  dma_display->fillRect(0, 10, 64, 8, myBLACK);
  dma_display->setCursor(2, 10);
  if (now.day >= 1 && now.day <= 7) {
    dma_display->print(days[now.day - 1]);
    lastDisplayedDay = now.day;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  rtc.initiate();
  delay(50);
  if (rtc.ishalted()) rtc.start();

  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  delay(10);

  STATE_YEAR       = now.year;
  STATE_MONTH      = now.month;
  STATE_DAY        = now.date;
  STATE_HOUR       = now.hour;
  STATE_MINUTE     = now.min;
  lastSecond       = now.sec;
  lastDisplayedDay = now.day;

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.gpio.e   = E_PIN;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
  dma_display->clearScreen();

  myBLACK  = dma_display->color565(0,   0,   0);
  myWHITE  = dma_display->color565(255, 255, 255);
  myRED    = dma_display->color565(0,   0,   255);
  myGREEN  = dma_display->color565(255, 0,   0);
  myBLUE   = dma_display->color565(0,   255, 0);
  myYELLOW = dma_display->color565(220, 0,   255);

  dma_display->fillScreen(myBLACK);
  Serial.println("Ready");
}

// ─────────────────────────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Button polling ────────────────────────────────────────────────────────
  static unsigned long lastButtonCheck = 0;
  static uint8_t noneCount = 0;

  if (now - lastButtonCheck >= 20) {
    lastButtonCheck = now;

    int val = analogRead(BUTTON_PIN);
    Button b = NONE;
    if      (val < 150)  b = SW1_LEFT;
    else if (val < 700)  b = SW2_OK;
    else if (val < 1400) b = SW3_ALARM;
    else if (val < 2100) b = SW4_RIGHT;
    else if (val < 3000) b = SW5_ENTER;

    if (b == NONE) {
      if (++noneCount >= 3) { buttonWasDown = false; noneCount = 0; }
    } else {
      noneCount = 0;
      if (!buttonWasDown) { buttonWasDown = true; processButton(b); }
    }
    return;
  }

  // ── Alarm animation ───────────────────────────────────────────────────────
  if (alarmRunning) {
    runAlarmAnimation();
    return;
  }

  // ── Check if alarm should fire ────────────────────────────────────────────
  if (alarmEnabled && currentMode == CLOCK) {
    if (rtc.checkAlarm()) {
      alarmRunning = true;
      alarmStart   = millis();
      return;
    }
  }

  // ── Nyan Cat ──────────────────────────────────────────────────────────────
  if (specialMode == NYAN_CAT) {
    runNyanCat();
    return;
  }

  // ── Arrow test mode ───────────────────────────────────────────────────────
  if (specialMode == ARROW_TEST) {
    // Auto-clear the arrow after 1 second
    if (arrowVisible && (millis() - arrowShownAt >= 1000)) {
      dma_display->fillRect(0, 0, 64, 32, myBLACK);
      arrowVisible = false;
    }
    return;
  }

  // ── Edit modes ────────────────────────────────────────────────────────────
  if (currentMode == EDIT_TIME) {
    displayEditTime();
    return;
  }

  if (currentMode == EDIT_ALARM) {
    displayEditAlarm();
    return;
  }

  // ── Normal clock ──────────────────────────────────────────────────────────
  static unsigned long lastStateUpdate = 0;
  if (now - lastStateUpdate >= 200) {
    lastStateUpdate = now;
    updateStates();
    return;
  }

  static unsigned long lastTimeDisplay = 0;
  if (now - lastTimeDisplay >= 500) {
    lastTimeDisplay = now;
    displayTime();
    return;
  }

  static unsigned long lastDateDisplay = 0;
  if (now - lastDateDisplay >= 1000) {
    lastDateDisplay = now;
    updateDateDisplay();
    return;
  }
}