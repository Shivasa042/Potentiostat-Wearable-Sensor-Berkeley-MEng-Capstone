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
  - [Quick USB lab scripts (`testing/`)](#quick-usb-lab-scripts-testing)
  - [Scenario 2 — Standalone with Button + SD Card](#scenario-2--standalone-with-button--sd-card)
  - [Scenario 3 — Battery-Powered Wearable with BLE](#scenario-3--battery-powered-wearable-with-ble)
  - [Scenario 4 — Automated Data Collection (Python)](#scenario-4--automated-data-collection-python)
- [Companion web app (`web/helpstat-app`)](#companion-web-app-webhelpstat-app)
- [Native iOS app (`ios/HELPStatCompanion`)](#native-ios-app-ioshelpstatcompanion)
- [Serial Command Reference](#serial-command-reference)
- [Python Scripts](#python-scripts)
- [Analyzing EIS Data (Nyquist and Bode)](#analyzing-eis-data-nyquist-and-bode)
- [Troubleshooting](#troubleshooting)
- [EIS accuracy, limitations, and best practices](#eis-accuracy-limitations-and-best-practices)
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
4. **Outputs results** as CSV over USB serial, transmits via BLE to the **web companion** (or other BLE clients), and/or saves to an SD card.
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
| **Wireless** | BLE streaming; **web** companion (`web/helpstat-app`), **native iOS** (`ios/HELPStatCompanion`), or legacy **Android** (`Originallibraries/`). |
| **User Input** | Physical push-button on GPIO 7 |
| **Indicator** | LED on GPIO 6 |
| **Power** | USB-C or 3.7 V LiPo battery |
| **RCAL** | On-board calibration resistor: firmware defaults to **1 kΩ** (WatchScript-style pairing with the RTIA schedule). If your PCB still has **100 Ω**, set **`rcalVal`** / **`RCAL:`** / NVS to the **measured** value — see [EIS accuracy](#eis-accuracy-limitations-and-best-practices). |

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
├── Originallibraries/         # Legacy Android BLE app (optional; not required for the web companion)
├── web/
│   └── helpstat-app/          # PWA-style companion (Web Bluetooth, Pacific-day history, plots)
├── ios/
│   └── HELPStatCompanion/     # Native SwiftUI iOS app (CoreBluetooth, same GATT + JSON history)
├── testing/                   # USB/serial capture, conversion, and plotting scripts
│   ├── scenario1_serial_sample.py  # Fast lab sweep or single-frequency capture → CSV (see below)
│   ├── board_status_wearable.py     # Print STATUS, enable WEARABLE:ON (optional wait for idle)
│   ├── eis_sweep.py           # Full-band EIS sweep script with Excel output
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
6. Click **Upload** (arrow icon) or run from the project root:
   ```bash
   pio run -t upload
   ```
   On Windows, if `pio` is not on your PATH, use:
   ```bash
   py -3 -m platformio run -t upload
   ```
   Close any **serial monitor** or **Python script** using the COM port first, or upload may fail with “port busy / access denied.”
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
   MEASURE:0,200000,1,10,0,0,1000,1,1,127000,150,0,0,200   — custom sweep (`rcalVal` = measured RCAL; use **100** only if a 100 Ω part is populated)
   ```
3. CSV data prints to the terminal. Copy-paste into a spreadsheet or use the Python scripts below.

### Quick USB lab scripts (`testing/`)

These scripts assume **Python 3** with **`pyserial`** (`pip install -r requirements.txt`). They default to the **ESP32-S3** USB serial port (VID **303A**) or you can pass **`COM4`** (or your port) explicitly.

| Script | What it does |
|--------|----------------|
| **`testing/scenario1_serial_sample.py`** | Sends **`WEARABLE:OFF`** (queued early), waits until the firmware is idle (**`Ready for next measurement`** or **`Wearable auto-sweep DISABLED`**), then runs a **`MEASURE`** and saves the **`=== EIS DATA CSV ===` … `=== END CSV ===`** block. **Default sweep:** **10 kHz → 1 kHz**, **2** DFT points, **150 mV**, **`rcalVal = 1000`** Ω (default aligned with **1 kΩ RCAL**). **Single frequency:** add the frequency in Hz as the **last** argument, e.g. `10000` → one point at 10 kHz (uses a narrow start/end span so total sweep points = 1; `startFreq == endFreq` is invalid in firmware). Outputs: `scenario1_eis_sample.csv` or `scenario1_eis_sample_single.csv`. |
| **`testing/board_status_wearable.py`** | Sends **`STATUS`**, then **`WEARABLE:ON`** (wearable state is **RAM-only** until power-cycle/reflash). Optional: `--wait-ready 600` waits for **`Ready for next measurement`** then prints **`STATUS`** again (useful after a long auto-sweep). |

**Examples (Windows, project root):**

```bash
py -3 testing/scenario1_serial_sample.py COM4
py -3 testing/scenario1_serial_sample.py COM4 10000
py -3 testing/board_status_wearable.py COM4
py -3 testing/board_status_wearable.py COM4 --wait-ready 600
```

**Wearable vs lab:** Firmware defaults to **`WEARABLE:OFF`** on USB/bench so a script can run a **short** sweep without waiting for a **200 kHz → 1 Hz** auto-sweep to finish first. For **Scenario 3** (timed sweeps), send **`WEARABLE:ON`** once (or use `board_status_wearable.py`). The first time you enable wearable with **`firstBoot`**, the firmware may **immediately** start the default full sweep.

**Single-frequency note:** The AD5940 path computes sweep length from **`log10(end/start) × numPoints`**. Equal start/end yields **zero** points, so the single-point helper uses **`end ≈ 1.035 × start`** with **`numPoints = 100`** so only **one** measurement runs at **`startFreq`**.

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

Best for: **continuous autonomous sweat monitoring** with a 2-electrode sensor.

The board is designed to operate standalone — no USB cable or laptop required at runtime (USB is optional for power, logging, or one-time setup).

#### Hardware setup

1. Flash the firmware once (via Scenario 1).
2. Connect the two electrodes from the watch back-plate to the AD5940's **CE0** (counter/excitation) and **SE0** (sense/working) pins.
3. Power the board from a **3.7 V LiPo** (field) or **USB** (bench / development). Insert a **FAT32 microSD card** if you want on-device CSV storage (optional; see below).

#### Turning on continuous (timed) sweeps

Firmware **defaults to `WEARABLE:OFF`** on USB/bench builds so lab scripts are not blocked by a long auto-sweep. For Scenario 3, you must **enable wearable mode** once per power session:

| Action | How |
|--------|-----|
| Enable timed sweeps | Serial: **`WEARABLE:ON`** (115200 baud, newline-terminated), or run **`py -3 testing/board_status_wearable.py COM4`** from the repo. |
| Check state | **`STATUS`** — shows *Wearable auto-sweep: ON/OFF*, **sweep interval** (seconds), and time since last sweep. |
| Change how often sweeps run | **`INTERVAL:N`** with **N ≥ 10** (default **300** = 5 minutes). Example: **`INTERVAL:120`** = every 2 minutes. |
| Stop auto-sweeps | **`WEARABLE:OFF`** — manual sweeps only (button / **`MEASURE`**). |

**Persistence:** **`WEARABLE:ON` / `OFF` and `INTERVAL` live in RAM.** After **power-cycle** or **reflash**, wearable is **OFF** again until you send **`WEARABLE:ON`**. To make “always on” permanent, change the firmware default (`wearableMode` in `src/main.cpp`) or re-enable over serial at each boot.

**First sweep after `WEARABLE:ON`:** If **`firstBoot`** is still true (wearable had not yet armed a sweep this boot), the firmware may start the **default full EIS sweep (200 kHz → 1 Hz)** **immediately**. Otherwise the next sweep is scheduled from the **interval timer** (and sending **`WEARABLE:ON`** resets the internal “last sweep” clock—expect up to one full **INTERVAL** wait before the next run in that case). Each full sweep can take **many minutes**.

#### How you get data when continuous sweeps are running

After **each** sweep completes, the firmware (see `src/main.cpp` → `printDataCSV`, `BLE_transmitResults`, `saveDataEIS`) outputs data on **three paths**. You can use **one or more** at the same time (e.g. BLE to phone + USB logging on a laptop).

| Path | What happens | Practical notes |
|------|----------------|-------------------|
| **USB serial (115200)** | Prints **`=== EIS DATA CSV ===` … `=== END CSV ===`** with columns **`Frequency(Hz),Real(Ohm),Imaginary(Ohm),Magnitude(Ohm),Phase(Degrees)`** plus calculated **Rct/Rs** block. | Works whenever USB is connected. Leave a terminal open or run a **logger** that reads COM and **appends** each CSV block to a file. **While `runSweep()` is active**, new serial **commands** are not parsed; the CSV still appears **after** each sweep finishes. |
| **Bluetooth Low Energy** | **`BLE_transmitResults()`** notifies **Rct**, **Rs**, then **per-point** frequency, real, imag, phase (deg), magnitude. | Use the **web companion** ([`web/helpstat-app`](#companion-web-app-webhelpstat-app)) in **Chrome** or **Edge** (Web Bluetooth), or the **native iOS app** ([`ios/HELPStatCompanion`](#native-ios-app-ioshelpstatcompanion)). Connect to **`HELPStat`**. The web app stores history **per Pacific calendar day** (IndexedDB); you can **export/import JSON**. iPhone must use the **native app** for live BLE (no Web Bluetooth in Safari). |
| **microSD card** | **`saveDataEIS()`** writes under **`/<folder>/`** with a **timestamped** base name, e.g. **`sweep_1Hz_200kHz_<millis>.csv`**, plus a **`# rcal_Ohm=… dieTemp_C_approx=…`** header line. | Requires a **mounted** card. Older tooling should **ignore `#` comment lines** when parsing numeric rows. |

#### Companions and legacy tools

- **Web (desktop/Android Chrome/Edge):** [`web/helpstat-app`](#companion-web-app-webhelpstat-app) — Live + History + export.
- **iOS:** [`ios/HELPStatCompanion`](#native-ios-app-ioshelpstatcompanion) — same idea natively.
- **Legacy Android:** optional Kotlin tree under `Originallibraries/`.

The board does **not** require USB for measurements in the field; USB is mainly for development, power, or **serial logging** when tethered.

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

## Companion web app (`web/helpstat-app`)

This is the **maintained** phone- and laptop-friendly UI: **Live** (connect, optional sweep parameters, run), **History** (pick a **Pacific** calendar day and sweep, Nyquist + Bode), and **About** (platform notes). Data is grouped **by day** using `America/Los_Angeles` (PST/PDT). Each completed sweep (when **Rs** is received over BLE) is appended to that day in **IndexedDB**. You can **export** / **import** JSON to move history between devices.

### Run locally (required for Web Bluetooth)

Browsers only expose Bluetooth from a **secure context** (HTTPS or `http://localhost`):

```bash
cd web/helpstat-app
python -m http.server 8080
```

Open `http://localhost:8080/`. Use **Chrome** or **Edge** on **Windows, macOS, Linux, or Android**.

### iPhone: web vs native

- The **web app cannot use Bluetooth on iPhone** (no Web Bluetooth in Safari or Chrome on iOS). For **live** BLE on iPhone, use the **native iOS project** below.
- You can still use the web UI on iPhone for **viewing** data: **Add to Home Screen**, then **History → Import** JSON from another device.

### Legacy Android project

An older Kotlin app still lives under `Originallibraries/` if you need a native Android build; it is **not** required for the web companion.

---

## Native iOS app (`ios/HELPStatCompanion`)

SwiftUI + **CoreBluetooth** companion for iPhone/iPad: **Live** (scan for `HELPStat`, connect, optional parameters, run sweep), **History** (Pacific calendar days, Nyquist + Bode via **Swift Charts**), **About**. Completed sweeps are saved as JSON under Application Support (`helpstat_history/YYYY-MM-DD.json`), compatible with **export/import** from the web app.

### Open in Xcode

1. On a Mac, install [Xcode](https://developer.apple.com/xcode/) (15+ recommended).
2. Open `ios/HELPStatCompanion/HELPStatCompanion.xcodeproj`.
3. Select your **Team** in **Signing & Capabilities** (target **HELPStatCompanion**). Change **Bundle Identifier** if it conflicts with an existing app ID.
4. Add a **1024×1024** App Icon in `Assets.xcassets` → **AppIcon** before archiving for TestFlight / App Store (the catalog ships with a placeholder slot).

### App Store

The project is set up for standard **automatic signing**. To distribute: **Product → Archive**, then upload via **Organizer**. You must have an **Apple Developer Program** membership.

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
| `WEARABLE:ON` | Enable automatic periodic EIS sweeps (use for Scenario 3 / field) |
| `WEARABLE:OFF` | Disable auto-sweep (default on USB/bench; manual / button only) |
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
MEASURE:0,200000,1,10,0,0,100,1,1,127000,150,0,0,200
MEASURE:0,200000,100,5,0,0,100,1,1,127000,150,0,0,100
SET:0,100,50000,20,0,0,100,1,1,127000,150,0,0,150
```

---

## Python Scripts

| Script | Purpose | Key Options |
|--------|---------|-------------|
| `testing/scenario1_serial_sample.py` | Short default sweep or single-frequency CSV capture; syncs with firmware idle | Port, optional `FREQ_HZ` last arg; see [Quick USB lab scripts](#quick-usb-lab-scripts-testing) |
| `testing/board_status_wearable.py` | `STATUS` + `WEARABLE:ON`; optional post-sweep `STATUS` | Port; `--wait-ready SEC` |
| `testing/eis_sweep.py` | Auto-detect board, full sweep, save to Excel | Edit constants at top of file |
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

## Troubleshooting

### Wearable timing (Scenario 3)

If the board runs **automatic sweeps about every 5 minutes**, **`WEARABLE:ON`** is active with **`INTERVAL:300`** (300 seconds). Use serial commands **`STATUS`** / **`SHOW`** to confirm. Change interval with **`INTERVAL:N`** (seconds, minimum 10). On USB/bench the firmware default is **`WEARABLE:OFF`** unless you enable it (state is **volatile** until you change it or reflash).

### COM port “access denied” / upload fails (Windows)

| Symptom | What to do |
|--------|------------|
| **`PermissionError` / port busy** when running Python or **PlatformIO upload** | Another program holds the COM port: **VS Code/Cursor serial monitor**, **PlatformIO device monitor**, or a **stuck `python.exe`** (e.g. `scenario1_serial_sample.py` still running). **End that task** in Task Manager or: `Get-CimInstance Win32_Process -Filter "Name = 'python.exe'" \| Select ProcessId, CommandLine` in PowerShell, then `Stop-Process -Id <pid> -Force`. |
| **Script times out** waiting for CSV | A **full wearable sweep** may still be running; **`runSweep()`** blocks serial parsing. Use **`WEARABLE:OFF`** (or `scenario1_serial_sample.py`, which waits for idle) or wait until **`Ready for next measurement`** before sending **`MEASURE`**. |

### CSV shows `inf`, huge |Z|, or endless validation retries

| Symptom | Things to check |
|--------|------------------|
| **`inf` magnitude / phase** | **Open circuit**, loose clip, or probe on the wrong net. For a bench check, use a **known resistor** (e.g. **1 kΩ**) **only** between **CE0** and **SE0** (2-electrode path). |
| **Fit stuck at default Rct/Rs** | Poor data quality — fix **wiring** and **RCAL** / parameter estimates before trusting fitted values. |
| **Python capture misses rows** | Firmware prints the header **`Frequency(Hz),...`** (not only `Freq(Hz)`). Use current **`testing/eis_sweep.py`** or **`testing/scenario1_serial_sample.py`** which match the **`=== EIS DATA CSV ===` … `=== END CSV ===`** block. |

### Oscilloscope on the cell

- You should see a **roughly sinusoidal** AC waveform at the **current sweep frequency**; **fuzz** on top is normal (digital noise, probes, ground loops, retries when the firmware **re-measures** a point).
- The scope shows **voltage vs time**, not the **serial CSV stream**. When the sweep **steps** to another frequency, the **period** of the waveform **changes**.
- **~200 mV/div** can be appropriate for small excitation; use **shorter ground**, **bandwidth limit**, or **averaging** on the scope to see the sine more clearly. Prefer **differential** probing if **USB ground** causes artifacts.

### Cleaning data: physics-aware checks and simple filtering (no ML required)

**Machine learning is not required** to improve usability of EIS CSV exports. Use **hardware fixes first**, then **post-processing**:

1. **Load the numeric table**  
   Columns are typically: `Frequency(Hz)`, `Real(Ohm)`, `Imaginary(Ohm)`, `Magnitude(Ohm)`, `Phase(Degrees)` (between the firmware CSV markers).

2. **Physics-aware QC (flag or drop bad points)**  
   - Remove **`NaN` / `inf`** in any numeric column.  
   - Keep **`Frequency(Hz) > 0`**.  
   - **Consistency:** compare reported magnitude to \(\sqrt{Z'^2 + Z''^2}\); flag rows where relative error exceeds a tolerance (e.g. **10–20%**) after accounting for rounding.  
   - **Spikes (sorted by frequency):** flag adjacent points where **|Z|** jumps by an unrealistic ratio (e.g. **>5×**) with no smooth trend — often a bad gain point or glitch.  
   - **Optional bound:** if you know the cell is ~**1 kΩ**, flag **|Z| > ~10 MΩ** as likely open-lead.

3. **Simple filtering (after QC)**  
   - Along **sorted frequency**, apply a **moving median** (window 3) to **Magnitude** and **Phase** to suppress isolated outliers.  
   - **Best denoiser in the lab:** run **several sweeps** and take the **median or mean** of Real/Imag at each frequency.

4. **Plot**  
   Use **`testing/plot_eis_from_file.py`** on the cleaned CSV (or Excel) to inspect Nyquist and Bode.

**Optional:** ML may help **downstream tasks** (classification, fusion), not basic cleanup.

---

## EIS accuracy, limitations, and best practices

The firmware runs a **real** AD5940 EIS pipeline (excitation → TIA → DFT → ratiometric scaling vs **RCAL**). Reported **Z**, **Rct**, and **Rs** are only as trustworthy as **hardware, calibration, and assumptions** below.

### WatchScript-aligned acquisition (default firmware behavior)

Compared with an earlier capstone revision, the **EIS sweep path** is aligned with a **WatchScript-style** measurement engine:

- **`calHSTIA` table** in `src/main.cpp` maps frequency bands to **HSTIA RTIA** (40k → 10k → 5k → 1k → 200 Ω) with a top threshold at **200 kHz** so **1 Hz–200 kHz** sweeps remain valid.
- **`configureFrequency()`** → **`setHSTIAWithHysteresis()`** applies **10% boundary hysteresis** to reduce RTIA chatter; **`setHSTIA()`** uses **`AD5940_GetFreqParameters()`** for **per-frequency SINC2/SINC3 OSR and DFT length** (not a single fixed 16384-point setting during the sweep).
- After each point, **`logSweep()`** then **`configureFrequency(_currentFreq)`** runs so the **next** frequency’s gain and filters are set **before** the following RCAL/cell sequence (same ordering as WatchScript).
- **`runSweep()`** always calls **`AD5940_DFTMeasureWithAveraging(_numAverages)`** with default **`_numAverages = 3`**; only **validator-approved** samples contribute to the average. **`AD5940_DFTMeasure()`** (if used elsewhere) **retries** up to four times and stores **NaN** if all attempts fail validation.
- **Default `rcalVal` / `_rcalVal` is 1000 Ω** to match a **1 kΩ on-board RCAL** recommended for this RTIA schedule. **NVS** may still hold an older value (e.g. 100 Ω) after upgrade — use **`RCAL:`** or erase the **`helpstat`** namespace if the stored value no longer matches the populated resistor.

**Hardware note:** If you keep a **100 Ω** RCAL, you **must** set **`rcalVal`** to the **measured** resistance (serial / BLE / NVS). Replacing the part with **1 kΩ** (tight tolerance) is the configuration this default expects.

### 1. RCAL value and resistor tolerance

Impedance magnitude is **scaled ratiometrically** against the **known calibration resistor** (`rcalVal` in `MEASURE` / `SET`, **`RCAL:`** + NVS, and defaults in `src/main.cpp` and `testing/` scripts). A **±5% RCAL** can introduce **roughly that same systematic bias** in |Z| if **`rcalVal`** does not match the **true** ohms.

**Mitigations (highest impact / lowest cost first):**

- **Measure RCAL with a good bench DMM** (or 4-wire ohms if available) **before each deployment** or whenever you care about absolute accuracy; pass that number as **`rcalVal`** everywhere (serial, BLE, Python), or use the serial **`RCAL:`** command (e.g. **`RCAL:987.6`**) to update runtime and NVS.
- For new PCB spins, use a **tighter-tolerance RCAL** (e.g. **0.1%–1%** metal film) so the **nominal** value stays close to truth between calibrations.

Scripts default to **1000 Ω** to match the **recommended 1 kΩ RCAL**; change the value if your board differs.

### 2. RCAL vs HSTIA (RTIA) — two different roles

- **RCAL** sets the **reference** used in the **ratio** that converts DFT results to **ohms** for the unknown branch.
- **HSTIA RTIA** (internal transimpedance gain, **200 Ω–160 kΩ**) sets how **cell current** is converted to **voltage** at the ADC so the signal stays in a useful range.

They are **not** the same knob. The value of **RCAL** does **not** fix **RTIA** for the whole sweep — **RTIA** still follows the **`calHSTIA`** schedule.

**What this firmware does:** On each frequency step, **`configureFrequency()`** calls **`setHSTIAWithHysteresis()`**, which selects **RTIA** from the **`calHSTIA` table** in `src/main.cpp` (frequency thresholds at **0.51, 1.5, 20, 150, 400, 200000 Hz** mapped to **40k, 10k, 5k, 5k, 1k, 200 Ω** RTIA settings). It also switches **ADC rate / HP vs LP mode** around **80 kHz**, adjusts **CTIA** via **`optimalCtia()`**, and applies **10% hysteresis** at boundaries to avoid gain chatter (see **[docs/NEW_FEATURES.md](docs/NEW_FEATURES.md)**).

**Residual risk:** The schedule is **frequency-based**, not **|Z|-based**. If the **cell impedance** at a given frequency is very far from what that gain step expects, you can still see **saturation**, **poor SNR**, or **validation failures** (`inf`, retries) even when RTIA is “correct” for frequency. Tighten the cell / amplitude / estimates, or narrow the sweep band for difficult loads.

### 3. ADC rate, filters, and DFT coherence

The README **[AD5940 Datasheet-Driven Improvements](#ad5940-datasheet-driven-improvements)** table summarizes **ADC rate**, **SINC OSR**, **DFT delay**, and related fixes. During **EIS sweeps**, **`setHSTIA()`** applies **`AD5940_GetFreqParameters()`** so **DFT length and OSRs** track **excitation frequency** (the **Sequencer / `AD5940_TDD`** init path may still show fixed **16384**-style defaults until the first **`configureFrequency()`**). Treat ultra-wide **1 Hz–200 kHz** sweeps as **operationally validated in this project**, not as a guarantee of **minimum error** at every point without bench cross-checks (known R, RC network, or reference instrument).

### 4. SD card files

`saveDataEIS()` writes **timestamped** CSV files under the folder/file base set in `src/main.cpp` (and a **`#`** metadata line). Parsers should skip comment lines.

### 5. LMA fit (Rct, Rs) and initial guesses

**`rct_est`** and **`rs_est`** (parameters 10–11 in the 14-field `MEASURE` format) seed the **Levenberg–Marquardt** fit. Defaults (**127 kΩ / 150 Ω**) match **one** development context; for other electrolytes, fouling, or electrode areas they can be **wrong**. Poor seeds → **wrong local minima** or poor convergence while CSV magnitudes may still look “reasonable.” **Tune estimates** to your cell order-of-magnitude; use **Nyquist** and **known physics** to sanity-check fitted **Rct/Rs**.

### 6. Temperature

**`readInternalTemperature()`** in `HELPStat.cpp` reads the **AD5940 die** temperature with an **approximate** conversion — useful for **logging** or drift monitoring. The firmware does **not** currently **apply** a temperature correction to reported **Z** or to sweat conductivity. For comparability across sessions, **log temperature** (or ambient/skin proxy) and **avoid** comparing runs at very different thermal conditions without post-processing.

### Summary

The **largest practical systematic levers** are **`rcalVal` matching the real RCAL** (prefer **measured** ohms and, if possible, a **1 kΩ** part matched to the default), the **frequency/RTIA schedule**, and **multi-sample validation/averaging** per point. **RTIA is switched during the sweep**, but **extreme |Z| vs gain** can still break points. Treat **LMA outputs** as **model-dependent**; treat **temperature** as **uncorrected** in firmware today.

---

## Further Documentation

Detailed technical documentation is in the `docs/` folder:

- **[docs/MEASUREMENT_MODES.md](docs/MEASUREMENT_MODES.md)** — Complete command format, parameter descriptions, and all measurement mode details.
- **[docs/NEW_FEATURES.md](docs/NEW_FEATURES.md)** — Implemented improvements: data validation, gain hysteresis, averaging, temperature sensing, notch filter, auto-gain selection.
- **[docs/IMPROVEMENTS.md](docs/IMPROVEMENTS.md)** — Future improvement roadmap based on AD5940/AD5941 datasheet analysis (sequencer, FIFO, power management, etc.).

BLE companions: **`web/helpstat-app`** (desktop/Android Chrome), **`ios/HELPStatCompanion`** (native iPhone/iPad), and the legacy Android tree in **`Originallibraries/`**.

---

## License

MIT License

Copyright (c) 2024 Linnes Lab, Purdue University, West Lafayette, IN, USA

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
