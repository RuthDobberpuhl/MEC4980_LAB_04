// main.cpp
// Simple demo that reads tilt from a BMI270 accelerometer and
// displays a minimal indicator on a Qwiic OLED. A button cycles
// through display modes (Off, Two-axis, X only, Y only, Raw data).
//
// Notes:
// - This file only adds comments and explanatory notes. No logic
//   or behavior of the original program was changed.
// - Variables marked volatile are modified from an ISR.

#include <Arduino.h> 
#include <Wire.h> 
#include <stdint.h>  
#include <math.h> 
// libraries for led screen and accel
#include "SparkFun_BMI270_Arduino_Library.h"
#include <SparkFun_Qwiic_OLED.h>

// Create objects for the OLED and IMU
QwiicMicroOLED myOLED; 
BMI270 imu;

// --- Button and timing state (for ISR/debounce) ---
// 'volatile' because these may be changed in an interrupt handler
volatile int buttonCounter = 0; // unused in current logic but kept for future use
bool prevPressed = false;       // simple state-tracking helper (not used in ISR)
bool off = false;
int switchPin = 10;             // pin connected to the push button
// timing thresholds (milliseconds)
unsigned long debounceDelay = 50;    // ignore presses closer than this
unsigned long doublePressTime = 500; // threshold to classify double-press
unsigned long longPressTime = 3000; // threshold to classify double-press
unsigned long prevTime = 0;          // last recorded press time (ms)

// --- Orientation state (angles in degrees) ---
float theta = 0.0; // rotation about X (computed from accel)
float psi = 0.0;   // rotation about Y (computed from accel)
float phi = 0.0;   // reserved / unused in current code

// Pixel offsets used to draw a small triangular indicator on the OLED.
// The arrays hold relative offsets (x,y) for 16 pixels that form a
// triangular shape. Changing these changes the icon used to indicate
// tilt direction/amount.
int arrLx[] = {0,  1, 1, 1,  2,  2, 2, 2, 2,  3,  3,  3, 3, 3, 3, 3};
int arrLy[] = {0, -1, 0, 1, -2, -1, 0, 1, 2, -3, -2, -1, 0, 1, 2, 3};
int arrLxsmall[] = {0,  1, 1, 1,  2,  2, 2, 2, 2};
int arrLysmall[] = {0, -1, 0, 1, -2, -1, 0, 1, 2};
// Small printable buffer used when drawing raw data strings to the OLED.
char pout[30];

// Possible button press classifications produced by the ISR logic.
enum PressType {
  NoPress,      // 0: no new press
  SinglePress,  // 1: single press detected
  DoublePress,  // 2: quick double press detected
  LongPress     // 3: reserved for long-press detection (not implemented)
};

// Modes / states for the display state machine. StateLength helps
// cycle through the states using modulo arithmetic.
enum MachineState {
  OffState,  // 0
  TwoAxis,   // 1: show both X and Y tilt indicators
  XAxis,     // 2: show X axis only (same icon in current code)
  YAxis,     // 3: show Y axis only (not implemented)
  RawData,   // 4: show raw accel values
  StateLength
};

// Current press and state; volatile because ISR updates press.
volatile PressType currentPress = NoPress;
volatile MachineState currentState = OffState;

// drawTriangle
// Draws a small triangular indicator at an (xOff,yOff) base position.
// xDir/yDir act as multipliers to flip or mirror the triangle.
// If 'swap' is true the x/y coordinates are swapped when drawing
// (useful for reusing the same shape rotated 90 degrees).
void drawTriangle(int xOff, int yOff, int xDir, int yDir, bool swap) {
  if (!swap) {
    for (int i = 0; i < 16; i++) {
      // myOLED.pixel expects (x, y, color) -- 255 for 'on'
      myOLED.pixel(xOff + xDir * arrLx[i], yOff + yDir * arrLy[i], 255);
    }
  } else {
    // swap coordinates to rotate the icon 90 degrees. The original
    // implementation applied the x/y multipliers to the wrong axes,
    // producing a mirrored/incorrect orientation. Use arrLy for the
    // x component and arrLx for the y component while preserving
    // xDir/yDir multipliers.
    for (int i = 0; i < 16; i++) {
      myOLED.pixel(xOff + xDir * arrLy[i], yOff + yDir * arrLx[i], 255);
    }
  }
}

void drawTriangleSmall(int xOff, int yOff, int xDir, int yDir, bool swap) {
  if (!swap) {
    for (int i = 0; i < 9; i++) {
      // myOLED.pixel expects (x, y, color) -- 255 for 'on'
      myOLED.pixel(xOff + xDir * arrLxsmall[i], yOff + yDir * arrLysmall[i], 255);
    }
  } else {
    // swap coordinates to rotate the icon 90 degrees. The original
    // implementation applied the x/y multipliers to the wrong axes,
    // producing a mirrored/incorrect orientation. Use arrLy for the
    // x component and arrLx for the y component while preserving
    // xDir/yDir multipliers.
    for (int i = 0; i < 9; i++) {
      myOLED.pixel(xOff + xDir * arrLysmall[i], yOff + yDir * arrLxsmall[i], 255);
    }
  }
}

// getXangle / getYangle
// Compute tilt in degrees from accelerometer readings using
// the standard atan2 / vector magnitude method. These return
// the approximate angle between the gravity vector and the axis.
float getXangle() {
    return atan2(imu.data.accelX, sqrt(imu.data.accelY * imu.data.accelY + imu.data.accelZ * imu.data.accelZ)) * 180.0 / PI;
}

float getYangle() {
    return atan2(imu.data.accelY, sqrt(imu.data.accelX * imu.data.accelX + imu.data.accelZ * imu.data.accelZ)) * 180.0 / PI;
}

// buttonPress
// Interrupt Service Routine attached to the button pin. It uses
// millis() to debounce and to classify a single vs quick double press.
// Important: Keep ISRs short — here we only compute timing and set a
// volatile state variable for the main loop to act on.
void buttonPress() {
  unsigned long currentTime = millis();
  // Simple debounce: ignore presses occurring too quickly after the last
  if (currentTime - prevTime > debounceDelay) {
    if (currentTime - prevTime < doublePressTime) {
      // Two presses close together -> double press
      currentPress = DoublePress;}
    else if (currentTime - prevTime > longPressTime){
      currentPress = LongPress;
      }

    else {
      // Otherwise treat as a single press
      currentPress = SinglePress;
    }
    // record last press time
    prevTime = currentTime;
  }
}

void setup() {
  // Allow power to settle and USB serial to enumerate
  delay(1000);
  Serial.begin(9600); 
  while (!Serial) {
    // yield to avoid blocking if Serial isn't ready immediately
    yield();
  } 

  // Configure the button pin and attach an interrupt handler.
  // INPUT_PULLDOWN is useful on boards that support it; ensure your
  // hardware supports the chosen mode.
  pinMode(switchPin, INPUT_PULLDOWN);
  // Attach the ISR to the pin. The ISR must be fast and should avoid
  // using slow calls (Serial, delay, etc.). Here we use RISING edge.
  attachInterrupt(switchPin, buttonPress, RISING); 

  // Initialize I2C and IMU. If initialization fails it will retry
  // forever (useful for debugging / ensuring required sensors are present).
  Wire.begin();
  while (imu.beginI2C(0x68) != BMI2_OK) {
    delay(1000);
  }

  // Initialize the OLED; like the IMU we retry until success.
  while (!myOLED.begin()) {
    delay(1000);
  }

  Serial.println("Everything started!!!!!");
}

void loop() {
  // React to a double press by advancing the state machine. The code
  // cycles through states but then forces the state to be at least 1
  // which effectively skips the OffState when cycling.
  if (currentPress == DoublePress ) { //&& currentState != OffState
    if (off == false){
    currentState = (MachineState)(((int)currentState + 1) % (int)StateLength);
    // ensure we don't accidentally go back to OffState when cycling
    currentState = (MachineState)max((int)currentState, 1);}
  }

  if (currentPress != NoPress) {
    if (off == false){
    // For debugging: print the new state and clear the press flag so
    // that the main loop can act on it once only.
    Serial.print("Current State type: ");
    Serial.println((int)currentState);
    currentPress = NoPress;}
  }  

    if (currentPress == LongPress) {
        myOLED.erase(); 
        off = !off;
        if (off == true){
        currentState = OffState;}
        else {
          off == false;
          currentState =TwoAxis;
        }
    }
    // Clear the press event so we only act once per press.
    currentPress = NoPress;
    delay(100);


  // Prepare the display for fresh content
  myOLED.erase(); 

  // Read sensor data and compute tilt angles
  imu.getSensorData();
  theta = getXangle();
  psi = getYangle();

  // Render different content depending on the state machine
  switch (currentState)
  {
  case OffState:
    // Minimal indicator when 'off'
    myOLED.text(5,5, "OFF");
    break;

  case TwoAxis: 
    // Show a left/right indicator based on theta and an up/down
    // indicator based on psi. drawTriangle will place a small icon
    // near the appropriate screen edge.
    if (theta > 0.0) {
        if(theta> 0.5){
        drawTriangle(0, 23, 1, 1, false);}
        else{
        drawTriangleSmall(0, 23, 1, 1, false);}
    }
     else {
      if(theta<-0.5){
      drawTriangle(63, 23, -1, 1, false);}
      else{
      drawTriangleSmall(63, 23, -1, 1, false);
      }
    }
    if (psi > 0.0) {
      // positive psi -> draw near top
      if(psi> 0.5){
        drawTriangle(32, 0, 1, 1, true);}
        else{
        drawTriangleSmall(32, 0, 1, 1, true);}
    }
    else {
      if(psi<-0.5){
        drawTriangle(32, 45, 1, -1, true);}
        else{
        drawTriangleSmall(32, 45, 1, -1, true);}
      ;
    }
    break;

  case XAxis: 
    if (theta > 0.0) {
        if(theta> 0.5){
        drawTriangle(0, 23, 1, 1, false);}
        else{
        drawTriangleSmall(0, 23, 1, 1, false);}
    }
     else {
      if(theta<-0.5){
      drawTriangle(63, 23, -1, 1, false);}
      else{
      drawTriangleSmall(63, 23, -1, 1, false);
      }
    }
    break;

  case YAxis: 
    if (psi > 0.0) {
      // positive psi -> draw near top
      if(psi> 0.5){
        drawTriangle(32, 0, 1, 1, true);}
        else{
        drawTriangleSmall(32, 0, 1, 1, true);}
    }
    else {
      if(psi<-0.5){
        drawTriangle(32, 45, 1, -1, true);}
        else{
        drawTriangleSmall(32, 45, 1, -1, true);}
      ;
    }
    break;

  case RawData: 
    // Print raw accelerometer values on the OLED (formatted)
    sprintf(pout, "ax: %.2f", imu.data.accelX);
    myOLED.text(0,0, pout);
    sprintf(pout, "ay: %.2f", imu.data.accelY);
    myOLED.text(0,10, pout);
    sprintf(pout, "az: %.2f", imu.data.accelZ);
    myOLED.text(0,20, pout);
    break;

  default:
    break;
  }

  // Push the buffer to the display
  myOLED.display();
}