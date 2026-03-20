# HELPStat — Wearable Potentiostat / EIS Sensor

A portable Electrochemical Impedance Spectroscopy (EIS) system built around an **ESP32-S3** microcontroller and the **Analog Devices AD5940** analog front-end. Designed for wearable biosensing applications such as real-time sweat analysis, the board performs frequency-swept impedance measurements and delivers results over **USB serial**, **Bluetooth Low Energy (BLE)**, and **SD card** storage.

## Table of Contents

- [What This Project Does](#what-this-project-does)
- [Hardware Overview](#hardware-overview)
- [Repository Structure](#repository-structure)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Clone the Repository](#clone-the-repository)
  - [Build and Flash the Firmware](#build-and-flash-the-firmware)
- [Usage Scenarios](#usage-scenarios)
  - [Scenario 1 — Laptop + USB Serial (Development)](#scenario-1--laptop--usb-serial-development)
  - [Scenario 2 — Standalone with Button + SD Card](#scenario-2--standalone-with-button--sd-card)
  - [Scenario 3 — Battery-Powered Wearable with BLE](#scenario-3--battery-powered-wearable-with-ble)
  - [Scenario 4 — Automated Data Collection (Python)](#scenario-4--automated-data-collection-python)
- [Serial Command Reference](#serial-command-reference)
- [Python Scripts](#python-scripts)
- [Pin Configuration](#pin-configuration)
- [Custom PCB Support](#custom-pcb-support)
- [How EIS Works on This Board](#how-eis-works-on-this-board)
- [Further Documentation](#further-documentation)
- [License](#license)

---

## What This Project Does

1. **Generates a sinusoidal AC excitation** via the AD5940's waveform generator (WGTYPE_SIN).
2. **Sweeps across a configurable frequency range** (e.g. 10 Hz to 100 kHz) with a programmable number of points per decade.
3. **Measures the complex impedance** (real, imaginary, magnitude, phase) at each frequency using the AD5940's on-chip DFT engine.
4. **Outputs results** as CSV over USB serial, transmits via BLE to a phone app, and/or saves to an SD card.
5. **Calculates equivalent circuit parameters** (Rct, Rs) using a Levenberg-Marquardt fit.

The system supports seven measurement modes (EIS is fully implemented; others are stub-ready):

| Mode | Code | Description |
|------|------|-------------|
| EIS | 0 | Electrochemical Impedance Spectroscopy |
| CV | 1 | Cyclic Voltammetry (stub) |
| IT/CA | 2 | Chronoamperometry (stub) |
| CP | 3 | Chronopotentiometry (stub) |
| DPV | 4 | Differential Pulse Voltammetry (stub) |
| SWV | 5 | Square Wave Voltammetry (stub) |
| OCP | 6 | Open Circuit Potential (stub) |

---

## Hardware Overview

| Component | Description |
|-----------|-------------|
| **MCU** | ESP32-S3 (240 MHz dual-core, Wi-Fi + BLE 5.0) |
| **AFE** | AD5940 / AD5941 analog front-end (waveform generator, DFT engine, programmable gain TIA) |
| **Interface** | SPI between ESP32-S3 and AD5940 |
| **Storage** | MicroSD card slot (SPI) |
| **Wireless** | BLE for real-time data streaming to a companion Android app |
| **User Input** | Physical push-button on GPIO 7 |
| **Indicator** | LED on GPIO 6 |
| **Power** | USB-C or 3.7 V LiPo battery |

---

## Repository Structure

```
.
├── src/
│   └── main.cpp              # Firmware entry point (serial parser, button handler, measurement loop)
├── lib/
│   ├── HELPStatLib/           # Core library
│   │   ├── HELPStat.cpp/h    # High-level measurement, BLE, SD, sweep logic
│   │   ├── ad5940.c/h        # AD5940 low-level register driver
│   │   ├── ad594x.cpp        # AD594x wrapper functions
│   │   ├── Impedance.c/h     # Impedance calculation helpers
│   │   ├── lma.cpp/h         # Levenberg-Marquardt curve fitting
│   │   └── constants.h       # Pin definitions and constants
│   └── Eigen/                 # Eigen C++ math library (used by LMA)
├── boards/
│   └── custom_s3.json         # Custom PCB board definition for PlatformIO
├── docs/
│   ├── MEASUREMENT_MODES.md   # Detailed command format and parameter reference
│   ├── NEW_FEATURES.md        # Implemented features (validation, averaging, etc.)
│   └── IMPROVEMENTS.md        # Roadmap of AD5940 improvements from datasheet analysis
├── Originallibraries/         # Reference copy of the original HELPStat Android app and library
├── replug_and_sweep.py        # One-shot USB sweep script (Windows, handles USB re-enumeration)
├── eis_sweep.py               # Clean EIS sweep script with Excel output
├── send_eis_sample.py         # CLI tool with --port, --start-hz, --end-hz, --out options
├── eis_csv_to_xlsx.py         # Convert SD card CSV to Excel
├── eis_example.py             # Example: sweep + Nyquist/Bode plotting
├── platformio.ini             # PlatformIO build configuration
├── requirements.txt           # Python dependencies
└── README.md                  # This file
```

---

## Getting Started

### Prerequisites

| Tool | Purpose | Install |
|------|---------|---------|
| **VS Code** | IDE | [code.visualstudio.com](https://code.visualstudio.com/) |
| **PlatformIO** | Build system for ESP32 | Install the PlatformIO extension in VS Code |
| **Python 3.8+** | Data capture and analysis scripts | [python.org](https://www.python.org/) |
| **USB driver** | ESP32-S3 USB Serial/JTAG | Built into Windows 10+, macOS, and Linux |

> **Note:** The Arduino IDE is **not required**. PlatformIO uses the Arduino framework as a library.

### Clone the Repository

```bash
git clone https://github.com/Shivasa042/Potentiostat-Wearable-Sensor.git
cd Potentiostat-Wearable-Sensor
```

### Build and Flash the Firmware

1. Open the project folder in VS Code (File > Open Folder).
2. PlatformIO will auto-detect `platformio.ini` and install dependencies.
3. Connect your board via USB-C.
4. Select the build environment from the status bar:
   - `env:esp32-s3-dev-module` — for the ESP32-S3 DevKitC
   - `env:custom-board` — for a custom PCB (see [Custom PCB Support](#custom-pcb-support))
5. Click **Build** (checkmark icon) or press `Ctrl+Alt+B`.
6. Click **Upload** (arrow icon) or run:
   ```bash
   pio run -t upload
   ```
7. Open the serial monitor (plug icon) at **115200 baud**.

**Windows Note:** After flashing, you may need to unplug and re-plug the USB cable once. This forces the ESP32-S3 to exit its ROM bootloader and boot into the application firmware. This is only needed for USB serial development, not for standalone operation.

Install the Python dependencies for the data capture scripts:

```bash
pip install -r requirements.txt
```

---

## Usage Scenarios

### Scenario 1 — Laptop + USB Serial (Development)

Best for: development, debugging, and quick characterization.

1. Flash the firmware and open a serial monitor at 115200 baud.
2. Type commands directly:
   ```
   HELP                    — list all commands
   MEASURE:SAMPLE          — run default 10 Hz – 100 kHz sweep
   MEASURE:0,10,100000,10,0,0,1000,1,1,127000,150,0,0,200  — custom sweep
   ```
3. CSV data prints to the terminal. Copy-paste into a spreadsheet or use the Python scripts below.

### Scenario 2 — Standalone with Button + SD Card

Best for: field measurements without any computer.

1. Flash the firmware once (via Scenario 1).
2. Insert a FAT32-formatted MicroSD card.
3. Power the board via USB power bank or LiPo battery.
4. **Press the button on GPIO 7** to start a default EIS sweep (10 Hz – 100 kHz, 200 mV amplitude).
5. The LED turns off during measurement and turns back on when complete.
6. Results are saved to the SD card at `/eis/sweep_10Hz_100kHz.csv`.
7. Remove the SD card and convert the CSV to Excel:
   ```bash
   python eis_csv_to_xlsx.py --in sweep_10Hz_100kHz.csv --out results.xlsx
   ```

### Scenario 3 — Battery-Powered Wearable with BLE

Best for: continuous real-time monitoring (e.g. sweat analysis).

1. Flash the firmware once.
2. Power the board from a 3.7 V LiPo battery.
3. On your Android phone, open the **HELPStat companion app** (source in `Originallibraries/`).
4. Scan for BLE devices and connect to `HELPStat`.
5. Press the physical button to trigger a sweep, or send parameters via BLE.
6. Real-time impedance data (frequency, real, imaginary, magnitude, phase) streams to the app via BLE notifications.

The board does **not** require a USB connection or laptop for measurements — the plug/unplug cycle is only an artifact of the Windows USB serial driver during development. In battery-powered mode, the firmware boots cleanly on power-up and the button triggers sweeps immediately.

### Scenario 4 — Automated Data Collection (Python)

Best for: batch measurements, lab automation, data logging to Excel.

**Quick sweep with Excel output (Windows):**

```bash
python eis_sweep.py
```

This script auto-detects the ESP32-S3 on USB, sends a sweep command, captures the CSV output, and saves it as `eis_sweep_data.xlsx`.

**CLI tool with full control:**

```bash
python send_eis_sample.py --port COM4 --start-hz 100 --end-hz 50000 --amplitude-mv 150 --out sweep.xlsx
```

**Multiple sweeps with plotting:**

```bash
python eis_example.py --port COM4 --times 5 --out dataset.csv
```

**First-time USB sweep on Windows** (handles the one-time replug):

```bash
python replug_and_sweep.py
```

Follow the on-screen prompts to unplug and replug the USB cable. Data is saved to `eis_sweep_data.xlsx`.

---

## Serial Command Reference

All commands are sent over USB serial at **115200 baud**, terminated by a newline (`\n`).

| Command | Description |
|---------|-------------|
| `HELP` or `?` | List available commands |
| `STATUS` | Show whether a measurement is queued |
| `SHOW` | Print current configuration parameters |
| `CHECK` | Read the AD5940 chip ID to verify SPI connectivity |
| `MEASURE` | Start measurement with current parameters |
| `MEASURE:SAMPLE` | Run default EIS sweep (10 Hz – 100 kHz) |
| `MEASURE:<14 params>` | Start measurement with inline parameters |
| `SET:<14 params>` | Set parameters without starting a measurement |

### Parameter Format (14 values, comma-separated)

```
MEASURE:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude
```

| # | Parameter | Type | Description |
|---|-----------|------|-------------|
| 1 | mode | int | 0=EIS, 1=CV, 2=IT, 3=CP, 4=DPV, 5=SWV, 6=OCP |
| 2 | startFreq | float | Start frequency in Hz |
| 3 | endFreq | float | End frequency in Hz |
| 4 | numPoints | int | Points per decade |
| 5 | biasVolt | float | DC bias voltage (V) |
| 6 | zeroVolt | float | Zero voltage (V) |
| 7 | rcalVal | float | Calibration resistor (Ohms) |
| 8 | extGain | int | Excitation buffer gain (1=x2, 2=x0.25) |
| 9 | dacGain | int | DAC gain (1=x1, 0=x0.2) |
| 10 | rct_est | float | Estimated charge transfer resistance (Ohms) |
| 11 | rs_est | float | Estimated solution resistance (Ohms) |
| 12 | numCycles | int | Number of measurement cycles |
| 13 | delaySecs | int | Delay between cycles (seconds) |
| 14 | amplitude | float | AC excitation amplitude (mV, 0–800) |

A legacy 12-parameter format (without mode and amplitude) is also supported for backward compatibility.

### Example Commands

```
MEASURE:SAMPLE
MEASURE:0,200000,10,5,0,0,1000,1,1,127000,150,0,0,200
MEASURE:0,10,100000,10,0,0,1000,1,1,127000,150,0,0,100
SET:0,100,50000,20,0,0,1000,1,1,127000,150,0,0,150
```

---

## Python Scripts

| Script | Purpose | Key Options |
|--------|---------|-------------|
| `eis_sweep.py` | Auto-detect board, sweep, save to Excel | Edit constants at top of file |
| `send_eis_sample.py` | Full CLI tool for custom sweeps | `--port`, `--start-hz`, `--end-hz`, `--amplitude-mv`, `--out` |
| `eis_example.py` | Sweep with Nyquist + Bode plotting | `--port`, `--times`, `--out` |
| `replug_and_sweep.py` | Windows USB re-enumeration handler | Edit constants at top of file |
| `eis_csv_to_xlsx.py` | Convert SD card CSV to Excel | `--in`, `--out` |

### Python Dependencies

```
pyserial
openpyxl
numpy
matplotlib
```

Install all at once:
```bash
pip install -r requirements.txt
```

---

## Pin Configuration

Default pin assignments (defined in `lib/HELPStatLib/constants.h`):

| Signal | GPIO | Description |
|--------|------|-------------|
| MOSI | 35 | SPI data out to AD5940 |
| MISO | 37 | SPI data in from AD5940 |
| SCK | 36 | SPI clock |
| CS | 11 | AD5940 chip select |
| RESET | 10 | AD5940 reset |
| INTERRUPT | 9 | AD5940 interrupt |
| CS_SD | 21 | SD card chip select |
| BUTTON | 7 | Physical push-button (active LOW, internal pull-up) |
| LED | 6 | Status indicator LED |

Pins can be overridden via build flags in `platformio.ini`:

```ini
build_flags =
    -DMOSI=35
    -DMISO=37
    -DSCK=36
    -DCS=11
    -DRESET=10
    -DESP32_INTERRUPT=9
    -DCS_SD=21
```

---

## Custom PCB Support

1. Copy `boards/custom_s3.json` and edit the flash size, VID/PID, and other parameters for your hardware.
2. Select `env:custom-board` in `platformio.ini` or create a new environment.
3. Override pin definitions using `-D` build flags (see above).
4. Set `upload_port` and `monitor_port` for your board's COM port.

---

## How EIS Works on This Board

1. The **AD5940 waveform generator** produces a sine wave at the target frequency:
   ```
   WgCfg.WgType = WGTYPE_SIN
   SinCfg.SinFreqWord = AD5940_WGFreqWordCal(frequency, sysClkFreq)
   SinCfg.SinAmplitudeWord = (amplitude_mV / 800.0) * 2047
   ```
2. This AC excitation is applied to the electrochemical cell through the HSDAC and excitation buffer.
3. The resulting current flows through the **HSTIA** (high-speed transimpedance amplifier), which converts it to a voltage. The TIA gain resistor is automatically selected based on frequency.
4. The AD5940's on-chip **DFT engine** performs a discrete Fourier transform on the measured signal to extract the real and imaginary components.
5. **Impedance** is calculated: Z = V_excitation / I_measured, decomposed into real (Z') and imaginary (Z'') parts.
6. The firmware sweeps through all frequencies, performing RCAL (calibration) and RZ (unknown impedance) measurements at each point.
7. Results are output as CSV, transmitted via BLE, and saved to SD card.

---

## Further Documentation

Detailed technical documentation is in the `docs/` folder:

- **[docs/MEASUREMENT_MODES.md](docs/MEASUREMENT_MODES.md)** — Complete command format, parameter descriptions, and all measurement mode details.
- **[docs/NEW_FEATURES.md](docs/NEW_FEATURES.md)** — Implemented improvements: data validation, gain hysteresis, averaging, temperature sensing, notch filter, auto-gain selection.
- **[docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md)** — Future improvement roadmap based on AD5940/AD5941 datasheet analysis (sequencer, FIFO, power management, etc.).

The original HELPStat Android companion app source code and library are preserved in `Originallibraries/` for reference.

---

## License

MIT License

Copyright (c) 2024 Linnes Lab, Purdue University, West Lafayette, IN, USA

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
