/*
PMT Monitor/controller with Arduino Due.
1. Read and display PMT voltages on analog ports (ADC0, ADC1)
2. Provide and display command voltages to the PMT on DAC0 and DAC1.
3. Monitor tube trip status (Hamamatsu H7422P-40 with M9012 controller), and reset PS.

Designed to be used with a basic serial LCD display (16x2) on Serial3

Paul B. Manis, Ph.D.
UNC Chapel Hill
This work was supported by NIH/NIDCD grant DC0099809
7/3/2014 Initial commit

This software is free to use and modify under the MIT License (see README.md on github site).

*/
#include <stdio.h>
#include <stdlib.h>

// assign ports
#define PMT1V 0   // DAC to read
#define PMT2V 1
#define PMTCTL1 22  // digital line to monitor for status
#define PMTCTL2 24

int v1, v2;

// Definitions for the LCD:
#define lcdCmd             0xFE   //command prefix
#define lcdCmdSpecial      0x7C   //special command prefix
#define clearLCD           0x01   //Clear entire LCD screen
#define displayOff         0x08   //Display off
#define displayOn          0x0C   //Display on
#define hideCursor         0x0C   //Make cursor invisible
#define ulCursor           0x0E   //Show underline cursor
#define blkCursor          0x0D   //Show blinking block cursor
#define cursorPos          0x80   //set cursor  + position  (row 1=0 to 15, row 2 = 64 to 79)
#define scrollRight        0x1C
#define scrollLeft         0x18
#define cursorRight        0x14
#define cursorLeft         0x10
#define lcdLine1           0x80 // LCD line start addressing (instead of cursorPos)
#define lcdLine2           0xC0
#define lcdLine3           0x94
#define lcdLine4           0xD4

char strPMT1[21];
char strPMT2[21];
char strStr1[21];
char strStr2[21];

const float vscale = 1000./4096.;  // 3.3V max input = 4096, 4096 = 1000V
int v1thr = 2.0;  // microamps
int v2thr = 10.0;

int PMT1AnodeMax = 1000;  // anode voltage, V  
int PMT2AnodeMax = 600;
float PMT1Anode = 800.;
float PMT2Anode = 700.;

float PMT1min = 0.5, PMT1max = 0.9;   // output voltages to scale
float PMT2min = 0.5, PMT2max = 1.1;   // min 0, max V is PMTAnode V
const float dacScale = 4096./3.3;  // AD units per volt.

void setup() {
  // put your setup code here, to run once:
  Serial3.begin(9600); // set up serial port for 9600 baud
  SerialUSB.begin(0);  // back to host computer - speed irrelevant.
  while(!SerialUSB);  // wait for it to complete
  delay(500); // wait for display to boot up
  Serial3.write(lcdCmd);
  Serial3.write(clearLCD);
  Serial3.write(lcdCmd);
  Serial3.write(hideCursor);
  analogReadResolution(12);
  analogWriteResolution(12);
  setAnode1(PMT1Anode);
  setAnode2(PMT2Anode);
  digitalWrite(PMTCTL1, HIGH); // enable PMT1
  digitalWrite(PMTCTL2, HIGH); // and 2
  LCD_NotifyReset(1);
  LCD_NotifyReset(2);
}

void loop() {
// 1. Read both ports and check to see if over voltage - if so, shut down pmt
  int v1, v2;
  char cmd;
  v1 = analogRead(PMT1V);
  v2 = analogRead(PMT2V);
  v1 *= vscale;
  v2 *= vscale;
  if (v1 > v1thr) {
    digitalWrite(PMTCTL1, LOW);
    LCD_NotifyOver(1);
  }
  if (v2 > v2thr) {
    digitalWrite(PMTCTL2, LOW);
    LCD_NotifyOver(2);
  }  
  LCD_Update(1, (float) v1);
  LCD_Update(2, (float) v2);
  delayMicroseconds(5000);
  //
  // process incoming commands from computer
  if(SerialUSB.available() > 0) {
    cmd = SerialUSB.read();
    processCmd(cmd);
  
}

void processCmd(char cmd) {
  int device, value;
  char strV[21];
  switch(cmd) {
  case 'v':  //  set a voltage command is: "v1,500\n"
    device = SerialUSB.parseInt();  // get the device
    value = SerialUSB.parseInt();  // get the value
    if (device == 1) {
      setAnode1(float(value));
    }
    if (device == 2) {
      setAnode2(float(value));
    }
    break;
  case 'r':  // read a volage: "r1" for pmt1, etc
    device=SerialUSB.parseInt()
    if (device == 1) {
      value = analogRead(PMT1V)*vscale;
      SerialUSB.print("V1=")
      SerialUSB.println(dtostrf((float)vcmd, 5, 1, strV));
    }
    if (device == 2) {
      value = analogRead(PMT2V)*vscale;
      SerialUSB.print("V2=")
      SerialUSB.println(dtostrf((float)value, 5, 1, strV));
    }
  }
}

//*****************************************************************
// set anode voltages

void setAnode1(float v) {
  char strV[21];
  float vf;
  int vcmd;
  vf = v/PMT1AnodeMax;  // vf if fraction of voltage range
  vcmd = PMT1min + vf*PMT1max;  // scale for command 
  analogWrite(DAC0, int(dacScale*vcmd));
  SerialUSB.println(dtostrf((float)vcmd, 5, 2, strV));
  SerialUSB.println(dtostrf((float)(vcmd*dacScale), 5, 0, strV));
}
void setAnode2(float v) {
  float vf;
  int vcmd;
  vf = v/PMT2AnodeMax;  // vf if fraction of voltage range
  vcmd = PMT2min + vf*PMT2max;  // scale for command 
  analogWrite(DAC1, int(dacScale*vcmd));
}

float getAnode(int device) {
  
}
//*****************************************************************
// Routine to control data display to the LCD
// Parameters:
// 1. Line number (int)
// 2. parameter (float)
//
void LCD_Update(int pmtNo, float value) {
    char strV[21]; 
    switch(pmtNo) {
    case 1: // PMT1
        Serial3.write(lcdCmd);
        Serial3.write(lcdLine1);
        Serial3.print("PMT1 ");
        dtostrf((float)value, 3, 0, strV);
        Serial3.print(strV);
        Serial3.print("V");
        SerialUSB.println(strV);
        break;
    case 2:  // PMT2
        Serial3.write(lcdCmd);
        Serial3.write(lcdLine2);
        Serial3.print("PMT2 ");
        dtostrf((float)value, 3, 0, strV);
        Serial3.print(strV);
        Serial3.print("V");
        SerialUSB.println(strV);
        break;
    }
}

void LCD_NotifyOver(int pmtNo) {
  switch(pmtNo) {
    case 1:
      Serial3.write(lcdCmd);
      Serial3.write(cursorPos);
      Serial3.write(14);
      Serial3.print("*");
      break;  
    case 2:
      Serial3.write(lcdCmd);
      Serial3.write(cursorPos);
      Serial3.write(78);
      Serial3.print("*");   
      break;
    }
}

void LCD_NotifyReset(int pmtNo) {
  switch(pmtNo) {
    case 1:
      Serial3.write(lcdCmd);
      Serial3.write(cursorPos);
      Serial3.write(14);
      Serial3.print(" ");  
      break;
    case 2:
      Serial3.write(lcdCmd);
      Serial3.write(cursorPos);
      Serial3.write(78);
      Serial3.print(" ");   
      break;
    }
}
