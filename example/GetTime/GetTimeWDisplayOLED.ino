#include <DS1302.hpp>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─────────────────────────────────────────────────────────────────────────────
// DS1302 configuration
// ─────────────────────────────────────────────────────────────────────────────

#define DS1302_RST 33
#define DS1302_DAT 27
#define DS1302_CLK 14

// ─────────────────────────────────────────────────────────────────────────────
// OLED configuration
// ─────────────────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void printTwoDigits(Adafruit_SSD1306& disp, uint8_t value) {
  if (value < 10) {
    disp.print('0');
  }
  disp.print(value);
}

void printDayName(Adafruit_SSD1306& disp, uint8_t day) {
  const char* days[] = {"???", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  if (day >= 1 && day <= 7) {
    disp.print(days[day]);
  } else {
    disp.print("???");
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

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  rtc.initiate();
  delay(50);
  
  rtc.halt();
  delay(50);

  // Set time (update these to your current time)
  Ds1302::dateTime setTime;
  setTime.year = 26;     
  setTime.month = 6;     
  setTime.date = 1;      
  setTime.hour = 12;     
  setTime.min = 0;       
  setTime.sec = 0;       
  setTime.day = 1;       // Sunday (1=Sunday, 7=Saturday)
  
  rtc.setDateTime(&setTime);
  delay(50);
  
  rtc.start();
  delay(100);

  Serial.println("DS1302 RTC with OLED Display started");
}

// ─────────────────────────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  Ds1302::dateTime now;
  rtc.getDateTime(&now);

  display.clearDisplay();
  
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.print("20");
  printTwoDigits(display, now.year);
  display.print("-");
  printTwoDigits(display, now.month);
  display.print("-");
  printTwoDigits(display, now.date);
  
  display.setTextSize(3);
  display.setCursor(0, 24);
  printTwoDigits(display, now.hour);
  display.print(":");
  printTwoDigits(display, now.min);
  display.print(":");
  printTwoDigits(display, now.sec);
  
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("Day: ");
  printDayName(display, now.day);
  
  display.display();
  
  delay(1000);
}