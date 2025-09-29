#include <Arduino.h>
#define MICRO
#include <stdint.h>
#include <SparkFun_Qwiic_OLED.h>
#include "SparkFun_LSM6DSV16X.h"
#include <Wire.h>
#include "SparkFun_BMI270_Arduino_Library.h"
#include <SparkFun_Qwiic_OLED.h>
//libraries for led 

//create appropriate object for oled
QwiicMicroOLED myOLED;
BMI270 imu;

SparkFun_LSM6DSV16X myLSM;

// Structs for X,Y,Z data
sfe_lsm_data_t accelData;
sfe_lsm_data_t gyroData;

volatile int buttonCounter = 0;
bool prevPressed = false;
int switchPin = 10;
unsigned long debounceDelay = 50;
unsigned long doublePressTime = 500;
unsigned long prevTime = 0;

float theta = 0.0;
float psi = 0.0;
float phi = 0.0;

int arrLx[]={0, 1,1,1, 2, 2,2,2,2, 3, 3, 3,3,3,3,3};
int arrLy[]={0,-1,1,1,-2,-1,0,1,2,-3,-2,-1,0,1,2,3};
char pout[30];

enum PressType{
  NoPress, //0
  SinglePress, //1
  DoublePress, //2
  LongPress, //3
};

enum MachineState{
  OffState,
  TwoAxis,
  XAxis,
  YAxis,
  RawData,
  StateLength
};

volatile PressType currentPress = NoPress;
volatile MachineState currentState = OffState;

//Triangle draw function
void drawTriangle(int xOff, int yOff, int xDir, int yDir,bool swap){
  if(!swap){
  for (int i = 0; i<16; i++){
    myOLED.pixel(xOff+xDir*arrLx[i],yOff+yDir* arrLy[i],255);
  }}
  else{
    for (int i = 0; i<16; i++){
    myOLED.pixel(xOff+xDir*arrLy[i],yOff+yDir* arrLx[i],255);
  }
}

float getXangle(){
    return atan2(imu.data.accelX,sqrt(imu.data.accelY*imu.data.accelY+imu.data.accelZ*imu.data.accelZ))*180.0/PI;
}

float getYangle(){
        return atan2(imu.data.accelY,sqrt(imu.data.accelX*imu.data.accelX+imu.data.accelZ*imu.data.accelZ))*180.0/PI;

}
        //theta = atan2(imu.data.accelX,sqrt(imu.data.accelY*imu.data.accelY+imu.data.accelZ*imu.data.accelZ))
        //psi= atan2(imu.data.accelY,sqrt(imu.data.accelX*imu.data.accelX+imu.data.accelZ*imu.data.accelZ))
        //phi = atan2(imu.data.accelZ,sqrt(imu.data.accelY*imu.data.accelY+imu.data.accelX*imu.data.accelX))


void buttonPress() {
  unsigned long currentTime = millis();
  if (currentTime - prevTime > debounceDelay) {
    if(currentTime-prevTime <doublePressTime){
      currentPress = DoublePress;
    }else{
      currentPress =SinglePress;
    }
    prevTime = currentTime;
  }
}




void setup() {
  Wire.begin();
  delay(2000);
  Serial.begin(9600);
  delay(2000);
  pinMode(switchPin, INPUT_PULLDOWN);
  attachInterrupt(switchPin, buttonPress, RISING);

  //start LED screen
  while (!myOLED.begin()){
    delay(1000);
  }
 
}

//start accel
Wire.begin();
while (!imu.beginI2C(0x68) !=BMI2_OK){
  delay(1000)
}
//start LED Screen
while(!myOLED.begin){
  delay(1000);
  println("screen started");
}

void loop() {
  if(currentPress == DoublePress ){ //&& currentState !=OffState
    currentState = (MachineState)(((int)currentState + 1) %(int)StateLength);
    currentState = (MachineState)max((int)currentState,1);
  }
  if(currentPress != NoPress){
    Serial.print("Current State type:");
    Serial.println((int)currentState);
    currentPress = NoPress;
  }
  if(currentState = (MachineState)(OffState)){

  }
  if(currentState = (MachineState)(TwoAxis)){
    
  }
  if(currentState = (MachineState)(XAxis)){
    
  }
  if(currentState = (MachineState)(YAxis)){
    
  }
  if(currentState = (MachineState)(RawData)){
    
  }
  if(currentState = (MachineState)(StateLength)){
    
  }
  
    //erase screen
    myOLED.erase();
    //get imu data
    theta = getXangle();
    psi = getYangle();

    imu.getSensorData();
    switch(currentState)
    {
      case OffState:
        myOLED.text(5,5,"It worked!");
      break;
      case TwoAxis:
        if (theta>0.0){
        drawTriangle(0,23,1,1,false);
        }
      else{
        drawTriangle(63,23,-1,1,false);}
      break;
      
      case XAxis:
        //get accel data
        //see if angle > some min
        //draw triangle
      break;
      case YAxis:
      break;
      case RawData:
        sprintf(pout,"ax:%.2f", imu.data.accelX);
        myOLED.text(0,0,pout);
        sprintf(pout,"ay:%.2f", imu.data.accelY);
        myOLED.text(0,10,pout);
        sprintf(pout,"az:%.2f", imu.data.accelZ);
        myOLED.text(0,20,pout);
        break;
      default:
      break;
    }
    //display
    myOLED.display();
}