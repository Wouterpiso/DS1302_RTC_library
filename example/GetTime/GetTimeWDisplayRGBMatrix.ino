#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <DS1302.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// RGB Matrix configuration
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

// ─────────────────────────────────────────────────────────────────────────────
// DS1302 configuration
// ─────────────────────────────────────────────────────────────────────────────
#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

MatrixPanel_I2S_DMA *dma_display = nullptr;
Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

uint16_t myBLACK, myWHITE, myRED, myGREEN, myBLUE;

// Clock state
uint8_t STATE_YEAR = 00;
uint8_t STATE_MONTH = 1;
uint8_t STATE_DAY = 1;
uint8_t STATE_HOUR = 0;
uint8_t STATE_MINUTE = 0;

// Display state
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastColonToggleTime = 0;
bool colonVisible = true;
uint8_t lastSecond = 255;
uint8_t lastDisplayedDay = 99;

const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

uint8_t getDaysInMonth(uint8_t month, uint8_t year) {
  if (month == 2 && (year % 4 == 0)) {
    return 29; // Leap year
  }
  return daysInMonth[month - 1];
}

void printTwoDigits(uint8_t value) {
  if (value < 10) {
    dma_display->print('0');
  }
  dma_display->print(value);
}

// ─────────────────────────────────────────────────────────────────────────────
// States
// ─────────────────────────────────────────────────────────────────────────────

void updateStates() {
  unsigned long currentTime = millis();
  
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

// ─────────────────────────────────────────────────────────────────────────────
// Date Logic
// ─────────────────────────────────────────────────────────────────────────────

void displayTime() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastDisplayUpdateTime < 500) {
    return;
  }
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
  if (colonVisible) {
    dma_display->print(":");
  } else {
    dma_display->print(" ");
  }
  
  dma_display->setCursor(28, 18);
  printTwoDigits(STATE_MINUTE);
  
  dma_display->setTextSize(1);
  dma_display->setCursor(53, 25);
  printTwoDigits(lastSecond);
}

void updateDateDisplay() {
  if (lastDisplayedDay != 99 && lastDisplayedDay == STATE_DAY) {
    return;
  }
  
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

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  rtc.initiate();
  delay(50);

  if (rtc.ishalted()) {
    rtc.start();
  }

  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  delay(10);
  
  STATE_YEAR = now.year;
  STATE_MONTH = now.month;
  STATE_DAY = now.date;
  STATE_HOUR = now.hour;
  STATE_MINUTE = now.min;
  lastSecond = now.sec;
  lastDisplayedDay = now.day;

  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   
    PANEL_RES_Y,   
    PANEL_CHAIN    
  );

  mxconfig.gpio.e = E_PIN;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90); // 0-255
  dma_display->clearScreen();

  myBLACK = dma_display->color565(0, 0, 0);
  myWHITE = dma_display->color565(255, 255, 255);
  myRED = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE = dma_display->color565(0, 0, 255);

  dma_display->fillScreen(myBLACK);
  
  dma_display->setTextWrap(false);
  dma_display->setTextSize(1);
  dma_display->setTextColor(myBLUE);
  dma_display->setCursor(2, 2);
  char dateStr[11];
  sprintf(dateStr, "20%02d-%02d-%02d", STATE_YEAR, STATE_MONTH, STATE_DAY);
  dma_display->print(dateStr);
  
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  dma_display->setTextColor(myGREEN);
  dma_display->setCursor(2, 10);
  if (lastDisplayedDay >= 1 && lastDisplayedDay <= 7) {
    dma_display->print(days[lastDisplayedDay - 1]);
  }

  Serial.println("DS1302 RTC + RGB Matrix initialized");
}

// ─────────────────────────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  updateStates();
  displayTime();
  updateDateDisplay();
}