/*
PMT Monitor/controller with Arduino Due.
1. Read and display PMT currents (peak or mean) on analog ports (ADC0, ADC1)
2. Provide and display command voltages to the PMT on DAC0 and DAC1.
  -- The DACs on the Due put out 0.55 (1/6 of 3.3V) to about 2.6 (5/6 of 3.3V). This
  means that the outputs have to be level shifted to reach 0, which requires at least
  a dual op-amp for the two channels, and a handful of resistors.
  A simpler solution is to provide manual control of the PMT voltage with a 10-turn 
  potentiometer, such that the proper voltage range for the PMT is acheived. We then
  can read the command voltage on additional DACs to report to the host if needed (and,
  display on the display). In the future, we can provide an option for either manual
  control or computer control by building the appropriate control circuitry.
3. Monitor tube trip status (Hamamatsu H7422P-40 with M9012 controller), and provide
  a signal to reset the PS.

Designed to be used with a basic serial LCD display (16x2, Hitachi command set)
connected to TX on Serial3. 

Paul B. Manis, Ph.D.
UNC Chapel Hill
This work was supported by NIH/NIDCD grant DC0099809
8/3/2014 Initial commit
8/4/2014 Major changes to structure, using arrays so that a large number of PMTs could
(in theory) be controlled. This also simplifies the coding. 
Added peak/mean measurement of PMT current, and ability to control the time interval over
which the measurements are made.

This software is free to use and modify under the MIT License (see README.md on github site).

*/

#include <stdio.h>
#include <stdlib.h>

// assign hardware ports
const unsigned PMT_monitor[] = {0, 1};   // DAC to read
const unsigned PMT_cmd[]     = {DAC0, DAC1}; //
const int PMT_ERR_TTL[]      = {22, 24};  // digital line to monitor for status from H7422P-40/M9012
const int PMT_POW_TTL[]      = {23, 25};  // if using using 7422, digital command to reset

// Definitions for the LCD:
#define lcdCmd             0xFE   //command prefix
#define lcdCmdSpecial      0x7C   //special command prefix
#define clearLCD           0x01   //Clear entire LCD screen
#define displayOff         0x08   //Display off
#define displayOn          0x0C   //Display on
#define hideCursor         0x0C   //Make cursor invisible
#define ulCursor           0x0E   //Show underline cursor
#define blkCursor          0x0D   //Show blinking block cursor
#define cursorPos          0x80   //OR with cursor position  (row 1=0 to 15, row 2 = 64 to 79, row3 = 16-31, row4 = 80-95)
#define scrollRight        0x1C
#define scrollLeft         0x18
#define cursorRight        0x14
#define cursorLeft         0x10
#define lcdLine1           0x80 // LCD line start addressing (instead of cursorPos)
#define lcdLine2           0xC0
#define lcdLine3           0x94
#define lcdLine4           0xD4

const int lcdBacklight[] = {};

char str[121];  // general scratch space to build print strings.

/*  Parameters for our specific PMTs and hardware*/

float I_PMT[]    = {0., 0.};  // monitor currents from PMTs
float PMTAnode[] = {0., 0.};  // set to 90% of maximum
const float GI_PMT[]  = {0.5, 0.1};  // gain of each channel's current preamplifiers, microamperes per volt.
const float Thr_PMT[] = {1.0, 5.0}; // Threshold for PMT1, microamperes (H7422P-40) and PMT2
const float PMTmin[]  = {0.5, 0.5};
const float PMTmax[]  = {0.8, 1.1};   // command voltage range, H7422P-40

float Imeas[]   = {0., 0.};  // measures of current over time (peak or average, in tmeas msec blocks)
float tmeas     = 200.;
float nsamp     = 0.0;
float time_zero = 0.;
char mode       = 'p';  // single character for data sampling mode: m for mean, p for peak

const float DACscale = 4096/3.3;  // 3.3V max output = 4096, 4096 = 1000V
const float ADCscale = 3.3/4096.;  // 3.3.V yields 4096 A-D units, so ADCscale * A-D units is V
const int NPMT = 2;  // number of PMTs supported by this code

void setup() {
  int i;
  /* initialize variables */
  for (i=0; i < NPMT; i++) {
    PMTAnode[i] = PMTmax[i]*0.9;  // Set to 90% of maximum
  }
  
  // Set up serial port to display, USB port, and hardware configuration
  Serial3.begin(9600); // set up serial port for 9600 baud
  SerialUSB.begin(0);  // back to host computer - speed irrelevant.
  while(!SerialUSB);  // wait a USB serial connection to complete
  delay(500); // wait for display to boot up
  Serial3.write(lcdCmd);
  Serial3.write(clearLCD);
  Serial3.write(lcdCmd);
  Serial3.write(hideCursor);  // clear LCD and hide cursor
  analogReadResolution(12);  // use max resolution on all ADC/DAC
  analogWriteResolution(12);
  for (i=0; i < NPMT; i++) {
    setAnode(i, 0.);  // set initial voltages to 0
    digitalWrite(PMT_POW_TTL[i], HIGH); // enable PMT1
  }
  delay(1000.); // give a second to power up
  for (i = 0; i < NPMT; i++) {
    setAnode(i, PMTAnode[i]);  // set voltages to 90% of maximum
    LCD_Notify_Reset(i);
  }
  time_zero = millis();
}

void loop() {
// 1. Read both ports and check to see if over voltage - if so, shut down pmt
  int i;
  char cmd;
  nsamp += 1.0; // count times through loop for mean calculation
  for (i=0; i<NPMT; i++) {
    I_PMT[i] = analogRead(PMT_monitor[i]);  // get voltage
    sprintf(str, "%d = %f at %f", i, I_PMT[i], float(millis()-time_zero));
    SerialUSB.println(str);
    I_PMT[i] *= ADCscale*GI_PMT[i]; // convert to voltage, then current in microamps
    if (I_PMT[i] > Thr_PMT[i]) {
      digitalWrite(PMT_POW_TTL[i], LOW);  // turn off power to PMT
      LCD_Notify_Over(i);
    } 
    if (mode == 'm') {  // keep running sum and count
      Imeas[i] = Imeas[i] + I_PMT[i];
    }
    if (mode == 'p') { // keep track of peak current
      if (I_PMT[i] > Imeas[i]) {
        Imeas[i] = I_PMT[i];
      }
    }
  }
  // check if time to update display
  if ((millis()-time_zero) >= tmeas) {
    time_zero = millis();  // reset time count
    for (i=0; i < NPMT; i++) {
      if (mode == 'm') {
        Imeas[i] = Imeas[i]/nsamp; // compute mean
      }
      LCD_I_Update(i, Imeas[i]);
      Imeas[i] = 0.0; // reset values
    }
    nsamp = 0.0;
  }
  // delayMicroseconds(1000);  // cut sample rate to 1 kHz...
  
  // process incoming commands from computer over usb port - if there are any!
  if(SerialUSB.available() > 0) {
    cmd = SerialUSB.read();
    processCmd(cmd);
  }
}

void processCmd(char cmd) {
  int device;
  float interval;
  float value;
  char str[81];
  switch(cmd) {
  case 'v':  //  set a voltage command is: "v1,0.8\n"
    device = SerialUSB.parseInt();  // get the device
    value = SerialUSB.parseFloat();  // get the value
    if (device >= 0 && device < NPMT) {
      PMTAnode[device] = value;
      setAnode(device, value);
    }
    break;
  case 'r':  // read a volage: "r1" for pmt1, etc
    device=SerialUSB.parseInt();
    if (device >= 0 && device < NPMT) {
      value = analogRead(PMT_monitor[device])*ADCscale*GI_PMT[device];
      sprintf(str, "I%1d=%6.3f uA", device, value);
      SerialUSB.println(str);
    }
    break;
  case 'R':  // reset a port
    device = SerialUSB.parseInt();
    if (device >= 0 && device < NPMT) {
      resetPMT(device);       
     }
     break;
  case 'a':  //interogate current anode settings
    device = SerialUSB.parseInt();
    if (device >= 0 && device < NPMT) {
       sprintf(str, "Anode%1d=%4.2f V", device, PMTAnode[device]);
       SerialUSB.println(str);       
    }
    break;
  case 'p': //set peak reading mode
    mode = 'p';
    LCD_PeakMean(0, mode);
    LCD_PeakMean(1, mode);
    break;
  case 'm': // set mean reading mode
    mode = 'm';
    LCD_PeakMean(0, mode);
    LCD_PeakMean(1, mode);
    break;
    
  case 't': // get a measurement interval (default set above)
    interval = SerialUSB.parseFloat();
    if (interval > 10. && interval < 10000.) {  // only accept reasonable values
      tmeas = interval;
    }
    break;
  case '?': // print a list of commands
    sprintf(str, "vn,x.y : set the command voltage to the PMT n");
    SerialUSB.println(str);
    sprintf(str, "rn      : read the current from the selected PMT n");
    SerialUSB.println(str);
    sprintf(str, "Rn      : Reset the power to the selected PMT n");
    SerialUSB.println(str);
    sprintf(str, "an      : read the anode voltage setting from the selected PMT n");
    SerialUSB.println(str);
    sprintf(str, "p       : select Peak reading mode");
    SerialUSB.println(str);
    sprintf(str, "m       : select Mean reading mode");
    SerialUSB.println(str);
    sprintf(str, "t###.   : set the reading/averaging period (msec, float)");
    SerialUSB.println(str);
  }
}

//*****************************************************************
// set anode voltages

void setAnode(int pmt, float v) {
  char str[81];
  float vf, vcmd;
  PMTAnode[pmt] = v;
  vf = v/PMTmax[pmt];  // vf if fraction of voltage range
  vcmd = PMTmin[pmt] + vf*PMTmax[pmt];  // scale for command 
  analogWrite(PMT_cmd[pmt], int(DACscale*vcmd));
  LCD_Anode_Update(pmt, v);
}

// restore power and set the command to the previous value
void resetPMT(int pmt) {
    digitalWrite(PMT_POW_TTL[pmt], HIGH); // enable PMT1
    setAnode(pmt, PMTAnode[pmt]);
    LCD_Notify_Reset(pmt);

}
//*****************************************************************
// Routine to display PMT current levels to the LCD
// Parameters:
// 1. Line number (int)
// 2. parameter (float) or char (for peak/mean mode)
//
const int linecmd[] = {lcdLine1, lcdLine2, lcdLine3, lcdLine4};

const int cpos[] = {0, 64, 16, 80}; // initial numbers to offset cursor by
const int offset_over = 9;
const int offset_mode = 8;
const int offset_anode = 12; // leave space before...

void LCD_I_Update(int pmt, float value) {
  char strV[21];
  Serial3.write(lcdCmd);
  Serial3.write(linecmd[pmt]);
  sprintf(strV, "%6.3fuA", value);
  Serial3.print(strV);
}

// indicate whether readings are peak or mean
void LCD_PeakMean(int pmt, char mode) {
  Serial3.write(lcdCmd);
  Serial3.write(cursorPos | (cpos[pmt]+offset_mode));
  if(mode == 'p') {
    Serial3.print("p");  // could use symbols instead...
  }
  if(mode == 'm') {
    Serial3.print("m");
  }
}

void LCD_Anode_Update(int pmt, float value) {
  char strV[21];
  Serial3.write(lcdCmd);
  Serial3.write(cursorPos | (cpos[pmt]+offset_anode));
  sprintf(strV, "%4.2f", value);
  Serial3.print(strV);
}
// notify user of over-current condition
void LCD_Notify_Over(int pmt) {
  Serial3.write(lcdCmd);
  Serial3.write(cursorPos | (cpos[pmt]+offset_over));
  Serial3.print("*");
}

// reset the over-current condition notification
void LCD_Notify_Reset(int pmt) {
  Serial3.write(lcdCmd);
  Serial3.write(cursorPos | (cpos[pmt]+offset_over));
  Serial3.print(" ");  
}
