/*
Version of the perch rotation controller that slowly rotates the stepper motor
counterclockwise if the digital input pin 9 is high, 
clockwise if the digital input pin 5 is high
or holds position when both inputs are low.

Original code by Matteo Mischiati - July 2015
Modified by Abel Corver - October 2016 - February 2017

Modified by Abel Corver for compatibility with 2 motors - October 2016
 
Partly based on:
 
 TMC26XExample.ino - - TMC26X Stepper library Example for Wiring/Arduino
 
 Copyright (c) 2011, Interactive Matter, Marcus Nowotny
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/

// Include libraries
#include <SPI.h>
#include "TMC26XStepper.h"

// ================================================================================================
// Global variables & constants
// ================================================================================================

// Print debug output to Serial?
#define DEBUG             false

// Number of motors controlled by this Arduino
#define NUM_MOTORS        2

// Pins from Arduino to Stepper Shield
const int PIN_CS    [] = { 6 , 2 };
const int PIN_DIR   [] = { 7 , 3 };
const int PIN_STEP  [] = { 8 , 4 };
const int PIN_ENABLE[] = { 9 , 5 };

#define TIMER_CLOCK_FREQ  2000000.0    //2MHz for /8 prescale from 16MHz
#define INITIAL_CURRENT   175          //in mA

// We have a stepper motor with 200 steps per rotation, CS pin 2, dir pin 6, step pin 7 and a current of 200mA
TMC26XStepper steppers[2] = {
  
  // stepper 1
  TMC26XStepper( 
    200 , 
    PIN_CS   [0],
    PIN_DIR  [0],
    PIN_STEP [0],
    INITIAL_CURRENT),
  // Stepper 2
  TMC26XStepper( 
    200 , 
    PIN_CS   [1],
    PIN_DIR  [1],
    PIN_STEP [1],
    INITIAL_CURRENT)
};

// Whether to use digital pins to determine motor rotation ("true" interfacing with Cortex)
bool g_UseSerialCommands = false; 

// Global motor control variables
int g_MotorDir[NUM_MOTORS];
int g_MotorVelDir[NUM_MOTORS];
int g_MotorVel[NUM_MOTORS];

int g_MinMotorVel = 0; 
int g_MaxMotorVel = 5; 

int curr_step       [NUM_MOTORS];
int sgFilter        = 1;
int STEPS_PER_CYCLE = 7; 

// Input pins
const int pinMotorOn  [] = {33, 27}; // Motor turning?
const int pinMotorDir [] = {29, 23}; // What direction is that motor turning in?

// Serial variables
#define PC_SERIAL_BUF_LENGTH    512
#define PC_SERIAL_MAX_ARGS      16
char    g_PCSerialBuffer[PC_SERIAL_BUF_LENGTH];
uint8_t g_PCSerialBufferIdx;
char*   g_PCSerialArgs[PC_SERIAL_MAX_ARGS];

// ================================================================================================
// Setup
// ================================================================================================

void setup() {

  // Initialize variables
  for (int i = 0; i < NUM_MOTORS; i++) {
    
    curr_step[i]  = g_MotorDir[i] = g_MotorVel[i] = 0;
    g_MotorVelDir[i] = 1;
  }

  pinMode      ( 40, OUTPUT );
  digitalWrite ( 40, HIGH   );
  pinMode      ( 41, INPUT  );

  // Initialize each motor
  for (int i = 0; i < NUM_MOTORS; i++) {

    // Configure the enable pin
    pinMode      ( PIN_ENABLE[i], OUTPUT );
    digitalWrite ( PIN_ENABLE[i], LOW   );
    
    // initialize the input pins
    pinMode(pinMotorOn [i], INPUT); 
    pinMode(pinMotorDir[i], INPUT); 
        
    // Initialize motor
    //   (char constant_off_time, char blank_time, char hysteresis_start, char hysteresis_end, char hysteresis_decrement)
    steppers[i].setSpreadCycleChopper ( 2, 24, 8, 6, 0 );     // settings: t_off = 2; t_blank = 24; h_start = 8; h_end = 6; h_decrement = 0;
    steppers[i].setRandomOffTime      ( 0 );
    steppers[i].setMicrosteps         ( 256 );                //was originally (32)
    steppers[i].setStallGuardThreshold( 40, 1 );              //was originally (4.0) //1 means filter on
    steppers[i].start                 ();
  }

  // Initialize serial communication
  Serial.begin(9600); //shuld it be 115200?
  cmdIdentify(NULL, NULL);
}

// ================================================================
// Helper functions
// ================================================================

// -- command parsing function
uint8_t parse(char *line, char **argv, uint8_t maxArgs, char sep = ' ') {

  uint8_t argCount = 0;

  while (*line != '\0') {
    /* if not the end of line ....... */
    while (*line == sep || *line == '\n')
      *line++ = '\0';     /* replace commas and white spaces with 0    */
    *argv++ = line;         /* save the argument position     */
    argCount++;
    if (argCount == maxArgs - 1)
      break;
    while (*line != '\0' && *line != sep && *line != '\n')
      line++;             /* skip the argument until ...    */
  }

  *argv = '\0';             /* mark the end of argument list  */
  return argCount;
}

// ================================================================
// Identify the controller
// ================================================================

void cmdIdentify(char** pArgs, uint8_t numArgs) {
  Serial.println("Rotating Perch Controller --- Last modified by Abel Corver, March 2017");
}

// ================================================================
// Set maximum rotation speed
// ================================================================

void cmdHelp(char** pArgs, uint8_t numArgs) {

  Serial.println("Available commands: ");
  Serial.println("rotate");
  Serial.println("vel");
  Serial.println("h");
}

// ================================================================
// Set maximum rotation speed
// ================================================================

void cmdSetRotationSpeedRange(char** pArgs, uint8_t numArgs) {

  if (numArgs != 2) {
    Serial.print  ("Invalid parameters (");
    Serial.print  (numArgs);
    Serial.println(" given): 'vel <minVelocity> <maxVelocity>");
    return;
  }
  
  g_MinMotorVel = String(pArgs[0]).toInt();
  g_MaxMotorVel = String(pArgs[1]).toInt();
  
  Serial.print("Set motor velocity range to [");
  Serial.print(g_MinMotorVel);
  Serial.print(",");
  Serial.print(g_MaxMotorVel);
  Serial.println("].");
}

// ================================================================
// Rotate a perch
// ================================================================

void cmdRotate(char** pArgs, uint8_t numArgs) {

  if (numArgs != 2) {
    Serial.print  ("Invalid parameters (");
    Serial.print  (numArgs);
    Serial.println(" given): 'rotate <perchIndex> <direction>");
    return;
  }

  int perchIdx = String(pArgs[0]).toInt();
  int dir      = String(pArgs[1]).toInt();

  g_MotorDir[perchIdx] = dir;
  
  Serial.print("Rotating perch ");
  Serial.print(perchIdx);
  Serial.print(" in direction ");
  Serial.print(dir);
  Serial.println(".");

  if (!g_UseSerialCommands) {
    
    g_UseSerialCommands = true;

    Serial.println("Switching from digital input commands to serial commands.");
  }
}

// ================================================================
// Process serial commands
// ================================================================

void processSerialCommand(char** pArgs, uint8_t numArgs) {

  String cmd = String( pArgs[0] );
  void (*fun)(char**, uint8_t);

  if (cmd == "?") { fun = &cmdHelp; } else 
  if (cmd == "rotate") { fun = &cmdRotate; } else 
  if (cmd == "vel") { fun = &cmdSetRotationSpeedRange; } else 
  if (cmd == "h") { fun = &cmdIdentify; } else {

    // Command not recognized....
    Serial.print("Command not recognized: ");
    Serial.println(cmd);
    return;
  }
  
  // Execute selected function
  fun( &(pArgs[1]), numArgs - 1 );
}

// ================================================================================================
// Loop
// ================================================================================================

void loop() {

  // -----------------------------------
  // Read PC inputs
  // -----------------------------------

  while ( Serial.available() > 0 )
  {
    char c = Serial.read();

    if ( c != '\n' ) {
      if (g_PCSerialBufferIdx > PC_SERIAL_BUF_LENGTH - 1) {
        // Error: Out of range
        Serial.println("Error: serial input buffer overflow. Resetting buffer.");
        // Clear buffer
        memset(g_PCSerialBuffer, 0, PC_SERIAL_BUF_LENGTH);
        g_PCSerialBufferIdx = 0;
      } else {
        // Save the new character and continue
        g_PCSerialBuffer[g_PCSerialBufferIdx] = c;
        g_PCSerialBufferIdx += 1;
      }
    } else {
      // Parse arguments
      uint8_t numArgs = parse( (char*) g_PCSerialBuffer,
                               g_PCSerialArgs, PC_SERIAL_MAX_ARGS);
      // Execute command
      processSerialCommand( (char**) g_PCSerialArgs, numArgs);
      // Clear buffer
      memset(g_PCSerialBuffer, 0, PC_SERIAL_BUF_LENGTH);
      g_PCSerialBufferIdx = 0;
    }
  }

  // -----------------------------------
  // Rotate motors
  // -----------------------------------
  
  for (int motorID = 0; motorID < NUM_MOTORS; motorID++) {
    
    // read the state of the input values:
    int  dir = g_MotorDir[motorID];
    bool isMotorOn = (dir!=0); 

    if (!g_UseSerialCommands) {
      isMotorOn  = (digitalRead(pinMotorOn [motorID]) == HIGH);
      dir        = (digitalRead(pinMotorDir[motorID]) == HIGH) ? 1 : -1;
    }
    
    // Serial.println("Motor "+String(motorID)+" is "+String(isMotorOn)+" in dir "+String(dir));
  
    // Is the current motor on and not moving?
    if (isMotorOn && !steppers[motorID].isMoving()) {
  
      g_MotorVel[motorID] += g_MotorVelDir[motorID];
  
      if ( g_MotorVel[motorID] > g_MaxMotorVel ) {
        g_MotorVelDir[motorID] = -1;
        g_MotorVel[motorID]  = g_MaxMotorVel;
      } else if ( g_MotorVel[motorID] < g_MinMotorVel ) {
        g_MotorVelDir[motorID] = 1;
        g_MotorVel[motorID]  = g_MinMotorVel;
      }
  
      // Set the g_MotorVel (negative if dir==-1)
      steppers[motorID].setSpeed( g_MotorVel[motorID] * dir );
  
      if (DEBUG) {
        Serial.print("Rotation requested: setting motor velocity to ");
        Serial.println( g_MotorVel[motorID] * dir );
      }
  
      // We want some kind of constant running time - so the length is just a product of g_MotorVel
      steppers[motorID].step( STEPS_PER_CYCLE * dir );   
    } 

    // Finally, move each stepper
    steppers[motorID].move();
  }
  delayMicroseconds(1000);
}


