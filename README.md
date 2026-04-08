# Wearable Potentiostat — EIS Sweat Sensor (Berkeley MEng Capstone)

Portable Electrochemical Impedance Spectroscopy (EIS) system for real-time sweat analysis. Built around an **ESP32-S3** MCU and **Analog Devices AD5940/AD5941** analog front-end (AFE). Two electrodes on a watch back-plate contact skin; the system sweeps **1 Hz – 200 kHz** and reports complex impedance over **USB serial**, **BLE**, and **SD card**.

---

## Hardware

| Component | Details |
|-----------|---------|
| **MCU** | ESP32-S3 (240 MHz, Wi-Fi + BLE 5.0) |
| **AFE** | AD5940 / AD5941 — waveform generator, DFT engine, programmable-gain TIA |
| **Electrodes** | 2-electrode (CE0 + SE0), mounted on Apple Watch back-plate for sweat contact |
| **SPI bus** | Shared: AFE (CS GPIO38), SD card (CS GPIO21 or 40), optional display |
| **Storage** | MicroSD card slot (FAT32, SPI) |
| **Wireless** | BLE — web companion, iOS app, or legacy Android |
| **Power** | USB-C or 3.7 V LiPo |
| **RCAL** | On-board calibration resistor, default **1 kΩ** — used internally by the AFE for ratiometric impedance calculation (not the load) |

### Two Board Variants

| | HELPStat V2 (bench/dev) | New_EIS_PCB_Akshay (final wearable) |
|---|---|---|
| AFE CS | GPIO 38 (or 11 on Eagle V2) | GPIO 38 |
| AFE interrupt | Hardware (GPIO 9) | None — SPI DFT polling |
| SD CS | GPIO 21 | GPIO 40 |
| Button | GPIO 7 | GPIO 13 |
| LED | GPIO 6 | None (use display) |
| Display | None | ST7789 240×280 SPI (CS 39, DC 3, RST 21) |
| PSRAM | No | Yes (OPI) |
| PlatformIO env | `lab-board-cs38` | `eis-pcb-akshay` |

KiCad 9 design files for the final PCB are in `boards/New_EIS_PCB_Akshay/`.

---

## Repository Structure

```
src/main.cpp                   Firmware entry point, serial commands, measurement loop
src/eis_board_ui.cpp           SPI display driver for New_EIS_PCB_Akshay
lib/HELPStatLib/               Core library (AD5940 driver, EIS sweep, BLE, SD, LMA fit)
  HELPStat.cpp/h               High-level sweep, BLE transmit, SD save
  ad5940.c/h                    Low-level AD5940 register driver
  ad594x.cpp                    MCU-side SPI, interrupt/poll, resource init
  Impedance.c/h                 Impedance math helpers
  lma.cpp/h                     Levenberg-Marquardt curve fitting (Rct, Rs)
  constants.h                   Pin definitions, board-specific macros
  eis_board_ui.h                Display UI header (C-linkage)
lib/Eigen/                     Eigen math library (used by LMA)
boards/custom_s3.json          PlatformIO board definition
boards/New_EIS_PCB_Akshay/     KiCad project (schematic + 4-layer PCB)
docs/                          Technical docs (features, modes, improvements)
testing/                       Python scripts for serial capture, plotting
web/helpstat-app/              Web BLE companion (Chrome/Edge)
ios/HELPStatCompanion/         Native iOS BLE companion (SwiftUI)
Originallibraries/             Legacy HELPStat reference (Android app, original firmware, Eagle PCB)
platformio.ini                 Build configuration
```

---

## Getting Started

### Prerequisites

- **VS Code + PlatformIO** extension (builds the firmware)
- **Python 3.8+** with `pip install -r requirements.txt` (for testing scripts)
- **USB-C cable** to the board

### Build and Flash

1. Open this folder in VS Code. PlatformIO auto-detects `platformio.ini`.
2. Connect the board via USB-C.
3. Build and upload (from project root):

```bash
# HELPStat V2 / bench board (CS=38):
python -m platformio run -e lab-board-cs38 -t upload --upload-port COM3

# New EIS PCB (Akshay):
python -m platformio run -e eis-pcb-akshay -t upload --upload-port COM3
```

Replace `COM3` with your actual port. Close any serial monitor before uploading.

4. Open a serial monitor at **115200 baud**.

> **Windows:** After the first flash, you may need to unplug and re-plug USB once so the ESP32-S3 exits the ROM bootloader.

---

## Usage

### Serial Commands (115200 baud, newline-terminated)

| Command | Description |
|---------|-------------|
| `HELP` | List all commands |
| `STATUS` | Show wearable timer state and last sweep time |
| `SHOW` | Print current measurement parameters |
| `CHECK` | Read AD5940 chip ID (SPI connectivity test) |
| `MEASURE` | Run sweep with current parameters |
| `MEASURE:SAMPLE` | Default sweep: 200 kHz → 1 Hz, 10 pts/decade, 200 mV |
| `MEASURE:<14 params>` | Custom sweep (see below) |
| `SET:<14 params>` | Set parameters without measuring |
| `WEARABLE:ON` / `OFF` | Enable/disable automatic timed sweeps |
| `INTERVAL:N` | Auto-sweep interval in seconds (min 10, default 300) |
| `RCAL:ohms` | Set RCAL value and save to NVS (e.g. `RCAL:987.6`) |
| `RCAL:SHOW` | Print stored and runtime RCAL values |
| `DISPLAYRANGE:min,max,val[,title]` | Update SPI display (Akshay PCB only) |

### 14-Parameter Format

```
MEASURE:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude
```

| # | Parameter | Example | Notes |
|---|-----------|---------|-------|
| 1 | mode | 0 | 0=EIS, 1=CV, 2=IT, 3=CP, 4=DPV, 5=SWV, 6=OCP |
| 2 | startFreq | 200000 | Hz (high end) |
| 3 | endFreq | 1 | Hz (low end) |
| 4 | numPoints | 10 | Points per decade |
| 5 | biasVolt | 0.0 | DC bias (V) |
| 6 | zeroVolt | 0.0 | Zero voltage (V) |
| 7 | rcalVal | 1000 | Calibration resistor (Ω) — match your board's RCAL |
| 8 | extGain | 1 | Excitation buffer gain (1=×2) |
| 9 | dacGain | 1 | DAC gain (1=×1) |
| 10 | rct_est | 127000 | Estimated Rct (Ω) — seed for LMA fit |
| 11 | rs_est | 150 | Estimated Rs (Ω) — seed for LMA fit |
| 12 | numCycles | 0 | Repeat cycles |
| 13 | delaySecs | 0 | Delay between cycles (s) |
| 14 | amplitude | 200 | AC excitation (mV, 0–800) |

### Quick USB Test (Python)

```bash
# Default 2-point sweep (10 kHz → 1 kHz):
python testing/scenario1_serial_sample.py COM3

# Single frequency at 10 kHz:
python testing/scenario1_serial_sample.py COM3 10000
```

### Three Usage Scenarios

1. **Laptop + USB (development)** — Flash, open serial, send `MEASURE:SAMPLE`, read CSV from terminal or Python script.
2. **Standalone (button + SD)** — Press button on GPIO 7 (or 13 on Akshay PCB). Results save to SD card at `/eis/sweep_1Hz_200kHz.csv`.
3. **Wearable (BLE, battery)** — Send `WEARABLE:ON` once. Board auto-sweeps at the configured interval. Data available via BLE (web/iOS companion) and SD card.

---

## How EIS Works on This Board

1. AD5940 waveform generator outputs a sine wave at the target frequency through **CE0** (counter electrode).
2. Current flows through the **load** (sweat between the two electrodes) and returns through **SE0** (sense electrode).
3. The **HSTIA** (transimpedance amplifier) converts the current to voltage. Gain resistor is auto-selected per frequency band.
4. The on-chip **DFT engine** extracts real and imaginary components.
5. Impedance is computed ratiometrically against **RCAL** (the known calibration resistor on the board).
6. The firmware sweeps all frequencies, outputting CSV with columns: `Frequency(Hz), Real(Ohm), Imaginary(Ohm), Magnitude(Ohm), Phase(Degrees)`.
7. A Levenberg-Marquardt fit estimates **Rct** (charge transfer resistance) and **Rs** (solution resistance).

### RCAL vs Load — What Each Does

- **RCAL** (on-board, 1 kΩ default): An internal **reference** resistor. The AFE measures it at each frequency to calibrate its gain path. It is NOT the thing being measured. You never connect wires to RCAL — it's part of the chip's internal measurement circuit.
- **Load** (between CE0 and SE0): The **unknown impedance** you are measuring. For bench testing this is a known resistor (e.g. 1 kΩ). For the wearable, this is **sweat on skin** between the two watch electrodes.

If there is **no conductive path** between CE0 and SE0 (open circuit, dry skin, electrodes not touching), the DFT engine gets no valid signal and times out — producing `NaN` results.

---

## Errors, Fixes, and Known Issues

### DFT Poll Timeout → NaN Results

**Symptom:** `ERROR: DFT poll timeout (~30 s)` and all CSV values are `nan`.

**Causes and fixes:**

| Cause | Fix |
|-------|-----|
| **No load between CE0 and SE0** | For bench testing, place a **1 kΩ resistor** across CE0–SE0. For sweat sensing, ensure electrodes are in contact with moist skin. |
| **Dry skin / no sweat** | The watch electrodes need sweat (a conductive medium) to close the circuit. Run after exercise or apply a small drop of saline. |
| **Wrong CS pin** | Verify `CS` in `constants.h` matches your PCB (38 for this project, 11 for HELPStat V2 Eagle). |
| **Missing interrupt wire** (HELPStat V2 only) | AFE GPIO0 must be wired to MCU GPIO9. On the Akshay PCB, this isn't needed (uses SPI polling). |

### COM Port Issues (Windows)

| Symptom | Fix |
|---------|-----|
| `PermissionError` / port busy | Another program holds the port (VS Code serial monitor, stuck Python script). Kill it first. |
| Port disappears after flash | Unplug and re-plug USB once. Normal for ESP32-S3 USB-Serial/JTAG on first flash. |
| Upload fails | Close serial monitor before uploading. |

### Build Errors

| Error | Fix |
|-------|-----|
| `pio not recognized` | Use `python -m platformio run ...` instead. |
| `#define CS` clash with Adafruit | Don't `#include "constants.h"` before Adafruit headers in display code. Already handled in `eis_board_ui.cpp`. |
| `-DMOSI` / `-DMISO` / `-DSCK` | Do NOT pass these as build flags on Arduino-ESP32 3.x — they clash with `pins_arduino.h`. Edit `constants.h` instead. |

### EIS Data Quality

| Issue | Explanation |
|-------|-------------|
| `inf` magnitude | Open circuit or loose electrode connection. |
| Jumps at certain frequencies | Gain switching boundary — 10% hysteresis is applied but extreme impedance mismatches can still cause artifacts. |
| Fit Rct/Rs stuck at defaults | Poor data quality. Fix wiring and RCAL value first, then tune `rct_est`/`rs_est` seeds. |
| Slow sweep at low frequencies | Normal. Settling delay is 4 excitation periods; at 1 Hz that's 4+ seconds per measurement point. |

---

## Firmware Features

- **Data validation**: Bad DFT measurements (NaN, inf, out-of-range) are detected and retried (up to 4×) or recorded as NaN.
- **DFT poll timeout**: 30-second timeout prevents infinite hangs on open circuit or missing interrupt.
- **Gain hysteresis**: 10% frequency margin on RTIA switching prevents oscillation at band boundaries.
- **Averaging**: Each frequency point is measured 3× (configurable) with only validated samples averaged.
- **SPI DFT polling**: For PCBs without a hardware interrupt line (Akshay), the firmware polls the AFE's DFTRDY flag over SPI.
- **RCAL in NVS**: `RCAL:` serial command stores your measured calibration resistor value in flash (survives reboot).
- **Wearable auto-sweep**: Configurable timer-based automatic sweeps for standalone field deployment.
- **Display UI** (Akshay PCB): Live sweep progress bar and numeric range display on ST7789 SPI screen.
- **Temperature logging**: Reads AD5940 die temperature (approximate, not applied as correction).

See `docs/NEW_FEATURES.md` for full details.

---

## Companion Apps

| Platform | Location | Notes |
|----------|----------|-------|
| **Web** (Chrome/Edge) | `web/helpstat-app/` | Web Bluetooth, runs from `localhost:8080`. No iPhone support (no Web Bluetooth in Safari). |
| **iOS** (native) | `ios/HELPStatCompanion/` | SwiftUI + CoreBluetooth. Open `.xcodeproj` in Xcode 15+. |
| **Android** (legacy) | `Originallibraries/` | Kotlin app, not actively maintained. |

---

## Pin Configuration

### HELPStat V2 / Bench Board

| Signal | GPIO | Notes |
|--------|------|-------|
| MOSI | 35 | SPI data out |
| MISO | 37 | SPI data in |
| SCK | 36 | SPI clock |
| CS (AFE) | 38 | AD5940 chip select (11 on V2 Eagle) |
| RESET | 10 | AD5940 reset |
| INTERRUPT | 9 | AD5940 GPIO0 → MCU |
| CS_SD | 21 | SD card chip select |
| BUTTON | 7 | Push-button (active LOW) |
| LED | 6 | Status LED |

### New_EIS_PCB_Akshay (Final Wearable)

| Signal | GPIO | Notes |
|--------|------|-------|
| MOSI | 35 | Shared SPI |
| MISO | 37 | Shared SPI |
| SCK | 36 | Shared SPI |
| CS (AFE) | 38 | AD5940 chip select |
| RESET | 10 | AD5940 reset |
| CS_SD | 40 | SD card chip select |
| BUTTON | 13 | SW1 push-button |
| TFT CS | 39 | Display chip select |
| TFT DC | 3 | Display data/command |
| TFT RST | 21 | Display reset |

No hardware interrupt line from AFE; firmware uses `AFE_USE_HARDWARE_IRQ_GPIO=0` (SPI polling).

---

## License

MIT License — Copyright (c) 2024 Linnes Lab, Purdue University

See LICENSE file for full text.
