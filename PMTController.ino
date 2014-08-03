/*
PMT Monitor/controller with Arduino Due.
1. Read and display PMT voltages on analog ports 


*/
#include <stdio.h>
#include <stdlib.h>

// assign ports
#define PMT1V 0
#define PMT2V 1
#define PMTCTL1 22
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
int v1thr = 0.5;
int v2thr = 1.0;

void setup() {
  // put your setup code here, to run once:
  Serial3.begin(9600); // set up serial port for 9600 baud
  delay(500); // wait for display to boot up
  Serial3.write(lcdCmd);
  Serial3.write(clearLCD);
  Serial3.write(lcdCmd);
  Serial3.write(hideCursor);
  analogReadResolution(12);
  analogWriteResolution(12);
  analogWrite(DAC0, 0.5/vscale);
  analogWrite(DAC1, 1.5/vscale);
  digitalWrite(PMTCTL1, HIGH); // enable PMT
  digitalWrite(PMTCTL2, HIGH); // both
  LCD_NotifyReset(1);
  LCD_NotifyReset(2);
}

void loop() {
// 1. Read both ports
  int v1, v2;
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
        break;
    case 2:  // PMT2
        Serial3.write(lcdCmd);
        Serial3.write(lcdLine2);
        Serial3.print("PMT2 ");
        dtostrf((float)value, 3, 0, strV);
        Serial3.print(strV);
        Serial3.print("V");
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
    case 2:
      Serial3.write(lcdCmd);
      Serial3.write(cursorPos);
      Serial3.write(78);
      Serial3.print("*");   
  }
}

void LCD_NotifyReset(int pmtNo) {
  switch(pmtNo) {
    case 1:
      Serial3.write(lcdCmd);
      Serial3.write(cursorPos);
      Serial3.write(14);
      Serial3.print(" ");  
    case 2:
      Serial3.write(lcdCmd);
      Serial3.write(cursorPos);
      Serial3.write(78);
      Serial3.print(" ");   
  }
}
