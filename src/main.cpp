/*
    AUTHORS: Kevin Alessandro Bautista, Shannon Riegle
    EMAIL: kbautis@purdue.edu, sdriegle@iu.edu

    DISCLAIMER: 
    Linnes Lab code, firmware, and software is released under the MIT License
    (http://opensource.org/licenses/MIT).
    
    The MIT License (MIT)
    
    Copyright (c) 2024 Linnes Lab, Purdue University, West Lafayette, IN, USA
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is furnished to do
    so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <Arduino.h>
#include "HELPStat.h"
#include <vector>
#include <string>
#include "esp_task_wdt.h"

// Forward declaration for MeasurementMode enum (defined in HELPStat.h)
// The enum is already included via HELPStat.h

/* Reference constants for gain values - taken from AD5941.h library by Analog Devices */
//#define HSTIARTIA_200               0     /**< HSTIA Internal RTIA resistor 200  */
//#define HSTIARTIA_1K                1     /**< HSTIA Internal RTIA resistor 1K   */
//#define HSTIARTIA_5K                2     /**< HSTIA Internal RTIA resistor 5K   */
//#define HSTIARTIA_10K               3     /**< HSTIA Internal RTIA resistor 10K  */
//#define HSTIARTIA_20K               4     /**< HSTIA Internal RTIA resistor 20K  */
//#define HSTIARTIA_40K               5     /**< HSTIA Internal RTIA resistor 40K  */
//#define HSTIARTIA_80K               6     /**< HSTIA Internal RTIA resistor 80K  */
//#define HSTIARTIA_160K              7     /**< HSTIA Internal RTIA resistor 160K */
//#define HSTIARTIA_OPEN              8     /**< Open internal resistor */

//#define EXCITBUFGAIN_2              0   /**< Excitation buffer gain is x2 */
//#define EXCITBUFGAIN_0P25           1   /**< Excitation buffer gain is x1/4 */

//#define HSDACGAIN_1                 0   /**< Gain is x1 */
//#define HSDACGAIN_0P2               1   /**< Gain is x1/5 */

/* Using I2C pins as GPIOs for buttons - Optional */
#define BUTTON 7
#define LEDPIN 6

// Forward declaration
void blinkLED(int cycles, bool state);

calHSTIA test[] = {          
  {0.51,    HSTIARTIA_40K},       
  {1.5,     HSTIARTIA_10K},
  {20,      HSTIARTIA_5K},
  {150,     HSTIARTIA_5K},
  {400,     HSTIARTIA_1K},
  {200000,  HSTIARTIA_200}  
};    
 
/* Variables for function inputs */
int gainSize = (int)sizeof(test) / sizeof(test[0]);

uint32_t numCycles = 0; 
uint32_t delaySecs = 0; 
uint32_t numPoints = 1; 

float startFreq = 200000; 
float endFreq = 1;
float biasVolt = 0.0; 
float zeroVolt = 0.0; 
float rcalVal = 1000; // Use the measured resistance of the chosen calibration resistor

int extGain = 1; 
int dacGain = 1; 

float rct_estimate = 127000;
float rs_estimate = 150;

HELPStat demo;

/* Initializing Analog Pins and Potentiostat pins */
// SD-card output location for EIS sweeps.
// saveDataEIS() writes to: /<folderName>/<fileName>.csv
String folderName = "eis";
String fileName = "sweep_1Hz_200kHz";

// Serial command parsing
String serialInput = "";
bool measurementRequested = false;

void parseSerialCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();
    
    if (command.length() == 0) return;
    
    // HELP command
    if (command == "HELP" || command == "?") {
      Serial.println("\n=== HELPStat Serial Commands ===");
      Serial.println("MEASURE - Start measurement with current/default parameters");
      Serial.println("MEASURE:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude");
      Serial.println("SET:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude");
      Serial.println("MEASURE:SAMPLE or MEASURE:DEFAULT - run built-in 1Hz-200kHz EIS sweep");
      Serial.println("\nMeasurement Modes:");
      Serial.println("  0 = EIS (Electrochemical Impedance Spectroscopy)");
      Serial.println("  1 = CV (Cyclic Voltammetry)");
      Serial.println("  2 = IT/CA (Chronoamperometry)");
      Serial.println("  3 = CP (Chronopotentiometry)");
      Serial.println("  4 = DPV (Differential Pulse Voltammetry)");
      Serial.println("  5 = SWV (Square Wave Voltammetry)");
      Serial.println("  6 = OCP (Open Circuit Potential)");
      Serial.println("\nParameters:");
      Serial.println("  mode: Measurement mode (0-6)");
      Serial.println("  startFreq: Start frequency (Hz) - for EIS only");
      Serial.println("  endFreq: End frequency (Hz) - for EIS only");
      Serial.println("  numPoints: Number of points per decade (EIS) or total points");
      Serial.println("  biasVolt: Bias voltage (V)");
      Serial.println("  zeroVolt: Zero voltage (V)");
      Serial.println("  rcalVal: Calibration resistor value (Ohms)");
      Serial.println("  extGain: External gain (1 or 2)");
      Serial.println("  dacGain: DAC gain (1 or 0.2)");
      Serial.println("  rct_est: Estimated Rct (Ohms)");
      Serial.println("  rs_est: Estimated Rs (Ohms)");
      Serial.println("  numCycles: Number of measurement cycles");
      Serial.println("  delaySecs: Delay between cycles (seconds)");
      Serial.println("  amplitude: Signal amplitude (mV, 0-800)");
      Serial.println("\nExamples:");
      Serial.println("  EIS: MEASURE:0,200000,1,10,0,0,1000,1,1,127000,150,0,0,200");
      Serial.println("  CV:  MEASURE:1,0,0,100,0.0,1.0,1000,1,1,0,0,1,0,50");
      Serial.println("SHOW - Display current settings");
      Serial.println("STATUS - Show measurement status");
      Serial.println("HELP - Show this help message");
      Serial.println("================================\n");
      return;
    }
    
    // STATUS command
    if (command == "STATUS") {
      Serial.println("\n=== STATUS ===");
      Serial.print("Measurement Requested: ");
      Serial.println(measurementRequested ? "YES" : "NO");
      Serial.println("================\n");
      return;
    }

    // CHECK command - verify AD5940 connectivity manually
    if (command == "CHECK") {
      uint32_t chipid = AD5940_ReadReg(REG_AFECON_CHIPID);
      Serial.print("AD5940 CHIPID read: 0x");
      Serial.println(chipid, HEX);
      if(chipid == 0x0000 || chipid == 0xFFFF) {
        Serial.println("-> WARNING: AD5940 not responding");
      } else {
        Serial.println("-> AD5940 appears connected");
      }
      return;
    }
    
    // SHOW command
    if (command == "SHOW") {
      demo.print_settings();
      return;
    }
    
    // MEASURE command (full parameter list)
    if (command.startsWith("MEASURE")) {
      if (command.length() > 8) {
        // Parse parameters from command
        String params = command.substring(8); // Skip "MEASURE:"
        int paramCount = 0;
        float paramsArray[14];  // Increased to 14 for mode + amplitude
        
        // Parse comma-separated values
        int startIdx = 0;
        for (int i = 0; i <= params.length(); i++) {
          if (i == params.length() || params.charAt(i) == ',') {
            if (paramCount < 14) {
              paramsArray[paramCount] = params.substring(startIdx, i).toFloat();
              paramCount++;
            }
            startIdx = i + 1;
          }
        }
        
        // Support both old format (12 params) and new format (14 params)
        if (paramCount == 12) {
          // Legacy format - default to EIS mode with 200mV amplitude
          demo.setParameters(
            MODE_EIS,           // mode (default EIS)
            paramsArray[0],      // startFreq
            paramsArray[1],      // endFreq
            (uint32_t)paramsArray[2],  // numPoints
            paramsArray[3],     // biasVolt
            paramsArray[4],     // zeroVolt
            paramsArray[5],     // rcalVal
            (int)paramsArray[6],  // extGain
            (int)paramsArray[7],  // dacGain
            paramsArray[8],     // rct_estimate
            paramsArray[9],     // rs_estimate
            (uint32_t)paramsArray[10], // numCycles
            (uint32_t)paramsArray[11], // delaySecs
            200.0                // amplitude (default 200mV)
          );
          Serial.println("Parameters set from command (legacy format, defaulting to EIS mode).");
        } else if (paramCount == 14) {
          // New format with mode and amplitude
          demo.setParameters(
            (int)paramsArray[0],  // mode
            paramsArray[1],       // startFreq
            paramsArray[2],       // endFreq
            (uint32_t)paramsArray[3],  // numPoints
            paramsArray[4],      // biasVolt
            paramsArray[5],      // zeroVolt
            paramsArray[6],      // rcalVal
            (int)paramsArray[7],  // extGain
            (int)paramsArray[8],  // dacGain
            paramsArray[9],      // rct_estimate
            paramsArray[10],      // rs_estimate
            (uint32_t)paramsArray[11], // numCycles
            (uint32_t)paramsArray[12], // delaySecs
            paramsArray[13]      // amplitude
          );
          Serial.println("Parameters set from command.");
        } else {
          Serial.println("ERROR: Invalid parameter count. Expected 12 (legacy) or 14 (new format) parameters.");
          Serial.println("New format: MEASURE:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude");
          return;
        }
      }
      // Trigger measurement
      measurementRequested = true;
      Serial.println("Measurement requested. Starting...");
      return;
    }

    // MEASURE:SAMPLE shortcut for a fixed 1 Hz – 40 MHz EIS sweep
    if (command == "MEASURE:SAMPLE" || command == "MEASURE:DEFAULT") {
      // configure a standard EIS sweep range with example defaults
      demo.setParameters(
        MODE_EIS,          // EIS mode
        200000.0,          // start frequency 200 kHz
        1.0,               // end frequency 1 Hz
        10,                // points per decade
        0.0,               // bias
        0.0,               // zero volt
        1000.0,            // rcal
        1,                 // extGain
        1,                 // dacGain
        127000.0,          // rct_est
        150.0,             // rs_est
        0,                 // numCycles
        0,                 // delaySecs
        200.0              // amplitude mV
      );
      measurementRequested = true;
      Serial.println("Default/sample EIS sweep requested (1Hz-200kHz)");
      return;
    }
    
    // SET command (set parameters without measuring)
    if (command.startsWith("SET:")) {
      String params = command.substring(4); // Skip "SET:"
      int paramCount = 0;
      float paramsArray[14];  // Increased to 14 for mode + amplitude
      
      int startIdx = 0;
      for (int i = 0; i <= params.length(); i++) {
        if (i == params.length() || params.charAt(i) == ',') {
          if (paramCount < 14) {
            paramsArray[paramCount] = params.substring(startIdx, i).toFloat();
            paramCount++;
          }
          startIdx = i + 1;
        }
      }
      
      // Support both old format (12 params) and new format (14 params)
      if (paramCount == 12) {
        // Legacy format - default to EIS mode with 200mV amplitude
        demo.setParameters(
          MODE_EIS,           // mode (default EIS)
          paramsArray[0],     // startFreq
          paramsArray[1],     // endFreq
          (uint32_t)paramsArray[2],  // numPoints
          paramsArray[3],     // biasVolt
          paramsArray[4],     // zeroVolt
          paramsArray[5],     // rcalVal
          (int)paramsArray[6],  // extGain
          (int)paramsArray[7],  // dacGain
          paramsArray[8],     // rct_estimate
          paramsArray[9],     // rs_estimate
          (uint32_t)paramsArray[10], // numCycles
          (uint32_t)paramsArray[11], // delaySecs
          200.0                // amplitude (default 200mV)
        );
        Serial.println("Parameters updated successfully (legacy format, defaulting to EIS mode).");
        demo.print_settings();
      } else if (paramCount == 14) {
        // New format with mode and amplitude
        demo.setParameters(
          (int)paramsArray[0],  // mode
          paramsArray[1],      // startFreq
          paramsArray[2],      // endFreq
          (uint32_t)paramsArray[3],  // numPoints
          paramsArray[4],      // biasVolt
          paramsArray[5],      // zeroVolt
          paramsArray[6],      // rcalVal
          (int)paramsArray[7],  // extGain
          (int)paramsArray[8],  // dacGain
          paramsArray[9],      // rct_estimate
          paramsArray[10],      // rs_estimate
          (uint32_t)paramsArray[11], // numCycles
          (uint32_t)paramsArray[12], // delaySecs
          paramsArray[13]      // amplitude
        );
        Serial.println("Parameters updated successfully.");
        demo.print_settings();
      } else {
        Serial.println("ERROR: Invalid parameter count. Expected 12 (legacy) or 14 (new format) parameters.");
      }
      return;
    }
    
    // Unknown command
    Serial.print("Unknown command: ");
    Serial.println(command);
    Serial.println("Type HELP for available commands.");
  }
}

void setup() {
  esp_task_wdt_deinit();

  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== HELPStat EIS Measurement System ===");
  Serial.println("Type HELP for available commands");
  Serial.println("=======================================\n");
  
  demo.BLE_setup();

  pinMode(LEDPIN, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  digitalWrite(LEDPIN, HIGH);
  
  demo.AD5940Start();

  {
    uint32_t chipid = AD5940_ReadReg(REG_AFECON_CHIPID);
    Serial.print("AD5940 connection check, CHIPID=0x");
    Serial.println(chipid, HEX);
    if(chipid == 0x0000 || chipid == 0xFFFF) {
      Serial.println("WARNING: AD5940 may not be connected or responding!");
    }
  }

  Serial.println("System ready! Type HELP for commands or MEASURE to start.");

  /* Main Testing Code - run these functions if you want to run without the loop */
//  demo.AD5940_TDD(100000, 0.1, 6, 0.0, 0.0, 9870, test, gainSize, 1, 1);
//  Serial.print("Calibration size: ");
//  Serial.println(gainSize);
  
  /* Testing Sweep - needs AD5940_TDD to be uncommented too */
//  demo.runSweep(0, 0);

  /* Testing SD card */
//  demo.printData(); `
//  demo.saveDataEIS("EIS data", "03-02-24-pc-ph-e2-5mM-kfecn-pbs-1Mtie");

  /* 
   *  Running Analog Devices Code for Impedance - adapted to use the gain array. 
   *  Note: this runs indefinitely. Comment out all the other demo functions 
   *  exceot AD5940_Start before running this. 
  */
  
  /* Start, Stop, Num points per decade, gain size, and gain array */
//    demo.AD5940_Main(100000, 0.1, 6, gainSize, test);

  /* Testing ADC Voltage Readings */

//  demo.AD5940_TDDNoise(0.0, 0.0); // Using default 1.11V reference
//  demo.AD5940_ADCMeasure();

    /* Noise Measurements*/
//    delay(2000);
//    demo.ADCNoiseTest();
//    demo.AD5940_HSTIARcal(HSTIARTIA_160K, 149700); 

    /* If you want to save noise data - optional but might contribute to noise */
//    demo.saveDataNoise("03-21-24", "5k-ohms");
}

void loop() {
  // Check for serial commands
  parseSerialCommand();
  
  
  // physical button can also trigger default/sample sweep
  if(digitalRead(BUTTON) == LOW && !measurementRequested) {
    // debounce simple
    delay(50);
    if(digitalRead(BUTTON) == LOW) {
      Serial.println("Button pressed; starting default sample EIS");
      demo.setParameters(
        MODE_EIS, 200000.0, 1.0, 10, 0.0, 0.0, 1000.0, 1, 1, 127000.0, 150.0, 0, 0, 200.0);
      measurementRequested = true;
    }
  }

  // Check if measurement was requested via serial
  if (measurementRequested) {
    measurementRequested = false;
    digitalWrite(LEDPIN, LOW);
    
    Serial.println("\n=== Starting Measurement ===");
    demo.print_settings();
    Serial.println("============================\n");
    
    // Get measurement mode and route to appropriate function
    MeasurementMode mode = demo.getMeasurementMode();
    
    switch(mode) {
      case MODE_EIS:
        // Configure AD5940 with current parameters
        demo.AD5940_TDD(test, gainSize);
        
        // Run the frequency sweep
        Serial.println("Running frequency sweep...");
        demo.runSweep();
        
        // Calculate equivalent circuit parameters
        Serial.println("\nCalculating Rct and Rs...");
        demo.calculateResistors();
        
        // Output results as CSV to serial monitor
        Serial.println("\n=== Measurement Complete ===");
        demo.printDataCSV();
        
        // Also transmit via BLE if connected
        demo.BLE_transmitResults();
        
        // Save to SD card (optional)
        demo.saveDataEIS();
        break;
        
      case MODE_CV:
        demo.runCV();
        break;
        
      case MODE_IT:
        demo.runIT();
        break;
        
      case MODE_CP:
        demo.runCP();
        break;
        
      case MODE_DPV:
        demo.runDPV();
        break;
        
      case MODE_SWV:
        demo.runSWV();
        break;
        
      case MODE_OCP:
        demo.runOCP();
        break;
        
      default:
        Serial.println("ERROR: Unknown measurement mode!");
        break;
    }
    
    // Reset LED
    delay(500);
    blinkLED(0, 1);
    Serial.println("\nReady for next measurement. Type MEASURE to run again.\n");
  }
  
  delay(10); // Small delay to prevent CPU spinning
}

void blinkLED(int cycles, bool state) {
  if(state) 
  {
    digitalWrite(LEDPIN, HIGH);
    delay(10);
  }
  else
  {
    for(int i = 0; i < cycles; i++)
    {
      digitalWrite(LEDPIN, HIGH); 
      delay(1000); 
      digitalWrite(LEDPIN, LOW);
      delay(1000);
    }
  }
}

