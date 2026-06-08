#include <DS1302.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// DS1302 configuration
// ─────────────────────────────────────────────────────────────────────────────
#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────
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

  rtc.initiate();
  delay(50);

  rtc.halt();
  delay(50);

  Ds1302::dateTime dt;

  // ===== SET YOUR TIME HERE =====
  dt.sec   = 0;
  dt.min   = 30;
  dt.hour  = 14;

  dt.date  = 5;
  dt.month = 6;
  dt.year  = 26;  // 2026

  dt.day   = Ds1302::FRIDAY;
  // ==============================

  rtc.setDateTime(&dt);
  delay(50);

  rtc.start();
  delay(100);

  Serial.println("RTC initialized");
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