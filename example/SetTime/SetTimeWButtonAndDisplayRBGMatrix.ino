#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <DS1302.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// RGB Matrix Panel Configuration (64x64 HUB75 display + DS1302 RTC + buttons)
// ─────────────────────────────────────────────────────────────────────────────

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

// RGB matrix GPIO mapping
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

// DS1302 RTC pins
#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

// Analog button input (resistor ladder)
#define BUTTON_PIN 34

// ─────────────────────────────────────────────────────────────────────────────
// Core objects
// ─────────────────────────────────────────────────────────────────────────────

MatrixPanel_I2S_DMA *dma_display = nullptr;
Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

// Precomputed RGB565 colors used across UI
uint16_t myBLACK, myWHITE, myRED, myGREEN, myBLUE;

// ─────────────────────────────────────────────────────────────────────────────
// Edit mode state
// ─────────────────────────────────────────────────────────────────────────────

enum Button { NONE, SW1_LEFT, SW2_OK, SW4_RIGHT };

bool editMode        = false;
bool editScreenDirty = false;
bool buttonWasDown   = false;

// Editable fields for date/time
enum Field { YEAR, MONTH, DAY, HOUR, MINUTE };
Field field = YEAR;

// Current RTC-backed state (mirrored locally for editing)
uint8_t STATE_YEAR   = 0;
uint8_t STATE_MONTH  = 1;
uint8_t STATE_DAY    = 1;
uint8_t STATE_HOUR   = 0;
uint8_t STATE_MINUTE = 0;

// Timing/state tracking for display refresh logic
unsigned long lastDisplayUpdateTime = 0;
bool    colonVisible     = true;
uint8_t lastSecond       = 255;
uint8_t lastDisplayedDay = 99;
uint8_t currentSecond    = 0;

// Days per month lookup table (non-leap year baseline)
const uint8_t daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns number of days in a month (handles leap years simply via %4 rule)
uint8_t getDaysInMonth(uint8_t m, uint8_t y) {
  if (m == 2 && (y % 4 == 0)) return 29;
  return daysInMonth[m - 1];
}

// Prints a 2-digit value (leading zero if needed)
void printTwoDigits(uint8_t value) {
  if (value < 10) dma_display->print('0');
  dma_display->print(value);
}

// ─────────────────────────────────────────────────────────────────────────────
// Time editing logic
// ─────────────────────────────────────────────────────────────────────────────

// Writes edited values back to RTC
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

// Adjusts currently selected field in edit mode
void adjust(int dir) {
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

// Handles button input (enter, next field, adjust values)
void processButton(Button b) {
  if (b == SW2_OK) {
    if (!editMode) {
      // Enter edit mode
      editMode        = true;
      field           = YEAR;
      editScreenDirty = true;
      dma_display->fillScreen(myBLACK);
      return;
    }

    // Move through fields, or apply when finished
    if (field == MINUTE) {
      applyTime();
      editMode              = false;
      lastDisplayedDay      = 99;
      lastDisplayUpdateTime = 0;
      dma_display->fillScreen(myBLACK);
      return;
    }

    field = (Field)(field + 1);
    editScreenDirty = true;
    return;
  }

  // Ignore adjustments unless in edit mode
  if (!editMode) return;

  if (b == SW1_LEFT)  adjust(-1);
  if (b == SW4_RIGHT) adjust(+1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Edit mode display
// ─────────────────────────────────────────────────────────────────────────────

void displayEditMode() {
  if (!editScreenDirty) return;
  editScreenDirty = false;

  dma_display->fillScreen(myBLACK);

  dma_display->setTextWrap(false);

  dma_display->setTextSize(1);
  dma_display->setTextColor(myRED);
  dma_display->setCursor(2, 2);
  dma_display->print("EDIT MODE");

  const char* fieldNames[] = {"YEAR", "MON", "DAY", "HOUR", "MIN"};

  dma_display->setTextSize(2);
  dma_display->setTextColor(myWHITE);
  dma_display->setCursor(2, 18);
  dma_display->print(fieldNames[field]);

  // Display current value of selected field (bottom-right)
  dma_display->setTextSize(1);
  dma_display->setTextColor(myGREEN);
  dma_display->setCursor(53, 25);

  if (field == YEAR)       printTwoDigits(STATE_YEAR);
  else if (field == MONTH) printTwoDigits(STATE_MONTH);
  else if (field == DAY)   printTwoDigits(STATE_DAY);
  else if (field == HOUR)  printTwoDigits(STATE_HOUR);
  else if (field == MINUTE)printTwoDigits(STATE_MINUTE);
}

// ─────────────────────────────────────────────────────────────────────────────
// Clock update logic (RTC sync + manual rollover tracking)
// ─────────────────────────────────────────────────────────────────────────────

void updateStates() {
  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  delay(5);

  currentSecond = now.sec;

  // Detect second rollover to increment internal counters
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

// ─────────────────────────────────────────────────────────────────────────────
// Clock display (HH:MM + seconds)
// ─────────────────────────────────────────────────────────────────────────────

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
}

// ─────────────────────────────────────────────────────────────────────────────
// Date + weekday display
// ─────────────────────────────────────────────────────────────────────────────

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

  // Initialize state from RTC
  STATE_YEAR   = now.year;
  STATE_MONTH  = now.month;
  STATE_DAY    = now.date;
  STATE_HOUR   = now.hour;
  STATE_MINUTE = now.min;

  lastSecond       = now.sec;
  lastDisplayedDay = now.day;

  // Display configuration
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.gpio.e   = E_PIN;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
  dma_display->clearScreen();

  // Color setup
  myBLACK = dma_display->color565(0, 0, 0);
  myWHITE = dma_display->color565(255, 255, 255);
  myRED   = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE  = dma_display->color565(0, 0, 255);

  dma_display->fillScreen(myBLACK);

  Serial.println("Ready");
}

// ─────────────────────────────────────────────────────────────────────────────
// Main loop (button handling + timed tasks)
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  unsigned long now = millis();

  // Button polling with simple debounce
  static unsigned long lastButtonCheck = 0;
  static uint8_t noneCount = 0;

  if (now - lastButtonCheck >= 20) {
    lastButtonCheck = now;

    int val = analogRead(BUTTON_PIN);
    Button b = NONE;

    if      (val < 150)  b = SW1_LEFT;
    else if (val < 700)  b = SW2_OK;
    else if (val < 1400) b = NONE;
    else if (val < 2100) b = SW4_RIGHT;

    if (b == NONE) {
      noneCount++;
      if (noneCount >= 3) {
        buttonWasDown = false;
        noneCount = 0;
      }
    } else {
      noneCount = 0;
      if (!buttonWasDown) {
        buttonWasDown = true;
        processButton(b);
      }
    }

    return;
  }

  // Edit mode rendering
  if (editMode) {
    displayEditMode();
    return;
  }

  // Periodic clock updates
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