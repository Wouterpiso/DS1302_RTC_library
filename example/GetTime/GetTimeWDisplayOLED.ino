#include <DS1302.hpp>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DS1302_RST 33
#define DS1302_DAT 27
#define DS1302_CLK 14

// OLED display setup (128x64, I2C address 0x3C)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  // Initialize RTC
  rtc.initiate();
  delay(50);
  
  // Halt RTC before setting time
  rtc.halt();
  delay(50);

  // Set time (update these to your current time)
  Ds1302::dateTime setTime;
  setTime.year = 26;     // 2026
  setTime.month = 6;     // June
  setTime.date = 1;      // 1st
  setTime.hour = 12;     // 12
  setTime.min = 0;       // 00
  setTime.sec = 0;       // 00
  setTime.day = 1;       // Sunday (1=Sunday, 7=Saturday)
  
  rtc.setDateTime(&setTime);
  delay(50);
  
  // Now start the RTC
  rtc.start();
  delay(100);

  Serial.println("DS1302 RTC with OLED Display started");
}

void loop() {
  Ds1302::dateTime now;
  rtc.getDateTime(&now);

  // Clear display
  display.clearDisplay();
  
  // Set text size and color
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  // Display date on first line
  display.setCursor(0, 0);
  display.print("20");
  printTwoDigits(display, now.year);
  display.print("-");
  printTwoDigits(display, now.month);
  display.print("-");
  printTwoDigits(display, now.date);
  
  // Display time on second line (larger)
  display.setTextSize(3);
  display.setCursor(0, 24);
  printTwoDigits(display, now.hour);
  display.print(":");
  printTwoDigits(display, now.min);
  display.print(":");
  printTwoDigits(display, now.sec);
  
  // Display day of week on last line
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("Day: ");
  printDayName(display, now.day);
  
  // Update display
  display.display();
  
  delay(1000);
}

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
