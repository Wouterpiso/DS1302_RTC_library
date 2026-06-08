#include "DS1302.hpp"
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// DS1302 Register Map + Bit Definitions
// ─────────────────────────────────────────────────────────────────────────────
// These constants define DS1302 memory/register addresses and control bits.
// The chip uses a bit-addressed protocol with LSB-first data transfer.

#define REG_SECONDS 0x80
#define REG_MINUTES 0x82
#define REG_HOURS   0x84
#define REG_DATE    0x86
#define REG_MONTH   0x88
#define REG_DAY     0x8A
#define REG_YEAR    0x8C
#define REG_WP      0x8E
#define REG_BURST   0xBE

#define READ_FLAG   0x01

#define REG_SECONDS_READ  (REG_SECONDS | READ_FLAG)
#define REG_BURST_READ    (REG_BURST | READ_FLAG)

#define CH_BIT      0x80   // Clock Halt bit (seconds register)
#define HOUR_MASK   0x3F   // 24h mode mask (not always used)

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
// Stores GPIO pin mapping for 3-wire DS1302 interface.

Ds1302::Ds1302(uint8_t clockPin, uint8_t dataPin, uint8_t resetPin) {
    _clockPin = clockPin;
    _dataPin  = dataPin;
    _resetPin = resetPin;
}

// ─────────────────────────────────────────────────────────────────────────────
// Data format conversion (BCD ↔ Decimal)
// ─────────────────────────────────────────────────────────────────────────────
// DS1302 stores time in Binary-Coded Decimal (BCD), not raw integers.

uint8_t Ds1302::bcdToDec(uint8_t bcd) {
    return (uint8_t)(((bcd >> 4) * 10) + (bcd & 0x0F));
}

uint8_t Ds1302::decToBcd(uint8_t dec) {
    return (uint8_t)(((dec / 10) << 4) | (dec % 10));
}

// ─────────────────────────────────────────────────────────────────────────────
// GPIO helpers (data line direction control)
// ─────────────────────────────────────────────────────────────────────────────

int Ds1302::dataDitection() {
    return digitalRead(_dataPin);
}

void Ds1302::setDataDirection(int direction) {
    pinMode(_dataPin, direction);
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialization
// ─────────────────────────────────────────────────────────────────────────────
// Configures GPIO pins and sets safe idle states.

void Ds1302::initiate() {
    pinMode(_clockPin, OUTPUT);
    pinMode(_resetPin, OUTPUT);
    pinMode(_dataPin, INPUT);

    digitalWrite(_clockPin, LOW);
    digitalWrite(_resetPin, LOW);
}

// ─────────────────────────────────────────────────────────────────────────────
// RTC control (start / halt oscillator)
// ─────────────────────────────────────────────────────────────────────────────

void Ds1302::start() {
    disableWriteProtect();
    setHaltFlag(false);
}

void Ds1302::halt() {
    setHaltFlag(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Halt flag handling (read-modify-write burst transaction)
// ─────────────────────────────────────────────────────────────────────────────
// Reads all registers, modifies CH bit, then writes them back.

void Ds1302::setHaltFlag(bool halt) {
    prepRead(REG_BURST_READ);

    uint8_t regs[8];
    for (int i = 0; i < 8; i++) {
        regs[i] = readByte();
    }
    end();

    // Modify CH (clock halt) bit in seconds register
    if (halt) regs[0] |= 0x80;
    else      regs[0] &= 0x7F;

    // Control register (write protect)
    regs[7] = 0x80;

    disableWriteProtect();
    delay(5);

    prepWrite(REG_BURST);
    for (int i = 0; i < 8; i++) {
        writeByte(regs[i]);
    }
    end();
}

// ─────────────────────────────────────────────────────────────────────────────
// Status check
// ─────────────────────────────────────────────────────────────────────────────
// Returns true if oscillator is stopped (CH bit set).

bool Ds1302::ishalted() {
    prepRead(REG_SECONDS_READ);
    uint8_t sec = readByte();
    end();

    return (sec & CH_BIT) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Write protection control
// ─────────────────────────────────────────────────────────────────────────────

void Ds1302::disableWriteProtect() {
    prepWrite(REG_WP);
    writeByte(0x00);
    end();
}

// ─────────────────────────────────────────────────────────────────────────────
// Low-level bus cleanup
// ─────────────────────────────────────────────────────────────────────────────

void Ds1302::end() {
    setDataDirection(INPUT);
    digitalWrite(_resetPin, LOW);
    digitalWrite(_clockPin, LOW);
}

// ─────────────────────────────────────────────────────────────────────────────
// Transaction setup (READ)
// ─────────────────────────────────────────────────────────────────────────────

void Ds1302::prepRead(uint8_t address) {
    digitalWrite(_resetPin, HIGH);
    delayMicroseconds(10);

    setDataDirection(OUTPUT);
    writeByte(address);

    delayMicroseconds(10);
    setDataDirection(INPUT);
    delayMicroseconds(10);
}

// ─────────────────────────────────────────────────────────────────────────────
// Transaction setup (WRITE)
// ─────────────────────────────────────────────────────────────────────────────

void Ds1302::prepWrite(uint8_t address) {
    digitalWrite(_resetPin, HIGH);
    delayMicroseconds(10);

    setDataDirection(OUTPUT);
    writeByte(address);

    delayMicroseconds(10);
}

// ─────────────────────────────────────────────────────────────────────────────
// Clock pulse helper (unused in current flow but kept for completeness)
// ─────────────────────────────────────────────────────────────────────────────

void Ds1302::nextBit() {
    digitalWrite(_clockPin, HIGH);
    delayMicroseconds(1);
    digitalWrite(_clockPin, LOW);
    delayMicroseconds(1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Low-level byte transfer (LSB-first protocol)
// ─────────────────────────────────────────────────────────────────────────────

uint8_t Ds1302::readByte() {
    uint8_t val = 0;

    for (uint8_t i = 0; i < 8; i++) {
        if (digitalRead(_dataPin)) val |= (1 << i);

        digitalWrite(_clockPin, HIGH);
        delayMicroseconds(5);
        digitalWrite(_clockPin, LOW);
        delayMicroseconds(5);
    }

    return val;
}

void Ds1302::writeByte(uint8_t val) {
    for (uint8_t i = 0; i < 8; i++) {
        digitalWrite(_dataPin, (val >> i) & 0x01);

        delayMicroseconds(5);
        digitalWrite(_clockPin, HIGH);
        delayMicroseconds(5);
        digitalWrite(_clockPin, LOW);
        delayMicroseconds(5);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// High-level RTC read (burst mode)
// ─────────────────────────────────────────────────────────────────────────────
// Reads full date/time block in one transaction.

void Ds1302::getDateTime(dateTime* dt) {
    prepRead(REG_BURST_READ);

    uint8_t sec   = readByte();
    uint8_t min   = readByte();
    uint8_t hour  = readByte();
    uint8_t date  = readByte();
    uint8_t month = readByte();
    uint8_t day   = readByte();
    uint8_t year  = readByte();
    uint8_t ctrl  = readByte();

    end();

    dt->sec   = bcdToDec(sec & 0x7F);
    dt->min   = bcdToDec(min & 0x7F);
    dt->hour  = bcdToDec(hour & 0x3F);
    dt->date  = bcdToDec(date & 0x3F);
    dt->month = bcdToDec(month & 0x1F);
    dt->day   = bcdToDec(day & 0x07);
    dt->year  = bcdToDec(year & 0xFF);
}

// ─────────────────────────────────────────────────────────────────────────────
// High-level RTC write (burst mode)
// ─────────────────────────────────────────────────────────────────────────────

void Ds1302::setDateTime(dateTime* dt) {
    disableWriteProtect();

    prepWrite(REG_BURST);

    writeByte(decToBcd(dt->sec));
    writeByte(decToBcd(dt->min));
    writeByte(decToBcd(dt->hour));
    writeByte(decToBcd(dt->date));
    writeByte(decToBcd(dt->month));
    writeByte(decToBcd(dt->day));
    writeByte(decToBcd(dt->year));

    writeByte(0x80); // control register (write protect bit)

    end();
}

// ─────────────────────────────────────────────────────────────────────────────
// Raw register access
// ─────────────────────────────────────────────────────────────────────────────

uint8_t Ds1302::readRegister(uint8_t reg) {
    prepRead(reg | READ_FLAG);
    uint8_t val = readByte();
    end();
    return val;
}

// ─────────────────────────────────────────────────────────────────────────────
// Alarm system (software-based, not hardware RTC feature)
// ─────────────────────────────────────────────────────────────────────────────

void Ds1302::setAlarm(uint8_t hour, uint8_t min, uint8_t sec) {
    _alarm.hour   = hour;
    _alarm.min    = min;
    _alarm.sec    = sec;
    _alarm.active = true;

    _alarmTriggered = false;
}

void Ds1302::clearAlarm() {
    _alarm.active = false;
    _alarmTriggered = false;
}

bool Ds1302::checkAlarm() {
    if (!_alarm.active) return false;

    dateTime now;
    getDateTime(&now);

    bool match =
        (now.hour == _alarm.hour &&
         now.min  == _alarm.min  &&
         now.sec  == _alarm.sec);

    if (match && !_alarmTriggered) {
        _alarmTriggered = true;
        return true;
    }

    if (!match) {
        _alarmTriggered = false;
    }

    return false;
}

bool Ds1302::isAlarmActive() {
    return _alarm.active;
}