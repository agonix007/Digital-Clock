#include <Arduino.h>
#include <LedControl.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <DHT.h>
#include <Bounce2.h>

#define DIN_PIN 11
#define CLK_PIN 13
#define CS_PIN 10
#define NUM_DISPLAYS 4
#define BUTTON_PIN 2
#define DHTPIN 3
#define DHTTYPE DHT11

#define MIN_TEMP 0
#define MAX_TEMP 50
#define MIN_HUMIDITY 20
#define MAX_HUMIDITY 90

Bounce2::Button button = Bounce2::Button();

LedControl lc = LedControl(DIN_PIN, CLK_PIN, CS_PIN, NUM_DISPLAYS);
RTC_DS1307 rtc;
DHT dht(DHTPIN, DHTTYPE);

const uint8_t PROGMEM digits[][3] = {
    {0x1F, 0x11, 0x1F}, // 0
    {0x00, 0x00, 0x1F}, // 1
    {0x17, 0x15, 0x1D}, // 2
    {0x11, 0x15, 0x1F}, // 3
    {0x1C, 0x04, 0x1F}, // 4
    {0x1D, 0x15, 0x17}, // 5
    {0x1F, 0x15, 0x17}, // 6
    {0x10, 0x10, 0x1F}, // 7
    {0x1F, 0x15, 0x1F}, // 8
    {0x1D, 0x15, 0x1F}  // 9
};

const uint8_t PROGMEM colon[3] = {0x00, 0x0A, 0x00}; // :

const uint8_t PROGMEM font[][3] = {
    {0x1F, 0x11, 0x1F}, // 0
    {0x00, 0x00, 0x1F}, // 1
    {0x17, 0x15, 0x1D}, // 2
    {0x11, 0x15, 0x1F}, // 3
    {0x1C, 0x04, 0x1F}, // 4
    {0x1D, 0x15, 0x17}, // 5
    {0x1F, 0x15, 0x17}, // 6
    {0x10, 0x10, 0x1F}, // 7
    {0x1F, 0x15, 0x1F}, // 8
    {0x1D, 0x15, 0x1F}, // 9
    {0x00, 0x00, 0x00}, // space (10)
    {0x1F, 0x14, 0x1F}, // A (11)
    {0x1F, 0x15, 0x0A}, // B
    {0x1F, 0x11, 0x11}, // C
    {0x1F, 0x11, 0x0E}, // D
    {0x1F, 0x15, 0x11}, // E
    {0x1F, 0x14, 0x10}, // F
    {0x1F, 0x11, 0x17}, // G
    {0x1F, 0x04, 0x1F}, // H
    {0x11, 0x1F, 0x11}, // I
    {0x03, 0x01, 0x1F}, // J
    {0x1F, 0x04, 0x1B}, // K
    {0x1F, 0x01, 0x01}, // L
    {0x1F, 0x08, 0x1F}, // M
    {0x1F, 0x10, 0x0F}, // N
    {0x1F, 0x11, 0x1F}, // O
    {0x1F, 0x14, 0x1C}, // P
    {0x1C, 0x14, 0x1F}, // Q
    {0x1F, 0x16, 0x1D}, // R
    {0x1D, 0x15, 0x17}, // S
    {0x10, 0x1F, 0x10}, // T
    {0x1F, 0x01, 0x1F}, // U
    {0x1E, 0x01, 0x1E}, // V
    {0x1F, 0x02, 0x1F}, // W
    {0x1B, 0x04, 0x1B}, // X
    {0x1C, 0x07, 0x1C}, // Y
    {0x13, 0x15, 0x19}, // Z
    {0x01, 0x00, 0x00}, // . (37)
    {0x00, 0x0A, 0x00}, // : (38)
};

const char *DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char *MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

enum DisplayState
{
  TIME,
  DATE_DAY,
  TEMP_HUMIDITY
};
DisplayState currentState = TIME;
unsigned long lastStateChange = 0;

unsigned long lastButtonCheckTime = 0;
const unsigned long buttonCheckInterval = 1;

float lastValidTemp = 0;
float lastValidHumidity = 0;
unsigned long lastReadTime = 0;
const unsigned long sensorReadInterval = 5000;

void setup()
{
  for (int i = 0; i < NUM_DISPLAYS; i++)
  {
    lc.shutdown(i, false);
    lc.setIntensity(i, 2);
    lc.clearDisplay(i);
  }

  rtc.begin();
  if (!rtc.isrunning())
  {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  dht.begin();

  button.attach(BUTTON_PIN, INPUT_PULLUP);
  button.interval(1);
  button.setPressedState(LOW);
}

void drawSmallDigit(int device, int col, int digit)
{
  for (int i = 0; i < 3; i++)
  {
    byte columnData = pgm_read_byte(&digits[digit][i]);
    columnData = columnData << 2; // Shift up by 2 to center vertically
    lc.setColumn(device, col + i, columnData);
  }
}

void drawSmallColon(int device, int col)
{
  for (int i = 0; i < 3; i++)
  {
    byte columnData = pgm_read_byte(&colon[i]);
    columnData = columnData << 2; // Shift up by 2 to center vertically
    lc.setColumn(device, col + i, columnData);
  }
}

void drawChar(int device, int col, char c)
{
  int index;
  if (c >= '0' && c <= '9')
    index = c - '0';
  else if (c >= 'A' && c <= 'Z')
    index = c - 'A' + 11;
  else if (c == ' ')
    index = 10;
  else if (c == '.')
    index = 37;
  else if (c == ':')
    index = 38;
  else
    return; // Invalid character

  for (int i = 0; i < 3; i++)
  {
    byte columnData = pgm_read_byte(&font[index][i]);
    columnData = columnData << 2; // Shift up by 2 to center vertically
    lc.setColumn(device, col + i, columnData);
  }
}

void clearDisplay()
{
  for (int i = 0; i < NUM_DISPLAYS; i++)
  {
    lc.clearDisplay(i);
  }
}

void displayTime(DateTime now)
{
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
  // Draw second colon
  drawSmallColon(1, 5);
  // Draw seconds
  drawSmallDigit(0, 1, seconds / 10);
  drawSmallDigit(0, 5, seconds % 10);
}

void displayDateAndDay(DateTime now)
{
  char dateBuffer[6]; // "DDMMM"
  sprintf(dateBuffer, "%02d%s", now.day(), MONTHS[now.month() - 1]);

  const char *day = DAYS[now.dayOfTheWeek()];

  // Calculate total width
  int dateWidth = 5 * 4; // 5 characters for date, 4 pixels each
  int dayWidth = 3 * 4;  // 3 characters for day, 4 pixels each (increased from 3)

  // Calculate left padding to center the entire string
  int leftPad = (32 - (dateWidth + dayWidth)) / 2;

  // Display date
  for (int i = 0; i < 5; i++)
  {
    int position = leftPad + i * 4;
    drawChar(3 - position / 8, position % 8, dateBuffer[i]);
  }

  // Display day (using regular font)
  for (int i = 0; i < 3; i++)
  {
    int position = leftPad + dateWidth + 1 + i * 4; // Increased spacing to 4
    drawChar(3 - position / 8, position % 8, day[i]);
  }
}

void displayTempHumidity()
{
  char tempBuffer[4];
  char humBuffer[4];
  sprintf(tempBuffer, "%02d", (int)lastValidTemp);
  sprintf(humBuffer, "%02d", (int)lastValidHumidity);

  int leftPad = 1;

  for (int i = 0; i < 2; i++)
  {
    int position = leftPad + i * 4;
    drawChar(3 - position / 8, position % 8, tempBuffer[i]);
  }

  drawChar(2, 1, 'C');
  drawChar(2, 6, ':');
  drawChar(1, -1, ':');

  for (int i = 0; i < 2; i++)
  {
    int position = 4 + 16 + i * 4;
    drawChar(3 - position / 8, position % 8, humBuffer[i]);
  }

  drawChar(0, 4, 'H');
}

void updateSensorReadings()
{
  unsigned long currentTime = millis();
  if (currentTime - lastReadTime >= sensorReadInterval)
  {
    float newTemp = dht.readTemperature();
    float newHumidity = dht.readHumidity();

    if (!isnan(newTemp) && !isnan(newHumidity) &&
        newTemp >= MIN_TEMP && newTemp <= MAX_TEMP &&
        newHumidity >= MIN_HUMIDITY && newHumidity <= MAX_HUMIDITY)
    {
      lastValidTemp = newTemp;
      lastValidHumidity = newHumidity;
      lastReadTime = currentTime;
    }
  }
}

void handleButton()
{
  unsigned long currentTime = millis();

  if (currentTime - lastButtonCheckTime >= buttonCheckInterval)
  {
    lastButtonCheckTime = currentTime;

    button.update();

    if (button.fell())
    {
      if (currentState == TIME)
      {
        currentState = DATE_DAY;
      }
      else if (currentState == DATE_DAY)
      {
        currentState = TEMP_HUMIDITY;
      }
      else
      {
        currentState = TIME;
      }
      lastStateChange = currentTime;
      clearDisplay();
    }
  }
}

void updateDisplay()
{
  DateTime now = rtc.now();
  unsigned long currentTime = millis();

  if (currentState == TIME && currentTime - lastStateChange > 15000)
  {
    currentState = DATE_DAY;
    lastStateChange = currentTime;
    clearDisplay();
  }
  else if (currentState == DATE_DAY && currentTime - lastStateChange > 10000)
  {
    currentState = TEMP_HUMIDITY;
    lastStateChange = currentTime;
    clearDisplay();
  }
  else if (currentState == TEMP_HUMIDITY && currentTime - lastStateChange > 10000)
  {
    currentState = TIME;
    lastStateChange = currentTime;
    clearDisplay();
  }

  switch (currentState)
  {
  case TIME:
    displayTime(now);
    break;
  case DATE_DAY:
    displayDateAndDay(now);
    break;
  case TEMP_HUMIDITY:
    displayTempHumidity();
    break;
  }
}

void loop()
{
  handleButton();
  updateSensorReadings();
  updateDisplay();
}