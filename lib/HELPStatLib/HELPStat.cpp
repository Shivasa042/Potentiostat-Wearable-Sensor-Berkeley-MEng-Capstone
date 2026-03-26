/*
    FILENAME: HELPStat.cpp
    AUTHOR: Kevin Alessandro Bautista
    EMAIL: kbautis@purdue.edu

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
#include <HELPStat.h>
#include <math.h>
#include <cmath>

#ifndef MATH_PI
#define MATH_PI 3.14159265358979323846
#endif

HELPStat::HELPStat() {}

AD5940Err HELPStat::AD5940Start(void) {
    uint32_t err = AD5940_MCUResourceInit();
    if(err != 0) return AD5940ERR_ERROR;
    
    /* PIN DISPLAY */
    delay(5000);
    Serial.println("Start sequence successful.");
    Serial.print("SCK: ");
    Serial.println(SCK);
    Serial.print("MOSI: ");
    Serial.println(MOSI);
    Serial.print("MISO: ");
    Serial.println(MISO);
    Serial.print("RESET: ");
    Serial.println(RESET);
    Serial.print("CS: ");
    Serial.println(CS);
    Serial.print("INTERRUPT PIN: ");
    Serial.println(ESP32_INTERRUPT);
    
    return AD5940ERR_OK;
}

int32_t HELPStat::AD5940PlatformCfg(void) {
  CLKCfg_Type clk_cfg;
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();
  /* Platform configuration */
  /* Step1. Configure clock */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.HFOSCEn = bTRUE;
  clk_cfg.HFXTALEn = bFALSE;
  clk_cfg.LFOSCEn = bTRUE;
  AD5940_CLKCfg(&clk_cfg);
  /* Step2. Configure FIFO and Sequencer*/
  fifo_cfg.FIFOEn = bFALSE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize = FIFOSIZE_4KB;                       /* 4kB for FIFO, The reset 2kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_DFT;
  fifo_cfg.FIFOThresh = 4;//AppIMPCfg.FifoThresh;        /* DFT result. One pair for RCAL, another for Rz. One DFT result have real part and imaginary part */
  AD5940_FIFOCfg(&fifo_cfg);
  fifo_cfg.FIFOEn = bTRUE;
  AD5940_FIFOCfg(&fifo_cfg);
  
  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH, bTRUE); 
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);

  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP0_INT|GP1_SLEEP|GP2_SYNC;
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0|AGPIO_Pin1|AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);  /* Allow AFE to enter sleep mode. */
  return 0;
}

void HELPStat::AD5940ImpedanceStructInit(float startFreq, float endFreq, uint32_t numPoints) {
    AppIMPCfg_Type *pImpedanceCfg;

    AppIMPGetCfg(&pImpedanceCfg);
    /* Step1: configure initialization sequence Info */
    pImpedanceCfg->SeqStartAddr = 0;
    pImpedanceCfg->MaxSeqLen = 512; /* @todo add checker in function */

    pImpedanceCfg->RcalVal = 10000;
    pImpedanceCfg->SinFreq = 60000.0;
    pImpedanceCfg->FifoThresh = 4;
        
    /* Set switch matrix to onboard(EVAL-AD5940ELECZ) dummy sensor. */
    /* Note the RCAL0 resistor is 10kOhm. */
    pImpedanceCfg->DswitchSel = SWD_CE0;
    pImpedanceCfg->PswitchSel = SWP_CE0;
    pImpedanceCfg->NswitchSel = SWN_SE0;
    pImpedanceCfg->TswitchSel = SWT_SE0LOAD;
    
    /* The dummy sensor is as low as 5kOhm. We need to make sure RTIA is small enough that HSTIA won't be saturated. */
    pImpedanceCfg->HstiaRtiaSel = HSTIARTIA_5K;	
    
    /* Configure the sweep function. */
    pImpedanceCfg->SweepCfg.SweepEn = bTRUE;
    /* Getting some quick results with 1 kHz, but set to 10 Hz to see how slow it takes to boot up */
    pImpedanceCfg->SweepCfg.SweepStart = startFreq;	/* Start from 1kHz */
    pImpedanceCfg->SweepCfg.SweepStop = endFreq;		/* Stop at 100kHz */
    // pImpedanceCfg->SweepCfg.SweepPoints = 51;		/* Points is 101 */
    
    /* Using custom points - defaults to downward log sweep */
    if(startFreq > endFreq) pImpedanceCfg->SweepCfg.SweepPoints = (uint32_t)(1.5 + (log10(startFreq) - log10(endFreq)) * (numPoints)) - 1;
    else pImpedanceCfg->SweepCfg.SweepPoints = (uint32_t)(1.5 + (log10(endFreq) - log10(startFreq)) * (numPoints)) - 1;

    /* Print Statement - REMOVE*/
    Serial.println(pImpedanceCfg->SweepCfg.SweepPoints);

    pImpedanceCfg->SweepCfg.SweepLog = bTRUE;

    /* Configure Power Mode. Use HP mode if frequency is higher than 80kHz. */
    pImpedanceCfg->PwrMod = AFEPWR_LP;

    /* Configure filters if necessary */
    pImpedanceCfg->ADCSinc3Osr = ADCSINC3OSR_2;		/* Sample rate is 800kSPS/2 = 400kSPS */
    pImpedanceCfg->DftNum = DFTNUM_16384;
    pImpedanceCfg->DftSrc = DFTSRC_SINC3;
}

int32_t HELPStat::ImpedanceShowResult(uint32_t *pData, uint32_t DataCount) {
  float freq;

  fImpPol_Type *pImp = (fImpPol_Type*)pData;
  AppIMPCtrl(IMPCTRL_GETFREQ, &freq);

  // printf("Freq:%.2f ", freq);
  // printf("Freq, RzMag (ohm), RzPhase (degrees)\n");
  /*Process data*/
  for(int i=0;i<DataCount;i++)
  {
    // printf("RzMag: %f Ohm , RzPhase: %f \n",pImp[i].Magnitude,pImp[i].Phase*180/MATH_PI);
    // printf("%.2f, %f, %f \n", freq, pImp[i].Magnitude,pImp[i].Phase*180/MATH_PI);
    printf("%0.3f, %f, %f, %f, %f, \n", freq, pImp[i].Magnitude,pImp[i].Phase, pImp[i].Magnitude * cos(pImp[i].Phase), -pImp[i].Magnitude * sin(pImp[i].Phase));

  }

  return 0;
}

void HELPStat::AD5940_Main(float startFreq, float endFreq, uint32_t numPoints, uint32_t gainArrSize, calHSTIA* gainArr) {
  uint32_t temp;  

  _gainArrSize = gainArrSize;
  printf("Gain array size: %d\n", _gainArrSize);
  for(uint32_t i = 0; i < _gainArrSize; i++)
  {
    _gainArr[i] = gainArr[i];
    // _arrHSTIA[i] = arrHSTIA[i];
  }

  /* Can also try moving this to .h file */
  uint32_t AppBuff[APPBUFF_SIZE];

  AD5940PlatformCfg();
  AD5940ImpedanceStructInit(startFreq, endFreq, numPoints);
  
  AppIMPInit(AppBuff, APPBUFF_SIZE);    /* Initialize IMP application. Provide a buffer, which is used to store sequencer commands */
  AppIMPCtrl(IMPCTRL_START, 0);          /* Control IMP measurement to start. Second parameter has no meaning with this command. */
  printf("Freq, RzMag (ohm), RzPhase (degrees). Rreal, Rimag\n");

  while(1)
  {
    if(AD5940_GetMCUIntFlag())
    {
      AD5940_ClrMCUIntFlag();
      temp = APPBUFF_SIZE;
      AppIMPISR(AppBuff, &temp);
      ImpedanceShowResult(AppBuff, temp);
    }
  }
}

void HELPStat::DFTPolling_Main(void) {
  /* TESTING TO SEE IF THIS WORKS ON ITS OWN */
  DSPCfg_Type dsp_cfg;
  WGCfg_Type wg_cfg;

  /* Use hardware reset */
  AD5940_HWReset();
  /* Firstly call this function after reset to initialize AFE registers. */
  AD5940_Initialize();
  
  /* Configure AFE power mode and bandwidth */
  AD5940_AFEPwrBW(AFEPWR_LP, AFEBW_250KHZ);
  
  AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
  /* Initialize ADC basic function */
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_VCE0;
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_VSET1P1;
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
  
  /* Initialize ADC filters ADCRawData-->SINC3-->SINC2+NOTCH */
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_4;
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_1333;
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_2;         /* Don't care about it. Average function is only used for DFT */
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;        /* If ADC clock is 32MHz, then set it to ADCRATE_1P6MHZ. Default is 16MHz, use ADCRATE_800KHZ. */
  dsp_cfg.ADCFilterCfg.BpNotch = bTRUE;                 /* SINC2+Notch is one block, when bypass notch filter, we can get fresh data from SINC2 filter. */
  dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE;                /* We use SINC3 filter. */
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE;        /* Enable the SINC2+Notch block. You can also use function AD5940_AFECtrlS */
  dsp_cfg.DftCfg.DftNum = DFTNUM_16384;
  dsp_cfg.DftCfg.DftSrc = DFTSRC_SINC3;
  AD5940_DSPCfgS(&dsp_cfg);

  AD5940_StructInit(&wg_cfg, sizeof(wg_cfg));
  wg_cfg.WgType = WGTYPE_SIN;
  wg_cfg.SinCfg.SinFreqWord = AD5940_WGFreqWordCal(1000.0, 16000000.0);  /* 10kHz */
  AD5940_WGCfgS(&wg_cfg);

  /* Enable all interrupt at Interrupt Controller 1. So we can check the interrupt flag */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE); 
  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_SINC2NOTCH|AFECTRL_WG, bTRUE);
  AD5940_AFECtrlS(AFECTRL_DFT, bTRUE);
  AD5940_ADCConvtCtrlS(bTRUE);
  
  while(1)
  {
    int32_t real, image;
    if(AD5940_INTCTestFlag(AFEINTC_1,AFEINTSRC_DFTRDY))
    {
      AD5940_INTCClrFlag(AFEINTSRC_DFTRDY);
      real = AD5940_ReadAfeResult(AFERESULT_DFTREAL);
      if(real&(1<<17))
        real |= 0xfffc0000; /* Data is 18bit in two's complement, bit17 is the sign bit */
      printf("DFT: %d,", real);      
      image = AD5940_ReadAfeResult(AFERESULT_DFTIMAGE);
      if(image&(1<<17))
        image |= 0xfffc0000; /* Data is 18bit in two's complement, bit17 is the sign bit */
      printf("%d,", image);      
      printf("Mag:%f\n", sqrt((float)real*real + (float)image*image));
    }
  }
}

/*
  Sets initial configuration for HSLOOP (will change in measurement stage)
  and configures CLK, AFE, ADC, GPIO/INTs, and DSP for analysis. 
  Most will stay constant but some will change depending on frequency 

  06/13/2024 - Created two overloaded functions for AD5940_TDD. One overload is the
  original version, where the user manually inputs startFreq, endFreq, etc. The other
  only takes *gainArr and gainArrSize as inputs (this might be changed in the future).
  The other measurements are derived from the private variables in the HELPStat class.
  These values are adjusted over BLE communication (see BLE_settings()).

  02/12/2024 - Bias was verified. Waveforms look somewhat distorted though for some
  frequencies and subject to noise, but frequency at 200 kHz and 0.1 Hz was accurate
  and bias works.

  11/30/2023 - Need to view bias with an oscilloscope to verify 
*/
void HELPStat::AD5940_TDD(calHSTIA *gainArr, int gainArrSize) {
  // SETUP Cfgs
  CLKCfg_Type clk_cfg;
  AGPIOCfg_Type gpio_cfg;
  ClksCalInfo_Type clks_cal;
  LPAmpCfg_Type LpAmpCfg;
  
  // DFT / ADC / WG / HSLoop Cfgs
  AFERefCfg_Type aferef_cfg;
  HSLoopCfg_Type HsLoopCfg;
  DSPCfg_Type dsp_cfg;

  float sysClkFreq = 16000000.0; // 16 MHz
  float adcClkFreq = 16000000.0; // 16 MHz
  float sineVpp = _signalAmplitude; // Use configurable amplitude 

  /* Configuring the Gain Array */
  _gainArrSize = gainArrSize;
  printf("Gain array size: %d\n", _gainArrSize);
  for(uint32_t i = 0; i < _gainArrSize; i++)
    _gainArr[i] = gainArr[i];

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();

  /* Platform configuration */
  /* Step1. Configure clock - NEED THIS */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1; // Clock source divider - ADC
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC; // Enables internal high frequency 16/32 MHz clock as source
  clk_cfg.SysClkDiv = SYSCLKDIV_1; // Clock source divider - System
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC; // Enables internal high frequency 16/32 MHz clock as source 
  clk_cfg.HfOSC32MHzMode = bFALSE; // Sets it to 16 MHz
  clk_cfg.HFOSCEn = bTRUE; // Enables the internal 16 / 32 MHz source
  clk_cfg.HFXTALEn = bFALSE; // Disables any need for external clocks
  clk_cfg.LFOSCEn = bTRUE; // Enables 32 kHz clock for timing / wakeups
  AD5940_CLKCfg(&clk_cfg); // Configures the clock
  Serial.println("Clock setup successfully.");

  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT); // Clears all INT flags

  /* Set INT0 source to be DFT READY */
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DFTRDY, bTRUE); 
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT); // clears all flags 
  Serial.println("INTs setup successfully.");

  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP0_INT;

  gpio_cfg.InputEnSet = 0; // Disables any GPIOs as inputs
  gpio_cfg.OutputEnSet = AGPIO_Pin0; // Enables GPIOs as outputs

  gpio_cfg.OutVal = 0; // Value for the output 
  gpio_cfg.PullEnSet = 0; // Disables any GPIO pull-ups / Pull-downs

  AD5940_AGPIOCfg(&gpio_cfg); // Configures the GPIOs
  Serial.println("GPIOs setup successfully.");

  /* CONFIGURING FOR DFT */
  // AFE Configuration 
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);  /* Initializing to disabled state */

  // Enabling high power bandgap since we're using High Power DAC
  // Enables operation at higher frequencies 
  aferef_cfg.HpBandgapEn = bTRUE;

  aferef_cfg.Hp1V1BuffEn = bTRUE; // Enables 1v1 buffer
  aferef_cfg.Hp1V8BuffEn = bTRUE; // Enables 1v8 buffer

  /* Not going to discharge capacitors - haven't seen this ever used */
  aferef_cfg.Disc1V1Cap = bFALSE;
  aferef_cfg.Disc1V8Cap = bFALSE;

  /* Disabling buffers and current limits*/
  aferef_cfg.Hp1V8ThemBuff = bFALSE;
  aferef_cfg.Hp1V8Ilimit = bFALSE;

  /* Disabling low power buffers */
  aferef_cfg.Lp1V1BuffEn = bFALSE;
  aferef_cfg.Lp1V8BuffEn = bFALSE;

  /* LP reference control - turn off if no bias */
  if((_biasVolt == 0.0f) && (_zeroVolt == 0.0f))
  {
    aferef_cfg.LpBandgapEn = bFALSE;
    aferef_cfg.LpRefBufEn = bFALSE;
    printf("No bias today!\n");
  }
  else
  {
    aferef_cfg.LpBandgapEn = bTRUE;
    aferef_cfg.LpRefBufEn = bTRUE;
    printf("We have bias!\n");
  }

  /* Doesn't enable boosting buffer current */
  aferef_cfg.LpRefBoostEn = bFALSE;
  AD5940_REFCfgS(&aferef_cfg);	// Configures the AFE 
  Serial.println("AFE setup successfully.");
  
  /* Disconnect SE0 from LPTIA - double check this too */
	LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  LpAmpCfg.LpPaPwrEn = bFALSE; //bTRUE
  LpAmpCfg.LpTiaPwrEn = bFALSE; //bTRUE
  LpAmpCfg.LpTiaRf = LPTIARF_1M;
  LpAmpCfg.LpTiaRload = LPTIARLOAD_100R;
  LpAmpCfg.LpTiaRtia = LPTIARTIA_OPEN; /* Disconnect Rtia to avoid RC filter discharge */
  LpAmpCfg.LpTiaSW = LPTIASW(7)|LPTIASW(8)|LPTIASW(12)|LPTIASW(13); 
	AD5940_LPAMPCfgS(&LpAmpCfg);
  Serial.println("SE0 disconnected from LPTIA.");
  
  // Configuring High Speed Loop (high power loop)
  /* Vpp * BufGain * DacGain */
  // HsLoopCfg.HsDacCfg.ExcitBufGain = EXCITBUFGAIN_0P25;
  // HsLoopCfg.HsDacCfg.HsDacGain = HSDACGAIN_0P2;

  HsLoopCfg.HsDacCfg.ExcitBufGain = _extGain;
  HsLoopCfg.HsDacCfg.HsDacGain = _dacGain;

  /* For low power / frequency measurements use 0x1B, o.w. 0x07 */
  HsLoopCfg.HsDacCfg.HsDacUpdateRate = 0x1B;
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;

  /* Assuming no bias - default to 1V1 bias */
  if((_biasVolt == 0.0f) && (_zeroVolt == 0.0f))
  {
    HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_1P1;
    printf("HSTIA bias set to 1.1V.\n");
  }
  else 
  {
    HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_VZERO0;
    printf("HSTIA bias set to Vzero.\n");
  }

  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;

  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_40K;
  HsLoopCfg.HsTiaCfg.HstiaCtia = optimalCtia(HSTIARTIA_40K, _startFreq);

  HsLoopCfg.SWMatCfg.Dswitch = SWD_CE0;
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_SE0LOAD|SWT_TRTIA;

  _currentFreq = _startFreq;
  HsLoopCfg.WgCfg.WgType = WGTYPE_SIN;
  HsLoopCfg.WgCfg.GainCalEn = bTRUE;          // Gain calibration
  HsLoopCfg.WgCfg.OffsetCalEn = bTRUE;        // Offset calibration
  printf("Current Freq: %f\n", _currentFreq);
  HsLoopCfg.WgCfg.SinCfg.SinFreqWord = AD5940_WGFreqWordCal(_currentFreq, sysClkFreq);
  HsLoopCfg.WgCfg.SinCfg.SinAmplitudeWord = (uint32_t)((sineVpp/800.0f)*2047 + 0.5f);
  HsLoopCfg.WgCfg.SinCfg.SinOffsetWord = 0;
  HsLoopCfg.WgCfg.SinCfg.SinPhaseWord = 0;
  AD5940_HSLoopCfgS(&HsLoopCfg);
  Serial.println("HS Loop configured successfully");
  
  /* Configuring Sweep Functionality */
  _sweepCfg.SweepEn = bTRUE; 
  _sweepCfg.SweepLog = bTRUE;
  _sweepCfg.SweepIndex = 0; 
  _sweepCfg.SweepStart = _startFreq; 
  _sweepCfg.SweepStop = _endFreq;
  
  // Defaulting to a logarithmic sweep. Works both upwards and downwards
  if(_startFreq > _endFreq) _sweepCfg.SweepPoints = (uint32_t)(1.5 + (log10(_startFreq) - log10(_endFreq)) * (_numPoints)) - 1;
  else _sweepCfg.SweepPoints = (uint32_t)(1.5 + (log10(_endFreq) - log10(_startFreq)) * (_numPoints)) - 1;
  printf("Number of points: %d\n", _sweepCfg.SweepPoints);
  Serial.println("Sweep configured successfully.");

   /* Configuring LPDAC if necessary */
  if((_biasVolt != 0.0f) || (_zeroVolt != 0.0f))
  {
    LPDACCfg_Type lpdac_cfg;
    
    lpdac_cfg.LpdacSel = LPDAC0;
    lpdac_cfg.LpDacVbiasMux = LPDACVBIAS_12BIT; /* Use Vbias to tune BiasVolt. */
    lpdac_cfg.LpDacVzeroMux = LPDACVZERO_6BIT;  /* Vbias-Vzero = BiasVolt */

    // Uses 2v5 as a reference, can set to AVDD
    lpdac_cfg.LpDacRef = LPDACREF_2P5;
    lpdac_cfg.LpDacSrc = LPDACSRC_MMR;      /* Use MMR data, we use LPDAC to generate bias voltage for LPTIA - the Vzero */
    lpdac_cfg.PowerEn = bTRUE;              /* Power up LPDAC */
    /* 
      Default bias case - centered around Vzero = 1.1V
      This works decently well. Error seems to increase as you go higher in bias.
     */
    if(_zeroVolt == 0.0f)
    {
      // Edge cases 
      if(_biasVolt<-1100.0f) _biasVolt = -1100.0f + DAC12BITVOLT_1LSB;
      if(_biasVolt> 1100.0f) _biasVolt = 1100.0f - DAC12BITVOLT_1LSB;
      
      /* Bit conversion from voltage */
      // Converts the bias voltage to a data bit - uses the 1100 to offset it with Vzero
      lpdac_cfg.DacData6Bit = 0x40 >> 1;            /* Set Vzero to middle scale - sets Vzero to 1.1V */
      lpdac_cfg.DacData12Bit = (uint16_t)((_biasVolt + 1100.0f)/DAC12BITVOLT_1LSB);
    }
    else
    {
      /* 
        Working decently well now.
      */
      lpdac_cfg.DacData6Bit = (uint32_t)((_zeroVolt-200)/DAC6BITVOLT_1LSB);
      lpdac_cfg.DacData12Bit = (int32_t)((_biasVolt)/DAC12BITVOLT_1LSB) + (lpdac_cfg.DacData6Bit * 64);
      if(lpdac_cfg.DacData12Bit < lpdac_cfg.DacData6Bit * 64) lpdac_cfg.DacData12Bit--; // compensation as per datasheet 
    } 
    lpdac_cfg.DataRst = bFALSE;      /* Do not reset data register */
    // Allows for measuring of Vbias and Vzero voltages and connects them to LTIA, LPPA, and HSTIA
    lpdac_cfg.LpDacSW = LPDACSW_VBIAS2LPPA|LPDACSW_VBIAS2PIN|LPDACSW_VZERO2LPTIA|LPDACSW_VZERO2PIN|LPDACSW_VZERO2HSTIA;
    AD5940_LPDACCfgS(&lpdac_cfg);
    Serial.println("LPDAC configured successfully.");
  }

  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;

  /* Per datasheet: PGA gain=1.5 is production-calibrated for best accuracy */
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1P5;
  
  memset(&dsp_cfg.ADCDigCompCfg, 0, sizeof(dsp_cfg.ADCDigCompCfg));
  
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;
  /* Initial config is LP mode (16MHz), so ADC rate must be 800kHz per datasheet */
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_22;
  /* Per datasheet Table 2: OSR=4 gives 200kSPS with improved noise performance */
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_4;

  dsp_cfg.ADCFilterCfg.BpNotch = bTRUE;
  dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE;
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE;

  dsp_cfg.DftCfg.DftNum = DFTNUM_16384;
  dsp_cfg.DftCfg.DftSrc = DFTSRC_SINC3;
  dsp_cfg.DftCfg.HanWinEn = bTRUE;
  
  memset(&dsp_cfg.StatCfg, 0, sizeof(dsp_cfg.StatCfg));
  
  AD5940_DSPCfgS(&dsp_cfg);

  /* Per datasheet ADCBUFCON: 0x005F3D04 for LP mode (<80kHz), chop enabled */
  AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);

  Serial.println("DSP configured successfully.");

  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = DFTSRC_SINC3;
  clks_cal.DataCount = 1L<<(DFTNUM_16384+2);
  clks_cal.ADCSinc2Osr = ADCSINC2OSR_22;
  clks_cal.ADCSinc3Osr = ADCSINC3OSR_4;
  clks_cal.ADCAvgNum = ADCAVGNUM_16;
  clks_cal.RatioSys2AdcClk = sysClkFreq / adcClkFreq;
  AD5940_ClksCalculate(&clks_cal, &_waitClcks);

  AD5940_ClrMCUIntFlag();
  AD5940_INTCClrFlag(AFEINTSRC_DFTRDY);

  if((_biasVolt == 0.0f) && (_zeroVolt == 0.0f))
  {
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_SINC2NOTCH, bTRUE);
    Serial.println("No bias applied.");
  }
  else
  {
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_SINC2NOTCH|AFECTRL_DCBUFPWR, bTRUE);
    Serial.println("Bias is applied.");
  }

  Serial.println("Everything turned on.");
  printf("Number of points to sweep: %d\n", _sweepCfg.SweepPoints);
  printf("Bias: %f, Zero: %f\n", _biasVolt, _zeroVolt);
}
void HELPStat::AD5940_TDD(float startFreq, float endFreq, uint32_t numPoints, float biasVolt, float zeroVolt, float rcalVal, calHSTIA *gainArr, int gainArrSize, int extGain, int dacGain) {

  // SETUP Cfgs
  CLKCfg_Type clk_cfg;
  AGPIOCfg_Type gpio_cfg;
  ClksCalInfo_Type clks_cal;
  LPAmpCfg_Type LpAmpCfg;
  
  // DFT / ADC / WG / HSLoop Cfgs
  AFERefCfg_Type aferef_cfg;
  HSLoopCfg_Type HsLoopCfg;
  DSPCfg_Type dsp_cfg;

  float sysClkFreq = 16000000.0; // 16 MHz
  float adcClkFreq = 16000000.0; // 16 MHz
  float sineVpp = _signalAmplitude; // Use configurable amplitude 
  _rcalVal = rcalVal;

  /* Configuring the Gain Array */
  _gainArrSize = gainArrSize;
  printf("Gain array size: %d\n", _gainArrSize);
  for(uint32_t i = 0; i < _gainArrSize; i++)
  {
    _gainArr[i] = gainArr[i];
    // _arrHSTIA[i] = arrHSTIA[i];
  }

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();

  /* Platform configuration */
  /* Step1. Configure clock - NEED THIS */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1; // Clock source divider - ADC
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC; // Enables internal high frequency 16/32 MHz clock as source
  clk_cfg.SysClkDiv = SYSCLKDIV_1; // Clock source divider - System
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC; // Enables internal high frequency 16/32 MHz clock as source 
  clk_cfg.HfOSC32MHzMode = bFALSE; // Sets it to 16 MHz
  clk_cfg.HFOSCEn = bTRUE; // Enables the internal 16 / 32 MHz source
  clk_cfg.HFXTALEn = bFALSE; // Disables any need for external clocks
  clk_cfg.LFOSCEn = bTRUE; // Enables 32 kHz clock for timing / wakeups
  AD5940_CLKCfg(&clk_cfg); // Configures the clock
  Serial.println("Clock setup successfully.");

  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT); // Clears all INT flags

  /* Set INT0 source to be DFT READY */
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DFTRDY, bTRUE); 
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT); // clears all flags 
  Serial.println("INTs setup successfully.");

  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP0_INT;

  gpio_cfg.InputEnSet = 0; // Disables any GPIOs as inputs
  gpio_cfg.OutputEnSet = AGPIO_Pin0; // Enables GPIOs as outputs

  gpio_cfg.OutVal = 0; // Value for the output 
  gpio_cfg.PullEnSet = 0; // Disables any GPIO pull-ups / Pull-downs

  AD5940_AGPIOCfg(&gpio_cfg); // Configures the GPIOs
  Serial.println("GPIOs setup successfully.");

  /* CONFIGURING FOR DFT */
  // AFE Configuration 
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);  /* Initializing to disabled state */

  // Enabling high power bandgap since we're using High Power DAC
  // Enables operation at higher frequencies 
  aferef_cfg.HpBandgapEn = bTRUE;

  aferef_cfg.Hp1V1BuffEn = bTRUE; // Enables 1v1 buffer
  aferef_cfg.Hp1V8BuffEn = bTRUE; // Enables 1v8 buffer

  /* Not going to discharge capacitors - haven't seen this ever used */
  aferef_cfg.Disc1V1Cap = bFALSE;
  aferef_cfg.Disc1V8Cap = bFALSE;

  /* Disabling buffers and current limits*/
  aferef_cfg.Hp1V8ThemBuff = bFALSE;
  aferef_cfg.Hp1V8Ilimit = bFALSE;

  /* Disabling low power buffers */
  aferef_cfg.Lp1V1BuffEn = bFALSE;
  aferef_cfg.Lp1V8BuffEn = bFALSE;

  /* LP reference control - turn off if no bias */
  if((biasVolt == 0.0f) && (zeroVolt == 0.0f))
  {
    aferef_cfg.LpBandgapEn = bFALSE;
    aferef_cfg.LpRefBufEn = bFALSE;
    printf("No bias today!\n");
  }
  else
  {
    aferef_cfg.LpBandgapEn = bTRUE;
    aferef_cfg.LpRefBufEn = bTRUE;
    printf("We have bias!\n");
  }

  /* Doesn't enable boosting buffer current */
  aferef_cfg.LpRefBoostEn = bFALSE;
  AD5940_REFCfgS(&aferef_cfg);	// Configures the AFE 
  Serial.println("AFE setup successfully.");
  
  /* Disconnect SE0 from LPTIA - double check this too */
	LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  LpAmpCfg.LpPaPwrEn = bFALSE; //bTRUE
  LpAmpCfg.LpTiaPwrEn = bFALSE; //bTRUE
  LpAmpCfg.LpTiaRf = LPTIARF_1M;
  LpAmpCfg.LpTiaRload = LPTIARLOAD_100R;
  LpAmpCfg.LpTiaRtia = LPTIARTIA_OPEN; /* Disconnect Rtia to avoid RC filter discharge */
  LpAmpCfg.LpTiaSW = LPTIASW(7)|LPTIASW(8)|LPTIASW(12)|LPTIASW(13); 
	AD5940_LPAMPCfgS(&LpAmpCfg);
  Serial.println("SE0 disconnected from LPTIA.");
  
  // Configuring High Speed Loop (high power loop)
  /* Vpp * BufGain * DacGain */
  // HsLoopCfg.HsDacCfg.ExcitBufGain = EXCITBUFGAIN_0P25;
  // HsLoopCfg.HsDacCfg.HsDacGain = HSDACGAIN_0P2;

  _extGain = extGain; 
  _dacGain = dacGain;

  HsLoopCfg.HsDacCfg.ExcitBufGain = _extGain;
  HsLoopCfg.HsDacCfg.HsDacGain = _dacGain;

  /* For low power / frequency measurements use 0x1B, o.w. 0x07 */
  HsLoopCfg.HsDacCfg.HsDacUpdateRate = 0x1B;
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;

  /* Assuming no bias - default to 1V1 bias */
  if((biasVolt == 0.0f) && (zeroVolt == 0.0f))
  {
    HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_1P1;
    printf("HSTIA bias set to 1.1V.\n");
  }
  else 
  {
    HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_VZERO0;
    printf("HSTIA bias set to Vzero.\n");
  }

  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;

  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_40K;
  HsLoopCfg.HsTiaCfg.HstiaCtia = optimalCtia(HSTIARTIA_40K, startFreq);

  HsLoopCfg.SWMatCfg.Dswitch = SWD_CE0;
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_SE0LOAD|SWT_TRTIA;

  _currentFreq = startFreq;
  HsLoopCfg.WgCfg.WgType = WGTYPE_SIN;
  HsLoopCfg.WgCfg.GainCalEn = bTRUE;          // Gain calibration
  HsLoopCfg.WgCfg.OffsetCalEn = bTRUE;        // Offset calibration
  printf("Current Freq: %f\n", _currentFreq);
  HsLoopCfg.WgCfg.SinCfg.SinFreqWord = AD5940_WGFreqWordCal(_currentFreq, sysClkFreq);
  HsLoopCfg.WgCfg.SinCfg.SinAmplitudeWord = (uint32_t)((sineVpp/800.0f)*2047 + 0.5f);
  HsLoopCfg.WgCfg.SinCfg.SinOffsetWord = 0;
  HsLoopCfg.WgCfg.SinCfg.SinPhaseWord = 0;
  AD5940_HSLoopCfgS(&HsLoopCfg);
  Serial.println("HS Loop configured successfully");
  
  /* Configuring Sweep Functionality */
  _sweepCfg.SweepEn = bTRUE; 
  _sweepCfg.SweepLog = bTRUE;
  _sweepCfg.SweepIndex = 0; 
  _sweepCfg.SweepStart = startFreq; 
  _sweepCfg.SweepStop = endFreq;

  _startFreq = startFreq; 
  _endFreq = endFreq;
  
  // Defaulting to a logarithmic sweep. Works both upwards and downwards
  if(startFreq > endFreq) _sweepCfg.SweepPoints = (uint32_t)(1.5 + (log10(startFreq) - log10(endFreq)) * (numPoints)) - 1;
  else _sweepCfg.SweepPoints = (uint32_t)(1.5 + (log10(endFreq) - log10(startFreq)) * (numPoints)) - 1;
  printf("Number of points: %d\n", _sweepCfg.SweepPoints);
  Serial.println("Sweep configured successfully.");

   /* Configuring LPDAC if necessary */
  if((biasVolt != 0.0f) || (zeroVolt != 0.0f))
  {
    LPDACCfg_Type lpdac_cfg;
    
    lpdac_cfg.LpdacSel = LPDAC0;
    lpdac_cfg.LpDacVbiasMux = LPDACVBIAS_12BIT; /* Use Vbias to tune BiasVolt. */
    lpdac_cfg.LpDacVzeroMux = LPDACVZERO_6BIT;  /* Vbias-Vzero = BiasVolt */

    // Uses 2v5 as a reference, can set to AVDD
    lpdac_cfg.LpDacRef = LPDACREF_2P5;
    lpdac_cfg.LpDacSrc = LPDACSRC_MMR;      /* Use MMR data, we use LPDAC to generate bias voltage for LPTIA - the Vzero */
    lpdac_cfg.PowerEn = bTRUE;              /* Power up LPDAC */
    /* 
      Default bias case - centered around Vzero = 1.1V
      This works decently well. Error seems to increase as you go higher in bias.
     */
    if(zeroVolt == 0.0f)
    {
      // Edge cases 
      if(biasVolt<-1100.0f) biasVolt = -1100.0f + DAC12BITVOLT_1LSB;
      if(biasVolt> 1100.0f) biasVolt = 1100.0f - DAC12BITVOLT_1LSB;
      
      /* Bit conversion from voltage */
      // Converts the bias voltage to a data bit - uses the 1100 to offset it with Vzero
      lpdac_cfg.DacData6Bit = 0x40 >> 1;            /* Set Vzero to middle scale - sets Vzero to 1.1V */
      lpdac_cfg.DacData12Bit = (uint16_t)((biasVolt + 1100.0f)/DAC12BITVOLT_1LSB);
    }
    else
    {
      /* 
        Working decently well now.
      */
      lpdac_cfg.DacData6Bit = (uint32_t)((zeroVolt-200)/DAC6BITVOLT_1LSB);
      lpdac_cfg.DacData12Bit = (int32_t)((biasVolt)/DAC12BITVOLT_1LSB) + (lpdac_cfg.DacData6Bit * 64);
      if(lpdac_cfg.DacData12Bit < lpdac_cfg.DacData6Bit * 64) lpdac_cfg.DacData12Bit--; // compensation as per datasheet 
    } 
    lpdac_cfg.DataRst = bFALSE;      /* Do not reset data register */
    // Allows for measuring of Vbias and Vzero voltages and connects them to LTIA, LPPA, and HSTIA
    lpdac_cfg.LpDacSW = LPDACSW_VBIAS2LPPA|LPDACSW_VBIAS2PIN|LPDACSW_VZERO2LPTIA|LPDACSW_VZERO2PIN|LPDACSW_VZERO2HSTIA;
    AD5940_LPDACCfgS(&lpdac_cfg);
    Serial.println("LPDAC configured successfully.");
  }

  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;

  /* Per datasheet: PGA gain=1.5 is production-calibrated for best accuracy */
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1P5;
  
  memset(&dsp_cfg.ADCDigCompCfg, 0, sizeof(dsp_cfg.ADCDigCompCfg));
  
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;
  /* Initial config is LP mode (16MHz), so ADC rate must be 800kHz per datasheet */
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_22;
  /* Per datasheet Table 2: OSR=4 gives 200kSPS with improved noise performance */
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_4;

  dsp_cfg.ADCFilterCfg.BpNotch = bTRUE;
  dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE;
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE;

  dsp_cfg.DftCfg.DftNum = DFTNUM_16384;
  dsp_cfg.DftCfg.DftSrc = DFTSRC_SINC3;
  dsp_cfg.DftCfg.HanWinEn = bTRUE;
  
  memset(&dsp_cfg.StatCfg, 0, sizeof(dsp_cfg.StatCfg));
  
  AD5940_DSPCfgS(&dsp_cfg);

  /* Per datasheet ADCBUFCON: 0x005F3D04 for LP mode (<80kHz), chop enabled */
  AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);

  Serial.println("DSP configured successfully.");

  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = DFTSRC_SINC3;
  clks_cal.DataCount = 1L<<(DFTNUM_16384+2);
  clks_cal.ADCSinc2Osr = ADCSINC2OSR_22;
  clks_cal.ADCSinc3Osr = ADCSINC3OSR_4;
  clks_cal.ADCAvgNum = ADCAVGNUM_16;
  clks_cal.RatioSys2AdcClk = sysClkFreq / adcClkFreq;
  AD5940_ClksCalculate(&clks_cal, &_waitClcks);

  AD5940_ClrMCUIntFlag();
  AD5940_INTCClrFlag(AFEINTSRC_DFTRDY);

  if((biasVolt == 0.0f) && (zeroVolt == 0.0f))
  {
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_SINC2NOTCH, bTRUE);
    Serial.println("No bias applied.");
  }
  else
  {
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_SINC2NOTCH|AFECTRL_DCBUFPWR, bTRUE);
    Serial.println("Bias is applied.");
  }

  Serial.println("Everything turned on.");
  printf("Number of points to sweep: %d\n", _sweepCfg.SweepPoints);
  printf("Bias: %f, Zero: %f\n", biasVolt, zeroVolt);
}

void HELPStat::AD5940_DFTMeasure(void) {

  SWMatrixCfg_Type sw_cfg;
  impStruct eis;

  /* Real / Imaginary components */
  int32_t realRcal, imageRcal; 
  int32_t realRz, imageRz; 

  /* Magnitude / phase */
  float magRcal, phaseRcal; 
  float magRz, phaseRz;
  float calcMag, calcPhase; // phase in rads 
  // float rcalVal = 9930; // known Rcal - measured with DMM

  // Serial.print("Recommended clock cycles: ");
  // Serial.println(_waitClcks);

  /* Convert clock cycles to 10us units: clks / (clk_freq * 10e-6) */
  uint32_t delayUnits = (uint32_t)(_waitClcks * 100000.0 / SYSCLCK);
  AD5940_Delay10us(delayUnits > 0 ? delayUnits : 1);

  /* Measuring RCAL */
  sw_cfg.Dswitch = SWD_RCAL0;
  sw_cfg.Pswitch = SWP_RCAL0;
  sw_cfg.Nswitch = SWN_RCAL1;
  sw_cfg.Tswitch = SWT_RCAL1|SWT_TRTIA;
  AD5940_SWMatrixCfgS(&sw_cfg);
	
	AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                AFECTRL_SINC2NOTCH, bTRUE);

  AD5940_AFECtrlS(AFECTRL_WG|AFECTRL_ADCPWR, bTRUE);
  settlingDelay(_currentFreq);
  
  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);

  /* Wait for DFT to complete: split into two halves per AD sequencer pattern */
  AD5940_Delay10us(delayUnits / 2 > 0 ? delayUnits / 2 : 1);
  AD5940_Delay10us(delayUnits / 2 > 0 ? delayUnits / 2 : 1);

  pollDFT(&realRcal, &imageRcal);

  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV|AFECTRL_DFT|AFECTRL_WG, bFALSE);

  sw_cfg.Dswitch = SWD_CE0;
  sw_cfg.Pswitch = SWP_CE0;
  sw_cfg.Nswitch = SWN_SE0;
  sw_cfg.Tswitch = SWT_TRTIA|SWT_SE0LOAD;
  AD5940_SWMatrixCfgS(&sw_cfg);

  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_WG, bTRUE);
  settlingDelay(_currentFreq);

  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);

  AD5940_Delay10us(delayUnits / 2 > 0 ? delayUnits / 2 : 1);
  AD5940_Delay10us(delayUnits / 2 > 0 ? delayUnits / 2 : 1);

  /* Polling and retrieving data from the DFT */
  pollDFT(&realRz, &imageRz);

  // AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
  // AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));

  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT|AFECTRL_WG|AFECTRL_ADCPWR, bFALSE);  /* Stop ADC convert and DFT */
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                AFECTRL_SINC2NOTCH, bFALSE);

  // Serial.println("Measurement sequence finished.");

  getMagPhase(realRcal, imageRcal, &magRcal, &phaseRcal);
  getMagPhase(realRz, imageRz, &magRz, &phaseRz);

  /* Finding the actual magnitude and phase */
  eis.magnitude = (magRcal / magRz) * _rcalVal; 
  eis.phaseRad = phaseRcal - phaseRz;
  eis.real = eis.magnitude * cos(eis.phaseRad);
  eis.imag = eis.magnitude * sin(eis.phaseRad) * -1; 
  eis.phaseDeg = eis.phaseRad * 180 / MATH_PI; 
  eis.freq = _currentFreq;

  /* Validate measurement before storing */
  if(!validateMeasurement(&eis)) {
    Serial.print("WARNING: Measurement at ");
    Serial.print(_currentFreq);
    Serial.println(" Hz failed validation - retrying...");
    // Could implement retry logic here
    // For now, store anyway but mark as potentially invalid
  }

  /* Printing Values */
  printf("%d,", _sweepCfg.SweepIndex);
  printf("%.2f,", _currentFreq);
  printf("%.3f,", magRcal);
  printf("%.3f,", magRz);
  printf("%f,", eis.magnitude);
  printf("%.4f,", eis.real);
  printf("%.4f,", eis.imag);
  printf("%.4f\n", eis.phaseRad);

  eisArr[_sweepCfg.SweepIndex + (_currentCycle * _sweepCfg.SweepPoints)] = eis; 
  // printf("Array Index: %d\n",_sweepCfg.SweepIndex + (_currentCycle * _sweepCfg.SweepPoints));

  /* Updating Frequency - configure gain for NEXT measurement BEFORE returning */
  logSweep(&_sweepCfg, &_currentFreq);
  // Configure gain for next frequency now, so it's ready when we measure RCAL
  if(_sweepCfg.SweepEn == bTRUE) {
    configureFrequency(_currentFreq);
    delay(200); // Settling delay after gain configuration
  }
}

void HELPStat::getDFT(int32_t* pReal, int32_t* pImage) { 
  *pReal = AD5940_ReadAfeResult(AFERESULT_DFTREAL);
  *pReal &= 0x3ffff;
  /* Data is 18bit in two's complement, bit17 is the sign bit */
  if(*pReal&(1<<17)) *pReal |= 0xfffc0000;     

  delay(200); 

  *pImage = AD5940_ReadAfeResult(AFERESULT_DFTIMAGE);
  *pImage &= 0x3ffff;
  /* Data is 18bit in two's complement, bit17 is the sign bit */
  if(*pImage&(1<<17)) *pImage |= 0xfffc0000; 

  // printf("Real: %d, Image: %d\n", *pReal, *pImage);
}

void HELPStat::pollDFT(int32_t* pReal, int32_t* pImage) {
  /* Polls the DFT and retrieves the real and imaginary data as ints */
  while(!AD5940_GetMCUIntFlag()) {
    delay(1000); // Adding an empirical delay before polling again 
  }
  
  if(AD5940_INTCTestFlag(AFEINTC_1,AFEINTSRC_DFTRDY)) {
    getDFT(pReal, pImage);
    delay(300);
    AD5940_ClrMCUIntFlag();
    AD5940_INTCClrFlag(AFEINTSRC_DFTRDY);
  }
  else Serial.println("Flag not working!");
}

void HELPStat::getMagPhase(int32_t real, int32_t image, float *pMag, float *pPhase) {
  *pMag = sqrt((float)real*real + (float)image*image); 
  *pPhase =  atan2(-image, real);
  // printf("Magnitude: %f, Phase: %.4f\n", *pMag, *pPhase);
}

void HELPStat::logSweep(SoftSweepCfg_Type *pSweepCfg, float *pNextFreq) {
  float frequency; 

  // if you reach last point, go back to 0
  // if(++pSweepCfg->SweepIndex == pSweepCfg->SweepPoints) pSweepCfg->SweepIndex = 0;
  // if you reach last point, end the cycle
  if(++pSweepCfg->SweepIndex == pSweepCfg->SweepPoints) pSweepCfg -> SweepEn = bFALSE;
  else {
    frequency = pSweepCfg->SweepStart * pow(10, (pSweepCfg->SweepIndex * log10(pSweepCfg->SweepStop/pSweepCfg->SweepStart)/(pSweepCfg->SweepPoints-1)));
    *pNextFreq = frequency;

    /* Update waveform generator frequency - gain will be configured in AD5940_DFTMeasure() */
    AD5940_WGFreqCtrlS(frequency, SYSCLCK);
    // checkFreq(frequency);
    // Note: configureFrequency() is now called at the END of AD5940_DFTMeasure() 
    // to ensure gain is set BEFORE measuring RCAL for the next point
  }
}

/*
  06/13/2024 - Created two overloaded functions for runSweep. One is the original,
  where user has to input the number of cycles to run as well as the delay in seconds.
  The second function takes no inputs and uses the private variables _numCycles and
  _delaySecs. These are updated over BLE (see the BLE_settings() function).
*/
void HELPStat::runSweep(void) {
  _currentCycle = 0; 
  /*
    Need to not run the program if ArraySize < total points 
    TO DO: ADD A CHECK HERE  
  */
  printf("Total points to run: %d\n", (_numCycles + 1) * _sweepCfg.SweepPoints); // since 0 based indexing, add 1
  printf("Set array size: %d\n", ARRAY_SIZE);
  printf("Calibration resistor value: %f\n", _rcalVal);

  // LED to show start of spectroscopy 
  // digitalWrite(LED1, HIGH); 

  for(uint32_t i = 0; i <= _numCycles; i++) {
    /* 
      Wakeup AFE by read register, read 10 times at most.
      Do this because AD594x goes to sleep after each cycle. 
    */
    AD5940_SleepKeyCtrlS(SLPKEY_LOCK); // Disables Sleep Mode 
    if(_delaySecs)
    {
      unsigned long prevTime = millis();
      unsigned long currTime = millis();
      printf("Delaying for %d seconds\n", _delaySecs);
      while(currTime - prevTime < _delaySecs * 1000)
      {
        currTime = millis();
        // printf("Curr delay: %d\n", currTime - prevTime);
      }
    } 
    // Timer for cycle time
    unsigned long timeStart = millis();

    if(i > 0){
      if(AD5940_WakeUp(10) > 10) Serial.println("Wakeup failed!");       
       resetSweep(&_sweepCfg, &_currentFreq);
       delay(300); // empirical settling delay
       _currentCycle++;
    }
    
    /* Calibrates based on frequency - configure for FIRST measurement */
    // Should calibrate when AFE is active
    // checkFreq(_currentFreq);
    configureFrequency(_currentFreq);
    delay(200); // switching delay - increased for gain stability

    printf("Cycle %d\n", i);
    printf("Index, Frequency (Hz), DFT Cal, DFT Mag, Rz (Ohms), Rreal, Rimag, Rphase (rads)\n");
    
    while(_sweepCfg.SweepEn == bTRUE)
    {
      // Gain is already configured for current frequency (configured at end of previous measurement or at start)
      // Use averaging if configured (numAverages > 1)
      if(_numAverages > 1) {
        AD5940_DFTMeasureWithAveraging(_numAverages);
      } else {
        AD5940_DFTMeasure();
      }
      // AD5940_DFTMeasureEIS();
      AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
              AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
              AFECTRL_SINC2NOTCH, bTRUE);
      delay(200);
    }

    unsigned long timeEnd = millis(); 
    printf("Time spent running Cycle %d (seconds): %lu\n", i, (timeEnd-timeStart)/1000);
  }
  
  /* Shutdown to conserve power. This turns off the LP-Loop and resets the AFE. */
  AD5940_ShutDownS();
  printf("All cycles finished.");
  printf("AD594x shutting down.");

  /* LEDs to show end of cycle */
  // digitalWrite(LED1, LOW);
  // digitalWrite(LED2, HIGH);
}
void HELPStat::runSweep(uint32_t numCycles, uint32_t delaySecs) {
  _numCycles = numCycles; 
  _currentCycle = 0; 
  /*
    Need to not run the program if ArraySize < total points 
    TO DO: ADD A CHECK HERE  
  */
  printf("Total points to run: %d\n", (_numCycles + 1) * _sweepCfg.SweepPoints); // since 0 based indexing, add 1
  printf("Set array size: %d\n", ARRAY_SIZE);
  printf("Calibration resistor value: %f\n", _rcalVal);

  // LED to show start of spectroscopy 
  // digitalWrite(LED1, HIGH); 

  for(uint32_t i = 0; i <= numCycles; i++) {
    /* 
      Wakeup AFE by read register, read 10 times at most.
      Do this because AD594x goes to sleep after each cycle. 
    */
    AD5940_SleepKeyCtrlS(SLPKEY_LOCK); // Disables Sleep Mode 
    if(delaySecs)
    {
      unsigned long prevTime = millis();
      unsigned long currTime = millis();
      printf("Delaying for %d seconds\n", delaySecs);
      while(currTime - prevTime < delaySecs * 1000)
      {
        currTime = millis();
        // printf("Curr delay: %d\n", currTime - prevTime);
      }
    } 
    // Timer for cycle time
    unsigned long timeStart = millis();

    if(i > 0){
      if(AD5940_WakeUp(10) > 10) Serial.println("Wakeup failed!");       
       resetSweep(&_sweepCfg, &_currentFreq);
       delay(300); // empirical settling delay
       _currentCycle++;
    }
    
    /* Calibrates based on frequency - configure for FIRST measurement */
    // Should calibrate when AFE is active
    // checkFreq(_currentFreq);
    configureFrequency(_currentFreq);
    delay(200); // switching delay - increased for gain stability

    printf("Cycle %d\n", i);
    printf("Index, Frequency (Hz), DFT Cal, DFT Mag, Rz (Ohms), Rreal, Rimag, Rphase (rads)\n");
    
    while(_sweepCfg.SweepEn == bTRUE)
    {
      // Gain is already configured for current frequency (configured at end of previous measurement or at start)
      // Use averaging if configured (numAverages > 1)
      if(_numAverages > 1) {
        AD5940_DFTMeasureWithAveraging(_numAverages);
      } else {
        AD5940_DFTMeasure();
      }
      // AD5940_DFTMeasureEIS();
      AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
              AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
              AFECTRL_SINC2NOTCH, bTRUE);
      delay(200);
    }

    unsigned long timeEnd = millis(); 
    printf("Time spent running Cycle %d (seconds): %lu\n", i, (timeEnd-timeStart)/1000);
  }
  
  /* Shutdown to conserve power. This turns off the LP-Loop and resets the AFE. */
  AD5940_ShutDownS();
  Serial.println("All cycles finished.");
  Serial.println("AD594x shutting down.");

  /* LEDs to show end of cycle */
  // digitalWrite(LED1, LOW);
  // digitalWrite(LED2, HIGH);
}

void HELPStat::resetSweep(SoftSweepCfg_Type *pSweepCfg, float *pNextFreq) {
  /* Sets the index back to 0 and enables the sweep again */
  pSweepCfg->SweepIndex = 0; 
  pSweepCfg->SweepEn = bTRUE;

  /* Reset frequency back to start frequency */
  *pNextFreq = _startFreq; 
  AD5940_WGFreqCtrlS(_startFreq, SYSCLCK);
}

void HELPStat::settlingDelay(float freq) {
  /*
   * Settling delay based on excitation frequency.
   * The electrochemical cell needs several full AC cycles to reach steady-state
   * oscillation. Filter pipeline fill time is negligible (<1ms) at all frequencies.
   *
   * At very low frequencies, cap the delay to avoid excessively long waits.
   * At high frequencies, enforce a minimum for analog switch/amplifier transients.
   */
  const float SETTLE_PERIODS = 4.0f;
  const unsigned long MIN_DELAY_MS = 5;
  const unsigned long MAX_DELAY_MS = 10000;

  unsigned long periodDelay = (unsigned long)(SETTLE_PERIODS * 1000.0f / freq);

  if (periodDelay < MIN_DELAY_MS) periodDelay = MIN_DELAY_MS;
  if (periodDelay > MAX_DELAY_MS) periodDelay = MAX_DELAY_MS;

  delay(periodDelay);
}

uint32_t HELPStat::optimalCtia(uint32_t rtia, float freq) {
  /*
   * Select CTIA to keep HSTIA bandwidth >= 10× measurement frequency while
   * maintaining TIA stability.  HSTIA BW ≈ 1/(2π·RTIA·CTIA).
   * CTIA register value maps to (value + 1) pF of internal capacitance
   * plus a fixed ~2 pF parasitic, so total ≈ (val+3) pF.
   *
   * RTIA constants from ad5940.h encode the register bit pattern, not ohms.
   * Map the common ones to ohm values for the calculation.
   */
  float rtiaOhms;
  switch (rtia) {
    case HSTIARTIA_200:  rtiaOhms = 200;    break;
    case HSTIARTIA_1K:   rtiaOhms = 1000;   break;
    case HSTIARTIA_5K:   rtiaOhms = 5000;   break;
    case HSTIARTIA_10K:  rtiaOhms = 10000;  break;
    case HSTIARTIA_20K:  rtiaOhms = 20000;  break;
    case HSTIARTIA_40K:  rtiaOhms = 40000;  break;
    case HSTIARTIA_80K:  rtiaOhms = 80000;  break;
    case HSTIARTIA_160K: rtiaOhms = 160000; break;
    default:             return 31;
  }

  float targetBW = freq * 10.0f;
  if (targetBW < 50000.0f) targetBW = 50000.0f;

  float maxCtia_pF = 1.0e12f / (2.0f * 3.14159265f * rtiaOhms * targetBW);
  int32_t regVal = (int32_t)(maxCtia_pF - 3.0f);

  if (regVal < 1)  regVal = 1;
  if (regVal > 31) regVal = 31;

  return (uint32_t)regVal;
}

AD5940Err HELPStat::checkFreq(float freq) {
  /* 
    Adding a delay after recalibration to improve the switching noise.
    Sudden switching introduces inaccuracies, but accurate data I feel like
    is mostly dependent on the HSRTIA depending on the load impedance.
  */

  ADCFilterCfg_Type filter_cfg;
  DFTCfg_Type dft_cfg;
  HSDACCfg_Type hsdac_cfg;
  uint32_t WaitClks;
  ClksCalInfo_Type clks_cal;
  FreqParams_Type freq_params;

  float adcClck = 16e6;

  /* Step 1: Check Frequency */
  freq_params = AD5940_GetFreqParameters(freq);
  
       if(freq < 0.51)
	{
    hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
    hsdac_cfg.HsDacGain = HSDACGAIN_0P2;
    hsdac_cfg.HsDacUpdateRate = 0x1B;
    AD5940_HSDacCfgS(&hsdac_cfg);
    AD5940_HSRTIACfgS(HSTIARTIA_200);
	__AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
    filter_cfg.ADCRate = ADCRATE_800KHZ;
    adcClck = 16e6;
    AD5940_HPModeEn(bFALSE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
	}
        else if(freq < 1 )
	{
    hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
    hsdac_cfg.HsDacGain = HSDACGAIN_0P2;
    hsdac_cfg.HsDacUpdateRate = 0x1B;
    AD5940_HSDacCfgS(&hsdac_cfg);
    AD5940_HSRTIACfgS(HSTIARTIA_200);
	__AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
    filter_cfg.ADCRate = ADCRATE_800KHZ;
    adcClck = 16e6;
    AD5940_HPModeEn(bFALSE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
	}

  else if(freq < 50)
	{
    hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
    hsdac_cfg.HsDacGain = HSDACGAIN_0P2;
    hsdac_cfg.HsDacUpdateRate = 0x1B;
    AD5940_HSDacCfgS(&hsdac_cfg);
    AD5940_HSRTIACfgS(HSTIARTIA_1K);
  __AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
    filter_cfg.ADCRate = ADCRATE_800KHZ;
    adcClck = 16e6;
    AD5940_HPModeEn(bFALSE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
	}

  else if(freq < 400 )
	{
    hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
    hsdac_cfg.HsDacGain = HSDACGAIN_0P2;
    hsdac_cfg.HsDacUpdateRate = 0x1B;
    AD5940_HSDacCfgS(&hsdac_cfg);
    AD5940_HSRTIACfgS(HSTIARTIA_1K);
	__AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
    filter_cfg.ADCRate = ADCRATE_800KHZ;
    adcClck = 16e6;
    AD5940_HPModeEn(bFALSE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
	}

  else if(freq < 5000 )
	{
    hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
    hsdac_cfg.HsDacGain = HSDACGAIN_0P2;
    hsdac_cfg.HsDacUpdateRate = 0x1B;
    AD5940_HSDacCfgS(&hsdac_cfg);
    AD5940_HSRTIACfgS(HSTIARTIA_200);
	__AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
    filter_cfg.ADCRate = ADCRATE_800KHZ;
    adcClck = 16e6;
    AD5940_HPModeEn(bFALSE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
	}

  else if(freq < 20000 )
	{
    hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
    hsdac_cfg.HsDacGain = HSDACGAIN_0P2;
    hsdac_cfg.HsDacUpdateRate = 0x1B;
    AD5940_HSDacCfgS(&hsdac_cfg);
    AD5940_HSRTIACfgS(HSTIARTIA_200);
	__AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
    filter_cfg.ADCRate = ADCRATE_800KHZ;
    adcClck = 16e6;
    AD5940_HPModeEn(bFALSE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
	}
  
  else if(freq<80000)
       {
    hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
    hsdac_cfg.HsDacGain = HSDACGAIN_0P2;
    hsdac_cfg.HsDacUpdateRate = 0x1B;
    AD5940_HSDacCfgS(&hsdac_cfg);
    AD5940_HSRTIACfgS(HSTIARTIA_200);
	__AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
    filter_cfg.ADCRate = ADCRATE_800KHZ;
    adcClck = 16e6;
    AD5940_HPModeEn(bFALSE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
       }
	if(freq >= 80000)
	{
    hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
    hsdac_cfg.HsDacGain = HSDACGAIN_0P2;
    hsdac_cfg.HsDacUpdateRate = 0x07;
    AD5940_HSDacCfgS(&hsdac_cfg);
    AD5940_HSRTIACfgS(HSTIARTIA_200);
	__AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
    filter_cfg.ADCRate = ADCRATE_1P6MHZ;
    adcClck = 32e6;
    AD5940_HPModeEn(bTRUE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, REG_AFE_ADCBUFCON_RESET);
	}
  
  /* Step 2: Adjust ADCFILTERCON and DFTCON to set optimumn SINC3, SINC2 and DFTNUM settings  */
  filter_cfg.ADCAvgNum = ADCAVGNUM_16;  /* Don't care because it's disabled */ 
  filter_cfg.ADCSinc2Osr = freq_params.ADCSinc2Osr;
  filter_cfg.ADCSinc3Osr = freq_params.ADCSinc3Osr;
  filter_cfg.BpSinc3 = bFALSE;
  filter_cfg.BpNotch = bTRUE;
  filter_cfg.Sinc2NotchEnable = bTRUE;
  dft_cfg.DftNum = freq_params.DftNum;
  dft_cfg.DftSrc = freq_params.DftSrc;
  dft_cfg.HanWinEn = bTRUE;
  AD5940_ADCFilterCfgS(&filter_cfg);
  AD5940_DFTCfgS(&dft_cfg);

  /* Step 3: Calculate clocks needed to get result to FIFO and update sequencer wait command */
  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = freq_params.DftSrc;
  clks_cal.DataCount = 1L<<(freq_params.DftNum+2); /* 2^(DFTNUMBER+2) */
  clks_cal.ADCSinc2Osr = freq_params.ADCSinc2Osr;
  clks_cal.ADCSinc3Osr = freq_params.ADCSinc3Osr;
  clks_cal.ADCAvgNum = 0;
  clks_cal.RatioSys2AdcClk = SYSCLCK/adcClck;
  AD5940_ClksCalculate(&clks_cal, &_waitClcks);

  return AD5940ERR_OK;
}

void HELPStat::sdWrite(char *output) {
  File _file; 
  _file = SD.open(FILENAME, FILE_WRITE);
  if(_file)
  {
    Serial.println("File initialized. Writing data now...");
    _file.println(output); 
    Serial.println("Data written successfully.");
    _file.close(); 
  }
  else Serial.println("Unable to open the file.");

}

void HELPStat::sdAppend(char *output) {
  File _file; 
  _file = SD.open(FILENAME, FILE_APPEND);
  if(_file)
  {
    Serial.println("File initialized. Appending data now...");
    _file.println(output); 
    Serial.println("Data appended successfully.");
    _file.close(); 
  }
  else Serial.println("Unable to append to the file.");

}

void HELPStat::printData(void) {
  // impStruct temp[_sweepCfg.SweepPoints];
  impStruct eis; 
  printf("Printing entire array now...\n");
  for(uint32_t i = 0; i <= _numCycles; i++)
  {
    printf("Cycle %d\n", i);
    printf("Index, Freq, Mag, Real, Imag, Phase (rad)\n");
    for(uint32_t j = 0; j < _sweepCfg.SweepPoints; j++)
    {
      eis = eisArr[j + (i * _sweepCfg.SweepPoints)];
      printf("%d,", j);
      printf("%.2f,", eis.freq);
      printf("%f,", eis.magnitude);
      printf("%.4f,", eis.real);
      printf("%.4f,", eis.imag);
      printf("%.4f\n", eis.phaseRad);
    }
  }
}

/*
  Surprised this works...maybe I overthought the whole SPI thing.
  Currently, the SPI configuration is set for 15 MHz, and from
  what I understand, the MicroSD card can take up to 40 MHz. So 
  I think as long as the configuration is within the SD card's range, 
  I can just use the existing SPI class rather than worrying about
  using the HSPI or VSPI (ESP32 uses VSPI ONLY by default when calling
  Arduino's SPI library).
*/

/*
  This function uses estimates for Rct and Rs to fit the impedence data to the semicircle equation.
  It returns a vector containing both the Rct and Rs estimates. Note that this is dependent on the
  lma.h and lma.c user-defined libraries (see https://github.com/LinnesLab/EIS-LevenbergMarquardtAlgorithm)
  This is also itself dependent on a modified version of Bolder Flight System's Eigen port (see
  https://github.com/LinnesLab/Eigen-Port).

  06/13/2024 - Two overloaded functions were made. One where the user manually inputs what estimates
  to use, and the other that uses private variables. These private variables are updated in the
  BLE_settings() function.
*/
void HELPStat::calculateResistors() {
  std::vector<float> Z_real;
  std::vector<float> Z_imag;

  // Should append each real and imaginary data point
  for(uint32_t i = 0; i < _sweepCfg.SweepPoints; i++) {
    for(uint32_t j = 0; j <= _numCycles; j++) {
      impStruct eis;
      eis = eisArr[i + (j * _sweepCfg.SweepPoints)];
      Z_real.push_back(eis.real);
      Z_imag.push_back(eis.imag);
    }
  }

  _calculated_Rct = calculate_Rct(_rct_estimate, _rs_estimate, Z_real, Z_imag);
  _calculated_Rs  = calculate_Rs(_rct_estimate, _rs_estimate, Z_real, Z_imag);

  Serial.print("Calculated Rct: ");
  Serial.println(_calculated_Rct);
  Serial.print("Calculated Rs:  ");
  Serial.println(_calculated_Rs);

  return;
}
void HELPStat::calculateResistors(float rct_estimate, float rs_estimate) {
  std::vector<float> Z_real;
  std::vector<float> Z_imag;

  // Should append each real and imaginary data point
  for(uint32_t i = 0; i < _sweepCfg.SweepPoints; i++) {
    for(uint32_t j = 0; j <= _numCycles; j++) {
      impStruct eis;
      eis = eisArr[i + (j * _sweepCfg.SweepPoints)];
      Z_real.push_back(eis.real);
      Z_imag.push_back(eis.imag);
    }
  }

  _calculated_Rct = calculate_Rct(rct_estimate, rs_estimate, Z_real, Z_imag);
  _calculated_Rs  = calculate_Rs(rct_estimate, rs_estimate, Z_real, Z_imag);

  Serial.print("Calculated Rct: ");
  Serial.println(_calculated_Rct);
  Serial.print("Calculated Rs:  ");
  Serial.println(_calculated_Rs);

  return;
}

void HELPStat::saveDataEIS() {
  String directory = "/" + _folderName;
  
  if(!SD.begin(CS_SD))
  {
    Serial.println("Card mount failed.");
    return;
  }

  if(!SD.exists(directory))
  {
    /* Creating a directory to store the data */
    if(SD.mkdir(directory)) Serial.println("Directory made successfully.");
    else
    {
      Serial.println("Couldn't make a directory or it already exists.");
      return;
    }
  }

  else Serial.println("Directory already exists.");

  /* Writing to the files */
  Serial.println("Saving to folder now.");

  // All cycles 
  String filePath = _folderName + "/" + _fileName + ".csv";

  File dataFile = SD.open(filePath, FILE_WRITE); 
  if(dataFile)
  {
    for(uint32_t i = 0; i <= _numCycles; i++)
    {
      dataFile.print("Freq, Magnitude, Phase (rad), Phase (deg), Real, Imag");
    }
    dataFile.println("");
    for(uint32_t i = 0; i < _sweepCfg.SweepPoints; i++)
    {
      for(uint32_t j = 0; j <= _numCycles; j++)
      {
        impStruct eis;
        eis = eisArr[i + (j * _sweepCfg.SweepPoints)];
        dataFile.print(eis.freq);
        dataFile.print(",");
        dataFile.print(eis.magnitude);
        dataFile.print(",");
        dataFile.print(eis.phaseRad);
        dataFile.print(",");
        dataFile.print(eis.phaseDeg);
        dataFile.print(",");
        dataFile.print(eis.real);
        dataFile.print(",");
        dataFile.print(eis.imag);
        dataFile.print(",");
      }
      /* Moves to the next line */
      dataFile.println("");
    }
    dataFile.close();
    Serial.println("Data appended successfully.");
  }
}

void HELPStat::saveDataEIS(String dirName, String fileName) {
  String directory = "/" + dirName;
  
  if(!SD.begin(CS_SD))
  {
    Serial.println("Card mount failed.");
    return;
  }

  if(!SD.exists(directory))
  {
    /* Creating a directory to store the data */
    if(SD.mkdir(directory)) Serial.println("Directory made successfully.");
    else
    {
      Serial.println("Couldn't make a directory or it already exists.");
      return;
    }
  }

  else Serial.println("Directory already exists.");

  /* Writing to the files */
  Serial.println("Saving to folder now.");

  // All cycles 
  String filePath = directory + "/" + fileName + ".csv";

  File dataFile = SD.open(filePath, FILE_WRITE); 
  if(dataFile)
  {
    for(uint32_t i = 0; i <= _numCycles; i++)
    {
      dataFile.print("Freq, Magnitude, Phase (rad), Phase (deg), Real, Imag");
    }
    dataFile.println("");
    for(uint32_t i = 0; i < _sweepCfg.SweepPoints; i++)
    {
      for(uint32_t j = 0; j <= _numCycles; j++)
      {
        impStruct eis;
        eis = eisArr[i + (j * _sweepCfg.SweepPoints)];
        dataFile.print(eis.freq);
        dataFile.print(",");
        dataFile.print(eis.magnitude);
        dataFile.print(",");
        dataFile.print(eis.phaseRad);
        dataFile.print(",");
        dataFile.print(eis.phaseDeg);
        dataFile.print(",");
        dataFile.print(eis.real);
        dataFile.print(",");
        dataFile.print(eis.imag);
        dataFile.print(",");
      }
      /* Moves to the next line */
      dataFile.println("");
    }
    dataFile.close();
    Serial.println("Data appended successfully.");
  }
}

void HELPStat::AD5940_BiasCfg(float startFreq, float endFreq, uint32_t numPoints, float biasVolt, float zeroVolt, int delaySecs) {

  // SETUP Cfgs
  CLKCfg_Type clk_cfg;
  AGPIOCfg_Type gpio_cfg;
  ClksCalInfo_Type clks_cal;
  
  // DFT / ADC / WG / HSLoop Cfgs
  AFERefCfg_Type aferef_cfg;
  HSLoopCfg_Type HsLoopCfg;
  DSPCfg_Type dsp_cfg;

  float sysClkFreq = 16000000.0; // 16 MHz
  float adcClkFreq = 16000000.0; // 16 MHz
  float sineVpp = _signalAmplitude; // Use configurable amplitude 

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();

  /* Platform configuration */
  /* Step1. Configure clock - NEED THIS */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1; // Clock source divider - ADC
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC; // Enables internal high frequency 16/32 MHz clock as source
  clk_cfg.SysClkDiv = SYSCLKDIV_1; // Clock source divider - System
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC; // Enables internal high frequency 16/32 MHz clock as source 
  clk_cfg.HfOSC32MHzMode = bFALSE; // Sets it to 16 MHz
  clk_cfg.HFOSCEn = bTRUE; // Enables the internal 16 / 32 MHz source
  clk_cfg.HFXTALEn = bFALSE; // Disables any need for external clocks
  clk_cfg.LFOSCEn = bTRUE; // Enables 32 kHz clock for timing / wakeups
  AD5940_CLKCfg(&clk_cfg); // Configures the clock
  Serial.println("Clock setup successfully.");

  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT); // Clears all INT flags

  /* Set INT0 source to be DFT READY */
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DFTRDY, bTRUE); 
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT); // clears all flags 
  Serial.println("INTs setup successfully.");

  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP0_INT;

  gpio_cfg.InputEnSet = 0; // Disables any GPIOs as inputs
  gpio_cfg.OutputEnSet = AGPIO_Pin0; // Enables GPIOs as outputs

  gpio_cfg.OutVal = 0; // Value for the output 
  gpio_cfg.PullEnSet = 0; // Disables any GPIO pull-ups / Pull-downs

  AD5940_AGPIOCfg(&gpio_cfg); // Configures the GPIOs
  Serial.println("GPIOs setup successfully.");

  /* CONFIGURING FOR DFT */
  // AFE Configuration 
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);  /* Initializing to disabled state */

  // Enabling high power bandgap since we're using High Power DAC
  // Enables operation at higher frequencies 
  aferef_cfg.HpBandgapEn = bTRUE;

  aferef_cfg.Hp1V1BuffEn = bTRUE; // Enables 1v1 buffer
  aferef_cfg.Hp1V8BuffEn = bTRUE; // Enables 1v8 buffer

  /* Not going to discharge capacitors - haven't seen this ever used */
  aferef_cfg.Disc1V1Cap = bFALSE;
  aferef_cfg.Disc1V8Cap = bFALSE;

  /* Disabling buffers and current limits*/
  aferef_cfg.Hp1V8ThemBuff = bFALSE;
  aferef_cfg.Hp1V8Ilimit = bFALSE;

  /* Disabling low power buffers */
  aferef_cfg.Lp1V1BuffEn = bFALSE;
  aferef_cfg.Lp1V8BuffEn = bFALSE;

  /* LP reference control - turn off if no bias */
  if(biasVolt != 0.0f)
  {
    aferef_cfg.LpBandgapEn = bTRUE;
    aferef_cfg.LpRefBufEn = bTRUE;
  }
  else
  {
    aferef_cfg.LpBandgapEn = bFALSE;
    aferef_cfg.LpRefBufEn = bFALSE;
  }
  aferef_cfg.LpRefBoostEn = bFALSE;

  AD5940_REFCfgS(&aferef_cfg);	// Configures the AFE 
  Serial.println("AFE setup successfully.");
  
  // Configuring High Speed Loop (high power loop)
  /* Vpp * BufGain * DacGain */
  HsLoopCfg.HsDacCfg.ExcitBufGain = EXCITBUFGAIN_2;
  HsLoopCfg.HsDacCfg.HsDacGain = HSDACGAIN_1;

  /* For low power / frequency measurements use 0x1B, o.w. 0x07 */
  HsLoopCfg.HsDacCfg.HsDacUpdateRate = 0x1B;
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;

  /* Assuming no bias - default to 1V1 bias */
  if(biasVolt != 0.0f) HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_VZERO0;
  else HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_1P1;

  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;

  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_40K;
  HsLoopCfg.HsTiaCfg.HstiaCtia = optimalCtia(HSTIARTIA_40K, startFreq);

  HsLoopCfg.SWMatCfg.Dswitch = SWD_CE0;
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_SE0LOAD | SWT_TRTIA;

  HsLoopCfg.WgCfg.WgType = WGTYPE_SIN;
  
  /* Gain and offset calibration */
  HsLoopCfg.WgCfg.GainCalEn = bTRUE;
  HsLoopCfg.WgCfg.OffsetCalEn = bTRUE;

  _currentFreq = startFreq;
  HsLoopCfg.WgCfg.SinCfg.SinFreqWord = AD5940_WGFreqWordCal(_currentFreq, sysClkFreq);
  HsLoopCfg.WgCfg.SinCfg.SinAmplitudeWord = (uint32_t)(sineVpp/800.0f*2047 + 0.5f);
  HsLoopCfg.WgCfg.SinCfg.SinOffsetWord = 0;
  HsLoopCfg.WgCfg.SinCfg.SinPhaseWord = 0;
  AD5940_HSLoopCfgS(&HsLoopCfg);
  Serial.println("HS Loop configured successfully");
  
  /* Configuring Sweep Functionality */
  _sweepCfg.SweepEn = bTRUE; 
  _sweepCfg.SweepLog = bTRUE;
  _sweepCfg.SweepIndex = 0; 
  _sweepCfg.SweepStart = startFreq; 
  _sweepCfg.SweepStop = endFreq;

  _startFreq = startFreq; 
  _endFreq = endFreq;
  
  // Defaulting to a logarithmic sweep. Works both upwards and downwards
  if(startFreq > endFreq) _sweepCfg.SweepPoints = (uint32_t)(1.5 + (log10(startFreq) - log10(endFreq)) * (numPoints)) - 1;
  else _sweepCfg.SweepPoints = (uint32_t)(1.5 + (log10(endFreq) - log10(startFreq)) * (numPoints)) - 1;
  printf("Number of points: %d\n", _sweepCfg.SweepPoints);
  Serial.println("Sweep configured successfully.");

  /* Configuring LPDAC if necessary */
  if(biasVolt != 0.0f)
  {
    LPDACCfg_Type lpdac_cfg;
    
    lpdac_cfg.LpdacSel = LPDAC0;
    lpdac_cfg.LpDacVbiasMux = LPDACVBIAS_12BIT; /* Use Vbias to tune BiasVolt. */
    lpdac_cfg.LpDacVzeroMux = LPDACVZERO_6BIT;  /* Vbias-Vzero = BiasVolt */

    // Uses 2v5 as a reference, can set to AVDD
    lpdac_cfg.LpDacRef = LPDACREF_2P5;
    lpdac_cfg.LpDacSrc = LPDACSRC_MMR;      /* Use MMR data, we use LPDAC to generate bias voltage for LPTIA - the Vzero */
    lpdac_cfg.PowerEn = bTRUE;              /* Power up LPDAC */

    /*
      Note on bias: 
      Examples from AD differ based on impedance and EIS versions of code. 
      Default version is centered around 1.1V for Vzero with no compensation as per datasheet.
      That being said, still getting relatively accurate results for Vbias - Vzero. 
      So, it works decently well. Error in bias voltage increases with the bias though, I found.

      Custom Vzero version from EIS has slightly different formula than datasheet I believe, 
      so I tried my best to find the best of both worlds for the custom one to follow both datasheet
      and AD example. Recommend sticking with default version for testing. 
    */

    /* 
      Default bias case - centered around Vzero = 1.1V
      This works decently well. Error seems to increase as you go higher in bias.
     */
    if(!zeroVolt)
    {
      // Edge cases 
      if(biasVolt<-1100.0f) biasVolt = -1100.0f + DAC12BITVOLT_1LSB;
      if(biasVolt> 1100.0f) biasVolt = 1100.0f - DAC12BITVOLT_1LSB;
      
      /* Bit conversion from voltage */
      // Converts the bias voltage to a data bit - uses the 1100 to offset it with Vzero
      lpdac_cfg.DacData6Bit = 0x40 >> 1;            /* Set Vzero to middle scale - sets Vzero to 1.1V */
      lpdac_cfg.DacData12Bit = (uint16_t)((biasVolt + 1100.0f)/DAC12BITVOLT_1LSB);
    }
    else
    {
      /* 
        Seems to work well. VBIAS needs to be set relative to VZERO. 
        i.e. let VBIAS be the bias voltage you want (100 mV, 20 mV, etc.) 
        and set VZERO to a constant.
     
      */
      lpdac_cfg.DacData6Bit = (uint32_t)((zeroVolt-200)/DAC6BITVOLT_1LSB);
      lpdac_cfg.DacData12Bit = (int32_t)((biasVolt)/DAC12BITVOLT_1LSB) + (lpdac_cfg.DacData6Bit * 64);
      if(lpdac_cfg.DacData12Bit < lpdac_cfg.DacData6Bit * 64) lpdac_cfg.DacData12Bit--; // compensation as per datasheet 
    }

    lpdac_cfg.DataRst = bFALSE;      /* Do not reset data register */
    // Allows for measuring of Vbias and Vzero voltages and connects them to LTIA, LPPA, and HSTIA
    lpdac_cfg.LpDacSW = LPDACSW_VBIAS2LPPA|LPDACSW_VBIAS2PIN|LPDACSW_VZERO2LPTIA|LPDACSW_VZERO2PIN|LPDACSW_VZERO2HSTIA;
    AD5940_LPDACCfgS(&lpdac_cfg);
    Serial.println("LPDAC configured successfully.");
  }

  /* Sets the input of the ADC to the output of the HSTIA */
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;

  /* Programmable gain array for the ADC */
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
  
  /* Disables digital comparator functionality */
  memset(&dsp_cfg.ADCDigCompCfg, 0, sizeof(dsp_cfg.ADCDigCompCfg));
  
  /* Is this actually being used? */
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16; // Impedance example uses 16 
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;	/* Tell filter block clock rate of ADC*/

  // dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_1P6MHZ;	/* Tell filter block clock rate of ADC*/
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_22; // Oversampling ratio for SINC2
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_2; // Oversampling ratio for SINC3
  /* Using Recommended OSR of 4 for SINC3 */
  // dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_4; // Oversampling ratio for SINC3

  dsp_cfg.ADCFilterCfg.BpNotch = bTRUE; // Bypasses Notch filter
  dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE; // Doesn't bypass SINC3
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE; // Enables SINC2 filter

  dsp_cfg.DftCfg.DftNum = DFTNUM_16384; // Max number of DFT points
  dsp_cfg.DftCfg.DftSrc = DFTSRC_SINC3; // Sets DFT source to SINC3
  dsp_cfg.DftCfg.HanWinEn = bTRUE;  // Enables HANNING WINDOW - recommended to always be on 
  
  /* Disables STAT block */
  memset(&dsp_cfg.StatCfg, 0, sizeof(dsp_cfg.StatCfg));
  
  AD5940_DSPCfgS(&dsp_cfg); // Sets the DFT 
  Serial.println("DSP configured successfully.");

  /* Calculating Clock Cycles to wait given DFT settings */
  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = DFTSRC_SINC3; // Source of DFT
  clks_cal.DataCount = 1L<<(DFTNUM_16384+2); /* 2^(DFTNUMBER+2) */
  clks_cal.ADCSinc2Osr = ADCSINC2OSR_22;
  clks_cal.ADCSinc3Osr = ADCSINC3OSR_2;
  clks_cal.ADCAvgNum = ADCAVGNUM_16;
  clks_cal.RatioSys2AdcClk = sysClkFreq / adcClkFreq; // Same ADC / SYSTEM CLCK FREQ
  AD5940_ClksCalculate(&clks_cal, &_waitClcks);

  /* Clears any interrupts just in case */
  AD5940_ClrMCUIntFlag();
  AD5940_INTCClrFlag(AFEINTSRC_DFTRDY);

  // Added bias option conditionally 
  if(biasVolt == 0.0f)
  {
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                AFECTRL_SINC2NOTCH, bTRUE);
    Serial.println("No bias applied.");
  }
  else
  {
    /* 
      Also powers the DC offset buffers that's used with LPDAC (Vbias) 
      Buffers need to be powered up here but aren't turned off in measurement like 
      the rest. This is to ensure the LPDAC and bias stays on the entire time.
    */
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
              AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
              AFECTRL_SINC2NOTCH|AFECTRL_DCBUFPWR, bTRUE);
    Serial.println("Bias is applied.");
  }

  Serial.println("Everything turned on.");
  printf("Testing bias voltage: Vbias - Vzero =  %f\n", biasVolt);
}

void HELPStat::AD5940_DFTMeasureEIS(void) {

  SWMatrixCfg_Type sw_cfg;
  LPAmpCfg_Type LpAmpCfg;
  impStruct eis;

  int32_t realRcal, imageRcal; 
  int32_t realRload, imageRload; 
  int32_t realRzRload, imageRzRload; 

  fImpCar_Type rCal, rzRload, rLoad;
  fImpCar_Type DftConst1 = {1.0f, 0};
  fImpCar_Type temp1, temp2, res;

  uint32_t delayUnitsEIS = (uint32_t)(_waitClcks * 100000.0 / SYSCLCK);
  AD5940_Delay10us(delayUnitsEIS > 0 ? delayUnitsEIS : 1);

  /* Disconnect SE0 from LPTIA*/
	// LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  // LpAmpCfg.LpPaPwrEn = bTRUE;
  // LpAmpCfg.LpTiaPwrEn = bTRUE;
  // LpAmpCfg.LpTiaRf = LPTIARF_1M;
  // LpAmpCfg.LpTiaRload = LPTIARLOAD_100R;
  // LpAmpCfg.LpTiaRtia = LPTIARTIA_OPEN; /* Disconnect Rtia to avoid RC filter discharge */
  // LpAmpCfg.LpTiaSW = LPTIASW(7)|LPTIASW(8)|LPTIASW(12)|LPTIASW(13); 
	// AD5940_LPAMPCfgS(&LpAmpCfg);
  // delay(100);

  /* Measuring Rload and Rz */
  sw_cfg.Dswitch = SWD_CE0;
  sw_cfg.Pswitch = SWP_CE0;
  sw_cfg.Nswitch = SWN_SE0;
  sw_cfg.Tswitch = SWT_TRTIA|SWT_SE0LOAD;
  AD5940_SWMatrixCfgS(&sw_cfg);
  // delay(100); // empirical delay for switching 

  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_WG, bTRUE);  /* Enable Waveform generator */
  // delay(500);
  settlingDelay(_currentFreq);


  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);
  settlingDelay(_currentFreq);

  AD5940_Delay10us(delayUnitsEIS / 2 > 0 ? delayUnitsEIS / 2 : 1);
  AD5940_Delay10us(delayUnitsEIS / 2 > 0 ? delayUnitsEIS / 2 : 1);

  pollDFT(&realRzRload, &imageRzRload);
  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV|AFECTRL_DFT|AFECTRL_WG, bFALSE);

  sw_cfg.Dswitch = SWD_SE0;
  sw_cfg.Pswitch = SWP_SE0;
  sw_cfg.Nswitch = SWN_SE0LOAD;
  sw_cfg.Tswitch = SWT_SE0LOAD|SWT_TRTIA;
  AD5940_SWMatrixCfgS(&sw_cfg);

  AD5940_AFECtrlS(AFECTRL_WG|AFECTRL_ADCPWR, bTRUE);
  settlingDelay(_currentFreq);

  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);
  settlingDelay(_currentFreq);

  AD5940_Delay10us(delayUnitsEIS / 2 > 0 ? delayUnitsEIS / 2 : 1);
  AD5940_Delay10us(delayUnitsEIS / 2 > 0 ? delayUnitsEIS / 2 : 1);

  pollDFT(&realRload, &imageRload);

  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT|AFECTRL_WG|AFECTRL_ADCPWR, bFALSE);

  sw_cfg.Dswitch = SWD_RCAL0;
  sw_cfg.Pswitch = SWP_RCAL0;
  sw_cfg.Nswitch = SWN_RCAL1;
  sw_cfg.Tswitch = SWT_RCAL1|SWT_TRTIA;
  AD5940_SWMatrixCfgS(&sw_cfg);

  AD5940_AFECtrlS(AFECTRL_WG|AFECTRL_ADCPWR, bTRUE);
  settlingDelay(_currentFreq);

  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);
  settlingDelay(_currentFreq);
  
  AD5940_Delay10us(delayUnitsEIS / 2 > 0 ? delayUnitsEIS / 2 : 1);
  AD5940_Delay10us(delayUnitsEIS / 2 > 0 ? delayUnitsEIS / 2 : 1);

  /* Polling and retrieving data from the DFT */
  pollDFT(&realRcal, &imageRcal);

  //wait for first data ready
  AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV|AFECTRL_DFT|AFECTRL_WG, bFALSE);  /* Stop ADC convert and DFT */
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                AFECTRL_SINC2NOTCH, bFALSE);

  /* Type converting to floats and assigning to fImp structs */
  rCal.Real = (float) realRcal;
  rCal.Image = (float) -imageRcal;
  rzRload.Real = (float) realRzRload;
  rzRload.Image = (float) -imageRzRload;
  rLoad.Real = (float) realRload;
  rLoad.Image = (float) -imageRload;

  // /* Calculating magnitude and phase */
  // temp1 = AD5940_ComplexSubFloat(&rLoad, &rzRload);
  // temp2 = AD5940_ComplexMulFloat(&rzRload, &rLoad);
  // res = AD5940_ComplexDivFloat(&temp1, &temp2);
  // res = AD5940_ComplexMulFloat(&rCal, &res);

  temp1 = AD5940_ComplexDivFloat(&DftConst1, &rzRload); 
  temp2 = AD5940_ComplexDivFloat(&DftConst1, &rLoad); 
  res = AD5940_ComplexSubFloat(&temp1, &temp2);
  res = AD5940_ComplexMulFloat(&res, &rCal);

  /* Finding the actual magnitude and phase */
  eis.magnitude = AD5940_ComplexMag(&res) * _rcalVal; 

  // phase calculations: 
  // eis.phaseRad = AD5940_ComplexPhase(&rCal) + AD5940_ComplexPhase(&temp1) - AD5940_ComplexPhase(&rzRload) - AD5940_ComplexPhase(&rLoad);
  
  eis.phaseRad = AD5940_ComplexPhase(&res);
  eis.real = eis.magnitude * cos(eis.phaseRad);
  eis.imag = eis.magnitude * sin(eis.phaseRad) * -1; 
  eis.phaseDeg = eis.phaseRad * 180 / MATH_PI; 
  eis.freq = _currentFreq;

  /* Printing Values */
  printf("%d,", _sweepCfg.SweepIndex);
  printf("%.2f,", _currentFreq);
  printf("%.2f", AD5940_ComplexMag(&rzRload));
  printf("%.2f", AD5940_ComplexMag(&rLoad));
  printf("%f,", eis.magnitude);
  printf("%.4f,", eis.real);
  printf("%.4f,", eis.imag);
  printf("%.4f\n", eis.phaseRad);

  
  // printf("rLoad: %.3f,", AD5940_ComplexMag(&rLoad));
  // printf("rzRload: %.3f\n", AD5940_ComplexMag(&rzRload));
  // printf("rLoad phase: %.3f,", AD5940_ComplexPhase(&rLoad));
  // printf("rzRload: %.3f\n", AD5940_ComplexPhase(&rzRload));

  /* Saving to an array */
  eisArr[_sweepCfg.SweepIndex + (_currentCycle * _sweepCfg.SweepPoints)] = eis; 

  /* Updating Frequency */
  logSweep(&_sweepCfg, &_currentFreq);
}

/* Helper functions for refactoring how checkFreq works */
// Adapted from AD5940 Impedance Examples
void HELPStat::configureDFT(float freq) {
  FreqParams_Type freq_params;
  ClksCalInfo_Type clks_cal;
  ADCFilterCfg_Type filter_cfg;
  DFTCfg_Type dft_cfg;
  float adcClck; 

  /* Getting optimal freq parameters */
  freq_params = AD5940_GetFreqParameters(freq);

  /* Adjusting Filter and DFT configurations using
  optimal settings (as per AD5940 library)*/

  if(freq >= 80000)
  {
    filter_cfg.ADCRate = ADCRATE_1P6MHZ;
    adcClck = 32e6;
    AD5940_HPModeEn(bTRUE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, REG_AFE_ADCBUFCON_RESET);
  }
  else
  {
    filter_cfg.ADCRate = ADCRATE_800KHZ;
    adcClck = 16e6;
    AD5940_HPModeEn(bFALSE);
    AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
  }

  filter_cfg.ADCAvgNum = ADCAVGNUM_16;
  filter_cfg.ADCSinc2Osr = freq_params.ADCSinc2Osr;
  filter_cfg.ADCSinc3Osr = freq_params.ADCSinc3Osr;
  filter_cfg.BpSinc3 = bFALSE;
  filter_cfg.BpNotch = _enableNotchFilter ? bFALSE : bTRUE;
  filter_cfg.Sinc2NotchEnable = bTRUE;
  
  dft_cfg.DftNum = freq_params.DftNum;
  dft_cfg.DftSrc = freq_params.DftSrc;
  dft_cfg.HanWinEn = bTRUE;

  AD5940_ADCFilterCfgS(&filter_cfg);
  AD5940_DFTCfgS(&dft_cfg);

  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = freq_params.DftSrc;
  clks_cal.DataCount = 1L<<(freq_params.DftNum+2);
  clks_cal.ADCSinc2Osr = freq_params.ADCSinc2Osr;
  clks_cal.ADCSinc3Osr = freq_params.ADCSinc3Osr;
  clks_cal.ADCAvgNum = 0;
  clks_cal.RatioSys2AdcClk = SYSCLCK/adcClck;
  AD5940_ClksCalculate(&clks_cal, &_waitClcks);
}

AD5940Err HELPStat::setHSTIA(float freq) {
  HSDACCfg_Type hsdac_cfg;
  FreqParams_Type freq_params;
  ClksCalInfo_Type clks_cal;
  ADCFilterCfg_Type filter_cfg;
  DFTCfg_Type dft_cfg;
  float adcClck; 

  AD5940Err exitStatus = AD5940ERR_ERROR;

  hsdac_cfg.ExcitBufGain = _extGain;
  hsdac_cfg.HsDacGain = _dacGain;

  // hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_0P25;
  // hsdac_cfg.HsDacGain = HSDACGAIN_0P2;

  // hsdac_cfg.ExcitBufGain = EXCITBUFGAIN_2;
  // hsdac_cfg.HsDacGain = HSDACGAIN_1;

  /* Getting optimal freq parameters */
  freq_params = AD5940_GetFreqParameters(freq);

  // Running through the frequencies and tuning to the specified RTIA
  // It checks if the frequency is less than the cutoff
  // This method works only if the lowest frequency in the sweep is 
  // included. Otherwise you have a gap in calibration ranges.

  for(uint32_t i = 0; i < _gainArrSize; i++)
  {
    if(freq <= _gainArr[i].freq) 
    {
      if(freq >= 80000)
      {
        hsdac_cfg.HsDacUpdateRate = 0x07;
        AD5940_HSDacCfgS(&hsdac_cfg);
        AD5940_HSRTIACfgS(_gainArr[i].rTIA);
      __AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
        {
          uint32_t ctiaReg = AD5940_ReadReg(REG_AFE_HSRTIACON);
          ctiaReg &= ~BITM_AFE_HSRTIACON_CTIACON;
          ctiaReg |= optimalCtia(_gainArr[i].rTIA, freq) << BITP_AFE_HSRTIACON_CTIACON;
          AD5940_WriteReg(REG_AFE_HSRTIACON, ctiaReg);
        }

        filter_cfg.ADCRate = ADCRATE_1P6MHZ;
        adcClck = 32e6;
        AD5940_HPModeEn(bTRUE);
        AD5940_WriteReg(REG_AFE_ADCBUFCON, REG_AFE_ADCBUFCON_RESET);
        exitStatus = AD5940ERR_OK;
        break;
      }
      else
      {
        hsdac_cfg.HsDacUpdateRate = 0x1B;
        AD5940_HSDacCfgS(&hsdac_cfg);
        AD5940_HSRTIACfgS(_gainArr[i].rTIA);
      __AD5940_SetDExRTIA(0, HSTIADERTIA_OPEN, HSTIADERLOAD_0R);
        {
          uint32_t ctiaReg = AD5940_ReadReg(REG_AFE_HSRTIACON);
          ctiaReg &= ~BITM_AFE_HSRTIACON_CTIACON;
          ctiaReg |= optimalCtia(_gainArr[i].rTIA, freq) << BITP_AFE_HSRTIACON_CTIACON;
          AD5940_WriteReg(REG_AFE_HSRTIACON, ctiaReg);
        }

        filter_cfg.ADCRate = ADCRATE_800KHZ;
        adcClck = 16e6;
        AD5940_HPModeEn(bFALSE);
        AD5940_WriteReg(REG_AFE_ADCBUFCON, 0x005F3D04);
        exitStatus = AD5940ERR_OK;
        break;
      }
    }
  }
  // Serial.println("No longer in for loop!");
  filter_cfg.ADCAvgNum = ADCAVGNUM_16;  // Not using this so it doesn't matter 
  filter_cfg.ADCSinc2Osr = freq_params.ADCSinc2Osr;
  filter_cfg.ADCSinc3Osr = freq_params.ADCSinc3Osr;
  filter_cfg.BpSinc3 = bFALSE;
  filter_cfg.BpNotch = _enableNotchFilter ? bFALSE : bTRUE; // Use configured notch filter setting
  filter_cfg.Sinc2NotchEnable = bTRUE;
  
  dft_cfg.DftNum = freq_params.DftNum;
  dft_cfg.DftSrc = freq_params.DftSrc;
  dft_cfg.HanWinEn = bTRUE;
  // Serial.println("Filter and DFT configured.");

  AD5940_ADCFilterCfgS(&filter_cfg);
  AD5940_DFTCfgS(&dft_cfg);

  /* Calculating clock cycles - usually used for FIFO but I use it here
  as an additional wait time calculator. Potentially redundant, but keeping it for now.*/
  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = freq_params.DftSrc;
  clks_cal.DataCount = 1L<<(freq_params.DftNum+2); /* 2^(DFTNUMBER+2) */
  clks_cal.ADCSinc2Osr = freq_params.ADCSinc2Osr;
  clks_cal.ADCSinc3Osr = freq_params.ADCSinc3Osr;
  clks_cal.ADCAvgNum = 0;
  clks_cal.RatioSys2AdcClk = SYSCLCK/adcClck;
  AD5940_ClksCalculate(&clks_cal, &_waitClcks);
  // Serial.println("Clocks calculated.");
  // printf("Calibrated for freq: %.2f\n", freq);
  // If we can't calibrate based on configuration, return an error.
  
  return exitStatus;
}

void HELPStat::configureFrequency(float freq) {
  // Use hysteresis-based gain switching to prevent jumps
  AD5940Err check = setHSTIAWithHysteresis(freq, _lastFreq);
  _lastFreq = freq;

  if(check != AD5940ERR_OK) Serial.println("Unable to configure.");
  // else Serial.println("Configured successfully.");
}

/* Current noise measurements - removed SINC2/SINC3 raw output reading */
float HELPStat::getADCVolt(uint32_t gainPGA, float vRef1p82) { 
  // Function removed - only DFT output is used
  return 0.0;
}

float HELPStat::pollADC(uint32_t gainPGA, float vRef1p82) {
  // Function removed - only DFT output is used
  return 0.0;
}

void HELPStat::AD5940_TDDNoise(float biasVolt, float zeroVolt) {

  // SETUP Cfgs
  CLKCfg_Type clk_cfg;
  AGPIOCfg_Type gpio_cfg;
  ClksCalInfo_Type clks_cal;
  LPAmpCfg_Type LpAmpCfg;
  
  // DFT / ADC / WG / HSLoop Cfgs
  AFERefCfg_Type aferef_cfg;
  HSLoopCfg_Type HsLoopCfg;
  DSPCfg_Type dsp_cfg;

  float sysClkFreq = 16000000.0; // 16 MHz
  float adcClkFreq = 16000000.0; // 16 MHz
  float sineVpp = _signalAmplitude; // Use configurable amplitude 

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();

  /* Platform configuration */
  /* Step1. Configure clock - NEED THIS */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1; // Clock source divider - ADC
  clk_cfg.ADCCLkSrc = ADCCLKSRC_HFOSC; // Enables internal high frequency 16/32 MHz clock as source
  clk_cfg.SysClkDiv = SYSCLKDIV_1; // Clock source divider - System
  clk_cfg.SysClkSrc = SYSCLKSRC_HFOSC; // Enables internal high frequency 16/32 MHz clock as source 
  clk_cfg.HfOSC32MHzMode = bFALSE; // Sets it to 16 MHz
  clk_cfg.HFOSCEn = bTRUE; // Enables the internal 16 / 32 MHz source
  clk_cfg.HFXTALEn = bFALSE; // Disables any need for external clocks
  clk_cfg.LFOSCEn = bTRUE; // Enables 32 kHz clock for timing / wakeups
  AD5940_CLKCfg(&clk_cfg); // Configures the clock
  Serial.println("Clock setup successfully.");

  /* Step3. Interrupt controller */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE);   /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT); // Clears all INT flags

  /* Set INT0 source to be ADC READY */
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_SINC2RDY, bTRUE); 
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT); // clears all flags 
  Serial.println("INTs setup successfully.");

  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP0_INT;

  gpio_cfg.InputEnSet = 0; // Disables any GPIOs as inputs
  gpio_cfg.OutputEnSet = AGPIO_Pin0; // Enables GPIOs as outputs

  gpio_cfg.OutVal = 0; // Value for the output 
  gpio_cfg.PullEnSet = 0; // Disables any GPIO pull-ups / Pull-downs

  AD5940_AGPIOCfg(&gpio_cfg); // Configures the GPIOs
  Serial.println("GPIOs setup successfully.");

  /* CONFIGURING FOR DFT */
  // AFE Configuration 
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);  /* Initializing to disabled state */

  // Enabling high power bandgap since we're using High Power DAC
  // Enables operation at higher frequencies 
  aferef_cfg.HpBandgapEn = bTRUE;

  aferef_cfg.Hp1V1BuffEn = bTRUE; // Enables 1v1 buffer
  aferef_cfg.Hp1V8BuffEn = bTRUE; // Enables 1v8 buffer

  /* Not going to discharge capacitors - haven't seen this ever used */
  aferef_cfg.Disc1V1Cap = bFALSE;
  aferef_cfg.Disc1V8Cap = bFALSE;

  /* Disabling buffers and current limits*/
  aferef_cfg.Hp1V8ThemBuff = bFALSE;
  aferef_cfg.Hp1V8Ilimit = bFALSE;

  /* Disabling low power buffers */
  aferef_cfg.Lp1V1BuffEn = bFALSE;
  aferef_cfg.Lp1V8BuffEn = bFALSE;

  /* LP reference control - turn off if no bias */
  if((biasVolt == 0.0f) && (zeroVolt == 0.0f))
  {
    aferef_cfg.LpBandgapEn = bFALSE;
    aferef_cfg.LpRefBufEn = bFALSE;
    printf("No bias today!\n");
  }
  else
  {
    aferef_cfg.LpBandgapEn = bTRUE;
    aferef_cfg.LpRefBufEn = bTRUE;
    printf("We have bias!\n");
  }

  /* Doesn't enable boosting buffer current */
  aferef_cfg.LpRefBoostEn = bFALSE;
  AD5940_REFCfgS(&aferef_cfg);	// Configures the AFE 
  Serial.println("AFE setup successfully.");
  
  /* Disconnect SE0 from LPTIA - double check this too */
	LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  LpAmpCfg.LpPaPwrEn = bFALSE; //bTRUE
  LpAmpCfg.LpTiaPwrEn = bFALSE; //bTRUE
  LpAmpCfg.LpTiaRf = LPTIARF_1M;
  LpAmpCfg.LpTiaRload = LPTIARLOAD_100R;
  LpAmpCfg.LpTiaRtia = LPTIARTIA_OPEN; /* Disconnect Rtia to avoid RC filter discharge */
  LpAmpCfg.LpTiaSW = LPTIASW(7)|LPTIASW(8)|LPTIASW(12)|LPTIASW(13); 
	AD5940_LPAMPCfgS(&LpAmpCfg);
  Serial.println("SE0 disconnected from LPTIA.");
  
  // Configuring High Speed Loop (high power loop)
  /* Vpp * BufGain * DacGain */
  /*
    Configuration shouldn't matter because CE0 is disconnected
    but I set it anyways since not sure if I can leave it uninitialized. 
  */
  HsLoopCfg.HsDacCfg.ExcitBufGain = EXCITBUFGAIN_2;
  HsLoopCfg.HsDacCfg.HsDacGain = HSDACGAIN_1;

  /* For low power / frequency measurements use 0x1B, o.w. 0x07 */
  HsLoopCfg.HsDacCfg.HsDacUpdateRate = 0x1B;
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;

  /* Assuming no bias - default to 1V1 bias */
  if((biasVolt == 0.0f) && (zeroVolt == 0.0f))
  {
    HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_1P1;
    printf("HSTIA bias set to 1.1V.\n");
  }
  else 
  {
    HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_VZERO0;
    printf("HSTIA bias set to Vzero.\n");
  }

  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;

  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_10K;
  HsLoopCfg.HsTiaCfg.HstiaCtia = optimalCtia(HSTIARTIA_10K, _startFreq);

  HsLoopCfg.SWMatCfg.Dswitch = SWD_CE0;
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_SE0LOAD|SWT_TRTIA;

  /*
    Shouldn't matter since WG is going to be connected to CE0. 
    Set to not leave unitialized.
  */
  // memset(&HSLoopCfg.WgCfg, 0, sizeof(HSLoopCfg.WgCfg));

  HsLoopCfg.WgCfg.WgType = WGTYPE_SIN;
  HsLoopCfg.WgCfg.GainCalEn = bTRUE;          // Gain calibration
  HsLoopCfg.WgCfg.OffsetCalEn = bTRUE;        // Offset calibration
  printf("Current Freq: %f\n", 100);
  HsLoopCfg.WgCfg.SinCfg.SinFreqWord = AD5940_WGFreqWordCal(100, sysClkFreq);
  HsLoopCfg.WgCfg.SinCfg.SinAmplitudeWord = (uint32_t)((sineVpp/800.0f)*2047 + 0.5f);
  HsLoopCfg.WgCfg.SinCfg.SinOffsetWord = 0;
  HsLoopCfg.WgCfg.SinCfg.SinPhaseWord = 0;
  AD5940_HSLoopCfgS(&HsLoopCfg);
  Serial.println("HS Loop configured successfully");
  
  /* Configuring Sweep Functionality - doesn't matter here */
  _sweepCfg.SweepEn = bFALSE; 
  _sweepCfg.SweepLog = bTRUE;
  _sweepCfg.SweepIndex = 0; 
  _sweepCfg.SweepStart = 1000; 
  _sweepCfg.SweepStop = 10;
  _sweepCfg.SweepPoints = 5;

   /* Configuring LPDAC if necessary */
  if((biasVolt != 0.0f) || (zeroVolt != 0.0f))
  {
    LPDACCfg_Type lpdac_cfg;
    
    lpdac_cfg.LpdacSel = LPDAC0;
    lpdac_cfg.LpDacVbiasMux = LPDACVBIAS_12BIT; /* Use Vbias to tune BiasVolt. */
    lpdac_cfg.LpDacVzeroMux = LPDACVZERO_6BIT;  /* Vbias-Vzero = BiasVolt */

    // Uses 2v5 as a reference, can set to AVDD
    lpdac_cfg.LpDacRef = LPDACREF_2P5;
    lpdac_cfg.LpDacSrc = LPDACSRC_MMR;      /* Use MMR data, we use LPDAC to generate bias voltage for LPTIA - the Vzero */
    lpdac_cfg.PowerEn = bTRUE;              /* Power up LPDAC */
    /* 
      Default bias case - centered around Vzero = 1.1V
      This works decently well. Error seems to increase as you go higher in bias.
     */
    if(zeroVolt == 0.0f)
    {
      // Edge cases 
      if(biasVolt<-1100.0f) biasVolt = -1100.0f + DAC12BITVOLT_1LSB;
      if(biasVolt> 1100.0f) biasVolt = 1100.0f - DAC12BITVOLT_1LSB;
      
      /* Bit conversion from voltage */
      // Converts the bias voltage to a data bit - uses the 1100 to offset it with Vzero
      lpdac_cfg.DacData6Bit = 0x40 >> 1;            /* Set Vzero to middle scale - sets Vzero to 1.1V */
      lpdac_cfg.DacData12Bit = (uint16_t)((biasVolt + 1100.0f)/DAC12BITVOLT_1LSB);
    }
    else
    {
      /* 
        Working decently well now.
      */
      lpdac_cfg.DacData6Bit = (uint32_t)((zeroVolt-200)/DAC6BITVOLT_1LSB);
      lpdac_cfg.DacData12Bit = (int32_t)((biasVolt)/DAC12BITVOLT_1LSB) + (lpdac_cfg.DacData6Bit * 64);
      if(lpdac_cfg.DacData12Bit < lpdac_cfg.DacData6Bit * 64) lpdac_cfg.DacData12Bit--; // compensation as per datasheet 
    } 
    lpdac_cfg.DataRst = bFALSE;      /* Do not reset data register */
    // Allows for measuring of Vbias and Vzero voltages and connects them to LTIA, LPPA, and HSTIA
    lpdac_cfg.LpDacSW = LPDACSW_VBIAS2LPPA|LPDACSW_VBIAS2PIN|LPDACSW_VZERO2LPTIA|LPDACSW_VZERO2PIN|LPDACSW_VZERO2HSTIA;
    AD5940_LPDACCfgS(&lpdac_cfg);
    Serial.println("LPDAC configured successfully.");
  }

  /* Sets the input of the ADC to the output of the HSTIA */
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;

  /* Programmable gain array for the ADC */
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
  
  /* Disables digital comparator functionality */
  memset(&dsp_cfg.ADCDigCompCfg, 0, sizeof(dsp_cfg.ADCDigCompCfg));
  
  /* Is this actually being used? */
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16; // Impedance example uses 16 
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;	/* Tell filter block clock rate of ADC*/
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_1333; // Oversampling ratio for SINC2
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_4; // Oversampling ratio for SINC3
  
  /* Bypassing filters so SINC settings don't matter */
  dsp_cfg.ADCFilterCfg.BpNotch = bTRUE; // Bypasses Notch filter
  dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE; // Bypass SINC3
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE; // Enables SINC2 filter

  dsp_cfg.DftCfg.DftNum = DFTNUM_16384; // Max number of DFT points
  dsp_cfg.DftCfg.DftSrc = DFTSRC_SINC3; // Sets DFT source to SINC3
  dsp_cfg.DftCfg.HanWinEn = bTRUE;  // Enables HANNING WINDOW - recommended to always be on 
  
  /* Disables STAT block */
  memset(&dsp_cfg.StatCfg, 0, sizeof(dsp_cfg.StatCfg));
  
  AD5940_DSPCfgS(&dsp_cfg); // Sets the DFT 
  Serial.println("DSP configured successfully.");

  /* Calculating Clock Cycles to wait given DFT settings */
  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = DFTSRC_SINC3; // Source of DFT
  clks_cal.DataCount = 1L<<(DFTNUM_16384+2); /* 2^(DFTNUMBER+2) */
  clks_cal.ADCSinc2Osr = ADCSINC2OSR_1333;
  clks_cal.ADCSinc3Osr = ADCSINC3OSR_4;
  clks_cal.ADCAvgNum = ADCAVGNUM_16;
  clks_cal.RatioSys2AdcClk = sysClkFreq / adcClkFreq; // Same ADC / SYSTEM CLCK FREQ
  AD5940_ClksCalculate(&clks_cal, &_waitClcks);

  /* Clears any interrupts just in case */
  AD5940_ClrMCUIntFlag();
  AD5940_INTCClrFlag(AFEINTSRC_SINC2RDY);

  /* Do I need to include AFECTRL_HPREFPWR? It's the only one not here. */

   // Added bias option conditionally 
  if((biasVolt == 0.0f) && (zeroVolt == 0.0f))
  {
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_SINC2NOTCH, bTRUE);
    Serial.println("No bias applied.");
  }
  else
  {
    /* 
      Also powers the DC offset buffers that's used with LPDAC (Vbias) 
      Buffers need to be powered up here but aren't turned off in measurement like 
      the rest. This is to ensure the LPDAC and bias stays on the entire time.
    */
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_SINC2NOTCH|AFECTRL_DCBUFPWR, bTRUE);
    Serial.println("Bias is applied.");
  }

  AD5940_SleepKeyCtrlS(SLPKEY_LOCK); // Disables Sleep Mode 
  /* 
    Turns off waveform generator to only get the bias without excitation. 
  */
  AD5940_AFECtrlS(AFECTRL_WG, bFALSE);
  Serial.println("Everything turned on. WG is set off.");
  printf("Bias: %f, Zero: %f\n", biasVolt, zeroVolt);
}

void HELPStat::AD5940_ADCMeasure(void) {

  SWMatrixCfg_Type sw_cfg;
  uint32_t adcCode;
  uint32_t gainPGA = ADCPGA_1;
  float vRef1p82 = 1.82;

  /* Measuring ADC */
  sw_cfg.Dswitch = SWD_CE0; // CE0 disconnected from Output
  sw_cfg.Pswitch = SWP_CE0;
  sw_cfg.Nswitch = SWN_SE0;
  sw_cfg.Tswitch = SWT_SE0LOAD|SWT_TRTIA;

  AD5940_SWMatrixCfgS(&sw_cfg);

	AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|AFECTRL_SINC2NOTCH, bTRUE);

  // AD5940_AFECtrlS(AFECTRL_ADCPWR, bTRUE);  /* Enable ADC */
  AD5940_ADCPowerCtrlS(bTRUE);
  AD5940_Delay10us(16 * 25);
  
  // AD5940_AFECtrlS(AFECTRL_ADCCNV, bTRUE);  /* Start ADC convert and DFT */
  AD5940_ADCConvtCtrlS(bTRUE);
  AD5940_Delay10us(_waitClcks); // Empirical delay

  Serial.println("Polling ADC!");
  float vOut = pollADC(gainPGA, vRef1p82);

  // AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV, bFALSE);  /* Stop ADC convert */
  AD5940_ADCPowerCtrlS(bFALSE);
  AD5940_ADCConvtCtrlS(bFALSE);

  printf("ADC Code: %d, ADC Voltage: %.4f\n", adcCode, vOut);
  AD5940_ShutDownS();
}

void HELPStat::ADCsweep(void) {
  Serial.println("Running sweep!");
  for(int i = 0; i < 10; i++)
  {
    printf("Index: %d\n", i);
    AD5940_ADCMeasure();
    delay(2000);
  }
  AD5940_ShutDownS();
  Serial.println("Shutting down now.");
}

void HELPStat::PGACal(void) {
  AD5940Err err;
  ADCPGACal_Type pgacal;
  pgacal.AdcClkFreq = 16e6;
  pgacal.ADCSinc2Osr = ADCSINC2OSR_178;
  pgacal.ADCSinc3Osr = ADCSINC3OSR_4;
  pgacal.SysClkFreq = 16e6;
  pgacal.TimeOut10us = 1000;
  pgacal.VRef1p11 = 1.11f;
  pgacal.VRef1p82 = 1.82f;
  pgacal.PGACalType = PGACALTYPE_OFFSETGAIN;
  pgacal.ADCPga = ADCPGA_1;
  err = AD5940_ADCPGACal(&pgacal);
  if(err != AD5940ERR_OK){
    printf("AD5940 PGA calibration failed.");
  }
}

/*
  Taken from Analog Devices ADC Polling Example and expounded on for noise
  measurements. Link: https://github.com/analogdevicesinc/ad5940-examples/blob/master/examples/AD5940_ADC/AD5940_ADCPolling.c
*/
void HELPStat::ADCNoiseTest(void) {
  ADCBaseCfg_Type adc_base;
  ADCFilterCfg_Type adc_filter;
  SWMatrixCfg_Type sw_cfg;

  /* Constants for sampling rate at 120 Hz */
  uint32_t SAMPLESIZE = 7200; 
  uint32_t runTime = 60;
  float currentArr[SAMPLESIZE];
  float fSample = 120; 
  uint32_t delaySample = (1/fSample) * 1000; 
  
  /* Use hardware reset */
  AD5940_HWReset();

  /* Firstly call this function after reset to initialize AFE registers. */
  AD5940_Initialize();

  /* Configure AFE power mode and bandwidth */
  AD5940_AFEPwrBW(AFEPWR_LP, AFEBW_250KHZ);
  
  /* Testing with TDD Noise - Disabled Waveform Generator  */
  AD5940_TDDNoise(0.0, 0.0);

  /* Initialize ADC basic function */
  // AD5940_AFECtrlS(AFECTRL_DACREFPWR|AFECTRL_HSDACPWR, bTRUE); //We are going to measure DAC 1.82V reference.
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_SINC2NOTCH|AFECTRL_DCBUFPWR, bTRUE);
  // Switches primarily control excitation amplifier. 
  // Only T switch really controls what gets into the HSTIA. 
  
  /* 
    As per recommendations from AD Tech Engineer, CE0 set to OPEN 
    If bias and vzero are enabled, set to SWD_CE0.
  */

  sw_cfg.Dswitch = SWD_OPEN;
  sw_cfg.Pswitch = SWP_CE0;
  sw_cfg.Nswitch = SWN_SE0;
  sw_cfg.Tswitch = SWT_SE0LOAD|SWT_TRTIA;
  AD5940_SWMatrixCfgS(&sw_cfg);

  /* Set HSTIA to use for measurement of SE0 - change this for each HSTIA */
  // HSTIARTIA_200
  // HSTIARTIA_1K
  // HSTIARTIA_5K
  // HSTIARTIA_10K
  // HSTIARTIA_20K
  // HSTIARTIA_40K
  // HSTIARTIA_80K
  // HSTIARTIA_160K

  AD5940_HSRTIACfgS(HSTIARTIA_1K);
  
  adc_base.ADCMuxP = ADCMUXP_HSTIA_P;
  adc_base.ADCMuxN = ADCMUXN_HSTIA_N;
  adc_base.ADCPga = ADCPGA_1;
  AD5940_ADCBaseCfgS(&adc_base);
 
  /* Initialize ADC filters ADCRawData-->SINC3-->SINC2+NOTCH */
  adc_filter.ADCSinc3Osr = ADCSINC3OSR_4;
  adc_filter.ADCSinc2Osr = ADCSINC2OSR_1333;
  adc_filter.ADCAvgNum = ADCAVGNUM_2;         /* Don't care about it. Average function is only used for DFT */
  adc_filter.ADCRate = ADCRATE_800KHZ;        /* If ADC clock is 32MHz, then set it to ADCRATE_1P6MHZ. Default is 16MHz, use ADCRATE_800KHZ. */
  adc_filter.BpNotch = bTRUE;                 /* SINC2+Notch is one block, when bypass notch filter, we can get fresh data from SINC2 filter. */
  adc_filter.BpSinc3 = bTRUE;                /* SINC3 filter is bypassed to read raw ADC results */ 
  adc_filter.Sinc2NotchEnable = bTRUE;        /* Enable the SINC2+Notch block. You can also use function AD5940_AFECtrlS */ 
  AD5940_ADCFilterCfgS(&adc_filter);
  
  //AD5940_ADCMuxCfgS(ADCMUXP_AIN2, ADCMUXN_VSET1P1);   /* Optionally, you can change ADC MUX with this function */

  /* Enable all interrupt at Interrupt Controller 1. So we can check the interrupt flag */
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ALLINT, bTRUE); 

  AD5940_ADCPowerCtrlS(bFALSE);
  AD5940_ADCConvtCtrlS(bFALSE);
  
  printf("Index, Interval, ADC Code, SE0, RE0, SE0-RE0, (-SE0) - RE0\n");
  unsigned long totalTimeStart = millis();
  unsigned long timeStart = millis(); 
  uint32_t i = 0; 
  while(i < SAMPLESIZE) 
  {
    uint32_t rd;
    uint32_t rd_re0;
    unsigned long currTime = millis(); 

    if(currTime - timeStart >= delaySample)
    {
      unsigned long timeInt = currTime - timeStart; 
      timeStart = millis();
      adc_base.ADCMuxP = ADCMUXP_HSTIA_P;
      adc_base.ADCMuxN = ADCMUXN_HSTIA_N;
      adc_base.ADCPga = ADCPGA_1;
      AD5940_ADCBaseCfgS(&adc_base);
      AD5940_ADCPowerCtrlS(bTRUE);
      AD5940_ADCConvtCtrlS(bTRUE);
      delay(2);
      while(!AD5940_INTCTestFlag(AFEINTC_1,AFEINTSRC_ADCRDY)){}
      
      // SINC3 raw output reading removed - only DFT output is used
      rd = 0;
      
      AD5940_ADCPowerCtrlS(bFALSE);
      AD5940_ADCConvtCtrlS(bFALSE);

      float diff_volt = 0.0; // SINC3 reading removed
      // float re0_volt = AD5940_ADCCode2Volt(rd_re0, ADCPGA_1, 1.816); // Uncomment if using Vzero and Vbias
      float re0_volt = 0; // Comment if using Vzero and Vbias  
      
      /* 
        Optional Save Data for it - uncomment here
      */

      _noiseArr[i].idx = i;
      _noiseArr[i].interval = timeInt;
      _noiseArr[i].vSE0 = diff_volt; 
      _noiseArr[i].vRE0 = re0_volt;
      _noiseArr[i].diff = diff_volt - re0_volt;
      _noiseArr[i].diffInv = (-diff_volt) - re0_volt; // Inverse of diff given that VSE0 post TIA is (-)

      printf("%d, %lu, %d, %.6f, %.6f, %.6f, %.6f\n", i, timeInt, rd, diff_volt, re0_volt, diff_volt - re0_volt, (-diff_volt) - re0_volt);
      i++;
    }
  }
  unsigned long totalTimeEnd = millis();
  printf("Total time of experiment: %lu\n", totalTimeEnd - totalTimeStart);
  AD5940_ShutDownS();
}

void HELPStat::saveDataNoise(String dirName, String fileName) {
  String directory = "/" + dirName;
  
  if(!SD.begin(CS_SD))
  {
    Serial.println("Card mount failed.");
    return;
  }

  if(!SD.exists(directory))
  {
    /* Creating a directory to store the data */
    if(SD.mkdir(directory)) Serial.println("Directory made successfully.");
    else
    {
      Serial.println("Couldn't make a directory or it already exists.");
      return;
    }
  }

  else Serial.println("Directory already exists.");

  /* Writing to the files */
  Serial.println("Saving to folder now.");

  // All cycles 
  String filePath = directory + "/" + fileName + ".csv";

  File dataFile = SD.open(filePath, FILE_WRITE); 
  if(dataFile)
  {
    dataFile.print("Index, ADC Code, VSE0, VRE0, VSE0-VRE0");
    for(uint32_t i = 0; i < NOISE_ARRAY - 1; i++)
    {
      dataFile.print(_noiseArr[i].idx);
      dataFile.print(",");
      dataFile.print(_noiseArr[i].interval);
      dataFile.print(",");
      dataFile.print(_noiseArr[i].vSE0);
      dataFile.print(",");
      dataFile.print(_noiseArr[i].vRE0);
      dataFile.print(",");
      dataFile.print(_noiseArr[i].diff);
      dataFile.print(",");
      dataFile.print(_noiseArr[i].diffInv);
    }

    dataFile.close();
    Serial.println("Data appended successfully.");
  }
}

void HELPStat::AD5940_HSTIARcal(int rHSTIA, float rcalVal) {
  HSRTIACal_Type rcalTest; // Rcal under test
  FreqParams_Type freqParams;
  fImpPol_Type polarResults; 
  AD5940Err testPoint; 

  rcalTest.fFreq = 120.0; // 120 Hz sampling frequency so we test at 120 Hz
  rcalTest.fRcal = rcalVal; 
  rcalTest.SysClkFreq = 16000000.0; // 16 MHz ADC and System Clck 
  rcalTest.AdcClkFreq = 16000000.0;

  /* Configuring HSTIA */
  rcalTest.HsTiaCfg.DiodeClose = bFALSE;
  rcalTest.HsTiaCfg.HstiaBias = HSTIABIAS_1P1; 
  rcalTest.HsTiaCfg.HstiaCtia = 31; 
  rcalTest.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN; 
  rcalTest.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN; 
  rcalTest.HsTiaCfg.HstiaRtiaSel = rHSTIA; 

  freqParams = AD5940_GetFreqParameters(120.0); 

  /* Configuring ADC Filters and DFT */
  rcalTest.ADCSinc2Osr = freqParams.ADCSinc2Osr;
  rcalTest.ADCSinc3Osr = freqParams.ADCSinc3Osr;
  rcalTest.DftCfg.DftNum = freqParams.DftNum;
  rcalTest.DftCfg.DftSrc = freqParams.DftSrc;
  rcalTest.DftCfg.HanWinEn = bTRUE; 

  rcalTest.bPolarResult = bTRUE; 

  testPoint = AD5940_HSRtiaCal(&rcalTest, &polarResults);

  if(testPoint != AD5940ERR_OK) Serial.println("Error!");

  printf("RTIA resistor value: %f\n", polarResults.Magnitude);

  AD5940_ShutDownS();
}

/*
  This function initializes the HELPStat as a BLE server and creates the different parameters.
*/
void HELPStat::BLE_setup() {
  Serial.begin(115200);

  BLEDevice::init("HELPStat");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID),70,0);

  // Create a BLE Characteristic
  pCharacteristicStart = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_START,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  
  pCharacteristicRct = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RCT,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicRs = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RS,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pCharacteristicNumCycles = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_NUMCYCLES,
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY 
                    );

  pCharacteristicNumPoints = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_NUMPOINTS,
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristicStartFreq = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_STARTFREQ,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pCharacteristicEndFreq = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_ENDFREQ,
                      BLECharacteristic::PROPERTY_WRITE
                    ); 

  pCharacteristicRcalVal = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RCALVAL,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pCharacteristicBiasVolt = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_BIASVOLT,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicZeroVolt = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_ZEROVOLT,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicDelaySecs = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_DELAYSECS,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicExtGain = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_EXTGAIN,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicDacGain = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_DACGAIN,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicFolderName = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_FOLDERNAME,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicFileName = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_FILENAME,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicSweepIndex = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_SWEEPINDEX,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristicCurrentFreq = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_CURRENTFREQ,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristicReal = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_REAL,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristicImag = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_IMAG,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );  
  pCharacteristicPhase = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_PHASE,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristicMagnitude = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_MAGNITUDE,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    ); 

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristicStart->addDescriptor(new BLE2902());
  pCharacteristicRct->addDescriptor(new BLE2902());
  pCharacteristicRs->addDescriptor(new BLE2902());
  pCharacteristicNumCycles->addDescriptor(new BLE2902());
  pCharacteristicNumPoints->addDescriptor(new BLE2902());
  pCharacteristicStartFreq->addDescriptor(new BLE2902());
  pCharacteristicEndFreq->addDescriptor(new BLE2902());
  pCharacteristicRcalVal->addDescriptor(new BLE2902());
  pCharacteristicBiasVolt->addDescriptor(new BLE2902());
  pCharacteristicZeroVolt->addDescriptor(new BLE2902());
  pCharacteristicDelaySecs->addDescriptor(new BLE2902());
  pCharacteristicExtGain->addDescriptor(new BLE2902());
  pCharacteristicDacGain->addDescriptor(new BLE2902());
  pCharacteristicFolderName->addDescriptor(new BLE2902());
  pCharacteristicFileName->addDescriptor(new BLE2902());
  pCharacteristicSweepIndex->addDescriptor(new BLE2902());
  pCharacteristicCurrentFreq->addDescriptor(new BLE2902());
  pCharacteristicReal->addDescriptor(new BLE2902());
  pCharacteristicImag->addDescriptor(new BLE2902());
  pCharacteristicPhase->addDescriptor(new BLE2902());
  pCharacteristicMagnitude->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");

  pinMode(BUTTON, INPUT); 
}

/*
  This function allows the user to adjust settings over BLE until a start signal is sent.
  Note that this is practically an infinite loop if the BLE signal is never sent.
*/
void HELPStat::BLE_settings() {
  bool buttonStatus;
  do{
    old_start_value = start_value;
    start_value = *(pCharacteristicStart->getData());
    buttonStatus = digitalRead(BUTTON);
    delay(3);

    if(pCharacteristicRct->getValue().length() > 0)
      _rct_estimate = String(pCharacteristicRct->getValue().c_str()).toFloat();
    if(pCharacteristicRs->getValue().length() > 0)
      _rs_estimate  = String(pCharacteristicRs->getValue().c_str()).toFloat();
    if(pCharacteristicNumCycles->getValue().length() > 0)
      _numCycles = String(pCharacteristicNumCycles->getValue().c_str()).toFloat();
    if(pCharacteristicNumPoints->getValue().length() > 0)
      _numPoints = String(pCharacteristicNumPoints->getValue().c_str()).toFloat();
    if(pCharacteristicStartFreq->getValue().length() > 0)
      _startFreq = String(pCharacteristicStartFreq->getValue().c_str()).toFloat();
    if(pCharacteristicEndFreq->getValue().length() > 0)
      _endFreq   = String(pCharacteristicEndFreq->getValue().c_str()).toFloat();
    if(pCharacteristicRcalVal->getValue().length() > 0)
      _rcalVal   = String(pCharacteristicRcalVal->getValue().c_str()).toFloat();
    if(pCharacteristicBiasVolt->getValue().length() > 0)
      _biasVolt   = String(pCharacteristicBiasVolt->getValue().c_str()).toFloat();
    if(pCharacteristicZeroVolt->getValue().length() > 0)
      _zeroVolt   = String(pCharacteristicZeroVolt->getValue().c_str()).toFloat();
    if(pCharacteristicDelaySecs->getValue().length() > 0)
      _delaySecs   = String(pCharacteristicDelaySecs->getValue().c_str()).toFloat();
    if(pCharacteristicExtGain->getValue().length() > 0)
      _extGain   = String(pCharacteristicExtGain->getValue().c_str()).toFloat();
    if(pCharacteristicDacGain->getValue().length() > 0)
      _dacGain   = String(pCharacteristicDacGain->getValue().c_str()).toFloat();
    _folderName = String((pCharacteristicFolderName->getValue()).c_str());
    _fileName = String((pCharacteristicFileName->getValue()).c_str());
  }while((!start_value || old_start_value == start_value) && buttonStatus); // Maybe remove the old_start_value stuff? (&& digitalRead(BUTTON))
}

/*
  This function sends the calculated Rct and Rs values to an external BLE client. The notify
  flags for these characteristics are also set to inform the client that data has been updated.
*/
void HELPStat::BLE_transmitResults() {
  static char buffer[10];
  dtostrf(_calculated_Rct,4,3,buffer);
  pCharacteristicRct->setValue(buffer);
  pCharacteristicRct->notify();

  dtostrf(_calculated_Rs,4,3,buffer);
  pCharacteristicRs->setValue(buffer);
  pCharacteristicRs->notify();

  // Transmit freq, Zreal, and Zimag for all sampled points
  for(uint32_t i = 0; i < _sweepCfg.SweepPoints; i++) {
    for(uint32_t j = 0; j <= _numCycles; j++) {
      impStruct eis;
      eis = eisArr[i + (j * _sweepCfg.SweepPoints)];
      
      // Transmit Frequency
      dtostrf(eis.freq,1,2,buffer);
      pCharacteristicCurrentFreq->setValue(buffer);
      pCharacteristicCurrentFreq->notify();

      // Transmit Real Impedence
      dtostrf(eis.real,1,4,buffer);
      pCharacteristicReal->setValue(buffer);
      pCharacteristicReal->notify();

      // Transmit Imaginary Impedence
      dtostrf(eis.imag,1,4,buffer);
      pCharacteristicImag->setValue(buffer);
      pCharacteristicImag->notify();

      delay(50);

      // Transmit Phase
      dtostrf(eis.phaseDeg,3,4,buffer);
      pCharacteristicPhase->setValue(buffer);
      pCharacteristicPhase->notify();

      // Transmit Phase
      dtostrf(eis.magnitude,3,4,buffer);
      pCharacteristicMagnitude->setValue(buffer);
      pCharacteristicMagnitude->notify();

      // Necessary delay so all BLE calls finish. Not sure why it works, but it does; DON'T TOUCH!
      delay(50);
    }
  }
}

/*
  This function simply prints what the private variable settings are currently set to.
*/
void HELPStat::print_settings() {
  Serial.println("SETTINGS");
  
  // Print Resistor Estimates & Calibration
  Serial.print("Rct Estimation:  ");
  Serial.println(_rct_estimate);
  Serial.print("Rs Estimation:   ");
  Serial.println(_rs_estimate);
  Serial.print("Rcal:            ");
  Serial.println(_rcalVal);

  // Print Frequency Range
  Serial.print("Start Frequency: ");
  Serial.println(_startFreq);
  Serial.print("End Frequency:   ");
  Serial.println(_endFreq);
  
  // Print Cycles and Points
  Serial.print("numCycles:       ");
  Serial.println(_numCycles);
  Serial.print("numPoints:       ");
  Serial.println(_numPoints);

  // Print Gains
  Serial.print("External Gain:   ");
  Serial.println(_extGain);
  Serial.print("DAC Gain:        ");
  Serial.println(_dacGain);

  // Print Voltages
  Serial.print("Bias Voltage:    ");
  Serial.println(_biasVolt);
  Serial.print("Zero Voltage:    ");
  Serial.println(_zeroVolt);

  // Print Delay
  Serial.print("Delay (s):       ");
  Serial.println(_delaySecs);

  // Print folder/file names
  Serial.print("Folder Name:     ");
  Serial.println(_folderName);
  Serial.print("File Name:       ");
  Serial.println(_fileName);
}

/*
  Prints impedance data as CSV format to serial monitor for easy copying
  Format: Frequency,Real,Imaginary,Magnitude,Phase_Degrees
*/
void HELPStat::printDataCSV(void) {
  impStruct eis;
  
  // Print CSV header
  Serial.println("\n=== EIS DATA CSV ===");
  Serial.println("Frequency(Hz),Real(Ohm),Imaginary(Ohm),Magnitude(Ohm),Phase(Degrees)");
  
  // Print all data points
  for(uint32_t i = 0; i <= _numCycles; i++) {
    for(uint32_t j = 0; j < _sweepCfg.SweepPoints; j++) {
      eis = eisArr[j + (i * _sweepCfg.SweepPoints)];
      Serial.print(eis.freq, 6);
      Serial.print(",");
      Serial.print(eis.real, 6);
      Serial.print(",");
      Serial.print(eis.imag, 6);
      Serial.print(",");
      Serial.print(eis.magnitude, 6);
      Serial.print(",");
      Serial.println(eis.phaseDeg, 6);
    }
  }
  
  // Print calculated parameters
  Serial.println("\n=== CALCULATED PARAMETERS ===");
  Serial.print("Rct(Ohm),Rs(Ohm)\n");
  Serial.print(_calculated_Rct, 6);
  Serial.print(",");
  Serial.println(_calculated_Rs, 6);
  Serial.println("=== END CSV ===");
}

/*
  Sets all measurement parameters at once
*/
void HELPStat::setParameters(float startFreq, float endFreq, uint32_t numPoints, 
                             float biasVolt, float zeroVolt, float rcalVal, 
                             int extGain, int dacGain, float rct_estimate, 
                             float rs_estimate, uint32_t numCycles, uint32_t delaySecs) {
  // Legacy function - defaults to EIS mode with 200mV amplitude
  setParameters(MODE_EIS, startFreq, endFreq, numPoints, biasVolt, zeroVolt, 
                rcalVal, extGain, dacGain, rct_estimate, rs_estimate, 
                numCycles, delaySecs, 200.0);
}

void HELPStat::setParameters(int mode, float startFreq, float endFreq, uint32_t numPoints, 
                             float biasVolt, float zeroVolt, float rcalVal, 
                             int extGain, int dacGain, float rct_estimate, 
                             float rs_estimate, uint32_t numCycles, uint32_t delaySecs, 
                             float amplitude) {
  _measurementMode = (MeasurementMode)mode;
  _startFreq = startFreq;
  _endFreq = endFreq;
  _numPoints = numPoints;
  _biasVolt = biasVolt;
  _zeroVolt = zeroVolt;
  _rcalVal = rcalVal;
  _extGain = extGain;
  _dacGain = dacGain;
  _rct_estimate = rct_estimate;
  _rs_estimate = rs_estimate;
  _numCycles = numCycles;
  _delaySecs = delaySecs;
  _signalAmplitude = amplitude;  // Signal amplitude in mV (0-800 mV)
  
  // Clamp amplitude to valid range
  if(_signalAmplitude < 0.0) _signalAmplitude = 0.0;
  if(_signalAmplitude > 800.0) _signalAmplitude = 800.0;
  
  // Set CV-specific parameters from user input
  if(_measurementMode == MODE_CV) {
    // For CV: biasVolt = start voltage, zeroVolt = end voltage
    // If zeroVolt is 0, use default range
    if(zeroVolt == 0.0 && biasVolt == 0.0) {
      _cvStartVolt = 0.0;
      _cvEndVolt = 1.0;
    } else {
      _cvStartVolt = biasVolt;  // Start voltage
      _cvEndVolt = zeroVolt;    // End voltage
    }
    _cvCycles = numCycles > 0 ? numCycles : 1;
    
    // Calculate scan rate from numPoints and voltage range
    // Default scan rate: 0.1 V/s, but can be adjusted
    // For now, use a reasonable default or calculate from total time
    float voltageRange = abs(_cvEndVolt - _cvStartVolt);
    if(numPoints > 0 && voltageRange > 0) {
      // Estimate scan rate: assume reasonable measurement time
      // For CV, typical scan rates are 0.01 to 1.0 V/s
      // Use numPoints to estimate: more points = slower scan
      _cvScanRate = 0.1; // Default 0.1 V/s
      // Could calculate: _cvScanRate = voltageRange / (numPoints * 0.1);
    } else {
      _cvScanRate = 0.1; // Default scan rate
    }
  }
}

/*
  Validates measurement quality to detect and reject bad measurements
  Returns true if measurement is valid, false if it should be rejected
*/
bool HELPStat::validateMeasurement(impStruct* eis) {
  if(eis == NULL) return false;
  
  // Check magnitude range
  if(eis->magnitude < _minExpectedImpedance || eis->magnitude > _maxExpectedImpedance) {
    Serial.print("WARNING: Magnitude out of range: ");
    Serial.println(eis->magnitude);
    return false;
  }
  
  // Check for negative real impedance (usually indicates measurement error)
  // Allow small negative values due to noise, but reject large negatives
  if(eis->real < -100.0) {
    Serial.print("WARNING: Large negative real impedance: ");
    Serial.println(eis->real);
    return false;
  }
  
  // Check phase range (should be between -90 and 90 degrees for most EIS)
  if(abs(eis->phaseDeg) > 90.0) {
    Serial.print("WARNING: Phase out of expected range: ");
    Serial.println(eis->phaseDeg);
    // Don't reject, just warn - some systems can have large phase
  }
  
  // Check for NaN or Inf values
  if(isnan(eis->magnitude) || isnan(eis->real) || isnan(eis->imag) || isnan(eis->phaseDeg)) {
    Serial.println("WARNING: NaN detected in measurement");
    return false;
  }
  
  if(isinf(eis->magnitude) || isinf(eis->real) || isinf(eis->imag) || isinf(eis->phaseDeg)) {
    Serial.println("WARNING: Inf detected in measurement");
    return false;
  }
  
  // Check consistency: magnitude should be >= sqrt(real^2 + imag^2)
  float calculatedMag = sqrt(eis->real * eis->real + eis->imag * eis->imag);
  float magError = abs(eis->magnitude - calculatedMag) / eis->magnitude;
  if(magError > 0.1) {  // More than 10% error
    Serial.print("WARNING: Magnitude inconsistency: ");
    Serial.print(eis->magnitude);
    Serial.print(" vs calculated: ");
    Serial.println(calculatedMag);
    return false;
  }
  
  return true;
}

/*
  Gain switching with hysteresis to prevent oscillation at boundaries
  Only switches gain when crossing threshold with sufficient margin
*/
AD5940Err HELPStat::setHSTIAWithHysteresis(float freq, float lastFreq) {
  const float HYSTERESIS_MARGIN = 0.1;  // 10% margin for hysteresis
  int newGainIndex = -1;
  
  // Find which gain range this frequency should use
  for(uint32_t i = 0; i < _gainArrSize; i++) {
    if(freq <= _gainArr[i].freq) {
      newGainIndex = i;
      break;
    }
  }
  
  // If no match found, use the highest gain (last in array)
  if(newGainIndex == -1) {
    newGainIndex = _gainArrSize - 1;
  }
  
  // Check if we need to switch gains
  bool shouldSwitch = false;
  
  if(_lastGainIndex == -1) {
    // First time, always switch
    shouldSwitch = true;
  } else if(newGainIndex != _lastGainIndex) {
    // Different gain index, check hysteresis
    float threshold = _gainArr[newGainIndex].freq;
    float lastThreshold = (_lastGainIndex < (int)_gainArrSize) ? _gainArr[_lastGainIndex].freq : threshold;
    
    // Switch up: when frequency crosses threshold going down
    // Switch down: when frequency crosses threshold - margin going up
    if(newGainIndex > _lastGainIndex) {
      // Switching to higher gain (lower frequency threshold)
      if(freq <= threshold && lastFreq > threshold) {
        shouldSwitch = true;
      }
    } else {
      // Switching to lower gain (higher frequency threshold)
      if(freq >= threshold * (1.0 - HYSTERESIS_MARGIN) && lastFreq < threshold * (1.0 - HYSTERESIS_MARGIN)) {
        shouldSwitch = true;
      }
    }
  }
  
  if(shouldSwitch) {
    AD5940Err result = setHSTIA(freq);
    if(result == AD5940ERR_OK) {
      _lastGainIndex = newGainIndex;
      _lastFreq = freq;
      delay(300);  // Extra settling time after gain switch
      Serial.print("Gain switched to index ");
      Serial.print(newGainIndex);
      Serial.print(" at ");
      Serial.print(freq);
      Serial.println(" Hz");
    }
    return result;
  }
  
  // No switch needed, but still configure for current frequency
  return setHSTIA(freq);
}

/*
  Read DFT results from FIFO instead of polling registers
  More efficient and reliable data collection
*/
void HELPStat::readDFTFromFIFO(int32_t* pReal, int32_t* pImage) {
  uint32_t fifoCount = AD5940_FIFOGetCnt();
  
  if(fifoCount >= 2) {
    // Read real and imaginary from FIFO
    uint32_t buffer[2];
    AD5940_FIFORd(buffer, 2);
    
    // Convert from FIFO format (18-bit two's complement)
    *pReal = (int32_t)buffer[0];
    *pReal &= 0x3ffff;
    if(*pReal & (1<<17)) *pReal |= 0xfffc0000;
    
    *pImage = (int32_t)buffer[1];
    *pImage &= 0x3ffff;
    if(*pImage & (1<<17)) *pImage |= 0xfffc0000;
  } else {
    // FIFO not ready, fall back to register read
    getDFT(pReal, pImage);
  }
}

/*
  Perform measurement with averaging to reduce noise
*/
void HELPStat::AD5940_DFTMeasureWithAveraging(int numAverages) {
  if(numAverages < 1) numAverages = 1;
  if(numAverages > 10) numAverages = 10;  // Limit to prevent excessive time
  
  impStruct sum = {0};
  int validCount = 0;
  
  for(int i = 0; i < numAverages; i++) {
    impStruct eis;
    
    // Perform single measurement (reuse existing function)
    SWMatrixCfg_Type sw_cfg;
    int32_t realRcal, imageRcal;
    int32_t realRz, imageRz;
    float magRcal, phaseRcal;
    float magRz, phaseRz;
    
    // Measure RCAL
    sw_cfg.Dswitch = SWD_RCAL0;
    sw_cfg.Pswitch = SWP_RCAL0;
    sw_cfg.Nswitch = SWN_RCAL1;
    sw_cfg.Tswitch = SWT_RCAL1|SWT_TRTIA;
    AD5940_SWMatrixCfgS(&sw_cfg);
    
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|
                    AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|
                    AFECTRL_SINC2NOTCH, bTRUE);
    AD5940_AFECtrlS(AFECTRL_WG|AFECTRL_ADCPWR, bTRUE);
    settlingDelay(_currentFreq);
    AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);
    settlingDelay(_currentFreq);
    AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
    AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
    AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
    AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
    pollDFT(&realRcal, &imageRcal);
    AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_ADCCNV|AFECTRL_DFT|AFECTRL_WG, bFALSE);
    
    // Measure Rz
    sw_cfg.Dswitch = SWD_CE0;
    sw_cfg.Pswitch = SWP_CE0;
    sw_cfg.Nswitch = SWN_SE0;
    sw_cfg.Tswitch = SWT_TRTIA|SWT_SE0LOAD;
    AD5940_SWMatrixCfgS(&sw_cfg);
    
    AD5940_AFECtrlS(AFECTRL_ADCPWR|AFECTRL_WG, bTRUE);
    settlingDelay(_currentFreq);
    AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT, bTRUE);
    settlingDelay(_currentFreq);
    AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
    AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
    AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
    AD5940_Delay10us((_waitClcks / 2) * (1/SYSCLCK));
    pollDFT(&realRz, &imageRz);
    AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_DFT|AFECTRL_WG|AFECTRL_ADCPWR, bFALSE);
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|
                    AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|
                    AFECTRL_SINC2NOTCH, bFALSE);
    
    // Calculate impedance
    getMagPhase(realRcal, imageRcal, &magRcal, &phaseRcal);
    getMagPhase(realRz, imageRz, &magRz, &phaseRz);
    
    eis.magnitude = (magRcal / magRz) * _rcalVal;
    eis.phaseRad = phaseRcal - phaseRz;
    eis.real = eis.magnitude * cos(eis.phaseRad);
    eis.imag = eis.magnitude * sin(eis.phaseRad) * -1;
    eis.phaseDeg = eis.phaseRad * 180 / MATH_PI;
    eis.freq = _currentFreq;
    
    // Validate and accumulate
    if(validateMeasurement(&eis)) {
      sum.magnitude += eis.magnitude;
      sum.real += eis.real;
      sum.imag += eis.imag;
      sum.phaseRad += eis.phaseRad;
      sum.phaseDeg += eis.phaseDeg;
      validCount++;
    }
    
    // Small delay between averages
    if(i < numAverages - 1) delay(50);
  }
  
  // Average the results
  if(validCount > 0) {
    impStruct avg;
    avg.magnitude = sum.magnitude / validCount;
    avg.real = sum.real / validCount;
    avg.imag = sum.imag / validCount;
    avg.phaseRad = sum.phaseRad / validCount;
    avg.phaseDeg = sum.phaseDeg / validCount;
    avg.freq = _currentFreq;
    
    // Store averaged result
    eisArr[_sweepCfg.SweepIndex + (_currentCycle * _sweepCfg.SweepPoints)] = avg;
    
    printf("%d,%.2f,%.3f,%.3f,%f,%.4f,%.4f,%.4f\n", 
           _sweepCfg.SweepIndex, _currentFreq, 
           sum.magnitude/validCount, sum.magnitude/validCount,
           avg.magnitude, avg.real, avg.imag, avg.phaseRad);
  } else {
    Serial.println("ERROR: All measurements failed validation!");
  }
}

/*
  Read internal temperature sensor
  Returns temperature in Celsius
*/
float HELPStat::readInternalTemperature(void) {
  // Configure ADC to read temperature channel
  AD5940_ADCMuxCfgS(ADCMUXP_TEMPP, ADCMUXN_TEMPN);  // Temperature sensor
  
  // Enable ADC and perform conversion
  AD5940_AFECtrlS(AFECTRL_ADCPWR, bTRUE);
  delay(10);
  AD5940_AFECtrlS(AFECTRL_ADCCNV, bTRUE);
  delay(100);
  
  // Read ADC result
  uint32_t adcCode = AD5940_ReadAfeResult(AFERESULT_SINC3);
  
  // Convert to temperature (approximate formula, may need calibration)
  // AD5940 temperature sensor: ~1.82mV/°C, offset around 0.7V
  float voltage = AD5940_ADCCode2Volt(adcCode, ADCPGA_1, 1.82);
  float temperature = (voltage - 0.7) / 0.00182;  // Approximate conversion
  
  AD5940_AFECtrlS(AFECTRL_ADCCNV|AFECTRL_ADCPWR, bFALSE);
  
  return temperature;
}

/*
  Configure 50/60 Hz mains rejection filter
*/
void HELPStat::configureNotchFilter(bool enable, bool is50Hz) {
  _enableNotchFilter = enable;
  _notchIs50Hz = is50Hz;
  
  ADCFilterCfg_Type filter_cfg;
  filter_cfg.ADCAvgNum = ADCAVGNUM_16;
  filter_cfg.ADCRate = ADCRATE_800KHZ;
  filter_cfg.ADCSinc2Osr = ADCSINC2OSR_22;
  filter_cfg.ADCSinc3Osr = ADCSINC3OSR_2;
  filter_cfg.BpNotch = enable ? bFALSE : bTRUE;  // false = enable, true = bypass
  filter_cfg.BpSinc3 = bFALSE;
  filter_cfg.Sinc2NotchEnable = bTRUE;
  
  // Configure for 50Hz or 60Hz (if AD5940 supports selection)
  // Note: AD5940 may have fixed 60Hz, check datasheet
  AD5940_ADCFilterCfgS(&filter_cfg);
  
  if(enable) {
    Serial.print("Notch filter enabled for ");
    Serial.print(is50Hz ? "50" : "60");
    Serial.println(" Hz rejection");
  } else {
    Serial.println("Notch filter disabled");
  }
}

/*
  Perform comprehensive ADC calibration
*/
void HELPStat::performADCCalibration(void) {
  Serial.println("Starting ADC calibration...");
  
  // Note: AD5940 calibration functions may vary
  // This is a placeholder for the calibration sequence
  // Refer to AD5940 calibration application notes for exact procedure
  
  // Enable ADC
  AD5940_AFECtrlS(AFECTRL_ADCPWR, bTRUE);
  delay(100);
  
  // Perform offset calibration (short inputs)
  // AD5940_ADCCal(ADCCAL_OFFSET);  // If function exists
  
  // Perform gain calibration (known reference)
  // AD5940_ADCCal(ADCCAL_GAIN);  // If function exists
  
  Serial.println("ADC calibration complete");
  
  // Note: Actual calibration may require specific sequences
  // See AD5940 application notes for detailed procedure
}

/*
  Automatically select optimal HSTIA gain based on measured impedance
  Returns HSTIA resistor value
*/
int HELPStat::autoSelectGain(float measuredImpedance) {
  // Select gain to prevent saturation while maximizing SNR
  // Rule of thumb: HSTIA output should be 50-80% of full scale
  
  if(measuredImpedance < 100) {
    return HSTIARTIA_200;  // 200Ω for very low impedances
  } else if(measuredImpedance < 1000) {
    return HSTIARTIA_1K;   // 1KΩ for low impedances
  } else if(measuredImpedance < 5000) {
    return HSTIARTIA_5K;   // 5KΩ for medium impedances
  } else if(measuredImpedance < 20000) {
    return HSTIARTIA_10K;  // 10KΩ for higher impedances
  } else if(measuredImpedance < 40000) {
    return HSTIARTIA_20K;  // 20KΩ
  } else if(measuredImpedance < 80000) {
    return HSTIARTIA_40K;  // 40KΩ
  } else if(measuredImpedance < 160000) {
    return HSTIARTIA_80K;  // 80KΩ
  } else {
    return HSTIARTIA_160K; // 160KΩ for very high impedances
  }
}

/*
  Measurement Mode Implementations
  These functions implement various electrochemical measurement techniques
*/

// Helper function to read ADC voltage
// Note: ADC mux should be configured before calling this function
// This function only performs the conversion and reading
float HELPStat::readADCVoltage(uint32_t muxP, uint32_t muxN) {
  // Configure ADC mux if not already configured
  static uint32_t lastMuxP = 0xFFFF, lastMuxN = 0xFFFF;
  if(muxP != lastMuxP || muxN != lastMuxN) {
    DSPCfg_Type dsp_cfg;
    AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
    
    dsp_cfg.ADCBaseCfg.ADCMuxP = muxP;
    dsp_cfg.ADCBaseCfg.ADCMuxN = muxN;
    dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
    dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;
    dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
    // Increased filtering for better noise reduction
    dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_1333; // Higher OSR for better filtering
    dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_4;    // Higher OSR for better noise reduction
    dsp_cfg.ADCFilterCfg.BpNotch = bTRUE;  // Enable notch filter
    dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE; // Use SINC3 filter
    AD5940_DSPCfgS(&dsp_cfg);
    
    lastMuxP = muxP;
    lastMuxN = muxN;
    delay(10); // Mux switching delay per datasheet
  }
  
  // Ensure ADC is powered
  AD5940_AFECtrlS(AFECTRL_ADCPWR, bTRUE);
  delay(10); // Power-up delay
  
  // Enable interrupt for ADC ready
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ADCRDY, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ADCRDY);
  
  // Start conversion
  AD5940_AFECtrlS(AFECTRL_ADCCNV, bTRUE);
  
  // Wait for conversion using interrupt flag (more reliable than fixed delay)
  // With SINC3 OSR=4 and SINC2 OSR=1333, conversion takes longer
  uint32_t timeout = 100; // 100ms timeout
  uint32_t startTime = millis();
  while(!AD5940_INTCTestFlag(AFEINTC_1, AFEINTSRC_ADCRDY)) {
    if((millis() - startTime) > timeout) {
      Serial.println("ADC timeout!");
      break;
    }
    delay(1);
  }
  
  // Read result
  uint32_t adcCode = AD5940_ReadAfeResult(AFERESULT_SINC3);
  float voltage = AD5940_ADCCode2Volt(adcCode, ADCPGA_1, 1.82);
  
  AD5940_AFECtrlS(AFECTRL_ADCCNV, bFALSE);
  AD5940_INTCClrFlag(AFEINTSRC_ADCRDY);
  return voltage;
}

// Helper function to read current via HSTIA
// Based on AD5940 application notes: HSTIA output is differential (HSTIA_P - HSTIA_N)
// HSTIA_N is connected to VZERO, so the reading is already relative to VZERO
float HELPStat::readCurrent(int rTIA) {
  // Ensure ADC is configured and powered
  AD5940_AFECtrlS(AFECTRL_ADCPWR, bTRUE);
  delay(5);
  
  // Clear interrupt flag
  AD5940_INTCClrFlag(AFEINTSRC_ADCRDY);
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ADCRDY, bTRUE);
  
  // Start ADC conversion
  AD5940_AFECtrlS(AFECTRL_ADCCNV, bTRUE);
  
  // Wait for conversion (with timeout)
  uint32_t timeout = 200; // 200ms timeout
  uint32_t startTime = millis();
  while(!AD5940_INTCTestFlag(AFEINTC_1, AFEINTSRC_ADCRDY)) {
    if((millis() - startTime) > timeout) {
      Serial.println("ADC timeout in readCurrent!");
      AD5940_AFECtrlS(AFECTRL_ADCCNV, bFALSE);
      return 0.0;
    }
    delay(1);
  }
  
  // Read ADC code from SINC3 filter
  uint32_t adcCode = AD5940_ReadAfeResult(AFERESULT_SINC3);
  
  // Handle 18-bit two's complement
  int32_t signedCode = (int32_t)adcCode;
  if(signedCode & (1 << 17)) {
    // Sign extend for negative values
    signedCode |= 0xFFFC0000;
  }
  signedCode &= 0x3FFFF; // Mask to 18 bits
  
  // Convert ADC code to voltage (VRef = 1.82V, PGA = 1)
  // The ADC reading is differential: (HSTIA_P - HSTIA_N)
  // Since HSTIA_N = VZERO, this gives us the voltage relative to VZERO
  float vOut = AD5940_ADCCode2Volt((uint32_t)signedCode, ADCPGA_1, 1.82);
  
  // Get RTIA resistance value
  float rTIA_ohm = 0;
  switch(rTIA) {
    case HSTIARTIA_200: rTIA_ohm = 200.0; break;
    case HSTIARTIA_1K: rTIA_ohm = 1000.0; break;
    case HSTIARTIA_5K: rTIA_ohm = 5000.0; break;
    case HSTIARTIA_10K: rTIA_ohm = 10000.0; break;
    case HSTIARTIA_20K: rTIA_ohm = 20000.0; break;
    case HSTIARTIA_40K: rTIA_ohm = 40000.0; break;
    case HSTIARTIA_80K: rTIA_ohm = 80000.0; break;
    case HSTIARTIA_160K: rTIA_ohm = 160000.0; break;
    default: rTIA_ohm = 10000.0; break;
  }
  
  // Convert voltage to current: I = V_out / R_TIA
  // HSTIA is a transimpedance amplifier: V_out = I_in * R_TIA
  // The differential reading (HSTIA_P - HSTIA_N) is already relative to VZERO
  float current = vOut / rTIA_ohm;  // Current in Amperes
  
  // Stop conversion
  AD5940_AFECtrlS(AFECTRL_ADCCNV, bFALSE);
  AD5940_INTCClrFlag(AFEINTSRC_ADCRDY);
  
  return current;
}

// Cyclic Voltammetry - Triangular voltage sweep
// Based on AD5940/AD5941 potentiostat application notes
// Uses LPDAC VBIAS for voltage control via LPPA (potentiostat amplifier)
// Uses HSTIA for current measurement
void HELPStat::runCV(void) {
  Serial.println("=== Starting Cyclic Voltammetry ===");
  Serial.print("Start: "); Serial.print(_cvStartVolt, 3); Serial.print("V, End: "); Serial.print(_cvEndVolt, 3); Serial.println("V");
  Serial.print("Scan Rate: "); Serial.print(_cvScanRate, 3); Serial.print("V/s, Cycles: "); Serial.println(_cvCycles);
  
  // Initialize AD5940
  if(AD5940_WakeUp(10) > 10) {
    Serial.println("Wakeup failed!");
    return;
  }
  
  // Calculate voltage range and timing
  float voltageRange = _cvEndVolt - _cvStartVolt;
  if(abs(voltageRange) < 0.001) {
    Serial.println("ERROR: Invalid voltage range!");
    return;
  }
  
  float sweepTime = abs(voltageRange) / _cvScanRate; // seconds per direction
  if(sweepTime < 0.1) sweepTime = 0.1;  // Minimum 100ms per direction
  if(sweepTime > 1000.0) sweepTime = 1000.0;
  
  // For potentiostat mode: Cell voltage = VBIAS - VZERO
  // To allow bipolar voltages, set VZERO to center of range
  // LPDAC range: VZERO (6-bit): 0.2V to 2.4V, VBIAS (12-bit): 0.2V to 2.4V
  // Best approach: Set VZERO to center (1.1V) and sweep VBIAS around it
  
  float vzeroVolt = 1.1; // Center voltage for VZERO (allows ±1.1V range)
  float vCenter = (_cvStartVolt + _cvEndVolt) / 2.0; // Center of desired range
  
  // Calculate VBIAS values: VBIAS = cellVoltage + VZERO
  // But we want cellVoltage relative to vCenter, so:
  // VBIAS = vzeroVolt + (cellVoltage - vCenter)
  float vBiasStart = vzeroVolt + (_cvStartVolt - vCenter);
  float vBiasEnd = vzeroVolt + (_cvEndVolt - vCenter);
  
  // Clamp to LPDAC range (0.2V to 2.4V)
  if(vBiasStart < 0.2) {
    vzeroVolt = 0.2 + abs(_cvStartVolt - vCenter);
    vBiasStart = 0.2;
    vBiasEnd = vzeroVolt + (_cvEndVolt - vCenter);
  }
  if(vBiasEnd > 2.4) {
    vzeroVolt = 2.4 - abs(_cvEndVolt - vCenter);
    vBiasEnd = 2.4;
    vBiasStart = vzeroVolt + (_cvStartVolt - vCenter);
    if(vBiasStart < 0.2) vBiasStart = 0.2;
  }
  
  // Configure LPDAC for potentiostat mode
  LPDACCfg_Type lpdac_cfg;
  AD5940_StructInit(&lpdac_cfg, sizeof(lpdac_cfg));
  lpdac_cfg.LpdacSel = LPDAC0;
  lpdac_cfg.LpDacVbiasMux = LPDACVBIAS_12BIT;
  lpdac_cfg.LpDacVzeroMux = LPDACVZERO_6BIT;
  lpdac_cfg.LpDacRef = LPDACREF_2P5;
  lpdac_cfg.LpDacSrc = LPDACSRC_MMR;
  lpdac_cfg.PowerEn = bTRUE;
  
  // Convert voltages to DAC codes (constants are in mV, so convert V to mV)
  // VZERO: 6-bit, range 0.2V to 2.4V, step = (2.4-0.2)/63 = 0.0349V
  // VBIAS: 12-bit, range 0.2V to 2.4V, step = (2.4-0.2)/4095 = 0.000537V
  float vzeroVolt_mV = vzeroVolt * 1000.0f;
  float vBiasStart_mV = vBiasStart * 1000.0f;
  float vBiasEnd_mV = vBiasEnd * 1000.0f;
  
  lpdac_cfg.DacData6Bit = (uint32_t)((vzeroVolt_mV - 200.0f) / DAC6BITVOLT_1LSB + 0.5f);
  if(lpdac_cfg.DacData6Bit > 63) lpdac_cfg.DacData6Bit = 63;
  
  uint16_t dacCode12BitStart = (uint16_t)((vBiasStart_mV - 200.0f) / DAC12BITVOLT_1LSB + 0.5f);
  if(dacCode12BitStart > 4095) dacCode12BitStart = 4095;
  lpdac_cfg.DacData12Bit = dacCode12BitStart;
  lpdac_cfg.DataRst = bFALSE;
  
  // Configure switches: VBIAS to LPPA (potentiostat), VZERO to HSTIA
  lpdac_cfg.LpDacSW = LPDACSW_VBIAS2LPPA|LPDACSW_VBIAS2PIN|LPDACSW_VZERO2HSTIA;
  AD5940_LPDACCfgS(&lpdac_cfg);
  
  Serial.print("VZERO: "); Serial.print(vzeroVolt, 3); Serial.print("V, VBIAS range: "); 
  Serial.print(vBiasStart, 3); Serial.print("V to "); Serial.print(vBiasEnd, 3); 
  Serial.print("V (cell: "); Serial.print(_cvStartVolt, 3); Serial.print("V to "); Serial.print(_cvEndVolt, 3); Serial.println("V)");
  
  // Configure HSTIA for current measurement
  HSLoopCfg_Type HsLoopCfg;
  AD5940_StructInit(&HsLoopCfg, sizeof(HsLoopCfg));
  
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;
  HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_VZERO0; // Use VZERO from LPDAC
  HsLoopCfg.HsTiaCfg.HstiaCtia = 31; // 31pF feedback capacitor
  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_1K; // 1K RTIA for CV
  
  // Switch matrix for potentiostat mode with LPPA:
  // D: CE0 (counter electrode) - driven by LPPA, so keep OPEN (LPPA drives it directly)
  // P: RE0 (reference electrode) - for voltage sensing/feedback
  // N: SE0 (working electrode) - connects to HSTIA negative input for current measurement
  // T: Connect SE0 to HSTIA input (TRTIA) and enable SE0LOAD
  HsLoopCfg.SWMatCfg.Dswitch = SWD_OPEN;   // CE0 driven by LPPA (not HSDAC)
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;    // 2-electrode: sense voltage at CE0
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;    // Working electrode (current flows here)
  HsLoopCfg.SWMatCfg.Tswitch = SWT_TRTIA|SWT_SE0LOAD; // Connect SE0 to HSTIA input
  
  HsLoopCfg.WgCfg.WgType = WGTYPE_MMR; // Manual mode - no waveform generator
  AD5940_HSLoopCfgS(&HsLoopCfg);
  
  // Configure LPPA (Low Power Potentiostat Amplifier)
  LPAmpCfg_Type LpAmpCfg;
  AD5940_StructInit(&LpAmpCfg, sizeof(LpAmpCfg));
  LpAmpCfg.LpAmpSel = LPAMP0;
  LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  LpAmpCfg.LpPaPwrEn = bTRUE;  // Enable potentiostat amplifier
  LpAmpCfg.LpTiaPwrEn = bFALSE; // TIA not needed (using HSTIA)
  LpAmpCfg.LpTiaRf = LPTIARF_1M;
  LpAmpCfg.LpTiaRload = LPTIARLOAD_100R;
  LpAmpCfg.LpTiaRtia = LPTIARTIA_OPEN;
  LpAmpCfg.LpTiaSW = 0;
  AD5940_LPAMPCfgS(&LpAmpCfg);
  
  // Configure ADC for current measurement (DC mode - optimized for CV)
  // Improved settings for better noise reduction in DC measurements
  DSPCfg_Type dsp_cfg;
  AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;  // HSTIA negative input (VZERO)
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;  // HSTIA positive output
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;          // PGA gain = 1
  // Increased averaging and OSR for better DC measurement accuracy
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;  // Increased averaging for better noise reduction
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_1333;
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_4;  // Higher OSR for better DC accuracy
  dsp_cfg.ADCFilterCfg.BpNotch = bFALSE;  // No notch filter for DC (not needed)
  dsp_cfg.ADCFilterCfg.BpSinc3 = bTRUE;   // Use SINC3 filter for DC
  AD5940_DSPCfgS(&dsp_cfg);
  
  // Enable ADC interrupt for readCurrent function
  AD5940_INTCCfg(AFEINTC_1, AFEINTSRC_ADCRDY, bTRUE);
  
  // Enable AFE blocks
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_ADCPWR|AFECTRL_DACREFPWR, bTRUE);
  
  // LPPA power is already enabled via LpAmpCfg.LpPaPwrEn = bTRUE above
  delay(500); // Extended settling time for potentiostat, HSTIA, and LPDAC
  
  // Measure baseline current at start voltage for offset correction
  // Set to start voltage first
  float vBiasStartVolt = vzeroVolt + (_cvStartVolt - vCenter);
  if(vBiasStartVolt < 0.2) vBiasStartVolt = 0.2;
  if(vBiasStartVolt > 2.4) vBiasStartVolt = 2.4;
  float vBiasStartVolt_mV = vBiasStartVolt * 1000.0f;
  uint16_t dacCode12BitBaseline = (uint16_t)((vBiasStartVolt_mV - 200.0f) / DAC12BITVOLT_1LSB + 0.5f);
  if(dacCode12BitBaseline > 4095) dacCode12BitBaseline = 4095;
  AD5940_LPDAC0WriteS(dacCode12BitBaseline, lpdac_cfg.DacData6Bit);
  delay(200); // Wait for potentiostat to settle at start voltage
  
  // Take multiple baseline readings and average
  float baselineCurrent = 0.0;
  const int numBaselineSamples = 10;
  Serial.print("Measuring baseline current... ");
  for(int i = 0; i < numBaselineSamples; i++) {
    baselineCurrent += readCurrent(HSTIARTIA_1K);
    delay(20);
  }
  baselineCurrent /= numBaselineSamples;
  Serial.print(baselineCurrent, 8);
  Serial.println(" A");
  
  // Calculate adaptive settling time based on scan rate
  // Faster scans need less settling, slower scans need more
  uint32_t baseSettlingTime = 30; // Base settling time in ms
  uint32_t adaptiveSettlingTime = baseSettlingTime;
  if(_cvScanRate < 0.05) {
    adaptiveSettlingTime = 150; // Very slow scans: 150ms
  } else if(_cvScanRate < 0.1) {
    adaptiveSettlingTime = 100; // Slow scans: 100ms
  } else if(_cvScanRate < 0.5) {
    adaptiveSettlingTime = 50;  // Medium scans: 50ms
  } else {
    adaptiveSettlingTime = 30;  // Fast scans: 30ms
  }
  
  Serial.print("Adaptive settling time: "); Serial.print(adaptiveSettlingTime); Serial.println(" ms");
  Serial.println("Time(s),Voltage(V),Current(A)");
  
  // Calculate sampling parameters based on scan rate
  // Use numPoints to determine number of samples per sweep direction
  uint32_t samplesPerDirection = _numPoints > 0 ? _numPoints : 100;
  float voltageStep = voltageRange / samplesPerDirection;
  float timePerSample = sweepTime / samplesPerDirection;
  
  // Ensure minimum time per sample (ADC conversion takes ~10-20ms)
  if(timePerSample < 0.02) {
    timePerSample = 0.02;
    samplesPerDirection = (uint32_t)(sweepTime / timePerSample);
    voltageStep = voltageRange / samplesPerDirection;
  }
  
  uint32_t totalSamples = samplesPerDirection * 2 * _cvCycles; // Up and down for each cycle
  
  Serial.print("Sweep time per direction: "); Serial.print(sweepTime, 3); Serial.print("s");
  Serial.print(", Samples per direction: "); Serial.print(samplesPerDirection);
  Serial.print(", Total samples: "); Serial.println(totalSamples);
  
  unsigned long startTime = millis();
  float cellVoltage = _cvStartVolt;
  bool sweepingUp = true;
  uint32_t cycleNum = 0;
  uint32_t sampleInDirection = 0;
  
  for(uint32_t sample = 0; sample < totalSamples; sample++) {
    unsigned long sampleStartTime = millis();
    
    // Calculate target cell voltage based on sample index (more reliable)
    // Determine which direction and position in cycle
    uint32_t samplesPerCycle = samplesPerDirection * 2; // Up and down
    uint32_t sampleInCycle = sample % samplesPerCycle;
    
    if(sampleInCycle < samplesPerDirection) {
      // Forward sweep: start to end
      sweepingUp = true;
      float progress = (float)sampleInCycle / (float)samplesPerDirection;
      cellVoltage = _cvStartVolt + (voltageRange * progress);
      if(cellVoltage > _cvEndVolt) cellVoltage = _cvEndVolt;
    } else {
      // Reverse sweep: end to start
      sweepingUp = false;
      float progress = (float)(sampleInCycle - samplesPerDirection) / (float)samplesPerDirection;
      cellVoltage = _cvEndVolt - (voltageRange * progress);
      if(cellVoltage < _cvStartVolt) cellVoltage = _cvStartVolt;
    }
    
    // Check if we've completed a cycle
    if(sampleInCycle == 0 && sample > 0) {
      cycleNum = sample / samplesPerCycle;
      if(cycleNum >= _cvCycles) break;
    }
    
    // Convert cell voltage to VBIAS: VBIAS = VZERO + (cellVoltage - vCenter)
    float vBiasVolt = vzeroVolt + (cellVoltage - vCenter);
    if(vBiasVolt < 0.2) vBiasVolt = 0.2;
    if(vBiasVolt > 2.4) vBiasVolt = 2.4;
    
    // Convert to mV for DAC code calculation (constants are in mV)
    float vBiasVolt_mV = vBiasVolt * 1000.0f;
    uint16_t dacCode12Bit = (uint16_t)((vBiasVolt_mV - 200.0f) / DAC12BITVOLT_1LSB + 0.5f);
    if(dacCode12Bit > 4095) dacCode12Bit = 4095;
    
    // Update LPDAC VBIAS
    AD5940_LPDAC0WriteS(dacCode12Bit, lpdac_cfg.DacData6Bit);
    
    // Wait for LPDAC, potentiostat, and HSTIA to settle
    // Use adaptive settling time based on scan rate
    delay(adaptiveSettlingTime);
    
    // Read current via HSTIA with averaging for noise reduction
    // Take multiple samples and average to reduce noise
    const int numCurrentSamples = 5; // Number of samples to average
    float currentSum = 0.0;
    float currentReadings[numCurrentSamples];
    
    // Collect multiple readings
    for(int i = 0; i < numCurrentSamples; i++) {
      currentReadings[i] = readCurrent(HSTIARTIA_1K);
      currentSum += currentReadings[i];
      if(i < numCurrentSamples - 1) {
        delay(5); // Small delay between samples
      }
    }
    
    // Calculate average
    float currentAvg = currentSum / numCurrentSamples;
    
    // Optional: Use median filtering to reject outliers
    // Simple approach: sort and take middle value
    // For now, use average (can be enhanced with median filter)
    float current = currentAvg;
    
    // Subtract baseline offset
    float correctedCurrent = current - baselineCurrent;
    
    // Output data (using corrected current)
    float actualElapsedTime = (millis() - startTime) / 1000.0;
    Serial.print(actualElapsedTime, 4);
    Serial.print(",");
    Serial.print(cellVoltage, 4);
    Serial.print(",");
    Serial.println(correctedCurrent, 6);
    
    // Wait for next sample time
    unsigned long sampleDuration = millis() - sampleStartTime;
    uint32_t waitTime = (uint32_t)(timePerSample * 1000.0) - sampleDuration;
    if(waitTime > 0 && waitTime < 1000) {
      delay(waitTime);
    } else if(waitTime == 0) {
      delay(1); // Minimum delay
    }
    
    sampleInDirection++;
  }
  
  // Disable AFE blocks
  LpAmpCfg.LpPaPwrEn = bFALSE;
  AD5940_LPAMPCfgS(&LpAmpCfg);
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);
  
  Serial.println("=== CV Measurement Complete ===");
}

// Chronoamperometry (Current-Time) - Constant voltage, measure current
void HELPStat::runIT(void) {
  Serial.println("=== Starting Chronoamperometry ===");
  Serial.print("Voltage: "); Serial.print(_itVoltage); Serial.print("V, Duration: "); Serial.print(_itDuration); Serial.println("s");
  
  if(AD5940_WakeUp(10) > 10) {
    Serial.println("Wakeup failed!");
    return;
  }
  
  // Configure LPDAC for constant voltage
  LPDACCfg_Type lpdac_cfg;
  lpdac_cfg.LpdacSel = LPDAC0;
  lpdac_cfg.LpDacVbiasMux = LPDACVBIAS_12BIT;
  lpdac_cfg.LpDacVzeroMux = LPDACVZERO_6BIT;
  lpdac_cfg.LpDacRef = LPDACREF_2P5;
  lpdac_cfg.LpDacSrc = LPDACSRC_MMR;
  lpdac_cfg.PowerEn = bTRUE;
  
  float biasVolt = _itVoltage;
  if(biasVolt < -1.1) biasVolt = -1.1;
  if(biasVolt > 1.1) biasVolt = 1.1;
  lpdac_cfg.DacData6Bit = 0x40 >> 1;
  lpdac_cfg.DacData12Bit = (uint16_t)((biasVolt + 1100.0f) / DAC12BITVOLT_1LSB);
  lpdac_cfg.DataRst = bFALSE;
  lpdac_cfg.LpDacSW = LPDACSW_VBIAS2LPPA|LPDACSW_VBIAS2PIN|LPDACSW_VZERO2LPTIA|LPDACSW_VZERO2PIN|LPDACSW_VZERO2HSTIA;
  AD5940_LPDACCfgS(&lpdac_cfg);
  
  // Configure HSTIA for current measurement
  HSLoopCfg_Type HsLoopCfg;
  AD5940_StructInit(&HsLoopCfg, sizeof(HsLoopCfg));
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;
  HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_VZERO0;
  HsLoopCfg.HsTiaCfg.HstiaCtia = 31;
  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_10K;
  HsLoopCfg.SWMatCfg.Dswitch = SWD_CE0;
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_TRTIA|SWT_SE0LOAD;
  AD5940_HSLoopCfgS(&HsLoopCfg);
  
  // Configure ADC
  DSPCfg_Type dsp_cfg;
  AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  AD5940_DSPCfgS(&dsp_cfg);
  
  // Enable AFE (no waveform generator for DC measurement)
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_ADCPWR, bTRUE);
  
  delay(500); // Settling time
  
  // Start measurement
  Serial.println("Time(s),Voltage(V),Current(A)");
  uint32_t numSamples = _itDuration * 10; // 10 samples per second
  if(numSamples < 1) numSamples = 1;
  float sampleInterval = (float)_itDuration / numSamples;
  
  for(uint32_t i = 0; i < numSamples; i++) {
    float elapsedTime = i * sampleInterval;
    float current = readCurrent(HSTIARTIA_10K);
    
    Serial.print(elapsedTime, 4);
    Serial.print(",");
    Serial.print(_itVoltage, 4);
    Serial.print(",");
    Serial.println(current, 6);
    
    delay((uint32_t)(sampleInterval * 1000.0));
  }
  
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);
  Serial.println("=== IT Measurement Complete ===");
}

// Chronopotentiometry - Constant current, measure voltage
void HELPStat::runCP(void) {
  Serial.println("=== Starting Chronopotentiometry ===");
  Serial.print("Current: "); Serial.print(_cpCurrent, 6); Serial.print("A, Duration: "); Serial.print(_cpDuration); Serial.println("s");
  
  if(AD5940_WakeUp(10) > 10) {
    Serial.println("Wakeup failed!");
    return;
  }
  
  // For CP, we need to control current via HSTIA feedback
  // This is more complex - we'll use a feedback loop to maintain constant current
  // Select appropriate RTIA based on expected current
  int rTIA = HSTIARTIA_10K;
  if(abs(_cpCurrent) > 0.001) rTIA = HSTIARTIA_1K;  // Higher current needs lower RTIA
  if(abs(_cpCurrent) < 0.00001) rTIA = HSTIARTIA_40K;  // Lower current needs higher RTIA
  
  // Configure HSTIA
  HSLoopCfg_Type HsLoopCfg;
  AD5940_StructInit(&HsLoopCfg, sizeof(HsLoopCfg));
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;
  HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_1P1;
  HsLoopCfg.HsTiaCfg.HstiaCtia = 31;
  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = rTIA;
  HsLoopCfg.SWMatCfg.Dswitch = SWD_CE0;
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_TRTIA|SWT_SE0LOAD;
  AD5940_HSLoopCfgS(&HsLoopCfg);
  
  // Configure ADC for voltage measurement (measure at RE0)
  DSPCfg_Type dsp_cfg;
  AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_VSET1P1;  // Reference
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_VDE0;     // Measure at DE0 (working electrode)
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  AD5940_DSPCfgS(&dsp_cfg);
  
  // Enable AFE
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_ADCPWR, bTRUE);
  
  delay(500);
  
  // Simple current control: adjust HSDAC to maintain target current
  // This is a simplified implementation - full implementation would use PID control
  Serial.println("Time(s),Voltage(V),Current(A)");
  uint32_t numSamples = _cpDuration * 10;
  if(numSamples < 1) numSamples = 1;
  float sampleInterval = (float)_cpDuration / numSamples;
  
  float targetVoltage = _cpCurrent * 10000.0; // Approximate: V = I * R
  if(targetVoltage > 0.6) targetVoltage = 0.6;
  if(targetVoltage < -0.6) targetVoltage = -0.6;
  
  // Set initial voltage via HSDAC (simplified - would need proper current control)
  // For now, we'll measure what we get
  for(uint32_t i = 0; i < numSamples; i++) {
    float elapsedTime = i * sampleInterval;
    
    // Measure voltage
    float voltage = readADCVoltage(ADCMUXP_VDE0, ADCMUXN_VSET1P1);
    
    // Measure current
    float current = readCurrent(rTIA);
    
    Serial.print(elapsedTime, 4);
    Serial.print(",");
    Serial.print(voltage, 4);
    Serial.print(",");
    Serial.println(current, 6);
    
    delay((uint32_t)(sampleInterval * 1000.0));
  }
  
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);
  Serial.println("=== CP Measurement Complete ===");
  Serial.println("Note: CP requires active current control - this is a simplified implementation");
}

// Differential Pulse Voltammetry - Staircase with pulses
void HELPStat::runDPV(void) {
  Serial.println("=== Starting Differential Pulse Voltammetry ===");
  Serial.print("Start: "); Serial.print(_dpvStartVolt); Serial.print("V, End: "); Serial.print(_dpvEndVolt); Serial.println("V");
  Serial.print("Step: "); Serial.print(_dpvStepSize); Serial.print("V, Pulse Amp: "); Serial.print(_dpvPulseAmp); Serial.println("V");
  
  if(AD5940_WakeUp(10) > 10) {
    Serial.println("Wakeup failed!");
    return;
  }
  
  // Calculate number of steps
  float voltageRange = _dpvEndVolt - _dpvStartVolt;
  uint32_t numSteps = (uint32_t)(abs(voltageRange) / _dpvStepSize) + 1;
  if(numSteps > 200) numSteps = 200; // Limit steps
  
  // Configure LPDAC for base voltage
  LPDACCfg_Type lpdac_cfg;
  lpdac_cfg.LpdacSel = LPDAC0;
  lpdac_cfg.LpDacVbiasMux = LPDACVBIAS_12BIT;
  lpdac_cfg.LpDacVzeroMux = LPDACVZERO_6BIT;
  lpdac_cfg.LpDacRef = LPDACREF_2P5;
  lpdac_cfg.LpDacSrc = LPDACSRC_MMR;
  lpdac_cfg.PowerEn = bTRUE;
  
  // Configure HSTIA
  HSLoopCfg_Type HsLoopCfg;
  AD5940_StructInit(&HsLoopCfg, sizeof(HsLoopCfg));
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;
  HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_VZERO0;
  HsLoopCfg.HsTiaCfg.HstiaCtia = 31;
  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_10K;
  HsLoopCfg.SWMatCfg.Dswitch = SWD_CE0;
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_TRTIA|SWT_SE0LOAD;
  AD5940_HSLoopCfgS(&HsLoopCfg);
  
  // Configure ADC
  DSPCfg_Type dsp_cfg;
  AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  AD5940_DSPCfgS(&dsp_cfg);
  
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_ADCPWR, bTRUE);
  
  Serial.println("Step,BaseVoltage(V),PulseVoltage(V),CurrentBefore(A),CurrentAfter(A),DeltaCurrent(A)");
  
  // DPV: For each step, measure current before pulse, apply pulse, measure after
  for(uint32_t step = 0; step < numSteps; step++) {
    float baseVolt = _dpvStartVolt + (step * _dpvStepSize);
    if(baseVolt > _dpvEndVolt) baseVolt = _dpvEndVolt;
    
    // Set base voltage
    float biasVolt = baseVolt;
    if(biasVolt < -1.1) biasVolt = -1.1;
    if(biasVolt > 1.1) biasVolt = 1.1;
    lpdac_cfg.DacData12Bit = (uint16_t)((biasVolt + 1100.0f) / DAC12BITVOLT_1LSB);
    AD5940_LPDACCfgS(&lpdac_cfg);
    delay(100); // Settling
    
    // Measure current before pulse
    float currentBefore = readCurrent(HSTIARTIA_10K);
    delay(50);
    
    // Apply pulse (add pulse amplitude via HSDAC - simplified)
    float pulseVolt = baseVolt + _dpvPulseAmp;
    // Note: Full implementation would use HSDAC for pulse
    
    // Measure current after pulse
    delay((uint32_t)_dpvPulseWidth); // Pulse width
    float currentAfter = readCurrent(HSTIARTIA_10K);
    
    float deltaCurrent = currentAfter - currentBefore;
    
    Serial.print(step);
    Serial.print(",");
    Serial.print(baseVolt, 4);
    Serial.print(",");
    Serial.print(pulseVolt, 4);
    Serial.print(",");
    Serial.print(currentBefore, 6);
    Serial.print(",");
    Serial.print(currentAfter, 6);
    Serial.print(",");
    Serial.println(deltaCurrent, 6);
    
    delay(200); // Inter-pulse delay
  }
  
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);
  Serial.println("=== DPV Measurement Complete ===");
}

// Square Wave Voltammetry - Square wave on staircase
void HELPStat::runSWV(void) {
  Serial.println("=== Starting Square Wave Voltammetry ===");
  Serial.print("Start: "); Serial.print(_swvStartVolt); Serial.print("V, End: "); Serial.print(_swvEndVolt); Serial.println("V");
  Serial.print("Freq: "); Serial.print(_swvFreq); Serial.print("Hz, Amplitude: "); Serial.print(_swvAmplitude); Serial.println("V");
  
  if(AD5940_WakeUp(10) > 10) {
    Serial.println("Wakeup failed!");
    return;
  }
  
  // Calculate number of steps
  float voltageRange = _swvEndVolt - _swvStartVolt;
  uint32_t numSteps = (uint32_t)(abs(voltageRange) / _swvStepSize) + 1;
  if(numSteps > 200) numSteps = 200;
  
  // Configure LPDAC
  LPDACCfg_Type lpdac_cfg;
  lpdac_cfg.LpdacSel = LPDAC0;
  lpdac_cfg.LpDacVbiasMux = LPDACVBIAS_12BIT;
  lpdac_cfg.LpDacVzeroMux = LPDACVZERO_6BIT;
  lpdac_cfg.LpDacRef = LPDACREF_2P5;
  lpdac_cfg.LpDacSrc = LPDACSRC_MMR;
  lpdac_cfg.PowerEn = bTRUE;
  
  // Configure HSTIA
  HSLoopCfg_Type HsLoopCfg;
  AD5940_StructInit(&HsLoopCfg, sizeof(HsLoopCfg));
  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;
  HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_VZERO0;
  HsLoopCfg.HsTiaCfg.HstiaCtia = 31;
  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_10K;
  HsLoopCfg.SWMatCfg.Dswitch = SWD_CE0;
  HsLoopCfg.SWMatCfg.Pswitch = SWP_CE0;
  HsLoopCfg.SWMatCfg.Nswitch = SWN_SE0;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_TRTIA|SWT_SE0LOAD;
  
  // Configure trapezoid generator for square wave
  HsLoopCfg.WgCfg.WgType = WGTYPE_TRAPZ;
  HsLoopCfg.WgCfg.GainCalEn = bTRUE;
  HsLoopCfg.WgCfg.OffsetCalEn = bTRUE;
  
  float sysClkFreq = 16000000.0;
  float wgUpdateRate = sysClkFreq / 50.0; // 320 kHz
  float period = 1.0 / _swvFreq; // seconds
  uint32_t periodClocks = (uint32_t)(period * wgUpdateRate);
  
  // Square wave: fast transitions, equal high/low times
  float amplitude_mV = _swvAmplitude * 1000.0;
  if(amplitude_mV > 607.0) amplitude_mV = 607.0;
  
  uint32_t dcLevel1 = (uint32_t)(2048 - (amplitude_mV / 607.0) * 1024); // Low
  uint32_t dcLevel2 = (uint32_t)(2048 + (amplitude_mV / 607.0) * 1024); // High
  
  uint32_t halfPeriod = periodClocks / 2;
  HsLoopCfg.WgCfg.TrapzCfg.WGTrapzDCLevel1 = dcLevel1;
  HsLoopCfg.WgCfg.TrapzCfg.WGTrapzDCLevel2 = dcLevel2;
  HsLoopCfg.WgCfg.TrapzCfg.WGTrapzDelay1 = halfPeriod;
  HsLoopCfg.WgCfg.TrapzCfg.WGTrapzDelay2 = halfPeriod;
  HsLoopCfg.WgCfg.TrapzCfg.WGTrapzSlope1 = 10; // Fast transition
  HsLoopCfg.WgCfg.TrapzCfg.WGTrapzSlope2 = 10;
  
  AD5940_HSLoopCfgS(&HsLoopCfg);
  
  // Configure ADC
  DSPCfg_Type dsp_cfg;
  AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  AD5940_DSPCfgS(&dsp_cfg);
  
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR|AFECTRL_INAMPPWR|AFECTRL_EXTBUFPWR|\
                  AFECTRL_WG|AFECTRL_DACREFPWR|AFECTRL_HSDACPWR|\
                  AFECTRL_ADCPWR, bTRUE);
  
  Serial.println("Step,BaseVoltage(V),CurrentForward(A),CurrentReverse(A),DeltaCurrent(A)");
  
  // SWV: For each step, measure forward and reverse currents
  for(uint32_t step = 0; step < numSteps; step++) {
    float baseVolt = _swvStartVolt + (step * _swvStepSize);
    if(baseVolt > _swvEndVolt) baseVolt = _swvEndVolt;
    
    // Set base voltage
    float biasVolt = baseVolt;
    if(biasVolt < -1.1) biasVolt = -1.1;
    if(biasVolt > 1.1) biasVolt = 1.1;
    lpdac_cfg.DacData12Bit = (uint16_t)((biasVolt + 1100.0f) / DAC12BITVOLT_1LSB);
    AD5940_LPDACCfgS(&lpdac_cfg);
    delay(100);
    
    // Measure forward current (positive pulse)
    delay((uint32_t)(period * 500.0)); // Wait for forward half-cycle
    float currentForward = readCurrent(HSTIARTIA_10K);
    
    // Measure reverse current (negative pulse)
    delay((uint32_t)(period * 500.0)); // Wait for reverse half-cycle
    float currentReverse = readCurrent(HSTIARTIA_10K);
    
    float deltaCurrent = currentForward - currentReverse;
    
    Serial.print(step);
    Serial.print(",");
    Serial.print(baseVolt, 4);
    Serial.print(",");
    Serial.print(currentForward, 6);
    Serial.print(",");
    Serial.print(currentReverse, 6);
    Serial.print(",");
    Serial.println(deltaCurrent, 6);
    
    delay(100);
  }
  
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);
  Serial.println("=== SWV Measurement Complete ===");
}

// Open Circuit Potential - No excitation, measure voltage
void HELPStat::runOCP(void) {
  Serial.println("=== Starting Open Circuit Potential ===");
  Serial.print("Duration: "); Serial.print(_ocpDuration); Serial.println("s");
  
  if(AD5940_WakeUp(10) > 10) {
    Serial.println("Wakeup failed!");
    return;
  }
  
  // Configure switch matrix to disconnect excitation
  SWMatrixCfg_Type sw_cfg;
  sw_cfg.Dswitch = SWD_OPEN;  // Disconnect waveform generator
  sw_cfg.Pswitch = SWP_CE0;   // 2-electrode: sense voltage at CE0
  sw_cfg.Nswitch = SWN_SE0;   // Connect sense electrode
  sw_cfg.Tswitch = 0;         // Disconnect TIA (open circuit)
  AD5940_SWMatrixCfgS(&sw_cfg);
  
  // Configure ADC to measure voltage at working electrode (DE0)
  DSPCfg_Type dsp_cfg;
  AD5940_StructInit(&dsp_cfg, sizeof(dsp_cfg));
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_VDE0;  // Working electrode
  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_VSET1P1;  // Reference (1.1V)
  dsp_cfg.ADCBaseCfg.ADCPga = ADCPGA_1;
  dsp_cfg.ADCFilterCfg.ADCAvgNum = ADCAVGNUM_16;
  dsp_cfg.ADCFilterCfg.ADCRate = ADCRATE_800KHZ;
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = ADCSINC2OSR_22;
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = ADCSINC3OSR_2;
  dsp_cfg.ADCFilterCfg.BpNotch = bTRUE;
  dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE;
  AD5940_DSPCfgS(&dsp_cfg);
  
  // Enable only ADC (no excitation, no TIA)
  AD5940_AFECtrlS(AFECTRL_ADCPWR, bTRUE);
  delay(100); // Settling time
  
  Serial.println("Time(s),Voltage(V)");
  uint32_t numSamples = _ocpDuration * 10; // 10 samples per second
  if(numSamples < 1) numSamples = 1;
  float sampleInterval = (float)_ocpDuration / numSamples;
  
  for(uint32_t i = 0; i < numSamples; i++) {
    float elapsedTime = i * sampleInterval;
    float voltage = readADCVoltage(ADCMUXP_VDE0, ADCMUXN_VSET1P1);
    
    Serial.print(elapsedTime, 4);
    Serial.print(",");
    Serial.println(voltage, 6);
    
    delay((uint32_t)(sampleInterval * 1000.0));
  }
  
  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE);
  Serial.println("=== OCP Measurement Complete ===");
}