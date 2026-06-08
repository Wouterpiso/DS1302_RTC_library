#pragma once

#include <stdint.h>

/**
 * @class DS1302
 * @brief Main class for DS1302 RTC communication.
 *
 * Handles reading and writing time from the RTC chip.
 */

class Ds1302 {
public:

    // ─────────────────────────────────────────────────────────────────────────
    // Date/Time structure (RTC format)
    // ─────────────────────────────────────────────────────────────────────────
    struct dateTime {
        uint8_t sec;
        uint8_t min;
        uint8_t hour;
        uint8_t date;
        uint8_t month;
        uint8_t day;
        uint8_t year;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Alarm structure (software-based alarm tracking)
    // NOTE: DS1302 has no true alarm hardware; this is handled in software.
    // ─────────────────────────────────────────────────────────────────────────

    struct AlarmTime {
        uint8_t sec;
        uint8_t min;
        uint8_t hour;
        bool    active;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Month enumeration (1–12)
    // ─────────────────────────────────────────────────────────────────────────
    enum month : uint8_t {
        JANUARY = 1,
        FEBRUARY,
        MARCH,
        APRIL,
        MAY,
        JUNE,
        JULY,
        AUGUST,
        SEPTEMBER,
        OCTOBER,
        NOVEMBER,
        DECEMBER
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Day-of-week enumeration (1–7)
    // ─────────────────────────────────────────────────────────────────────────────
    enum day : uint8_t {
        SUNDAY = 1,
        MONDAY,
        TUESDAY,
        WEDNESDAY,
        THURSDAY,
        FRIDAY,
        SATURDAY
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Constructor
    // ─────────────────────────────────────────────────────────────────────────
    Ds1302(uint8_t clockPin, uint8_t dataPin, uint8_t resetPin);

    // ─────────────────────────────────────────────────────────────────────────
    // RTC control
    // ─────────────────────────────────────────────────────────────────────────

    // Initialize GPIO pins (must be called before use)
    void initiate();

    // Start RTC oscillator (clears halt state + enables timekeeping)
    void start();

    // Stop RTC oscillator (halts timekeeping)
    void halt();

    // Returns true if oscillator is currently halted
    bool isHalted();

    // Disable write protection to allow register updates
    void disableWriteProtect();

    // ─────────────────────────────────────────────────────────────────────────
    // Time access
    // ─────────────────────────────────────────────────────────────────────────

    // Reads full date/time using burst mode
    void getDateTime(dateTime* dateTime);

    // Writes full date/time using burst mode
    void setDateTime(dateTime* dateTime);

    // ─────────────────────────────────────────────────────────────────────────
    // Alarm system (software-based)
    // ─────────────────────────────────────────────────────────────────────────

    // Set alarm time (hour/min/sec)
    void setAlarm(uint8_t hour, uint8_t min, uint8_t sec);

    // Disable alarm
    void clearAlarm();

    // Check if alarm condition is currently triggered
    bool checkAlarm();

    // Returns whether alarm is enabled
    bool isAlarmActive();

    // ─────────────────────────────────────────────────────────────────────────
    // Low-level register access
    // ─────────────────────────────────────────────────────────────────────────
    uint8_t readRegister(uint8_t reg);

private:

    // ─────────────────────────────────────────────────────────────────────────
    // Hardware pins (3-wire interface)
    // ─────────────────────────────────────────────────────────────────────────
    uint8_t _clockPin;
    uint8_t _dataPin;
    uint8_t _resetPin;

    // ─────────────────────────────────────────────────────────────────────────
    // Alarm state
    // ─────────────────────────────────────────────────────────────────────────
    AlarmTime _alarm;
    bool      _alarmTriggered;

    // ─────────────────────────────────────────────────────────────────────────
    // Low-level communication helpers
    // ─────────────────────────────────────────────────────────────────────────

    void prepRead(uint8_t address);
    void prepWrite(uint8_t address);
    void end();

    int  dataDirection();
    void setDataDirection(int direction);

    uint8_t readByte();
    void writeByte(uint8_t val);
    void nextBit();

    // ─────────────────────────────────────────────────────────────────────────
    // Data format conversion (BCD ↔ decimal)
    // ─────────────────────────────────────────────────────────────────────────
    uint8_t bcdToDec(uint8_t dec);
    uint8_t decToBcd(uint8_t bcd);

    // ─────────────────────────────────────────────────────────────────────────
    // Internal control (oscillator / halt bit management)
    // ─────────────────────────────────────────────────────────────────────────
    void setHaltFlag(bool halt);
};