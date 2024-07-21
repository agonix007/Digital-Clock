#include <Arduino.h>
#include <LedControl.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>

#define DIN_PIN 11
#define CLK_PIN 13
#define CS_PIN 10
#define NUM_DISPLAYS 4

LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, NUM_DISPLAYS);
RTC_DS1307 rtc;

const uint8_t PROGMEM digits[][3] = {
  {0x1F, 0x11, 0x1F}, //0 - 9
  {0x00, 0x00, 0x1F},
  {0x17, 0x15, 0x1D},
  {0x11, 0x15, 0x1F},
  {0x1C, 0x4,  0x1F},
  {0x1D, 0x15, 0x17},
  {0x1F, 0x15, 0x17},
  {0x10, 0x10, 0x1F},
  {0x1F, 0x15, 0x1F},
  {0x1D, 0x15, 0x1F}
};

const uint8_t PROGMEM colon[3] = {0x00, 0x0A, 0x00};  // :

void setup() {
  for (int i = 0; i < NUM_DISPLAYS; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 8);
    lc.clearDisplay(i);
  }

  rtc.begin();
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void drawSmallDigit(int device, int col, int digit) {
  for (int i = 0; i < 3; i++) {
    byte columnData = pgm_read_byte(&digits[digit][i]);
    columnData = columnData << 2;  // Shift up by 2 to center vertically
    lc.setColumn(device, col + i, columnData);
  }
}

void drawSmallColon(int device, int col) {
  for (int i = 0; i < 3; i++) {
    byte columnData = pgm_read_byte(&colon[i]);
    columnData = columnData << 2;  // Shift up by 2 to center vertically
    lc.setColumn(device, col + i, columnData);
  }
}

void clearDisplay() {
  for (int i = 0; i < NUM_DISPLAYS; i++) {
    lc.clearDisplay(i);
  }
}

void small_mode() {
  DateTime now = rtc.now();
  int hours = now.hour();
  int minutes = now.minute();
  int seconds = now.second();

  // Draw hours
  drawSmallDigit(3, 0, hours / 10);
  drawSmallDigit(3, 4, hours % 10);
    
  // Draw first colon
  drawSmallColon(2, 0);
    
  // Draw minutes
  drawSmallDigit(2, 4, minutes / 10);
  drawSmallDigit(1, 1, minutes % 10);
    
  // Always update seconds
  // Draw second colon
  drawSmallColon(1, 5);
  
  // Draw seconds
  drawSmallDigit(0, 1, seconds / 10);
  drawSmallDigit(0, 5, seconds % 10);
}

void loop() {
  static unsigned long lastUpdateTime = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdateTime >= 1000) {
    small_mode();
    lastUpdateTime = currentMillis;
  }
}