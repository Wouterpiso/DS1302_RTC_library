#include <DS1302.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// DS1302 configuration
// ─────────────────────────────────────────────────────────────────────────────

#define DS1302_RST 33
#define DS1302_DAT 27
#define DS1302_CLK 14

Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void printTwoDigits(uint8_t value) {
  if (value < 10) {
    Serial.print('0');
  }
  Serial.print(value);
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

  rtc.halt();
  delay(50);

  // Set initial RTC time
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

  Serial.println("DS1302 time reader started");
}

// ─────────────────────────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  Ds1302::dateTime now;
  rtc.getDateTime(&now);

  Serial.print("20");
  printTwoDigits(now.year);
  Serial.print('-');
  printTwoDigits(now.month);
  Serial.print('-');
  printTwoDigits(now.date);
  Serial.print(' ');
  printTwoDigits(now.hour);
  Serial.print(':');
  printTwoDigits(now.min);
  Serial.print(':');
  printTwoDigits(now.sec);
  Serial.print("  Day: ");
  Serial.println(now.day);

  delay(1000);
}
