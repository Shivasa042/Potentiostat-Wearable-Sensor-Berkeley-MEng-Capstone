#ifndef CONSTANTS_H
#define CONSTANTS_H

#define CLCK 240000000 / 16
#define BITS MSBFIRST
#define SPIMODE SPI_MODE0

/* ESP32 Feather */
// SPI / RESET / INT PINS
// By default the ESP32 Feather pinout (uncomment if used)
// #ifndef MOSI
// #define MOSI 18
// #endif
// #ifndef MISO
// #define MISO 19
// #endif
// #ifndef SCK
// #define SCK 5
// #endif
// #ifndef CS
// #define CS 33
// #endif
// #ifndef RESET
// #define RESET 21
// #endif
// #ifndef ESP32_INTERRUPT
// #define ESP32_INTERRUPT 14 // INTERRUPT PIN FOR ESP32
// #endif

// // ESP32 S3 SPI / RESET / INT PINS (default configuration used by project)
#ifndef MOSI
#define MOSI 35
#endif
#ifndef MISO
#define MISO 37
#endif
#ifndef SCK
#define SCK 36
#endif
#ifndef CS
#define CS 11
#endif
#ifndef RESET
#define RESET 10
#endif
#ifndef ESP32_INTERRUPT
#define ESP32_INTERRUPT 9
#endif

// SD CARD PINS
#ifndef CS_SD
#define CS_SD 21
#endif
// Need to make sure CLK doesn't exceed SD card maximum
// Using the same clock is fine for this case
#define SD_CLK 240000000 / 16
#define FILENAME "EISdata.csv"

/* USING SCL/SDA PINS AS LED DRIVERS FOR OUTPUT */
#define LED1 6
#define LED2 7


#define SERIAL_BAUD 115200 

#endif // CONSTANTS_H