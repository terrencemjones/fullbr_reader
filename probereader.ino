#include <avr/sleep.h>
#include <avr/power.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>
#include <EEPROM.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C

const int BATTERY_PIN = A0;  // Analog pin to read voltage
const float R1 = 100000.0;   // Resistor 1 value in voltage divider (100K)
const float R2 = 10000.0;    // Resistor 2 value in voltage divider (10K)

const int buttonPin = 2;  // the number of the pushbutton pin
const int ledPin = 13;    // the number of the LED pin

const unsigned long QTR_MINUTE = 15000;   // 1 minute in milliseconds
const unsigned long HALF_MINUTE = 30000;  // 30 seconds in milliseconds

volatile bool buttonPressed = false;
unsigned long loopStartTime = 0;
unsigned long lastButtonPressTime = 0;

const String custom_unit = "bar";
const String p1_label = "C367";
const float p1_slope = -4.46244;
const float p1_offset = -27.20527;
const String p2_label = "C396";
const float p2_slope = -4.36725;
const float p2_offset = -5.81237;
const String p3_label = "C456";
const float p3_slope = -4.36725;
const float p3_offset = -5.81237;
const String p4_label = "C567";
const float p4_slope = -4.36725;
const float p4_offset = -5.81237;


//Create an array to take average of weights. This helps smooth out jitter.
#define AVG_SIZE 4
float avgVals[AVG_SIZE];
byte avgValSpot = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
NAU7802 scale;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonInterrupt, FALLING);
  pinMode(BATTERY_PIN, INPUT);
  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.println("FULLBR_READER");
  display.setCursor(0, 38);
  display.println("By T. Jones");
  display.display();
  delay(2000);

  // Initialize NAU7802
  if (scale.begin() == false) {
    display.clearDisplay();
    display.setCursor(0, 34);
    // Display the raw reading
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.println("Probe board not found");

    display.display();
    while (1);
  }

  scale.setGain(NAU7802_GAIN_128);      // Set gain 128
  scale.setSampleRate(NAU7802_SPS_80);  //Set 80sps
  scale.setLDO(NAU7802_LDO_2V4);
  scale.calibrateAFE();  //Re-cal analog front end when we change gain, sample rate, or channel

  //show splash screen until analog cals are done
  display.clearDisplay();
  display.display();
  runMainLoop();
}

void loop() {
  if (buttonPressed) {
    buttonPressed = false;
    setup();
  } else {
    goToSleep();
  }
}


void runMainLoop() {
  Serial.println("Main loop started");
  digitalWrite(ledPin, HIGH);  // Turn on LED
  loopStartTime = millis();
  lastButtonPressTime = loopStartTime;

  int displayMode = 0; 


  while (true) {
    if (digitalRead(buttonPin) == LOW) {
      lastButtonPressTime = millis();
      displayMode = (displayMode + 1) % 5;  // Cycle through display modes
      delay(20);                            // Simple debounce
    }
    if (scale.available() == true) {
      int32_t rawReading = scale.getReading();
      float voltage = calculateVoltage(rawReading);
      //float currentWeight = scale.getWeight(true);//allowNegativeWeights=true

      avgVals[avgValSpot++] = rawReading;
      if (avgValSpot == AVG_SIZE) avgValSpot = 0;

      float avgVal = 0;
      for (int x = 0; x < AVG_SIZE; x++)
        avgVal += avgVals[x];
      avgVal /= AVG_SIZE;

      float avg_mv = calculateVoltage(avgVal);

      Serial.print("\tAvg mV: ");
      Serial.println(avg_mv, 5);  //Print 5 decimal places

      // Clear the display buffer
      display.clearDisplay();

      // Display the selected mode
      display.setTextSize(1);

      float displayValue;

      switch (displayMode) {
        case 0:
          display.setCursor(46, 0);
          display.print("ANALOG");
          displayValue = avg_mv;
          display.setCursor(110, 14);
          display.print("mV");
          display.setCursor(110, 22);
          display.print("/V");
          break;
        case 1:
          display.setCursor(48, 0);
          display.print(p1_label);
          display.setCursor(110, 14);
          displayValue = calculateCal(avg_mv, p1_slope, p1_offset);
          display.print(custom_unit);
          display.setCursor(0, 34);
          display.print("slope: ");
          display.print(p1_slope, 5);
          display.setCursor(0, 44);
          display.print("offset: ");
          display.print(p1_offset, 5);
          break;
        case 2:
          display.setCursor(48, 0);
          display.print(p2_label);
          display.setCursor(110, 14);
          displayValue = calculateCal(avg_mv, p2_slope, p2_offset);;
          display.print(custom_unit);
          display.setCursor(0, 34);
          display.print("slope: ");
          display.print(p2_slope, 5);
          display.setCursor(0, 44);
          display.print("offset: ");
          display.print(p2_offset, 5);
          break;
        case 3:
          display.setCursor(48, 0);
          display.print(p3_label);
          display.setCursor(110, 14);
          displayValue = calculateCal(avg_mv, p3_slope, p3_offset);
          display.print(custom_unit);
          display.setCursor(0, 34);
          display.print("slope: ");
          display.print(p3_slope, 5);
          display.setCursor(0, 44);
          display.print("offset: ");
          display.print(p3_offset, 5);
          break;
        case 4:
          display.setCursor(48, 0);
          display.print(p4_label);
          display.setCursor(110, 14);
          displayValue = calculateCal(avg_mv, p4_slope, p4_offset);
          display.print(custom_unit);
          display.setCursor(0, 34);
          display.print("slope: ");
          display.print(p4_slope, 5);
          display.setCursor(0, 44);
          display.print("offset: ");
          display.print(p4_offset, 5);
          break;
      }

      // Display the main value
      display.setCursor(14, 10);
      display.setTextSize(3);


      // Determine the number of decimal places based on the absolute value
      int decimalPlaces = (abs(displayValue) >= 10) ? 2 : 3;

      // Create a string to hold the formatted value
      char formattedValue[3];  // Adjust size if needed

      if (displayValue <= 0) {
        display.setCursor(0, 14);
        display.setTextSize(2);
        display.print("-");
        display.setTextSize(3);
        display.setCursor(14, 10);
        // Format the value with a fixed width of 7 characters (including the decimal point)
        display.print(abs(displayValue), decimalPlaces);
      } else {
        // Format the value with a fixed width of 7 characters (including the decimal point)
        display.print(displayValue, decimalPlaces);
      }

      // Remove leading spaces
      char* trimmedValue = formattedValue;
      while (*trimmedValue == ' ') trimmedValue++;

      // Print the trimmed, formatted value?
      //display.print(trimmedValue);

      // Display "press button to cycle cals" at the bottom
      display.setCursor(6, 56);
      display.setTextSize(1);
      display.print("PRESS BTN FOR CALS");

      float battv = readBatteryVoltage();
      display.setCursor(0, 0);
      display.setTextSize(1);
      display.print(battv, 1);
      display.print("V");

      blinkCircle();
      // Update the display
      display.display();
    }

    // Check if it's time to exit the loop
    unsigned long currentTime = millis();
    if (currentTime - loopStartTime >= HALF_MINUTE && currentTime - lastButtonPressTime >= QTR_MINUTE) {
      Serial.println("Main loop ended");
      return;
    }
    delay(333);  // Small delay to prevent hammering the I2C bus
  }
}

void goToSleep() {
  //Serial.println("Going to sleep...");
  Serial.flush();

  // Turn off LED
  digitalWrite(ledPin, LOW);

  display.clearDisplay();
  display.display();

  // Put NAU7802 to sleep
  scale.powerDown();

  // // Set unused pins as inputs with pull-ups
  // for (byte i = 0; i <= A5; i++) {
  //   pinMode(i, INPUT_PULLUP);
  // }

  // Disable ADC on Arduino
  ADCSRA = 0;

  // Set sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // Turn off brown-out enable in software
  MCUCR = bit(BODS) | bit(BODSE);
  MCUCR = bit(BODS);

  Wire.end();
  Serial.end();
  // Sleep
  sleep_cpu();

  // CPU will wake up here when interrupted
  sleep_disable();

  scale.powerUp();

  // Re-enable ADC on Arduino
  ADCSRA = 0b10000000;
}

void buttonInterrupt() {
  buttonPressed = true;
  // Wake up the CPU if it's sleeping
  sleep_disable();
}

float calculateVoltage(int32_t rawReading) {
  // The NAU7802 is a 24-bit ADC, so the full scale is 2^23 (signed)
  float voltage = (float)rawReading / 8388608.0 * (1000.0 / 128);
  return voltage;
}

float calculateCal(float avg_mv, float mp, float offset) {
  float bars = ((float)avg_mv * mp) + offset;
  return bars;
}


void blinkCircle() {
  static int cycleCount = 0;

  if (cycleCount == 0) {
    display.setCursor(122, -1);
    display.write(0x03);  // ASCII code for heart symbol
  }

  cycleCount = (cycleCount + 1) % 3;
  display.display();
}

float readBatteryVoltage() {
  int rawValue = analogRead(BATTERY_PIN);
  float voltage = (rawValue / 1024.0) * 5.0;          // Convert to voltage
  float batteryVoltage = voltage / (R2 / (R1 + R2));  // Account for voltage divider
  return batteryVoltage;
}