#include <DS1302.hpp>

// ================= PINS =================
#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

#define BUTTON_PIN 34

// ================= RTC =================
Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

// ================= BUTTON ENUM =================
enum Button {
  NONE,
  SW1_LEFT,
  SW2_OK,
  SW4_RIGHT
};

// ================= FIELD NAMES =================
const char* fieldNames[] = {
  "YEAR",
  "MONTH",
  "DAY",
  "HOUR",
  "MINUTE",
  "CONFIRM"
};

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

// ================= TIME STORAGE =================
uint8_t year = 26;
uint8_t month = 1;
uint8_t day = 1;
uint8_t hour = 0;
uint8_t minute = 0;

// ================= SIMPLE BUTTON STATE (FIXED) =================
bool buttonWasDown = false;

// ================= READ BUTTON =================
Button readButtonRaw() {
  int val = analogRead(BUTTON_PIN);

  Serial.println(val); // debug

  if (val < 150)  return SW1_LEFT;
  if (val < 700)  return SW2_OK;
  if (val < 1400) return NONE;
  if (val < 2100) return SW4_RIGHT;

  return NONE;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  rtc.initiate();
  rtc.start();

  // Load time from RTC
  Ds1302::dateTime now;
  rtc.getDateTime(&now);

  year   = now.year;
  month  = now.month;
  day    = now.date;
  hour   = now.hour;
  minute = now.min;

  Serial.println("System ready");
}

// ================= APPLY TIME =================
void applyTime() {
  Ds1302::dateTime dt;

  dt.year  = year;
  dt.month = month;
  dt.date  = day;
  dt.hour  = hour;
  dt.min   = minute;
  dt.sec   = 0;
  dt.day   = 1;

  rtc.setDateTime(&dt);
  rtc.start();
}

// ================= ADJUST VALUE =================
void adjust(int dir) {
  switch (field) {

    case YEAR:
      year = constrain(year + dir, 0, 99);
      break;

    case MONTH:
      month = constrain(month + dir, 1, 12);
      break;

    case DAY:
      day = constrain(day + dir, 1, 31);
      break;

    case HOUR:
      hour = constrain(hour + dir, 0, 23);
      break;

    case MINUTE:
      minute = constrain(minute + dir, 0, 59);
      break;

    default:
      break;
  }
}

// ================= BUTTON HANDLER (FIXED PROPER EDGE DETECTION) =================
void handleButtons() {
  Button b = readButtonRaw();

  // ================= RELEASE DETECTION =================
  if (b == NONE) {
    buttonWasDown = false;
    return;
  }

  // ================= ONLY ONE TRIGGER PER PRESS =================
  if (buttonWasDown) return;
  buttonWasDown = true;

  // ================= SW2 (NAVIGATION + CONFIRM) =================
  if (b == SW2_OK) {

    if (!editMode) {
      editMode = true;
      field = YEAR;
      Serial.println("EDIT MODE ON");
      return;
    }

    if (field == CONFIRM) {
      applyTime();
      editMode = false;
      Serial.println("TIME SAVED");
      return;
    }

    field = (Field)(field + 1);
    return;
  }

  if (!editMode) return;

  // ================= VALUE CHANGE (NOW REPEATABLE CORRECTLY) =================
  if (b == SW1_LEFT) adjust(-1);
  if (b == SW4_RIGHT) adjust(+1);
}

// ================= LOOP =================
void loop() {
  handleButtons();

  if (!editMode) {
    Ds1302::dateTime now;
    rtc.getDateTime(&now);

    Serial.print("20");
    Serial.print(now.year);
    Serial.print("-");
    Serial.print(now.month);
    Serial.print("-");
    Serial.print(now.date);
    Serial.print(" ");
    Serial.print(now.hour);
    Serial.print(":");
    Serial.print(now.min);
    Serial.print(":");
    Serial.println(now.sec);
  } 
  else {
    Serial.print("EDITING: ");
    Serial.print(fieldNames[field]);
    Serial.print(" | ");

    Serial.print(year); Serial.print("-");
    Serial.print(month); Serial.print("-");
    Serial.print(day); Serial.print(" ");
    Serial.print(hour); Serial.print(":");
    Serial.println(minute);
  }

  delay(100);
}