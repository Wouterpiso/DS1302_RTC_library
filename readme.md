# DS1302 RTC Library

Arduino C++ library for the **DS1302 Real Time Clock (RTC)** module.

This library provides a simple interface to read and write date/time
from the DS1302 chip using a 3-wire communication protocol.

---

## ✨ Features

- Read and write date & time
- Simple 3-wire interface (CE, I/O, SCLK)
- Compatible with Arduino, ESP8266, ESP32
- Low-level register access (if exposed in implementation)
- Optional RAM read/write support

---

## 🔌 Wiring (DS1302)

| DS1302 Pin | Arduino |
|------------|---------|
| VCC        | 5V      |
| GND        | GND     |
| CE         | Dx      |
| I/O        | Dx      |
| SCLK       | Dx      |

---

## 🚀 Installation

### Arduino IDE
1. Download the library (The DS1302 file).
2. Place in your library folder.
3. The library is now ready for use.