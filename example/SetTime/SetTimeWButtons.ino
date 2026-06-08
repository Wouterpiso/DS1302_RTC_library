#include <DS1302.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Hardware pin mapping (DS1302 RTC + analog button input)
// ─────────────────────────────────────────────────────────────────────────────

#define DS1302_RST 32
#define DS1302_DAT 21
#define DS1302_CLK 22

#define BUTTON_PIN 34

// ─────────────────────────────────────────────────────────────────────────────
// RTC instance
// ─────────────────────────────────────────────────────────────────────────────

Ds1302 rtc(DS1302_CLK, DS1302_DAT, DS1302_RST);

// ─────────────────────────────────────────────────────────────────────────────
// Button definitions (analog resistor ladder)
// ─────────────────────────────────────────────────────────────────────────────

enum Button {
  NONE,
  SW1_LEFT,
  SW2_OK,
  SW4_RIGHT
};

// ─────────────────────────────────────────────────────────────────────────────
// Field labels (for debug output / UI mapping)
// ─────────────────────────────────────────────────────────────────────────────

const char* fieldNames[] = {
  "YEAR",
  "MONTH",
  "DAY",
  "HOUR",
  "MINUTE",
  "CONFIRM"
};

// ─────────────────────────────────────────────────────────────────────────────
// Edit mode state machine
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Time buffer (local editable copy before writing to RTC)
// ─────────────────────────────────────────────────────────────────────────────

uint8_t year = 26;
uint8_t month = 1;
uint8_t day = 1;
uint8_t hour = 0;
uint8_t minute = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Button debounce / edge detection
// ─────────────────────────────────────────────────────────────────────────────

bool buttonWasDown = false;

// ─────────────────────────────────────────────────────────────────────────────
// Reads raw analog button input and maps it to logical button events
// ─────────────────────────────────────────────────────────────────────────────

Button readButtonRaw() {
  int val = analogRead(BUTTON_PIN);

  Serial.println(val); // debug raw ADC value

  if (val < 150)  return SW1_LEFT;
  if (val < 700)  return SW2_OK;
  if (val < 1400) return NONE;
  if (val < 2100) return SW4_RIGHT;

  return NONE;
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// Initializes RTC and loads current time into local variables
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  rtc.initiate();
  rtc.start();

  // Load time from RTC into local editable state
  Ds1302::dateTime now;
  rtc.getDateTime(&now);

  year   = now.year;
  month  = now.month;
  day    = now.date;
  hour   = now.hour;
  minute = now.min;

  Serial.println("System ready");
}

// ─────────────────────────────────────────────────────────────────────────────
// Writes edited time back to RTC
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Adjusts currently selected field value in edit mode
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Button handling (edge-detected input, prevents repeated triggers)
// ─────────────────────────────────────────────────────────────────────────────

void handleButtons() {
  Button b = readButtonRaw();

  // Reset edge state when no button is pressed
  if (b == NONE) {
    buttonWasDown = false;
    return;
  }

  // Prevent multiple triggers while holding button
  if (buttonWasDown) return;
  buttonWasDown = true;

  // ───────────────────────────────────────────────────────────────────────────
  // SW2: enter edit mode, cycle fields, or confirm save
  // ───────────────────────────────────────────────────────────────────────────

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

  // Ignore adjustments unless in edit mode
  if (!editMode) return;

  // ───────────────────────────────────────────────────────────────────────────
  // Value adjustment (LEFT / RIGHT)
  // ───────────────────────────────────────────────────────────────────────────

  if (b == SW1_LEFT)  adjust(-1);
  if (b == SW4_RIGHT) adjust(+1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main loop
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  handleButtons();

  // ───────────────────────────────────────────────────────────────────────────
  // Normal mode: print live RTC time
  // ───────────────────────────────────────────────────────────────────────────

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

  // ───────────────────────────────────────────────────────────────────────────
  // Edit mode: print currently edited values
  // ───────────────────────────────────────────────────────────────────────────

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