#include <DS1302.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// DS1302 RTC configuration
// ─────────────────────────────────────────────────────────────────────────────
#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

// ─────────────────────────────────────────────────────────────────────────────
// Alarm state
// ─────────────────────────────────────────────────────────────────────────────
bool alarmRinging = false;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
void printTwoDigits(uint8_t value) {
  if (value < 10) Serial.print('0');
  Serial.print(value);
}

void printTime(Ds1302::dateTime& dt) {
  Serial.print("20");
  printTwoDigits(dt.year);
  Serial.print('-');
  printTwoDigits(dt.month);
  Serial.print('-');
  printTwoDigits(dt.date);
  Serial.print(' ');
  printTwoDigits(dt.hour);
  Serial.print(':');
  printTwoDigits(dt.min);
  Serial.print(':');
  printTwoDigits(dt.sec);
  Serial.println();
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  rtc.initiate();
  delay(50);
  rtc.halt();
  delay(50);

  // Set an initial date/time
  Ds1302::dateTime setTime;
  setTime.year  = 26;
  setTime.month = 6;
  setTime.date  = 7;
  setTime.hour  = 12;
  setTime.min   = 0;
  setTime.sec   = 0;
  setTime.day   = Ds1302::SUNDAY;

  rtc.setDateTime(&setTime);
  delay(50);
  rtc.start();
  delay(100);

  // Schedule an alarm 10 seconds in the future
  uint8_t alarmSec = (setTime.sec + 10) % 60;
  uint8_t alarmMin = setTime.min + ((setTime.sec + 10) >= 60 ? 1 : 0);
  rtc.setAlarm(setTime.hour, alarmMin, alarmSec);

  Serial.println("Clock started. Alarm in 10 seconds.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {

  // Alarm active
  if (alarmRinging) {
    Serial.println("*** ALARM! *** Press any key to stop.");

    if (Serial.available() > 0) {
      Serial.read();
      alarmRinging = false;
      rtc.clearAlarm();
      Serial.println("Alarm stopped.\n");
    }

    delay(1000);
    return;
  }

  // Alarm triggered
  if (rtc.checkAlarm()) {
    alarmRinging = true;
    return;
  }

  // Normal clock display
  Ds1302::dateTime now;
  rtc.getDateTime(&now);
  printTime(now);

  delay(1000);
}