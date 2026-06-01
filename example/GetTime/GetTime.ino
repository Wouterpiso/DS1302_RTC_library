#include <DS1302.hpp>

#define DS1302_RST 33
#define DS1302_DAT 27
#define DS1302_CLK 14

Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

void printTwoDigits(uint8_t value) {
  if (value < 10) {
    Serial.print('0');
  }
  Serial.print(value);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  rtc.initiate();
  delay(50);
  
  // Halt RTC before setting time
  rtc.halt();
  delay(50);

  // Set time to compile time (year-month-date hour:min:sec)
  // Update these manually or the RTC will show compile time
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

  Serial.println("DS1302 time reader started");
}

void loop() {
  Ds1302::dateTime now;
  rtc.getDateTime(&now);

  // Debug: print raw seconds register and halted flag
  uint8_t rawSec = rtc.readRegister(0x80); // seconds register base
  Serial.print("Raw sec: 0x"); Serial.println(rawSec, HEX);
  Serial.print("Halted: "); Serial.println(rtc.ishalted());

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
