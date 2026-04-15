# Wearable Potentiostat — EIS Sweat Sensor (Berkeley MEng Capstone)

Portable Electrochemical Impedance Spectroscopy (EIS) system for real-time sweat analysis. Built around an **ESP32-S3** MCU and **Analog Devices AD5940/AD5941** analog front-end (AFE). Two electrodes on a watch back-plate contact skin; the system sweeps **1 Hz – 200 kHz** and reports complex impedance over **USB serial**, **BLE**, and **SD card**.

---

## Contents

1. [Hardware](#hardware)
2. [Repository structure](#repository-structure)
3. [Getting started](#getting-started)
4. [Usage](#usage) — serial commands, **Command Prompt / PowerShell**, Python scripts, scenarios
5. [Frequency range and sweep duration](#frequency-range-and-sweep-duration)
6. [How EIS works on this board](#how-eis-works-on-this-board)
7. [Errors, fixes, and known issues](#errors-fixes-and-known-issues)
8. [Firmware features](#firmware-features)
9. [Companion apps](#companion-apps) — **PHEW web app**, iOS, Android; BLE setup and usage
10. [Pin configuration](#pin-configuration)
11. [License](#license)

---

## Hardware

| Component | Details |
|-----------|---------|
| **MCU** | ESP32-S3 (240 MHz, Wi-Fi + BLE 5.0) |
| **AFE** | AD5940 / AD5941 — waveform generator, DFT engine, programmable-gain TIA |
| **Electrodes** | 2-electrode (CE0 + SE0), mounted on Apple Watch back-plate for sweat contact |
| **SPI bus** | Shared: AFE, SD card, and (Akshay PCB) ST7789 display — each device has its own CS |
| **Storage** | MicroSD card slot (FAT32, SPI) |
| **Wireless** | BLE — web companion, iOS app, or legacy Android |
| **Power** | USB-C or 3.7 V LiPo |
| **RCAL** | On-board calibration resistor, default **10 kΩ** — used internally by the AFE for ratiometric impedance calculation (not the load) |

### Two board variants

| | HELPStat V2 (bench/dev) | New_EIS_PCB_Akshay (final wearable) |
|---|---|---|
| AFE CS | GPIO 38 (or 11 on Eagle V2) | GPIO 38 |
| AFE interrupt | Hardware (GPIO 9) | None — SPI DFT polling |
| SD CS | GPIO 21 | GPIO 40 |
| Button | GPIO 7 | GPIO 13 |
| LED | GPIO 6 | None (use display) |
| Display | None | ST7789 240×280 SPI (ER-TFT1.69-4) — see [Pin configuration](#pin-configuration) |
| PSRAM | No | **Firmware uses internal RAM only** (`BOARD_HAS_PSRAM=0` in `eis-pcb-akshay`; avoids boot/USB instability when PSRAM does not init) |
| PlatformIO env | `lab-board-cs38` | `eis-pcb-akshay` |

KiCad 9 design files for the final PCB are in `boards/New_EIS_PCB_Akshay/`.

---

## Repository structure

```
src/main.cpp                   Firmware entry point, serial commands, measurement loop
src/eis_board_ui.cpp           SPI display driver for New_EIS_PCB_Akshay
lib/HELPStatLib/               Core library (AD5940 driver, EIS sweep, BLE, SD, LMA fit)
  HELPStat.cpp/h               High-level sweep, BLE transmit, SD save
  ad5940.c/h                   Low-level AD5940 register driver
  ad594x.cpp                   MCU-side SPI, interrupt/poll, resource init
  Impedance.c/h                Impedance math helpers
  lma.cpp/h                    Levenberg-Marquardt curve fitting (Rct, Rs)
  constants.h                  Pin definitions, board-specific macros
  eis_board_ui.h               Display UI header (C-linkage)
lib/Eigen/                     Eigen math library (used by LMA)
boards/custom_s3.json          PlatformIO board definition
boards/New_EIS_PCB_Akshay/     KiCad project (schematic + 4-layer PCB)
docs/                          Technical docs (features, modes, improvements)
testing/                       Python scripts for serial capture, plotting
web/helpstat-app/              PHEW web companion app (Chrome/Edge, Web Bluetooth)
ios/HELPStatCompanion/         Native iOS BLE companion (SwiftUI)
Originallibraries/             Legacy HELPStat reference (Android app, original firmware, Eagle PCB)
platformio.ini                 Build configuration
```

---

## Getting started

### Prerequisites

- **VS Code + PlatformIO** extension (builds the firmware)
- **Python 3.8+** with `pip install -r requirements.txt` (for testing scripts)
- **USB-C cable** to the board

### Build and flash

1. Open this folder in VS Code. PlatformIO auto-detects `platformio.ini`.
2. Connect the board via USB-C.
3. Build and upload (from project root):

```bash
# HELPStat V2 / bench board (CS=38, SPI DFT polling — no AFE IRQ required):
python -m platformio run -e lab-board-cs38 -t upload --upload-port COM3

# New EIS PCB (Akshay) — display + SD CS=40:
python -m platformio run -e eis-pcb-akshay -t upload --upload-port COM3
```

Replace `COM3` with your actual port. Close any serial monitor before uploading.

4. Open a serial monitor at **115200 baud**.

> **Windows:** After the first flash, you may need to unplug and re-plug USB once so the ESP32-S3 exits the ROM bootloader.

---

## Usage

All serial traffic is **115200 baud**, commands end with a **newline** (`\n` or `\r\n`).

### Command summary

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
| `STOP` | Abort a running sweep immediately (also works mid-sweep) |
| `RCAL:ohms` | Set RCAL value and save to NVS (e.g. `RCAL:10000`) |
| `RCAL:SHOW` | Print stored and runtime RCAL values |
| `DISPLAYRANGE:min,max,val[,title]` | Update SPI display (Akshay PCB only) |

### 14-parameter `MEASURE` format

```
MEASURE:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude
```

| # | Parameter | Example | Notes |
|---|-----------|---------|-------|
| 1 | mode | 0 | 0=EIS, 1=CV, 2=IT, 3=CP, 4=DPV, 5=SWV, 6=OCP |
| 2 | startFreq | 200000 | Hz (often the high end of the sweep) |
| 3 | endFreq | 1 | Hz (often the low end) |
| 4 | numPoints | 10 | Points per decade (log sweep) |
| 5 | biasVolt | 0.0 | DC bias (V) |
| 6 | zeroVolt | 0.0 | Zero voltage (V) |
| 7 | rcalVal | 10000 | Calibration resistor (Ohms) — match your board's RCAL |
| 8 | extGain | 1 | Excitation buffer gain (1=x2) |
| 9 | dacGain | 1 | DAC gain (1=x1) |
| 10 | rct_est | 127000 | Estimated Rct (Ohms) — seed for LMA fit |
| 11 | rs_est | 150 | Estimated Rs (Ohms) — seed for LMA fit |
| 12 | numCycles | 0 | Repeat cycles |
| 13 | delaySecs | 0 | Delay between cycles (s) |
| 14 | amplitude | 200 | AC excitation (mV, 0-800) |

**Example — full-span sweep (200 kHz → 1 Hz, 5 pts/decade, 10 kΩ RCAL, 150 mV):**

```
MEASURE:0,200000,1,5,0.0,0.0,10000.0,1,1,127000.0,150.0,0,0,150
```

### Running an EIS sweep from Command Prompt or PowerShell (Windows)

Windows does not include a serial terminal in `cmd.exe`. Use **Python + pyserial** (same as the repo's test scripts).

1. **Install once:**

```bash
pip install pyserial
```

2. **Close anything else using the COM port** (VS Code Serial Monitor, another Python script, etc.).

3. **Send a command and print the response** — adjust `COM3` and timeouts as needed. Long sweeps can take **many minutes**; increase the inner `while` time limit if needed.

**Default firmware sample sweep (`MEASURE:SAMPLE`):**

```powershell
python -c "import serial,time; s=serial.Serial('COM3',115200,timeout=600); s.write(b'MEASURE:SAMPLE\r\n'); t=time.time();
while time.time()-t<900:
    line=s.readline()
    if line: print(line.decode(errors='replace').rstrip())"
```

**Custom 14-parameter sweep** (same example as the table above):

```powershell
python -c "import serial,time; s=serial.Serial('COM3',115200,timeout=600); s.write(b'MEASURE:0,200000,1,5,0.0,0.0,10000.0,1,1,127000.0,150.0,0,0,150\r\n'); t=time.time();
while time.time()-t<900:
    line=s.readline()
    if line: print(line.decode(errors='replace').rstrip())"
```

4. **Interactive alternative** — type commands by hand:

```bash
python -m platformio device monitor -p COM3 -b 115200
```

Then type `MEASURE:SAMPLE` or a full `MEASURE:0,...` line and press Enter.

### Python scripts in `testing/`

```bash
# Default 2-point sweep (10 kHz → 1 kHz):
python testing/scenario1_serial_sample.py COM3

# Single frequency at 10 kHz:
python testing/scenario1_serial_sample.py COM3 10000
```

### Three usage scenarios

1. **Laptop + USB (development)** — Flash, open serial (or Command Prompt as above), send `MEASURE:SAMPLE`, read CSV from the terminal or a script.
2. **Standalone (button + SD)** — Press button on GPIO 7 (bench) or GPIO 13 (Akshay). Results save to SD at `/eis/sweep_1Hz_200kHz.csv` (path/name set in firmware).
3. **Wearable (BLE, battery)** — Send `WEARABLE:ON` once. Board auto-sweeps at the configured interval. Data via BLE (web/iOS) and SD card.

---

## Frequency range and sweep duration

- **Practical sweep span in this firmware:** about **1 Hz – 200 kHz** (high end is what the code and gain schedule target; AD5941 family supports a similar band depending on clock and configuration).
- **Number of frequency points** (log sweep, `startFreq > endFreq`):

  `SweepPoints = (1.5 + (log10(startFreq) - log10(endFreq)) x numPoints) - 1`

  Example: 200 kHz → 1 Hz at **5 points/decade** = **27 points**; at **10 points/decade** = **54 points**.

- **Time per point** depends on frequency (settling + DFT). **Low Hz is slow** (several seconds per point is normal).

### Measured sweep timings (10 kOhm load resistor)

| Sweep | Points | Approx. duration | Notes |
|-------|--------|-------------------|-------|
| Single-point 200 kHz | 1 | ~2 s | Fastest; use small span trick (see below) |
| Single-point 200 Hz | 1 | ~3 s | |
| 200 kHz → 10 Hz, 5 pts/decade | 22 | ~30 s | Good for quick characterization |
| 200 kHz → 1 Hz, 5 pts/decade | 27 | **~108 s** | Full wearable range (`MEASURE:SAMPLE` default) |
| 200 kHz → 1 Hz, 10 pts/decade | 54 | ~4-6 min | High-resolution; pass `numPoints=10` in custom `MEASURE:` |

Low-frequency points (< 10 Hz) are dominated by DFT collection time inside the AD5940 — this is a hardware constraint. The 1 Hz point alone takes several seconds.

### Sweep speed tuning (firmware defaults)

The firmware balances speed and accuracy through these parameters:

| Parameter | Value | Location | Effect |
|-----------|-------|----------|--------|
| Settling periods | 2 cycles | `HELPStat.cpp settlingDelay()` | Fewer AC cycles before DFT capture |
| Averages per point | 1 | `HELPStat.h _numAverages` | Single validated measurement per frequency |
| Gain-switch delay | 50 ms | `HELPStat.cpp` / `runSweep()` | Reduced from 200 ms; stable with resistive loads |
| Default pts/decade | 5 | `main.cpp MEASURE:SAMPLE` | 27 points for full 1 Hz–200 kHz span |

These can be increased for noisier electrochemical loads (e.g., set `_numAverages = 3` for sweat sensors). The custom `MEASURE:` command accepts any `numPoints` value.

### Sample measurement results (10 kOhm load resistor across CE0-SE0)

| Test | Magnitude | Phase | Rct (fit) | Rs (fit) |
|------|-----------|-------|-----------|----------|
| 200 kHz single | ~335 Ohm | ~2.6 deg | — | — |
| 200 Hz single | ~332 Ohm | ~0.05 deg | — | — |
| 200 kHz → 10 Hz (22 pts) | 311-357 Ohm | -4 to +4 deg | 32.3 Ohm | 323.4 Ohm |
| 200 kHz → 1 Hz (27 pts) | 303-365 Ohm | -3.4 to +4.5 deg | -29.5 Ohm | 325.5 Ohm |

These values are from a bench test with a nominal 10 kOhm resistor (actual ~327 Ohm measured — the parallel combination of the 10 kOhm load with the AFE's internal RTIA/feedback path produces the observed lower magnitude).

### Single-frequency caveat

If **`startFreq` equals `endFreq`**, the sweep math yields **zero points** and the measurement hangs. For an effectively single frequency, use a **tiny span**, e.g. `200000` → `199000` with `numPoints=1`.

### Stopping a sweep mid-flight

You can abort a running sweep at any time. The firmware checks for abort requests between each frequency point.

| Method | How | Where |
|--------|-----|-------|
| **PHEW web app** | Click the red **Stop** button (replaces "Run sweep" during a sweep) | `web/helpstat-app/` |
| **Serial** | Send `STOP` (case-insensitive) over the serial connection | Any serial terminal, pyserial script |
| **BLE programmatic** | Write byte `2` to the `start` characteristic | Any BLE client using the GATT UUIDs |

On abort the firmware shuts down the AFE cleanly, prints any partial CSV data collected so far, and skips the Rct/Rs curve fit and BLE data transmission (partial data would produce unreliable fits). The board is immediately ready for the next command.

### Long sweeps + USB

On some PCs the ESP32-S3 **USB Serial/JTAG** link can **glitch or disconnect** during long runs. The firmware still **writes results to the SD card** when a card is present — check `/eis/` on the card if the serial stream stops early.

---

## How EIS works on this board

1. AD5940 waveform generator outputs a sine wave at the target frequency through **CE0** (counter electrode).
2. Current flows through the **load** (sweat between the two electrodes) and returns through **SE0** (sense electrode).
3. The **HSTIA** (transimpedance amplifier) converts the current to voltage. Gain resistor is auto-selected per frequency band.
4. The on-chip **DFT engine** extracts real and imaginary components.
5. Impedance is computed ratiometrically against **RCAL** (the known calibration resistor on the board).
6. The firmware sweeps all frequencies, outputting CSV with columns: `Frequency(Hz), Real(Ohm), Imaginary(Ohm), Magnitude(Ohm), Phase(Degrees)`.
7. A Levenberg-Marquardt fit estimates **Rct** (charge transfer resistance) and **Rs** (solution resistance).

### RCAL vs load — what each does

- **RCAL** (on-board, 10 kΩ default): An internal **reference** resistor. The AFE measures it at each frequency to calibrate its gain path. It is **not** the unknown under test. You do not wire CE0/SE0 to RCAL for normal EIS.
- **Load** (between CE0 and SE0): The **unknown impedance** you are measuring. For bench testing this is often a known resistor. For the wearable, this is **sweat/skin** between the watch electrodes.

If there is **no conductive path** between CE0 and SE0 (open circuit, dry skin, electrodes not touching), the DFT engine gets no valid signal and times out — producing `NaN` results.

---

## Errors, fixes, and known issues

### DFT poll timeout → NaN results

**Symptom:** `ERROR: DFT poll timeout (~30 s)` and CSV values are `nan`.

| Cause | Fix |
|-------|-----|
| **No load between CE0 and SE0** | Bench: place a **known resistor** across CE0-SE0. Wearable: moist contact / sweat path. |
| **Dry skin / no sweat** | Electrodes need a conductive medium; saline drop for testing if needed. |
| **Wrong AFE CS pin** | Match `CS` in `constants.h` to your PCB (38 for Akshay / this bench setup; 11 for HELPStat V2 Eagle). |
| **Missing interrupt wire** (HELPStat V2 only) | AFE GPIO0 → MCU GPIO9. Akshay PCB uses **SPI polling** instead (`AFE_USE_HARDWARE_IRQ_GPIO=0` in `lab-board-cs38` / Akshay path). |

### PSRAM / boot loop / USB drops right after boot (Akshay env)

**Symptom:** ROM prints `PSRAM ID read error` or serial **disconnects** mid-banner; `ClearCommError` / port errors in Python.

**Fix:** Build **`eis-pcb-akshay`** with **PSRAM disabled** in `platformio.ini` (`board_build.psram_type = disabled`, `-DBOARD_HAS_PSRAM=0`). The chip may report embedded PSRAM in esptool while the ROM/app init still fails; **internal RAM** is enough for this firmware.

### Display not showing anything (Akshay PCB — ST7789)

**Symptom:** Firmware prints `eis_board_ui: display ready (ST7789)` but the screen is dark.

| Cause | Fix |
|-------|-----|
| **Backlight not enabled** | GPIO 14 (ESP.PWM) drives Q3 N-FET gate to sink LED cathode. Firmware now drives GPIO 14 HIGH after display init. |
| **FPC cable not seated** | The 18-pin 0.5 mm FPC connector (J13) has a flip-top ZIF latch. Ensure the cable is fully inserted and the latch is **closed**. |
| **SPI bus contention** | Firmware deselects AD5941 (GPIO 38 HIGH) before display transactions. If issues persist, check solder joints on J13. |

**Display backlight circuit (from KiCad):**

```
+3V3 → R (0402 current limiter) → J13 pin 3 (LED anode)
J13 pin 2 (LED cathode, /K) → Q3 drain (IRLML6344 N-FET)
Q3 source → GND
Q3 gate ← GPIO 14 (ESP.PWM)
```

The display is a **ER-TFT1.69-4** (BuyDisplay), 1.69" 240x280 IPS TFT, ST7789V controller, 4-wire SPI. Datasheet: [ER-TFT1.69-4](https://www.buydisplay.com/download/manual/ER-TFT1.69-4_Datasheet.pdf), [ST7789V IC](https://www.buydisplay.com/download/ic/ST7789.pdf).

On boot, the firmware runs a diagnostic sequence: **3 backlight blinks** (GPIO 14 toggle), then fills **RED → GREEN → BLUE** (600 ms each), then displays **HELLO WORLD** in white text. If no blinks are visible, the issue is hardware (FPC cable, backlight circuit, or display module).

### COM port issues (Windows)

| Symptom | Fix |
|---------|-----|
| `PermissionError` / port busy | Another program holds the port. Close Serial Monitor and kill stuck Python processes. |
| Port disappears after flash | Unplug and re-plug USB once (common on ESP32-S3). |
| Upload fails | Close serial monitor before uploading. |

### Build errors

| Error | Fix |
|-------|-----|
| `pio not recognized` | Use `python -m platformio run ...` |
| `#define CS` clash with Adafruit | Do not include `constants.h` before Adafruit headers in display code — already handled in `eis_board_ui.cpp`. |
| `-DMOSI` / `-DMISO` / `-DSCK` build flags | Do **not** pass these on Arduino-ESP32 3.x — they clash with `pins_arduino.h`. Edit `constants.h` instead. |

### EIS data quality

| Issue | Explanation |
|-------|-------------|
| `inf` magnitude | Open circuit or loose electrode connection. |
| Jumps at certain frequencies | RTIA gain switching; hysteresis helps but extreme mismatches can still show artifacts. |
| Fit Rct/Rs stuck at defaults | Poor data; fix wiring and **RCAL** first, then tune `rct_est` / `rs_est`. |
| Slow sweep at low frequencies | Normal — more cycles at low Hz. |

---

## Firmware features

- **Data validation**: Bad DFT measurements (NaN, inf, out-of-range) are detected and retried (up to 4x) or recorded as NaN.
- **DFT poll timeout**: ~30 s timeout prevents infinite hangs on open circuit or stuck interrupt path.
- **Gain hysteresis**: 10% frequency margin on RTIA switching reduces oscillation at band boundaries.
- **Averaging**: Configurable per-point averaging (`_numAverages`, default 1; increase for noisy electrochemical loads).
- **Speed-optimized defaults**: Settling delay (2 AC periods), 50 ms gain-switch delays, 5 pts/decade — full 1 Hz–200 kHz sweep in ~108 s.
- **Sweep abort**: `STOP` serial command or BLE `start=2` write aborts a running sweep between frequency points. AFE shuts down cleanly; partial data is printed.
- **SPI DFT polling**: On PCBs without AFE→MCU interrupt (Akshay), DFTRDY is polled over SPI.
- **RCAL in NVS**: `RCAL:` stores calibration value in flash (survives reboot).
- **Wearable auto-sweep**: Timer-based sweeps for field use (`WEARABLE:ON`, `INTERVAL:N`).
- **Display UI** (Akshay): Live sweep progress on ST7789; `DISPLAYRANGE` serial command for custom display.
- **Display backlight** (Akshay): GPIO 14 PWM drives Q3 N-FET for backlight control; blink test on boot.
- **Temperature logging**: AD5940 die temperature readout (approximate).

See `docs/NEW_FEATURES.md` for more detail.

---

## Companion apps

**GitHub only stores source code.** Nothing is automatically "built" or hosted for you. You run each app locally (or deploy the web app to your own hosting). The firmware must be on the board and **BLE enabled** in the sketch for phone/browser connections to work.

| Platform | Location | Notes |
|----------|----------|-------|
| **PHEW Web App** (Chrome/Edge) | `web/helpstat-app/` | Static site + Web Bluetooth. Needs **HTTPS or localhost**. |
| **iOS** (native) | `ios/HELPStatCompanion/` | Open in **Xcode**, build to a **physical iPhone** (recommended) or Simulator (BLE is limited). |
| **Android** (legacy) | `Originallibraries/HELPStat-main 4/HELPStat/Software/App/` | Open in **Android Studio**, Gradle build. Not actively maintained. |

### Bluetooth: advertised name, pairing, and which devices can connect

- **Name you should see when scanning:** **`HELPStat`**. The firmware sets this in `BLEDevice::init("HELPStat")` in `lib/HELPStatLib/HELPStat.cpp`. The PHEW web app and iOS companion filter on that name (`web/helpstat-app/js/constants.js`, `ios/.../GattConstants.swift`). Some OS Bluetooth lists show only a generic "LE" device if the name packet is delayed; the companion apps still discover **`HELPStat`** once advertising is up.

- **Not like a Bluetooth speaker.** The board exposes **Bluetooth Low Energy (BLE)** with a **custom GATT service** (primary service UUID **`4fafc201-1fb5-459e-8fcc-c5c9c331914b`**, defined with characteristics in `lib/HELPStatLib/HELPStat.h`). There is no standard "pair and any app works" profile for EIS data.

- **Who can connect:** Any **phone, tablet, or PC** that runs **client software** implementing that GATT layout — this repo's **PHEW web app** (Chrome/Edge + Web Bluetooth), **iOS HELPStatCompanion**, or **legacy Android** app — or **your own** app using the same UUIDs. Generic system "Pair new device" flows are not sufficient by themselves.

- **Firmware:** `setup()` must call **`BLE_setup()`** (already the case in `src/main.cpp`). Without running firmware, nothing advertises as **`HELPStat`**.

### PHEW Web App (`web/helpstat-app/`)

**PHEW** (Portable Health & Electrochemical Wearable) is the browser-based companion for this potentiostat. It connects to the board over **Bluetooth Low Energy**, lets you configure and trigger EIS sweeps, and visualizes results with interactive charts — all from a single static webpage with no server or installation required beyond a local file server.

#### What the app does

| Tab | Function |
|-----|----------|
| **Live** | Connect to the board via BLE, optionally adjust sweep parameters, and click **Run sweep** to trigger an EIS measurement. Incoming data streams in real-time over BLE notifications. When the sweep completes (the firmware sends back Rct and Rs fit values), it is automatically saved and the view switches to History. |
| **History** | Browse all saved sweeps organized by **Pacific calendar day** (America/Los_Angeles timezone). Select a day and sweep to view **Nyquist** (Zreal vs Zimag), **Bode magnitude** (|Z| vs log frequency), and **Bode phase** (phase vs log frequency) plots rendered with Chart.js. Export any day's data as JSON, or import JSON from another device. |
| **About** | Platform compatibility notes and links back to this README. |

#### Data flow

```
Board (firmware)  ──BLE GATT notifications──►  PHEW (browser)
                                                  │
                            frequency, real, imag, phase, magnitude
                            arrive point-by-point; Rct/Rs arrive last
                                                  │
                                                  ▼
                                          IndexedDB (browser)
                                          grouped by Pacific day
                                                  │
                                        ┌─────────┴─────────┐
                                        ▼                   ▼
                                   Chart.js plots     Export JSON file
```

All data is stored **locally in your browser** (IndexedDB). Nothing is sent to any server. Clearing browser data removes saved sweeps — use **Export JSON** to keep a backup.

#### Default sweep parameters (match firmware)

The form pre-fills values that match the firmware's `MEASURE:SAMPLE` defaults:

| Parameter | Default value | Notes |
|-----------|---------------|-------|
| Rct estimate | 127,000 Ω | Seed for Levenberg-Marquardt fit |
| Rs estimate | 150 Ω | Seed for LMA fit |
| Start frequency | 200,000 Hz | High end of sweep |
| End frequency | 1 Hz | Low end of sweep |
| Points / decade | 5 | 27 total points for full span |
| Rcal | 10,000 Ω | Must match on-board calibration resistor |
| Amplitude | 200 mV | AC excitation (set in firmware, not in web form) |

You can override any field before clicking **Run sweep**. Leave a field blank to use the firmware's built-in default.

#### How to access the PHEW web app (step by step)

1. **Why not double-click `index.html`?**
   [Web Bluetooth](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API) requires a **secure context** — the browser must see **`https://`** or **`http://localhost`**. Opening the file directly as `file://` blocks Bluetooth access.

2. **Start a local web server.** Open a terminal in the repo root and run:

   ```bash
   cd web/helpstat-app
   python -m http.server 8080
   ```

   > **Windows (PowerShell):** Use the same command — Python's built-in server works on all platforms. If `python` is not recognized, try `python3` or install Python from [python.org](https://python.org).

3. **Open the app in Chrome or Edge** — go to: [http://localhost:8080](http://localhost:8080)

   You should see the **PHEW** header with Live / History / About tabs.

4. **Power on the board** (USB-C or battery). The firmware starts BLE advertising automatically (`BLE_setup()` runs in `setup()`).

5. **Connect:**
   - Click the **Connect** button on the Live tab.
   - A browser Bluetooth permission dialog appears — select **`HELPStat`** from the device list and click **Pair**.
   - The status line updates to "Connected: HELPStat".

6. **Run a sweep:**
   - (Optional) Expand **Sweep parameters** and adjust values, or leave defaults.
   - Click **Run sweep**. A progress bar appears showing point count, current frequency, and elapsed time.
   - To cancel mid-sweep, click the red **Stop** button (replaces "Run sweep" while running). The firmware aborts cleanly and partial data is discarded.
   - Wait for the sweep to complete (~30 s for 10 Hz floor, ~108 s for 1 Hz floor at 5 pts/decade).
   - When done, the app auto-saves the sweep and switches to the **History** tab showing Nyquist and Bode plots.

7. **Review and export:**
   - Use the **Day** and **Sweep** dropdowns to browse past measurements.
   - Click **Export JSON** to download the day's data as a `phew_YYYY-MM-DD.json` file.
   - Use the **Import** button to load JSON exported from another machine or session.

8. **When finished,** press Ctrl+C in the terminal to stop the local server.

#### Supported browsers and platforms

| Platform | Browser | BLE support |
|----------|---------|-------------|
| **Windows** | Chrome, Edge | Full (recommended) |
| **macOS** | Chrome, Edge | Full |
| **Linux** | Chrome | Full (may need `chrome://flags/#enable-web-bluetooth`) |
| **Android** | Chrome | Full |
| **iPhone / iPad** | Safari, Chrome | **No** — Apple does not expose Web Bluetooth. Use the native iOS app below, or use the History tab to import JSON from another device. |

#### PWA / home screen install

PHEW includes a `manifest.webmanifest` so browsers offer **"Add to Home Screen"** / **"Install App"**. This creates a standalone shortcut that opens full-screen. Offline use is limited to viewing previously saved data; BLE requires a network context.

### iOS app (`ios/HELPStatCompanion/`)

1. You need a **Mac** with **Xcode** (15+ as noted in the table; use a current Xcode that supports the project format).

2. Open **`ios/HELPStatCompanion/HELPStatCompanion.xcodeproj`** in Xcode.

3. Select a run destination:
   - **Physical iPhone** (USB or wireless debugging): best for real BLE to the wearable.
   - **Simulator**: CoreBluetooth behavior is limited; prefer a real device for end-to-end tests.

4. Press **Run**. The first time, you may need to set your **Team** under Signing & Capabilities so Xcode can sign the app.

5. Apple does **not** install apps from GitHub automatically; **TestFlight** or the **App Store** only happen if you create an Apple Developer distribution pipeline yourself.

### Android app (legacy)

1. Install **Android Studio** (current stable).

2. **Open** the Gradle project at
   `Originallibraries/HELPStat-main 4/HELPStat/Software/App/`
   (the folder that contains the root `build.gradle.kts`).

3. Let Gradle sync, connect a phone with **USB debugging**, or use an emulator (BLE on emulators is often unavailable or flaky).

4. **Run** the app module from Android Studio.

5. This tree is **legacy**; expect older SDK assumptions and possible migration work.

---

## Pin configuration

### HELPStat V2 / bench board

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

### New_EIS_PCB_Akshay — MCU, SD, AFE

| Signal | GPIO | Notes |
|--------|------|-------|
| MOSI | 35 | Shared SPI |
| MISO | 37 | Shared SPI |
| SCK | 36 | Shared SPI |
| CS (AFE) | 38 | AD5940 chip select |
| RESET | 10 | AD5940 reset |
| CS_SD | 40 | SD card chip select |
| BUTTON | 13 | SW1 push-button |

### New_EIS_PCB_Akshay — display (ST7789, ER-TFT1.69-4)

The display shares the **same SPI bus** (SCK, MOSI, MISO) as the AFE and SD; only **CS / DC / RST / BL** are display-specific.

| Display signal | GPIO | KiCad net | J13 FPC pin | Notes |
|----------------|------|-----------|-------------|-------|
| **TFT CS** | 39 | /CS.SCREEN | 8 | Display chip select |
| **TFT DC** | 3 | /DC | 7 | Data / command |
| **TFT RST** | 21 | /SCREEN.RST | 11 | Display hardware reset |
| **TFT BL** | 14 | /ESP.PWM | — (via Q3) | Backlight control — drives Q3 N-FET gate |
| **SCK** | 36 | /SCLK | 9 | Shared with AFE / SD |
| **MOSI** | 35 | /MOSI | 10 | Shared |
| **MISO** | 37 | /MISO | — | Shared (not used by display) |

**J13 FPC connector full pinout (18-pin, 0.5 mm pitch ZIF):**

| Pin | Net | Function |
|-----|-----|----------|
| 1 | GND | Ground |
| 2 | /K | LED cathode → Q3 drain (backlight) |
| 3 | +3V3 (via R) | LED anode (backlight, current-limited) |
| 4 | +3V3 | Display power |
| 5 | GND | Ground |
| 6 | GND | Ground |
| 7 | /DC | Data/Command (GPIO 3) |
| 8 | /CS.SCREEN | Chip select (GPIO 39) |
| 9 | /SCLK | SPI clock (GPIO 36) |
| 10 | /MOSI | SPI data (GPIO 35) |
| 11 | /SCREEN.RST | Reset (GPIO 21) |
| 12 | GND | Ground |
| 13 | /CTP_SCL | Touch I2C clock (capacitive touch, optional) |
| 14 | /CTP_SDA | Touch I2C data (capacitive touch, optional) |
| 15 | /CTP_RST | Touch reset (GPIO 47) |
| 16 | /CTP_INT | Touch interrupt (GPIO 48) |
| 17 | +3V3 | Touch power |
| 18 | GND | Ground |

Panel: **240x280** IPS TFT, ST7789V controller, 4-wire SPI (see `src/eis_board_ui.cpp`).

No dedicated AFE interrupt line; firmware uses SPI polling for DFT ready.

---

## License

MIT License — Copyright (c) 2024 Linnes Lab, Purdue University

See LICENSE file for full text.
