#ifndef DS_1302
#define DS_1302

#include <stdint.h>

class Ds1302{
    public:
        typedef struct {
            uint8_t sec;
            uint8_t min;
            uint8_t hour;
            uint8_t date;
            uint8_t month;
            uint8_t day;
            uint8_t year;
        } dateTime;

        enum month : uint8_t {
            JANUARY = 1,
            FEBRUARY = 2,
            MARCH = 3,
            APRIL = 4,
            MAY = 5,
            JUNE = 6,
            JULY = 7,
            AUGUST = 8,
            SEPTEMBER = 9,
            OCTOBER = 10,
            NOVEMBER = 11,
            DECEMBER = 12
        };

        enum day : uint8_t {
            SUNDAY = 1,
            MONDAY = 2,
            TUESDAY = 3,
            WEDNESDAY = 4,
            THURSDAY = 5,
            FRIDAY = 6,
            SATURDAY = 7
        };

        Ds1302(uint8_t clockPin, uint8_t dataPin, uint8_t resetPin);
        
        void start();

        void halt();

        void initiate();

        bool ishalted();

        void disableWriteProtect();

        void getDateTime(dateTime* dateTime);

        void setDateTime(dateTime* dateTime);

        void setAlarm(dateTime* dateTime);

        uint8_t readRegister(uint8_t reg);

    private:
        uint8_t _clockPin;
        uint8_t _dataPin;
        uint8_t _resetPin;

        void prepRead(uint8_t address);
        void prepWrite(uint8_t address);
        void end();

        int dataDitection();
        void setDataDirection(int direction);

        uint8_t readByte();
        void writeByte(uint8_t val);
        void nextBit();

        uint8_t bcdToDec(uint8_t dec);
        uint8_t decToBcd(uint8_t bcd);

        void setHaltFlag(bool halt);

};

#endif