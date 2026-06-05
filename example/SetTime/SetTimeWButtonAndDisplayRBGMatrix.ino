#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <DS1302.hpp>

// ================= RGB MATRIX CONFIG =================
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

MatrixPanel_I2S_DMA *dma_display = nullptr;

// ================= DS1302 =================
#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

// ================= COLORS =================
uint16_t myBLACK, myWHITE, myRED, myGREEN, myBLUE;

// ================= TIME STATE =================
uint8_t STATE_YEAR = 0;
uint8_t STATE_MONTH = 1;
uint8_t STATE_DAY = 1;
uint8_t STATE_HOUR = 0;
uint8_t STATE_MINUTE = 0;

uint8_t lastSecond = 255;
uint8_t lastDisplayedDay = 99;

// ================= DISPLAY TIMING =================
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastColonToggleTime = 0;
bool colonVisible = true;

// ================= EDIT MODE =================
bool editMode = false;

enum Field {
  YEAR,
  MONTH,
  DAY,
  HOUR,
  MINUTE,
  CONFIRM
};

Field field = YEAR;

unsigned long lastFieldBlink = 0;
bool fieldVisible = true;

// ================= BUTTON =================
#define BUTTON_PIN 34

enum Button {
  NONE,
  SW1_LEFT,
  SW2_OK,
  SW4_RIGHT
};

bool buttonWasDown = false;

// ================= HELPERS =================
const uint8_t daysInMonth[] = {
  31,28,31,30,31,30,31,31,30,31,30,31
};

uint8_t getDaysInMonth(uint8_t month, uint8_t year) {
  if (month == 2 && (year % 4 == 0)) return 29;
  return daysInMonth[month - 1];
}

void printTwoDigits(uint8_t value) {
  if (value < 10) dma_display->print('0');
  dma_display->print(value);
}

bool showField(Field f) {
  if (!editMode) return true;
  return (field != f) || fieldVisible;
}

// ================= BUTTON READ =================
Button readButtonRaw() {
  int val = analogRead(BUTTON_PIN);

  if (val < 150)  return SW1_LEFT;
  if (val < 700)  return SW2_OK;
  if (val < 2100) return SW4_RIGHT;

  return NONE;
}

// ================= ADJUST =================
void adjust(int dir) {
  switch (field) {
    case YEAR:   STATE_YEAR   = constrain(STATE_YEAR + dir, 0, 99); break;
    case MONTH:  STATE_MONTH  = constrain(STATE_MONTH + dir, 1, 12); break;
    case DAY:    STATE_DAY    = constrain(STATE_DAY + dir, 1, 31); break;
    case HOUR:   STATE_HOUR   = constrain(STATE_HOUR + dir, 0, 23); break;
    case MINUTE: STATE_MINUTE = constrain(STATE_MINUTE + dir, 0, 59); break;
    default: break;
  }
}

// ================= BUTTON HANDLER =================
void handleButtons() {
  Button b = readButtonRaw();

  if (b == NONE) {
    buttonWasDown = false;
    return;
  }

  if (buttonWasDown) return;
  buttonWasDown = true;

  if (b == SW2_OK) {
    if (!editMode) {
      editMode = true;
      field = YEAR;
      return;
    }

    if (field == CONFIRM) {
      Ds1302::dateTime dt;
      dt.year = STATE_YEAR;
      dt.month = STATE_MONTH;
      dt.date = STATE_DAY;
      dt.hour = STATE_HOUR;
      dt.min = STATE_MINUTE;
      dt.sec = 0;
      dt.day = 1;

      rtc.setDateTime(&dt);
      rtc.start();

      editMode = false;
      return;
    }

    field = (Field)(field + 1);
    return;
  }

  if (!editMode) return;

  if (b == SW1_LEFT) adjust(-1);
  if (b == SW4_RIGHT) adjust(+1);
}

// ================= TIME UPDATE =================
void updateTime() {
  Ds1302::dateTime now;
  rtc.getDateTime(&now);

  STATE_YEAR = now.year;
  STATE_MONTH = now.month;
  STATE_DAY = now.date;
  STATE_HOUR = now.hour;
  STATE_MINUTE = now.min;
  lastSecond = now.sec;
}

// ================= DISPLAY =================
void displayTime() {
  unsigned long currentTime = millis();

  if (currentTime - lastDisplayUpdateTime < 500) return;
  lastDisplayUpdateTime = currentTime;

  if (currentTime - lastColonToggleTime >= 500) {
    colonVisible = !colonVisible;
    lastColonToggleTime = currentTime;
  }

  if (editMode && (currentTime - lastFieldBlink >= 400)) {
    fieldVisible = !fieldVisible;
    lastFieldBlink = currentTime;
  }

  dma_display->setTextSize(2);
  dma_display->setTextColor(myGREEN);

  dma_display->fillRect(0, 18, 64, 20, myBLACK);

  dma_display->setCursor(0, 18);

  if (showField(HOUR)) printTwoDigits(STATE_HOUR);
  else dma_display->print("  ");

  dma_display->setCursor(20, 18);

  if (colonVisible) dma_display->print(":");
  else dma_display->print(" ");

  dma_display->setCursor(28, 18);

  if (showField(MINUTE)) printTwoDigits(STATE_MINUTE);
  else dma_display->print("  ");

  dma_display->setTextSize(1);
  dma_display->setCursor(53, 25);

  if (!editMode) printTwoDigits(lastSecond);
  else dma_display->print("  ");
}

// ================= DATE =================
void updateDateDisplay() {
  const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

  dma_display->setTextSize(1);
  dma_display->setTextColor(myBLUE);

  dma_display->fillRect(2, 2, 60, 8, myBLACK);
  dma_display->setCursor(2, 2);

  char dateStr[11];
  sprintf(dateStr, "20%02d-%02d-%02d", STATE_YEAR, STATE_MONTH, STATE_DAY);

  if (showField(YEAR) || showField(MONTH) || showField(DAY))
    dma_display->print(dateStr);
  else
    dma_display->print("           ");

  dma_display->setTextColor(myGREEN);

  dma_display->fillRect(2, 10, 60, 8, myBLACK);
  dma_display->setCursor(2, 10);

  if (showField(DAY)) {
    Ds1302::dateTime now;
    rtc.getDateTime(&now);

    if (now.day >= 1 && now.day <= 7)
      dma_display->print(days[now.day - 1]);
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  rtc.initiate();
  delay(50);

  if (rtc.ishalted()) rtc.start();

  Ds1302::dateTime now;
  rtc.getDateTime(&now);

  STATE_YEAR = now.year;
  STATE_MONTH = now.month;
  STATE_DAY = now.date;
  STATE_HOUR = now.hour;
  STATE_MINUTE = now.min;
  lastSecond = now.sec;
  lastDisplayedDay = now.day;

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.gpio.e = E_PIN;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
  dma_display->clearScreen();

  myBLACK = dma_display->color565(0,0,0);
  myGREEN = dma_display->color565(0,255,0);
  myBLUE  = dma_display->color565(0,0,255);
}

// ================= LOOP =================
void loop() {
  handleButtons();

  if (!editMode) {
    updateTime();
  }

  displayTime();
  updateDateDisplay();
}