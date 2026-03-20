# HELPStat Library Improvements Based on AD5940/AD5941 Datasheet

Based on the [AD5940/AD5941 datasheet](ad5940-5941%20(2).pdf), here are key improvements that can be made to the HELPStat library and software:

## 1. **Utilize Sequencer for Autonomous Measurements** ⭐ HIGH PRIORITY

**Current State**: The code uses polling-based measurements (`AD5940_DFTMeasure()` polls for results manually)

**Improvement**: Use the AD5940's built-in sequencer for autonomous, cycle-accurate measurements

**Benefits**:
- Reduces MCU workload (ESP32 can do other tasks)
- More precise timing control
- Lower power consumption
- Better measurement repeatability

**Implementation**:
```cpp
// Create sequencer-based measurement function
void HELPStat::AD5940_SequencerMeasure(void) {
    // Program sequencer with measurement sequence
    // Use SEQID_0 for RCAL measurement
    // Use SEQID_1 for Rz measurement
    // Trigger sequencer and read from FIFO
}
```

**Reference**: Datasheet Section "Sequencer" - 6 kB SRAM for preprogrammed sequences

---

## 2. **Use FIFO for Data Collection** ⭐ HIGH PRIORITY

**Current State**: FIFO is configured but not actively used - code polls DFT results directly

**Improvement**: Read measurement data from FIFO instead of polling registers

**Benefits**:
- Faster data collection
- Less CPU overhead
- Can buffer multiple measurements
- More reliable data transfer

**Implementation**:
```cpp
// Replace pollDFT() with FIFO-based reading
void HELPStat::readDFTFromFIFO(int32_t* pReal, int32_t* pImage) {
    uint32_t fifoCount = AD5940_FIFOGetCnt();
    if(fifoCount >= 2) {  // Real and Imaginary
        *pReal = AD5940_FIFORd();
        *pImage = AD5940_FIFORd();
    }
}
```

**Reference**: Datasheet Section "Sequencer and FIFO Registers" - 4kB FIFO available

---

## 3. **Implement ADC Calibration** ⭐ MEDIUM PRIORITY

**Current State**: Basic calibration exists but could be more comprehensive

**Improvement**: Use AD5940's built-in ADC calibration features

**Features Available**:
- Offset calibration
- Gain calibration  
- Reference voltage calibration
- Temperature compensation

**Implementation**:
```cpp
void HELPStat::performADCCalibration(void) {
    // Offset calibration
    AD5940_ADCCal(ADCCAL_OFFSET);
    
    // Gain calibration
    AD5940_ADCCal(ADCCAL_GAIN);
    
    // Store calibration values
    // Use in subsequent measurements
}
```

**Reference**: Datasheet Section "ADC Calibration" - Multiple calibration modes available

---

## 4. **Enhanced Power Management** ⭐ MEDIUM PRIORITY

**Current State**: Sleep mode is disabled (`SLPKEY_LOCK`) during measurements

**Improvement**: Use wake-up timer for periodic measurements with power cycling

**Benefits**:
- Lower power consumption for battery operation
- Can wake up periodically for measurements
- Maintain sensor bias during sleep

**Implementation**:
```cpp
void HELPStat::configureWakeupTimer(uint32_t period_ms) {
    WUPTCfg_Type wupt_cfg;
    wupt_cfg.WuptEn = bTRUE;
    wupt_cfg.WuptOrder[0] = SEQID_0;
    wupt_cfg.SeqxWakeupTime[0] = 4;
    wupt_cfg.SeqxSleepTime[0] = (period_ms * 32) - 4;
    AD5940_WUPTCfg(&wupt_cfg);
}
```

**Reference**: Datasheet Section "Sleep and Wake-Up Timer" - Autonomous periodic measurements

---

## 5. **ADC Statistics for Noise Analysis** ⭐ LOW PRIORITY

**Current State**: Basic noise measurements exist but don't use ADC statistics registers

**Improvement**: Use built-in ADC statistics registers for noise analysis

**Features**:
- Mean, variance, min, max calculations
- Automatic statistics collection
- Less CPU overhead

**Implementation**:
```cpp
void HELPStat::enableADCStatistics(void) {
    StatCfg_Type stat_cfg;
    stat_cfg.StatEnable = bTRUE;
    stat_cfg.StatSource = STATSRC_ADC;
    stat_cfg.StatNum = 1000;  // Number of samples
    AD5940_StatCfg(&stat_cfg);
}
```

**Reference**: Datasheet Section "ADC Statistics Registers"

---

## 6. **Digital Postprocessing Options** ⭐ LOW PRIORITY

**Current State**: Postprocessing done in software

**Improvement**: Use AD5940's digital postprocessing features

**Features**:
- Digital filtering options
- Additional signal conditioning
- Can reduce software processing

**Reference**: Datasheet Section "ADC Digital Postprocessing Registers"

---

## 7. **Temperature Compensation** ⭐ MEDIUM PRIORITY

**Current State**: Temperature sensor not used

**Improvement**: Use internal temperature sensor for compensation

**Benefits**:
- Compensate for temperature drift
- More accurate measurements
- Can log temperature with data

**Implementation**:
```cpp
float HELPStat::readInternalTemperature(void) {
    // Configure ADC to read temperature channel
    // Read and convert to Celsius
    return temperature;
}
```

**Reference**: Datasheet Section "Internal Temperature Sensor Channel"

---

## 8. **Better Gain Switching with Hysteresis** ⭐ HIGH PRIORITY

**Current State**: Gain switches at exact frequency boundaries, causing jumps

**Improvement**: Implement hysteresis in gain selection to prevent oscillation

**Implementation**:
```cpp
AD5940Err HELPStat::setHSTIAWithHysteresis(float freq, float lastFreq) {
    static int lastGainIndex = -1;
    int newGainIndex = findGainIndex(freq);
    
    // Only switch if crossing threshold with sufficient margin
    if(newGainIndex != lastGainIndex) {
        float threshold = _gainArr[newGainIndex].freq;
        // Add hysteresis: switch up at threshold, switch down at threshold - 10%
        if((freq > threshold && lastFreq <= threshold) || 
           (freq < threshold * 0.9 && lastFreq >= threshold * 0.9)) {
            setHSTIA(freq);
            delay(300);  // Extra settling time after switch
            lastGainIndex = newGainIndex;
        }
    }
}
```

---

## 9. **Interrupt-Driven Measurements** ⭐ MEDIUM PRIORITY

**Current State**: Polling for DFT ready flag

**Improvement**: Use interrupts for measurement completion

**Benefits**:
- More efficient CPU usage
- Precise timing
- Can handle multiple measurements asynchronously

**Implementation**:
```cpp
void HELPStat::setupDFTInterrupt(void) {
    AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DFTRDY, bTRUE);
    // Attach interrupt handler
}

void IRAM_ATTR HELPStat::DFTInterruptHandler(void) {
    // Read DFT results
    // Signal measurement complete
}
```

---

## 10. **Advanced Waveform Generator Features** ⭐ LOW PRIORITY

**Current State**: Basic sine wave generation

**Improvement**: Use advanced waveform features

**Features Available**:
- Trapezoid waveforms
- Custom waveform patterns
- Phase control
- Multiple frequency components

**Reference**: Datasheet Section "Waveform Generator"

---

## 11. **50/60 Hz Mains Rejection Filter** ⭐ MEDIUM PRIORITY

**Current State**: Notch filter bypassed (`BpNotch = bTRUE`)

**Improvement**: Enable 50/60 Hz rejection filter when needed

**Benefits**:
- Reduce power line interference
- Better low-frequency measurements
- Can be enabled/disabled based on frequency

**Implementation**:
```cpp
void HELPStat::configureNotchFilter(bool enable, bool is50Hz) {
    filter_cfg.BpNotch = !enable;
    if(enable) {
        // Configure for 50Hz or 60Hz rejection
    }
}
```

**Reference**: Datasheet Section "50 Hz/60 Hz Mains Rejection Filter"

---

## 12. **Multi-Electrode Configurations** ⭐ LOW PRIORITY

**Current State**: Fixed 3-electrode configuration

**Improvement**: Support 2-wire, 3-wire, and 4-wire measurements

**Benefits**:
- More flexible measurement options
- Can optimize for different applications
- Support various sensor types

**Reference**: Datasheet Section "Programmable Switch Matrix"

---

## 13. **Data Validation and Quality Checks** ⭐ HIGH PRIORITY

**Current State**: No validation of measurement quality

**Improvement**: Add measurement validation

**Checks to Add**:
- Saturation detection (HSTIA output range)
- Signal-to-noise ratio
- Phase consistency
- Magnitude reasonableness
- Outlier detection

**Implementation**:
```cpp
bool HELPStat::validateMeasurement(impStruct* eis) {
    // Check for saturation
    if(eis->magnitude > MAX_EXPECTED_IMPEDANCE) return false;
    
    // Check phase range
    if(abs(eis->phaseDeg) > 90) return false;
    
    // Check for negative real (usually indicates error)
    if(eis->real < 0 && abs(eis->real) > 100) return false;
    
    return true;
}
```

---

## 14. **Automatic Gain Selection** ⭐ HIGH PRIORITY

**Current State**: Manual gain array configuration

**Improvement**: Automatically select optimal gain based on measured impedance

**Implementation**:
```cpp
int HELPStat::autoSelectGain(float measuredImpedance) {
    // Select HSTIA resistor based on measured impedance
    // Prevent saturation while maximizing SNR
    if(measuredImpedance < 100) return HSTIARTIA_200;
    else if(measuredImpedance < 1000) return HSTIARTIA_1K;
    // ... etc
}
```

---

## 15. **Measurement Averaging** ⭐ MEDIUM PRIORITY

**Current State**: Single measurement per frequency point

**Improvement**: Use AD5940's averaging capabilities or software averaging

**Benefits**:
- Reduce noise
- More stable measurements
- Better repeatability

**Implementation**:
```cpp
void HELPStat::measureWithAveraging(int numAverages) {
    impStruct sum = {0};
    for(int i = 0; i < numAverages; i++) {
        AD5940_DFTMeasure();
        // Accumulate results
    }
    // Average results
}
```

---

## Priority Recommendations

### Immediate (Fix Current Issues):
1. ✅ **Better Gain Switching** - Already increased delay to 200ms
2. **Data Validation** - Detect and reject bad measurements
3. **Gain Selection Hysteresis** - Prevent oscillation at boundaries

### Short Term (Improve Performance):
4. **FIFO-Based Data Collection** - Faster, more reliable
5. **Interrupt-Driven Measurements** - Better CPU efficiency
6. **ADC Calibration** - Improve accuracy

### Long Term (Advanced Features):
7. **Sequencer-Based Measurements** - Autonomous operation
8. **Power Management** - Battery optimization
9. **Temperature Compensation** - Environmental stability

---

## References

- [AD5940/AD5941 Datasheet](ad5940-5941%20(2).pdf) - Analog Devices
- HELPStat Supplemental Information - Section on gain array configuration

