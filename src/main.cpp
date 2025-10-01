#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>
#include <math.h>
#include "SparkFun_BMI270_Arduino_Library.h"
#include <SparkFun_Qwiic_OLED.h>

// Create objects for the OLED and IMU
QwiicMicroOLED myOLED;
BMI270 imu;

// --- Button and timing state ---
const int switchPin = 10;            // button pin
unsigned long debounceDelay = 50;    // ms
unsigned long doublePressTime = 500; // ms for double tap
unsigned long longPressTime = 3000;  // ms for long press

int lastButtonReading = LOW;
int buttonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long pressStart = 0;
unsigned long lastReleaseTime = 0;

// --- Orientation state (angles in degrees) ---
float theta = 0.0; // rotation about X
float psi = 0.0;   // rotation about Y

// Pixel offsets used to draw triangles
int arrLx[] = {0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3};
int arrLy[] = {0, -1, 0, 1, -2, -1, 0, 1, 2, -3, -2, -1, 0, 1, 2, 3};
int arrLxsmall[] = {0, 1, 1, 1, 2, 2, 2, 2, 2};
int arrLysmall[] = {0, -1, 0, 1, -2, -1, 0, 1, 2};
char pout[30]; // buffer for raw data text

// Modes / states
enum MachineState {
  OffState,  // 0
  TwoAxis,   // 1
  XAxis,     // 2
  YAxis,     // 3
  RawData,   // 4
  StateLength
};

MachineState currentState = TwoAxis;

// --- Drawing helpers ---
void drawTriangle(int xOff, int yOff, int xDir, int yDir, bool swap) {
  if (!swap) {
    for (int i = 0; i < 16; i++) {
      myOLED.pixel(xOff + xDir * arrLx[i], yOff + yDir * arrLy[i], 255);
    }
  } else {
    for (int i = 0; i < 16; i++) {
      myOLED.pixel(xOff + xDir * arrLy[i], yOff + yDir * arrLx[i], 255);
    }
  }
}

void drawTriangleSmall(int xOff, int yOff, int xDir, int yDir, bool swap) {
  if (!swap) {
    for (int i = 0; i < 9; i++) {
      myOLED.pixel(xOff + xDir * arrLxsmall[i], yOff + yDir * arrLysmall[i], 255);
    }
  } else {
    for (int i = 0; i < 9; i++) {
      myOLED.pixel(xOff + xDir * arrLysmall[i], yOff + yDir * arrLxsmall[i], 255);
    }
  }
}

// Tilt angle calculations
float getXangle() {
  return atan2(imu.data.accelX,
               sqrt(imu.data.accelY * imu.data.accelY +
                    imu.data.accelZ * imu.data.accelZ)) *
         180.0 / PI;
}

float getYangle() {
  return atan2(imu.data.accelY,
               sqrt(imu.data.accelX * imu.data.accelX +
                    imu.data.accelZ * imu.data.accelZ)) *
         180.0 / PI;
}

// --- Button handling (debounce, long press, double tap) ---
void handleButton() {
  int raw = digitalRead(switchPin);

  if (raw != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = raw;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (raw != buttonState) {
      buttonState = raw;

      if (buttonState == HIGH) {
        // pressed
        pressStart = millis();
      } else {
        // released
        unsigned long held = millis() - pressStart;

        if (held >= longPressTime) {
          // Long press: toggle Off ↔ TwoAxis
          if (currentState == OffState) {
            currentState = TwoAxis;
          } else {
            currentState = OffState;
          }
        } else {
          // Short press: check for double tap
          if (lastReleaseTime != 0 && (millis() - lastReleaseTime) <= doublePressTime) {
            // Double press → cycle mode (skip OffState)
            currentState = (MachineState)(((int)currentState + 1) % (int)StateLength);
            if (currentState == OffState) currentState = TwoAxis;
            lastReleaseTime = 0; // consume
          } else {
            lastReleaseTime = millis(); // first tap
          }
        }
      }
    }
  }
}

void setup() {
  delay(1000);
  Serial.begin(9600);
  while (!Serial) yield();

  pinMode(switchPin, INPUT_PULLDOWN);

  Wire.begin();
  while (imu.beginI2C(0x68) != BMI2_OK) {
    delay(1000);
  }

  while (!myOLED.begin()) {
    delay(1000);
  }

  Serial.println("Everything started!!!!!");
}

void loop() {
  handleButton();

  myOLED.erase();
  imu.getSensorData();
  theta = getXangle();
  psi = getYangle();

  switch (currentState) {
    case OffState:
      // Display off (blank)
      break;

    case TwoAxis:
      // X tilt
      if (theta > 0.5) drawTriangle(0, 23, 1, 1, false);
      else if (theta < -0.5) drawTriangle(63, 23, -1, 1, false);
      else drawTriangleSmall((theta > 0 ? 0 : 63), 23, (theta > 0 ? 1 : -1), 1, false);

      // Y tilt
      if (psi > 0.5) drawTriangle(32, 0, 1, 1, true);
      else if (psi < -0.5) drawTriangle(32, 45, 1, -1, true);
      else drawTriangleSmall(32, (psi > 0 ? 0 : 45), 1, (psi > 0 ? 1 : -1), true);
      break;

    case XAxis:
      if (theta > 0.5) drawTriangle(0, 23, 1, 1, false);
      else if (theta < -0.5) drawTriangle(63, 23, -1, 1, false);
      else drawTriangleSmall((theta > 0 ? 0 : 63), 23, (theta > 0 ? 1 : -1), 1, false);
      break;

    case YAxis:
      if (psi > 0.5) drawTriangle(32, 0, 1, 1, true);
      else if (psi < -0.5) drawTriangle(32, 45, 1, -1, true);
      else drawTriangleSmall(32, (psi > 0 ? 0 : 45), 1, (psi > 0 ? 1 : -1), true);
      break;

    case RawData:
      sprintf(pout, "ax: %.2f", imu.data.accelX);
      myOLED.text(0, 0, pout);
      sprintf(pout, "ay: %.2f", imu.data.accelY);
      myOLED.text(0, 10, pout);
      sprintf(pout, "az: %.2f", imu.data.accelZ);
      myOLED.text(0, 20, pout);
      break;

    default:
      break;
  }

  myOLED.display();
}
