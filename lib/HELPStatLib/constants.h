#ifndef CONSTANTS_H
#define CONSTANTS_H

/*
 * New_EIS_PCB_Akshay (KiCad, boards/New_EIS_PCB_Akshay): build with -DBOARD_EIS_PCB_AKSHAY=1
 * (see platformio env:eis-pcb-akshay). Uses GPIO38 AFE CS, no AFE GPIO0->MCU line — firmware
 * polls DFT-ready via SPI (AFE_USE_HARDWARE_IRQ_GPIO=0). SD card CS is on IO40 on that PCB.
 */
#if defined(BOARD_EIS_PCB_AKSHAY)
#ifndef AFE_USE_HARDWARE_IRQ_GPIO
#define AFE_USE_HARDWARE_IRQ_GPIO 0
#endif
#ifndef CS_SD
#define CS_SD 40
#endif
#ifndef EIS_UI_TFT_CS
#define EIS_UI_TFT_CS 39
#endif
#ifndef EIS_UI_TFT_DC
#define EIS_UI_TFT_DC 3
#endif
#ifndef EIS_UI_TFT_RST
#define EIS_UI_TFT_RST 21
#endif
#ifndef EIS_UI_DISPLAY_WIDTH
#define EIS_UI_DISPLAY_WIDTH 240
#endif
#ifndef EIS_UI_DISPLAY_HEIGHT
#define EIS_UI_DISPLAY_HEIGHT 280
#endif
#endif

#ifndef AFE_USE_HARDWARE_IRQ_GPIO
#define AFE_USE_HARDWARE_IRQ_GPIO 1
#endif

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
/* AFE SPI chip select: this repo’s active bench target uses GPIO38. HELPStat_V2 Eagle uses GPIO11 — use -DCS=11 or edit here if your PCB matches the schematic. */
#ifndef CS
#define CS 38
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