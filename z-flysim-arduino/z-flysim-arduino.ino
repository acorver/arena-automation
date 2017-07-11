
// Pins
#define PIN_ENCODER_1_A 3
#define PIN_ENCODER_1_B 5
#define PIN_ENCODER_2_A 22
#define PIN_ENCODER_2_B 20

const int MOTOR_DIRECTION_FACTOR[] = { 1, 1 };

#define NUM_MOTORS 2

#define SABERTOOTH_BAUD 9600
#define SABERTOOTH_ADDRESS 128

const int MOTOR_POWER_DEADBAND_UP  [] = {28, 30};
const int MOTOR_POWER_DEADBAND_DOWN[] = {10, 10};
const int MAX_MOTOR_POWER[] = {85, 85};
const int MIN_MOTOR_POWER[] = {-MOTOR_POWER_DEADBAND_DOWN[0] - 10, -MOTOR_POWER_DEADBAND_DOWN[1] - 10};

// Import serial library and set serial object connected to Sabertooth
#include <Sabertooth.h>
#define SabertoothTXPinSerial Serial2

// Library for PID / Encoder support
#include "PID_v1.h"
#include <Wire.h>
#include "Encoder.h"

// ----------------------
// Variables for core functionality
// ----------------------

// Timing variables
unsigned long CurrentTime = 0; // current time stamp
unsigned long LastTime    = 0;            // last time stamp
unsigned long g_CommandStartTime = 0;

// The name of the function currently running (for status/debugging purposes)
String g_CurrentUpdateFunctionStr = "";

// The function in charge of updating the trajectory until its completion
void (*g_CurrentUpdateFunction)(long elapsedTime);

// ----------------------
// Position information
// ----------------------

// The sabertooth motor driver
Sabertooth* pST; 

// The encoder objects
Encoder encoders[] = {
  Encoder(PIN_ENCODER_1_A, PIN_ENCODER_1_B),
  Encoder(PIN_ENCODER_2_A, PIN_ENCODER_2_B)
};

#define INVALID_DOUBLE 0

double g_Position[]       = {0, 0};
double g_TargetPosition[] = {0, 0};
double g_MotorPower[]     = {INVALID_DOUBLE, INVALID_DOUBLE};

bool g_MotorsOn[] = {false, false};
bool g_UsePID[] = {false, false};

double g_PositionPID_Kp   = 0.100;
double g_PositionPID_Ki   = 1.000;
double g_PositionPID_Kd   = 0.0005; 

PID g_PositionPID[] = {
  PID(&(g_Position[0]), &(g_MotorPower[0]), &(g_TargetPosition[0]), g_PositionPID_Kp, g_PositionPID_Ki, g_PositionPID_Kd, DIRECT),
  PID(&(g_Position[1]), &(g_MotorPower[1]), &(g_TargetPosition[1]), g_PositionPID_Kp, g_PositionPID_Ki, g_PositionPID_Kd, DIRECT)
};

long g_EncoderExtremes[][NUM_MOTORS] = {
  {0, 0}, 
  {0, 0}
};

// ================================================================
// Helper function for parsing Serial input
// ================================================================

// Read buffer for commands from PC
#define PC_SERIAL_BUF_LENGTH    512
#define PC_SERIAL_MAX_ARGS      16

char    g_PCSerialBuffer[PC_SERIAL_BUF_LENGTH];
uint8_t g_PCSerialBufferIdx;
char*   g_PCSerialArgs[PC_SERIAL_MAX_ARGS];

char    g_PCSerialBuffer2[PC_SERIAL_BUF_LENGTH];
uint8_t g_PCSerialBufferIdx2;
char*   g_PCSerialArgs2[PC_SERIAL_MAX_ARGS];

// -- command parsing function
uint8_t parse(char *line, char **argv, uint8_t maxArgs, char sep = ' ') {

  uint8_t argCount = 0;

  while (*line != '\0') {
    /* if not the end of line ....... */
    while (*line == sep || *line == '\n')
      *line++ = '\0';     /* replace commas and white spaces with 0    */
    *argv++ = line;       /* save the argument position     */
    argCount++;
    if (argCount == maxArgs - 1)
      break;
    while (*line != '\0' && *line != sep && *line != '\n')
      line++;             /* skip the argument until ...    */
  }

  *argv = '\0';           /* mark the end of argument list  */
  return argCount;
}

// ================================================================
// the setup routine runs once when you press reset:
// ================================================================

void setup() {
  // Wait for serial to connect
  Serial1.begin(115200);
  Serial.begin(115200);
  
  // IMPORTANT NOTE:
  //    PIN_ENCODER_A and PIN_ENCODER_B should *NOT* be configured as INPUT (or any other 
  //    pinMode(...) for that matter). This will invalidate the INTERRUPT logic set by the 
  //    Encoder.h library. Also note that using Encoder.h in INTERRUPT mode (rather than 
  //    polling mode) was found to be required to reach the level of performance 
  //    necessary for this library to function normally. 
  
  // Configure PID's
  for (int i = 0; i < NUM_MOTORS; i++) {

    // Set current encoder position to 0 (because that's what the PID original setpoint is)
    encoders[i].write(0);
    
    // Configure PIDs
    g_PositionPID[i].SetOutputLimits(MIN_MOTOR_POWER[i], MAX_MOTOR_POWER[i]);
    g_PositionPID[i].SetSampleTime(1);
    g_PositionPID[i].SetMode(AUTOMATIC);
  }

  cmdReset(NULL, NULL);
  
  // Print useful help information
  cmdIdentify(NULL, NULL);
  cmdHelp(NULL, NULL); 
}

// ================================================================
// ====== COMMAND: Reset
// ================================================================

void cmdReset(char** pArgs, uint8_t numArgs) {
  
  g_CommandStartTime = micros();
  
  g_CurrentUpdateFunction = updateReset;
  g_CurrentUpdateFunctionStr = "cmdReset";
}

void updateReset(long elapsedTime) {

  if ( (micros() - g_CommandStartTime) < 5e6 ) {
    pST = new Sabertooth(SABERTOOTH_ADDRESS);
    delay(200);
    
    while (!SabertoothTXPinSerial) { delay(1); }
    SabertoothTXPinSerial.begin(SABERTOOTH_BAUD);
  
    cmdAbort(NULL, NULL);

    for (int i = 0; i < NUM_MOTORS; i++) {
      encoders[i].write(0);
    }
  
    Serial.println("(Re-)established connection with Sabertooth motor driver.");
    
    g_CurrentUpdateFunction = NULL;
    g_CurrentUpdateFunctionStr = "";
  }
}

// ================================================================
// ====== COMMAND: Identify
// ================================================================

void cmdIdentify(char** pArgs, uint8_t numArgs) {
  Serial.println("Flysim Z controller --- last updated by Abel Corver, May 2017");
}

// ================================================================
// ====== COMMAND: Abort
// ================================================================

void cmdAbort(char** pArgs, uint8_t numArgs) {
  g_CurrentUpdateFunction = NULL;
  g_CurrentUpdateFunctionStr = "";

  // Stop motors
  g_MotorsOn[0] = g_MotorsOn[1] = false;
  g_MotorPower[0] = g_MotorPower[1] = INVALID_DOUBLE;
}

// ================================================================
// ====== COMMAND: Test speed resulting from certain power
// ================================================================

void cmdTestPower(char** pArgs, uint8_t numArgs) {
  
  int idx = 0;
  g_MotorsOn[idx] = true;
  g_UsePID[idx] = true;
  g_TargetPosition[idx] = 400;

  g_CommandStartTime = micros();

  g_CurrentUpdateFunction = updateTestPower;
  g_CurrentUpdateFunctionStr = "cmdTestPower";
}

double g_StartPosition[] = {0, 0};

void updateTestPower(long elapsedTime) {

  if ( abs( g_Position[0] - g_TargetPosition[0] ) < 50 ) {
    
    Serial.println("Reached center");
    g_CommandStartTime = micros();
    g_CurrentUpdateFunction = updateTestPower2;
  }
}

void updateTestPower2(long elapsedTime) {

  if ( (micros() - g_CommandStartTime) > 2e6) {
      
    int idx = 0;
    g_MotorsOn[idx] = true;
    g_UsePID[idx] = false;
    g_MotorPower[idx] = 40;
    
    g_CommandStartTime = micros();
    g_StartPosition[idx] = g_Position[idx];
    g_CurrentUpdateFunction = updateTestPower3;
  }
}

void updateTestPower3(long elapsedTime) {

  long timeSinceStart = micros() - g_CommandStartTime;

  if (timeSinceStart > 5e5) {
    float velocity = (g_Position[0] - g_StartPosition[0]) / float(timeSinceStart);
    Serial.print("Reached velocity: ");
    Serial.println(velocity);
    
    int idx = 0;
    g_MotorsOn[idx] = true;
    g_UsePID[idx] = true;
    g_TargetPosition[0] = 400;
    
    g_CurrentUpdateFunction = NULL;
    g_CurrentUpdateFunctionStr = "";
  }
}

// ================================================================
// ====== COMMAND: Set motor power
// ================================================================

void cmdPower(char** pArgs, uint8_t numArgs) {

  if (numArgs!=2) {
    Serial.println("Invalid arguments.");
    return;
  }

  int idx   = String(pArgs[0]).toInt();
  int power = String(pArgs[1]).toInt();

  if (idx < 0 || idx > 1 || power < -255 || power > 255) {
    Serial.println("Invalid arguments.");
    return;
  }

  // Update time
  g_CommandStartTime = micros();

  g_MotorsOn[idx] = true;
  g_UsePID[idx] = false;
  g_MotorPower[idx] = power;
}

// ================================================================
// ====== COMMAND: Position
// ================================================================

void cmdPosition(char** pArgs, uint8_t numArgs) {

  int idx = 0;
  int pos = 0;
  
  if (numArgs==2) {
    idx = String(pArgs[0]).toInt();
    pos = String(pArgs[1]).toInt();
    
    if (idx < 0 || idx > 1) { // TODO: Check for bounds here!!
      Serial.println("Invalid arguments.");
      return;
    }
  } else if (numArgs==1) {
    idx = -1;
    pos = String(pArgs[0]).toInt();
  } else {
    Serial.println("Invalid arguments.");
    return;
  }

  if (idx < 0) {
    for (int i = 0; i < NUM_MOTORS; i++) {
      if (abs(g_TargetPosition[i]-pos) > 0.0001) {
        // Update time
        g_CommandStartTime = micros();
      }
      
      g_MotorsOn[i] = true;
      g_UsePID[i] = true;
      g_TargetPosition[i] = pos;
    }
  } else {    
    g_MotorsOn[idx] = true;
    g_UsePID[idx] = true;
    g_TargetPosition[idx] = pos;
  }
}

// ================================================================
// ====== COMMAND: Help
// ================================================================

void cmdHelp(char** pArgs, uint8_t numArgs) {
  Serial.println("Available commands: ");
  Serial.println("h");
  Serial.println("abort");
  Serial.println("power");
  Serial.println("position");
}

// ================================================================
// Route serial commands to the right function
// ================================================================

void processSerialCommand(char** pArgs, uint8_t numArgs) {

  String cmd = String( pArgs[0] );
  void (*fun)(char**, uint8_t);

  // Determine function to execute
  if (cmd == "h"        ) { fun = &cmdIdentify; } else 
  if (cmd == "abort"    ) { fun = &cmdAbort; } else 
  if (cmd == "test_power"    ) { fun = &cmdTestPower; } else 
  if (cmd == "power"    ) { fun = &cmdPower; } else 
  if (cmd == "position" ) { fun = &cmdPosition; } else 
  if (cmd == "reset"    ) { fun = &cmdReset; } else 
  if (cmd == "?"        ) { fun = &cmdHelp; } else {
    
    // Command not recognized....
    Serial.print("Command not recognized: ");
    Serial.println(cmd);
    return;
  }
  

  if (g_CurrentUpdateFunction != NULL && fun != &cmdAbort && fun != &cmdIdentify && 
    fun != &cmdHelp) {
    Serial.println("Aborting command, other function in progress");
    return;
  }
  
  // Execute selected function
  fun( &(pArgs[1]), numArgs - 1 );
}

// ================================================================
// the loop routine runs over and over again forever:
// ================================================================

void loop() {
  
  // -----------------------------------
  // Update timing variables
  // -----------------------------------
  
  CurrentTime = micros();
  long elapsedTime = CurrentTime - LastTime;

  // -----------------------------------
  // Process serial commands
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
  
  while ( Serial1.available() > 0 )
  {
    char c = Serial1.read();

    if ( c != '\n' ) {
      if (g_PCSerialBufferIdx2 > PC_SERIAL_BUF_LENGTH - 1) {
        // Error: Out of range
        Serial.println("Error: serial input buffer overflow. Resetting buffer.");
        // Clear buffer
        memset(g_PCSerialBuffer2, 0, PC_SERIAL_BUF_LENGTH);
        g_PCSerialBufferIdx2 = 0;
      } else {
        // Save the new character and continue
        g_PCSerialBuffer2[g_PCSerialBufferIdx2] = c;
        g_PCSerialBufferIdx2 += 1;
      }
    } else {
      // Parse arguments
      uint8_t numArgs = parse( (char*) g_PCSerialBuffer2,
                               g_PCSerialArgs2, PC_SERIAL_MAX_ARGS);
      // Execute command
      processSerialCommand( (char**) g_PCSerialArgs2, numArgs);
      // Clear buffer
      memset(g_PCSerialBuffer2, 0, PC_SERIAL_BUF_LENGTH);
      g_PCSerialBufferIdx2 = 0;
    }
  }

  // -----------------------------------
  // Update / Power motor
  // -----------------------------------

  for (int i = 0; i < NUM_MOTORS; i++) {
    
    // Update position
    g_Position[i] = encoders[i].read();
    
    if (g_MotorsOn[i]) {
      if (g_UsePID[i]) {
        // Determine power to use using PID (this updates pre-assigned variables)
        g_PositionPID[i].Compute();
        // cycle motor power to overcome friction
        double e = g_Position[i] - g_TargetPosition[i];
        
        if (abs(e) < 12 || (micros() - g_CommandStartTime) > 10e6) {
          g_MotorPower[i] = 0.5 * (MOTOR_POWER_DEADBAND_DOWN[i]+MOTOR_POWER_DEADBAND_UP[i]);
          pST->motor(i+1, g_MotorPower[i]);
        } else if ( false && (millis() % 100) < 5 ) { //(e>=0?0:max(0, min(20, -(abs(e)>20?e:0) * 0.2))) ) {
          // pST->motor(i+1, (MOTOR_POWER_DEADBAND_DOWN[i]+MOTOR_POWER_DEADBAND_UP[i])/2);
          pST->motor(i+1, -10);
        } else if ( false && (millis() % 100) < 10 ) { //(e>=0?0:max(0, min(20, -(abs(e)>20?e:0) * 0.2))) ) {
          pST->motor(i+1,  30);
        } else {
          // Send motor speed to motor driver (Hardwired max-speed override)
          g_MotorPower[i] = max(min( (g_MotorPower[i] + ( g_MotorPower[i] > 0 ? MOTOR_POWER_DEADBAND_UP[i] : MOTOR_POWER_DEADBAND_DOWN[i] )) * 
            MOTOR_DIRECTION_FACTOR[i], MAX_MOTOR_POWER[i]), MIN_MOTOR_POWER[i]);
          pST->motor(i+1, g_MotorPower[i]);
        }
      } else {
        // Send motor speed to motor driver (Hardwired max-speed override)
        pST->motor(i+1, max(min(g_MotorPower[i] * MOTOR_DIRECTION_FACTOR[i], 80), -20));
      }
    } else {
      // Power down motor
      pST->motor(i+1, 0);
    }

    Serial.print("a ");
    Serial.print(digitalRead(PIN_ENCODER_1_A));
    Serial.print(",");
    Serial.print(g_Position[0]);
    Serial.print(",");
    Serial.print(g_Position[1]);
    Serial.print(",");
    
    Serial.print(g_MotorPower[0]);
    Serial.print(",");
    Serial.print(g_MotorPower[1]);
    Serial.print(",");
    
    Serial.println("");
  }
  
  // -----------------------------------
  // Update the currently running function
  // -----------------------------------

  // This depends on what command was executed
  if (g_CurrentUpdateFunction != NULL) {
    g_CurrentUpdateFunction(elapsedTime);
  }
  LastTime = CurrentTime;
}
