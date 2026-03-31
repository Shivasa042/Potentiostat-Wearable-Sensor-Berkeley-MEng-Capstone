# HELPStat Measurement Modes and Command Structure

## Overview

The HELPStat system supports multiple electrochemical measurement techniques using the AD5940/AD5941 analog front-end. All measurements can be controlled via serial commands with configurable signal amplitude.

## Command Format

### New Format (Recommended)
```
MEASURE:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude
```

### Legacy Format (Backward Compatible)
```
MEASURE:startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs
```
*Note: Legacy format defaults to EIS mode with 200mV amplitude*

## Measurement Modes

| Mode | Code | Description | Status |
|------|------|-------------|--------|
| **EIS** | 0 | Electrochemical Impedance Spectroscopy | ✅ Fully Implemented |
| **CV** | 1 | Cyclic Voltammetry | 🚧 Stub Implementation |
| **IT/CA** | 2 | Chronoamperometry (Current-Time) | 🚧 Stub Implementation |
| **CP** | 3 | Chronopotentiometry | 🚧 Stub Implementation |
| **DPV** | 4 | Differential Pulse Voltammetry | 🚧 Stub Implementation |
| **SWV** | 5 | Square Wave Voltammetry | 🚧 Stub Implementation |
| **OCP** | 6 | Open Circuit Potential | 🚧 Stub Implementation |

## Parameter Descriptions

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `mode` | int | 0-6 | Measurement mode (see table above) |
| `startFreq` | float | 0.1-200000 | Start frequency in Hz (EIS only) |
| `endFreq` | float | 0.1-200000 | End frequency in Hz (EIS only) |
| `numPoints` | uint32_t | 1-200 | Number of points per decade (EIS) or total points |
| `biasVolt` | float | -1.1 to +1.1 | Bias voltage in Volts |
| `zeroVolt` | float | 0.2 to 2.4 | Zero voltage in Volts |
| `rcalVal` | float | >0 | Calibration resistor value in Ohms |
| `extGain` | int | 1 or 2 | External buffer gain (1 = x2, 2 = x0.25) |
| `dacGain` | int | 1 or 0.2 | DAC gain (1 = x1, 0.2 = x0.2) |
| `rct_est` | float | >0 | Estimated charge transfer resistance in Ohms |
| `rs_est` | float | >0 | Estimated solution resistance in Ohms |
| `numCycles` | uint32_t | 0+ | Number of measurement cycles |
| `delaySecs` | uint32_t | 0+ | Delay between cycles in seconds |
| `amplitude` | float | 0-800 | Signal amplitude in millivolts (mV) |

## Command Examples

### EIS (Electrochemical Impedance Spectroscopy)
```
MEASURE:0,200000,10,5,0,0,100,1,1,127000,150,0,0,200
```
- Mode: 0 (EIS)
- Frequency range: 200kHz to 10Hz
- 5 points per decade
- No bias, 100Ω on-board RCAL (nominal; verify with a DMM)
- 200mV signal amplitude

### EIS with Custom Amplitude
```
MEASURE:0,200000,10,5,0,0,100,1,1,127000,150,0,0,100
```
- Same as above but with 100mV amplitude

### CV (Cyclic Voltammetry) - Stub
```
MEASURE:1,0,0,100,0.0,1.0,100,1,1,0,0,1,0,50
```
- Mode: 1 (CV)
- Note: CV implementation is currently a stub

### IT/CA (Chronoamperometry) - Stub
```
MEASURE:2,0,0,100,0.5,0,100,1,1,0,0,1,0,0
```
- Mode: 2 (IT/CA)
- Note: IT/CA implementation is currently a stub

### CP (Chronopotentiometry) - Stub
```
MEASURE:3,0,0,100,0,0,100,1,1,0,0,1,0,0
```
- Mode: 3 (CP)
- Note: CP implementation is currently a stub

### DPV (Differential Pulse Voltammetry) - Stub
```
MEASURE:4,0,0,100,0.0,1.0,100,1,1,0,0,1,0,25
```
- Mode: 4 (DPV)
- Note: DPV implementation is currently a stub

### SWV (Square Wave Voltammetry) - Stub
```
MEASURE:5,0,0,100,0.0,1.0,100,1,1,0,0,1,0,50
```
- Mode: 5 (SWV)
- Note: SWV implementation is currently a stub

### OCP (Open Circuit Potential) - Stub
```
MEASURE:6,0,0,100,0,0,100,1,1,0,0,10,0,0
```
- Mode: 6 (OCP)
- Note: OCP implementation is currently a stub

## SET Command

The `SET` command allows you to configure parameters without starting a measurement:

```
SET:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude
```

Example:
```
SET:0,200000,10,5,0,0,100,1,1,127000,150,0,0,200
```

After setting parameters, use `MEASURE` (without parameters) to start measurement with current settings.

## Other Commands

- `HELP` - Display command help
- `SHOW` - Display current settings
- `STATUS` - Show measurement status

## Signal Amplitude Control

The `amplitude` parameter controls the excitation signal amplitude in millivolts (mV). 

**Valid Range:** 0-800 mV

The amplitude is applied to the waveform generator (WG) and affects:
- EIS: Sine wave amplitude for impedance measurements
- CV: Peak-to-peak voltage of triangular sweep
- DPV/SWV: Pulse amplitude
- IT/CA/CP/OCP: Not applicable (DC measurements)

**Amplitude Calculation:**
The AD5940 HSDAC has a range of ±607 mV. With gain settings:
- Maximum theoretical: ~800 mV (with gain = 2)
- Recommended range: 10-600 mV for best linearity

## Implementation Status

### ✅ Fully Implemented
- **EIS Mode**: Complete implementation with frequency sweep, gain switching, calibration, and data output

### 🚧 Stub Implementations (Framework Ready)
The following modes have function stubs that print parameters but require full implementation:
- **CV**: Needs trapezoid waveform generator configuration for triangular sweep
- **IT/CA**: Needs LPDAC voltage control and HSTIA current measurement
- **CP**: Needs HSTIA current control and voltage measurement
- **DPV**: Needs trapezoid generator for staircase + pulse pattern
- **SWV**: Needs trapezoid generator for square wave pattern
- **OCP**: Needs WG disable and ADC voltage measurement

## Technical Notes

### AD5940/AD5941 Capabilities
Based on the AD5940/AD5941 datasheet:
- **Waveform Generator Types:**
  - `WGTYPE_SIN` (2): Sine wave (used for EIS)
  - `WGTYPE_TRAPZ` (3): Trapezoid (for CV, DPV, SWV)
  - `WGTYPE_MMR` (0): Direct DAC write (for custom waveforms)

- **Amplitude Control:**
  - Sine amplitude word: 0-2047 (maps to 0-800 mV)
  - Formula: `SinAmplitudeWord = (amplitude_mV / 800.0) * 2047`

- **Voltage Ranges:**
  - HSDAC output: ±607 mV
  - LPDAC VBIAS: 0.2V to 2.4V (12-bit)
  - LPDAC VZERO: 0.2V to 2.4V (6-bit)

### Gain Settings
- **External Buffer Gain (`extGain`):**
  - 1 = EXCITBUFGAIN_2 (gain = 2)
  - 2 = EXCITBUFGAIN_0P25 (gain = 0.25)

- **DAC Gain (`dacGain`):**
  - 1 = HSDACGAIN_1 (gain = 1)
  - 0.2 = HSDACGAIN_0P2 (gain = 0.2)

## Future Enhancements

1. **Complete CV Implementation:**
   - Configure trapezoid generator for triangular sweep
   - Implement voltage sweep with configurable scan rate
   - Add cycle control for multiple sweeps

2. **Complete IT/CA Implementation:**
   - Set constant voltage via LPDAC
   - Measure current over time via HSTIA
   - Output time-series data

3. **Complete CP Implementation:**
   - Control current via HSTIA gain/feedback
   - Measure voltage over time
   - Output time-series data

4. **Complete DPV Implementation:**
   - Generate staircase voltage via trapezoid generator
   - Add pulse modulation
   - Measure differential current

5. **Complete SWV Implementation:**
   - Generate square wave modulation
   - Combine with staircase voltage
   - Measure forward/reverse currents

6. **Complete OCP Implementation:**
   - Disable waveform generator
   - Measure open circuit voltage via ADC
   - Output time-series data

## References

- AD5940/AD5941 Datasheet (Rev. F)
- HELPStat Library Documentation
- Analog Devices Application Notes (AN-1557, AN-1563)

