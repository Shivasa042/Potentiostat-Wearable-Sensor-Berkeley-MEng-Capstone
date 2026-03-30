# HELPStat — Wearable Potentiostat / EIS Sensor

A portable Electrochemical Impedance Spectroscopy (EIS) system built around an **ESP32-S3** microcontroller and the **Analog Devices AD5940** analog front-end. Designed for wearable biosensing applications such as real-time sweat analysis on a smartwatch, the board performs frequency-swept impedance measurements over the **1 Hz – 200 kHz** range using a **2-electrode** configuration and delivers results over **USB serial**, **Bluetooth Low Energy (BLE)**, and **SD card** storage. In standalone wearable mode the firmware automatically triggers EIS sweeps on a configurable timer and waits between sweeps (USB serial stays responsive for development).

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
- [Analyzing EIS Data (Nyquist and Bode)](#analyzing-eis-data-nyquist-and-bode)
- [Pin Configuration](#pin-configuration)
- [Custom PCB Support](#custom-pcb-support)
- [How EIS Works on This Board](#how-eis-works-on-this-board)
- [Further Documentation](#further-documentation)
- [License](#license)

---

## What This Project Does

1. **Generates a sinusoidal AC excitation** via the AD5940's waveform generator (WGTYPE_SIN).
2. **Sweeps across a configurable frequency range** (default 1 Hz – 200 kHz) with a programmable number of points per decade.
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
| **Electrodes** | 2-electrode configuration (CE0 + SE0) — no separate reference electrode needed |
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
├── testing/                   # USB/serial capture, conversion, and plotting scripts
│   ├── eis_sweep.py           # Clean EIS sweep script with Excel output
│   ├── send_eis_sample.py     # CLI tool with --port, --start-hz, --end-hz, --out options
│   ├── eis_example.py         # Example: sweep + Nyquist/Bode plotting
│   ├── replug_and_sweep.py    # One-shot USB sweep (Windows, handles USB re-enumeration)
│   ├── eis_csv_to_xlsx.py     # Convert SD card CSV to Excel
│   ├── plot_eis_from_file.py  # Nyquist + Bode from saved CSV or Excel
│   └── sample_plots/          # Example figures (Nyquist, Bode, DFT impedance) from prior sweeps
├── platformio.ini             # PlatformIO build configuration
├── requirements.txt           # Python dependencies (used by testing/ scripts)
└── README.md                  # This file
```

The files under `testing/sample_plots/` (`nyquist.png`, `bode.png`, `dft_impedance_plots.png`) are **example EIS figures** saved from earlier measurement or simulation runs. They illustrate what Nyquist, Bode, and DFT-style impedance plots look like for this system; they are not required to build firmware or run scripts. Regenerate your own plots with `testing/plot_eis_from_file.py` or `testing/eis_example.py` from your CSV or Excel data.

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
git clone https://github.com/Shivasa042/Potentiostat-Wearable-Sensor-Berkeley-MEng-Capstone.git
cd Potentiostat-Wearable-Sensor-Berkeley-MEng-Capstone
```

If you keep a local copy named **Potentiostat-Wearable-Sensor-Berkeley-MEng-Capstone**, open that folder instead and run all commands from its root (next to `platformio.ini`).

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
   MEASURE:SAMPLE          — run default 1 Hz – 200 kHz sweep
   MEASURE:0,200000,1,10,0,0,1000,1,1,127000,150,0,0,200   — custom sweep
   ```
3. CSV data prints to the terminal. Copy-paste into a spreadsheet or use the Python scripts below.

### Scenario 2 — Standalone with Button + SD Card

Best for: field measurements without any computer.

1. Flash the firmware once (via Scenario 1).
2. Insert a FAT32-formatted MicroSD card.
3. Power the board via USB power bank or LiPo battery.
4. **Press the button on GPIO 7** to start a default EIS sweep (1 Hz – 200 kHz, 200 mV amplitude).
5. The LED turns off during measurement and turns back on when complete.
6. Results are saved to the SD card at `/eis/sweep_1Hz_200kHz.csv`.
7. Remove the SD card and convert the CSV to Excel:
   ```bash
   python testing/eis_csv_to_xlsx.py --in sweep_1Hz_200kHz.csv --out results.xlsx
   ```

### Scenario 3 — Battery-Powered Wearable with BLE (Apple Watch / Smartwatch Sweat Sensor)

Best for: continuous autonomous sweat monitoring with a 2-electrode sensor.

The board is designed to operate standalone — no USB cable or laptop required at runtime.

1. Flash the firmware once (via Scenario 1).
2. Connect the two electrodes from the watch back-plate to the AD5940's **CE0** (counter/excitation) and **SE0** (sense/working) pins.
3. Power the board from a 3.7 V LiPo battery.
4. On boot, the firmware automatically enters **wearable mode**:
   - An EIS sweep runs immediately on power-up.
   - After each sweep the firmware **waits** for the configured interval (default 5 minutes), then runs the next sweep, saves to SD card, and transmits via BLE.
5. On your phone, open the **HELPStat companion app** (source in `Originallibraries/`) or any generic BLE terminal.
6. Scan for BLE devices and connect to `HELPStat` to receive real-time impedance data.
7. To change the sweep interval or disable auto-sweep, connect via serial and type:
   ```
   INTERVAL:120       — sweep every 2 minutes
   WEARABLE:OFF       — stop auto-sweep (manual / button only)
   ```

The board does **not** require a USB connection or laptop for measurements — the plug/unplug cycle is only an artifact of the Windows USB serial driver during development. In battery-powered mode, the firmware boots cleanly on power-up and sweeps begin automatically.

### Scenario 4 — Automated Data Collection (Python)

Best for: batch measurements, lab automation, data logging to Excel.

**Quick sweep with Excel output (Windows):**

```bash
python testing/eis_sweep.py
```

This script auto-detects the ESP32-S3 on USB, sends a sweep command, captures the CSV output, and saves it as `eis_sweep_data.xlsx`.

**CLI tool with full control:**

```bash
python testing/send_eis_sample.py --port COM4 --start-hz 100 --end-hz 50000 --amplitude-mv 150 --out sweep.xlsx
```

**Multiple sweeps with plotting:**

```bash
python testing/eis_example.py --port COM4 --times 5 --out dataset.csv
```

**First-time USB sweep on Windows** (handles the one-time replug):

```bash
python testing/replug_and_sweep.py
```

Follow the on-screen prompts to unplug and replug the USB cable. Data is saved to `eis_sweep_data.xlsx`.

---

## Serial Command Reference

All commands are sent over USB serial at **115200 baud**, terminated by a newline (`\n`).

| Command | Description |
|---------|-------------|
| `HELP` or `?` | List available commands |
| `STATUS` | Show measurement queue and wearable timer status |
| `SHOW` | Print current configuration parameters |
| `CHECK` | Read the AD5940 chip ID to verify SPI connectivity |
| `MEASURE` | Start measurement with current parameters |
| `MEASURE:SAMPLE` | Run default EIS sweep (1 Hz – 200 kHz) |
| `MEASURE:<14 params>` | Start measurement with inline parameters |
| `SET:<14 params>` | Set parameters without starting a measurement |
| `WEARABLE:ON` | Enable automatic periodic EIS sweeps (default) |
| `WEARABLE:OFF` | Disable auto-sweep; manual / button triggers only |
| `INTERVAL:N` | Set auto-sweep interval to N seconds (min 10, default 300) |

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
MEASURE:0,200000,1,10,0,0,1000,1,1,127000,150,0,0,200
MEASURE:0,200000,100,5,0,0,1000,1,1,127000,150,0,0,100
SET:0,100,50000,20,0,0,1000,1,1,127000,150,0,0,150
```

---

## Python Scripts

| Script | Purpose | Key Options |
|--------|---------|-------------|
| `testing/eis_sweep.py` | Auto-detect board, sweep, save to Excel | Edit constants at top of file |
| `testing/send_eis_sample.py` | Full CLI tool for custom sweeps | `--port`, `--start-hz`, `--end-hz`, `--amplitude-mv`, `--out` |
| `testing/eis_example.py` | Sweep with Nyquist + Bode plotting | `--port`, `--times`, `--out` |
| `testing/replug_and_sweep.py` | Windows USB re-enumeration handler | Edit constants at top of file |
| `testing/eis_csv_to_xlsx.py` | Convert SD card CSV to Excel | `--in`, `--out` |
| `testing/plot_eis_from_file.py` | Nyquist + Bode from saved CSV or `.xlsx` | `--input`, `--output`, `--title` |

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

## Analyzing EIS Data (Nyquist and Bode)

After a sweep, the firmware prints a CSV block between `=== EIS DATA CSV ===` and `=== END CSV ===`. The header is:

`Frequency(Hz),Real(Ohm),Imaginary(Ohm),Magnitude(Ohm),Phase(Degrees)`

SD card files (under `/eis/`) use a similar layout with columns such as `Freq`, `Real`, and `Imag`. You can convert SD CSV to Excel with `testing/eis_csv_to_xlsx.py` if you prefer working in a spreadsheet.

### Prepare your file

1. **From serial:** Copy only the data rows (including the header line) into a new `.csv` file. Remove banner lines such as `=== EIS DATA CSV ===` so the **first line** is the column header and every following line is one frequency point.
2. **From Excel:** Put frequency, real impedance, and imaginary impedance in columns whose names contain `Frequency`/`Freq`, `Real`, and `Imaginary`/`Imag` (the script matches these flexibly).
3. **Invalid points:** Open-circuit or failed measurements often appear as `inf` or `nan`. The plotting script **drops** non-finite values automatically so the rest of the sweep still plots.

### Generate Nyquist and Bode plots

From the project folder (with `pip install -r requirements.txt` already done):

```bash
python testing/plot_eis_from_file.py --input your_sweep.csv
```

Save a PNG without relying on an interactive window:

```bash
python testing/plot_eis_from_file.py --input your_sweep.xlsx --output nyquist_bode.png --no-show
```

Optional title on the figure:

```bash
python testing/plot_eis_from_file.py -i sweep.csv --title "Sweat sensor — trial 1"
```

### How to read the plots

| Plot | Axes | Typical interpretation |
|------|------|-------------------------|
| **Nyquist** | Horizontal: real part of impedance Z′ (Ω). Vertical: **minus** imaginary part −Z″ (Ω). | A **semicircle** often indicates charge-transfer resistance (diameter related to Rct) and double-layer capacitance. The **left** intercept with the real axis is often related to **solution resistance** Rs. A **Warburg** (45° line) at low frequency indicates diffusion. |
| **Bode (magnitude)** | Frequency (log scale) vs \|Z\| (Ω). | Shows how total impedance changes with frequency; capacitive regions often show falling magnitude with frequency. |
| **Bode (phase)** | Frequency (log scale) vs phase (degrees). | Near **0°** behaves resistively; toward **−90°** behaves capacitively. |

These shapes depend on your chemistry (electrolyte, electrode materials, and whether a redox reaction is fast or slow). Compare sweeps taken under the same settings (amplitude, bias, temperature) to see changes from sweat composition or binding events.

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

## AD5940 Datasheet-Driven Improvements

The firmware incorporates several corrections and optimizations derived from the [AD5940/AD5941 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ad5940-5941.pdf):

| Change | Detail |
|--------|--------|
| **ADC rate fix** | Removed accidental 1.6 MHz overwrite in Low Power mode; correctly uses 800 kHz for frequencies below 80 kHz. |
| **ADCBUFCON register** | ADC buffer chop mode is now explicitly configured when switching between LP (<80 kHz) and HP (>=80 kHz) modes across all measurement paths. |
| **PGA gain** | Switched from `ADCPGA_1` to `ADCPGA_1P5`, the production-calibrated default recommended by the datasheet. |
| **SINC filter OSR** | SINC3 OSR raised from 2 to 4 and SINC2 OSR set to 22 for improved noise rejection at 200 kSPS. |
| **DFT delay calculation** | Fixed a zero-delay bug where `_waitClcks * (1/SYSCLCK)` evaluated to 0 due to integer division; now uses proper floating-point conversion. |
| **Settling delay** | Replaced flat 1-second delay with frequency-proportional timing (4 excitation periods, bounded 5 ms – 10 s), significantly speeding up mid- and high-frequency sweeps. |
| **Dynamic CTIA selection** | HSTIA feedback capacitance is now computed from the RTIA value and measurement frequency to maintain adequate TIA bandwidth (>=10x excitation frequency). |
| **2-electrode mode** | Switch matrix Pswitch changed from `SWP_RE0` (dedicated reference electrode) to `SWP_CE0` (sense at counter electrode) across all measurement paths, enabling a simple 2-electrode cell suitable for wearable sweat sensors. |
| **Wearable auto-sweep** | `loop()` includes a configurable timer that auto-triggers EIS sweeps and waits between measurements (USB-friendly; no light sleep on USB-Serial/JTAG). |

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
