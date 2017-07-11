
/*
  This script controls a bead on a line, and is updated to interface with the Arena Automation system.

  Last updated by Abel Corver, March 2017
  Derived from code by Matteo Mischiati & Huai-Ti Lin
*/

// ================================================================
// Includes
// ================================================================

#include <EEPROM.h>
#include <Encoder.h>
#include <Kangaroo.h>

#include <ArduinoJson.h>

// ================================================================
// Quick-access settings
// ================================================================

const bool TRIALS_REQUIRE_PERCH = true;

#define USE_KANGAROO_FOR_Z false

#define ENABLE_KANGAROO_Z1 true && USE_KANGAROO_FOR_Z
#define ENABLE_KANGAROO_Z2 true && USE_KANGAROO_FOR_Z

// ================================================================
// Helper functions
// ================================================================

// -- command parsing function
uint8_t parse(char *line, char **argv, uint8_t maxArgs, char sep = ',') {

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
//
// ================================================================

#define PC_SERIAL_BUF_LENGTH    512
#define PC_SERIAL_MAX_ARGS      16

// Read buffer for commands from PC
char    g_PCSerialBuffer[PC_SERIAL_BUF_LENGTH];
uint8_t g_PCSerialBufferIdx;
char*   g_PCSerialArgs[PC_SERIAL_MAX_ARGS];

// ----------------------
// Timing variables
// ----------------------

unsigned long delTime     = 0;
unsigned long Interval    = 20000;        // 20ms
unsigned long LastTime    = 0;            // last time stamp
unsigned long CurrentTime = 0;            // current time stamp
unsigned long StartTime   = CurrentTime;  // mark the t0 point in microsecond

// ----------------------
// Kangaroo variables
// ----------------------

#define TUNE_MODE_TEACH              1
#define TUNE_MODE_MECHANICAL_STOP    3                   // MechStop is default for the FlySim X control

Encoder Enc2(2, 3); // FlySim desktop model objects

#define MAX_X_ERROR         32                  // Maximum error in Kangaroo positioning that still counts as "goal achieved"
#define MAX_Z_ERROR         32                  // Maximum error in Kangaroo positioning that still counts as "goal achieved"

long WHEEL_SIZE_X  =  40;
float CONVERSION_X  =  WHEEL_SIZE_X * PI / 256;

long WHEEL_SIZE_Z  =  34;
float CONVERSION_Z  =  WHEEL_SIZE_Z * PI / 35;

KangarooSerial  *kangarooSerialX  = 0;   // This is the X-axis Kangaroo
KangarooChannel *kangarooX        = 0;   // This is the FlySim X

KangarooSerial  *kangarooSerialZ  = 0;   // This is the Z-axis Kangaroo
KangarooChannel *kangarooZ1       = 0;   // This is the FlySim pod 1
KangarooChannel *kangarooZ2       = 0;   // This is the FlySim pod 2

int g_AbortKangaroo = 0;

#define KANGAROO_COMMAND_TIMEOUT_MS   100

// ----------------------
//  Pins...
// ----------------------

#define PIN_DFPRESENT    8
#define PIN_DFMOVE       9
#define PIN_MACTRIGGER  10
#define PIN_SYNC        11

#define PIN_STOP         7

// ----------------------
//  Misc parameters
// ----------------------

// Maximum length in bytes/characters of JSON objets (e.g. status output)
#define JSON_MAX_LENGTH 2048

// Is cortex recording?
bool g_bCortexRecording = false;

// ----------------------
//  Variables for space exploration function
// ----------------------

long g_TimeUntilNextPosition = 0;

// ----------------------
// Variables indicating what function is running
// ----------------------

// The name of the function currently running (for status/debugging purposes)
String g_CurrentUpdateFunctionStr = "";

// The function in charge of updating the trajectory until its completion
void (*g_CurrentUpdateFunction)(long elapsedTime);

// Status of program
const char* g_Status = "";

// Function start time
long g_CurrentCommandStartTime;

// ----------------------
// sensor variables
// ----------------------

long g_CurPosX;
long g_CurPosZ1;
long g_CurPosZ2;

long g_CurVelX;
long g_CurVelZ1;
long g_CurVelZ2;

long g_MinPosX;
long g_MaxPosX;

// ----------------------
//  State variables for trajectory functions
// ----------------------

#define MVM_TYPE_WAIT    0
#define MVM_TYPE_LINEAR  1

#define MOVEMENT_SEGM_MAX_COUNT  64
#define MOVEMENT_SEGM_MAX_ARGS   5

long   g_MovementSegments[MOVEMENT_SEGM_MAX_COUNT][MOVEMENT_SEGM_MAX_ARGS];
long   g_curMovementSegment;
long   g_numMovementSegments;

#define NUM_VELOCITY_SEGMENTS 2

long g_VelocitySegments     [NUM_VELOCITY_SEGMENTS  ];
long g_VelocitySegmentBounds[NUM_VELOCITY_SEGMENTS - 1];

long currentVelocityNoiseX;
long targetVelocityNoiseTimeLeft;
long targetVelocityNoiseX;

long targetVelocityX;
long targetVelocityZ;
long targetPositionX;
long targetPositionZ;

long g_TimeUntilTrial = 0;
int  g_TrialIdx = 0;

long g_VelocityBoostTimeLeft = 0;
long g_VelocityBoostDir = 0;
bool g_VelocityBoostEnabled = false;

// ----------------------
//  DF state variables
// ----------------------

bool g_DfReadyOnPerch = false;

boolean       DF_presence         = 0;
boolean       DF_moving           = 0;
boolean       DF_ready            = 0;  //this variable indicates that the DF is ready for the next stimulus
unsigned long DF_idle_time        = 0;
unsigned long idle_time_threshold = 5 * 1000000; // if the DF is idling for 5s

// ================================================================
// Check DF status
// ================================================================

void DF_check() {

  if (DF_presence == 1 && DF_moving == 0) {
    DF_idle_time = DF_idle_time + delTime;
  } else {
    DF_idle_time = 0;
  }
  if (DF_idle_time >= idle_time_threshold) {
    DF_ready = 1;
  } else {
    DF_ready = 0;
  }
}

// ================================================================
// (Un)Trigger cortex
// ================================================================

void triggerCortex(bool trigger) {

  if (g_bCortexRecording != trigger) {

    g_bCortexRecording = trigger;

    digitalWrite(PIN_MACTRIGGER, LOW);
    delay(100);
    digitalWrite(PIN_MACTRIGGER, HIGH);
  }
}

void cmdTriggerCortex(char** pArgs, uint8_t numArgs) {

  triggerCortex(!g_bCortexRecording);
}

// ================================================================
// Setup
// ================================================================

void setup() {

  Serial.begin(9600); // This is COM6 that talks to the PC

  // Initialize random seed with noise from analog pin
  randomSeed(analogRead(0));

  // Initialize the emergency stop switch
  pinMode(PIN_STOP, INPUT);

  // This aux recieves DF presence signal
  pinMode(PIN_DFPRESENT, INPUT);

  // This aux recieves DF movement signal
  pinMode(PIN_DFMOVE, INPUT);

  // This aux initiate the recording via SyncMaster: rising edge triggers MAC capture, falling edge triggers Photron end
  pinMode(PIN_MACTRIGGER, OUTPUT);

  // This aux recieves the sync signal from SyncMaster
  pinMode(PIN_SYNC, INPUT);

  // Identify
  cmdIdentify(NULL, NULL);

  // (Try to) Start motors
  cmdStartKangaroo(NULL, NULL);
}

// ================================================================
// Error reporting function
// ================================================================

void sendError(const char* str) {
  Serial.println(str);
}

// ================================================================
// Detect error with Kangaroo operation
// ================================================================

bool kangarooAvailable(char a = ' ') {

  bool kx = true, kz = true;

  if (a == ' ' || a == 'x') {
    if (kangarooSerialX == 0) {
      kx = false;
    } else {
      if (kangarooX->getP().error()) {
        kx = false;
      }
    }
  }

  if (a == ' ' || a == 'z') {
    if (kangarooSerialZ == 0) {
      kz = false;
    } else {
      if (kangarooZ2 && kangarooZ2->getP().error()) {
        kz = false;
      }
      if (kangarooZ1 && kangarooZ1->getP().error()) {
        kz = false;
      }
    }
  }

  if (a == ' ') {
    return (kx && kz);
  } else if (a == 'x') {
    return kx;
  } else if (a == 'z') {
    return kz;
  } else {
    // Invalid parameters...
    return false;
  }
}

// ================================================================
// ====== COMMAND: IDENTIFY
// ================================================================

void cmdIdentify(char** pArgs, uint8_t numArgs) {
  Serial.println("FlySim Arduino Controller --- last edited by Abel Corver");
}

// ================================================================
// ====== COMMAND: STATUS
// ================================================================

void cmdStatus(char** pArgs, uint8_t numArgs) {

  Serial.print("{");

  Serial.print("\"function\":\""+g_CurrentUpdateFunctionStr+"\",");

  Serial.print("\"status\":\""+String(g_Status)+"\",");

  if (kangarooZ1) {
    Serial.print("\"status_z1\":\""+String(kangarooZ1->getP().error())+"\",");
  }
  if (kangarooZ2) {
    Serial.print("\"status_z2\":\""+String(kangarooZ2->getP().error())+"\",");
  }
  
  Serial.print("\"PosX\":" +String(g_CurPosX )+",");
  Serial.print("\"PosZ1\":"+String(g_CurPosZ1)+",");
  Serial.print("\"PosZ2\":"+String(g_CurPosZ2)+",");

  Serial.print("\"MinPosX\":"+String(g_MinPosX)+",");
  Serial.print("\"MaxPosX\":"+String(g_MaxPosX)+",");

  Serial.print("\"VelX\":" +String(g_CurVelX )+",");
  Serial.print("\"VelZ1\":"+String(g_CurVelZ1)+",");
  Serial.print("\"VelZ2\":"+String(g_CurVelZ2)+",");

  Serial.print("\"TargetPosX\":" +String(targetPositionX)+",");
  Serial.print("\"TargetPosZ1\":"+String(targetPositionZ)+",");
  Serial.print("\"TargetPosZ2\":"+String(targetPositionZ)+",");
  
  Serial.print("\"TargetVelX\":" +String(targetVelocityX)+",");
  Serial.print("\"TargetVelZ1\":"+String(targetVelocityZ)+",");
  Serial.print("\"TargetVelZ2\":"+String(targetVelocityZ)+",");
  
  Serial.print("\"TargetVelX_Segments\":[");
  for (int i = 0; i < NUM_VELOCITY_SEGMENTS  ; i++) {
    Serial.print(g_VelocitySegments[i]);
    if (i < NUM_VELOCITY_SEGMENTS-1) { Serial.print(","); }
  }
  Serial.print("],");
  
  Serial.print("\"TargetVelX_Bounds\":[");
  for (int i = 0; i < NUM_VELOCITY_SEGMENTS - 1; i++) {
     Serial.print(g_VelocitySegmentBounds[i]);
     if (i < NUM_VELOCITY_SEGMENTS-2) { Serial.print(","); }
  }
  Serial.print("],");

  Serial.print("\"TargetVelNoiseX\":"+String(targetVelocityNoiseX)+",");
  Serial.print("\"TimeUntilTrial\":"+String(g_TimeUntilTrial));
  
  Serial.println("}");
}

// ================================================================
// ====== COMMAND: (Re)Start Kangaroo drivers
// ================================================================

void cmdStartKangaroo(char** pArgs, uint8_t numArgs) {

  Serial.println("Starting Kangaroo drivers");

  // delete previous objects
  if (kangarooSerialX != 0 || kangarooSerialZ != 0) {

    kangarooX->powerDown();
    
    if (kangarooZ1 != NULL) {
      kangarooZ1->powerDown();
      delete kangarooZ1;
      kangarooZ1 = 0;
    }
    if (kangarooZ2 != NULL) {
      kangarooZ2->powerDown();
      delete kangarooZ2;
      kangarooZ2 = 0;
    }

    delete kangarooSerialX;
    delete kangarooSerialZ;
    delete kangarooX;
  }

  // Configure serial ports
  Serial1.begin(115200); // This is the serial port to FlySim
  if (USE_KANGAROO_FOR_Z) {
    Serial2.begin(9600); // This is the serial port for FlySim height controller
    kangarooSerialZ = new KangarooSerial(Serial2);           // This is the Z-axis Kangaroo
  } else {
    Serial2.begin(115200);
    while(!Serial2) { delay(1); }
    Serial2.println("reset");
  }
  
  // Configure Kangaroo serial
  kangarooSerialX = new KangarooSerial(Serial1);           // This is the X-axis Kangaroo
  
  delay(500);

  // Configure controllers
  kangarooX  = new KangarooChannel(*kangarooSerialX, '1');   // This is the FlySim X
  if (ENABLE_KANGAROO_Z1 && USE_KANGAROO_FOR_Z) {
    kangarooZ1 = new KangarooChannel(*kangarooSerialZ, '1');  // This is the FlySim pod 1
  }
  if (ENABLE_KANGAROO_Z2 && USE_KANGAROO_FOR_Z) {
    kangarooZ2 = new KangarooChannel(*kangarooSerialZ, '2');  // This is the FlySim pod 2
  }
  
  delay(500);

  // Configure timeouts
  kangarooX->commandTimeout(100);
  if (kangarooZ1 && USE_KANGAROO_FOR_Z) {
    kangarooZ1->commandTimeout(1000);
  }
  if (kangarooZ2 && USE_KANGAROO_FOR_Z) {
    kangarooZ2->commandTimeout(1000);
  }
  
  // Initialize the X controller
  KangarooError e = KANGAROO_NO_ERROR;

  // Initialize the Z controllers
  if (kangarooZ1 && USE_KANGAROO_FOR_Z) {
    e = kangarooZ1->start();
  }
  if (kangarooZ2 && USE_KANGAROO_FOR_Z) {
    e = kangarooZ2->start();
  }
  
  delay(2000);

  if (kangarooZ1 && USE_KANGAROO_FOR_Z) {
    e = kangarooZ1->home().wait(KANGAROO_COMMAND_TIMEOUT_MS).status().error();
    if (e != KANGAROO_NO_ERROR) {
      sendError("Kangaroo Z1 failed: ");
      Serial.println(e);
    } else {
      Serial.println("Kangaroo Z1 started successfully.");
    }
  }
  
  if (kangarooZ2 && USE_KANGAROO_FOR_Z) {
    e = kangarooZ2->home().wait(KANGAROO_COMMAND_TIMEOUT_MS).status().error();
    if (e != KANGAROO_NO_ERROR) {
      sendError("Kangaroo Z2 failed: ");
      Serial.println(e);
    } else {
      Serial.println("Kangaroo Z2 started successfully.");
    }
  }

  e = kangarooX->start();

  delay(2000);

  e = kangarooX->home().wait(4000).status().error();
  if (e != KANGAROO_NO_ERROR) {
    sendError("Kangaroo X failed: ");
    Serial.println(e);
  }

  Serial.println("Started Kangaroo drivers");
}

// ================================================================
// ====== COMMAND: TUNE
// ================================================================

void cmdTune(char** pArgs, uint8_t numArgs) {

  Serial.println("Starting auto-tuning");

  if (!kangarooAvailable('x')) {
    sendError("Error: Can't tune, X Kangaroo not initialized.");
    return;
  }

  KangarooError e = KANGAROO_NO_ERROR;

  // Enter the desired tune mode.
  long enterModeParams[1] = { TUNE_MODE_MECHANICAL_STOP };
  e = kangarooX->systemCommand(KANGAROO_SYS_TUNE_ENTER_MODE, false, enterModeParams, 1);

  if (e != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_ENTER_MODE failed: ");
    Serial.println(e);
    return;
  }

  // Set the disabled channel bitmask (only use channel 1, not channel 2)
  long disableChannelsParams[1] = { 2 };
  e = kangarooX->systemCommand(KANGAROO_SYS_TUNE_SET_DISABLED_CHANNELS, false, disableChannelsParams, 1);

  if (e != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_SET_DISABLED_CHANNELS failed: ");
    Serial.println(e);
    return;
  }

  // Begin the tune.
  long goParams[0];
  e = kangarooX->systemCommand(KANGAROO_SYS_TUNE_GO, false, goParams, 0);

  if (e != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_GO failed: ");
    Serial.println(e);
    return;
  }

  // Save the function we're using
  g_CurrentUpdateFunctionStr = "cmdTune";
  g_CurrentUpdateFunction = &updateTune;
}

void updateTune(long elapsedTime) {

  if ( CurrentTime - g_CurrentCommandStartTime > 30 * 1000000 ) {

    Serial.println("Tuning finished");
    g_CurrentUpdateFunctionStr = "";
    g_CurrentUpdateFunction = NULL;
  }
}


// ================================================================
// ====== COMMAND: TUNE
// ================================================================

void cmdTuneZ(char** pArgs, uint8_t numArgs) {

  Serial.println("Starting auto-tuning");

  /*
  if (!kangarooAvailable('z')) {
    sendError("Error: Can't tune, Z Kangaroo's not initialized.");
    return;
  }
  */

  KangarooError e1 = KANGAROO_NO_ERROR, e2 = KANGAROO_NO_ERROR;

  // Enter the desired tune mode.
  long enterModeParams[1] = { TUNE_MODE_MECHANICAL_STOP };
  e1 = kangarooZ1->systemCommand(KANGAROO_SYS_TUNE_ENTER_MODE, false, enterModeParams, 1);
  //e2 = kangarooZ2->systemCommand(KANGAROO_SYS_TUNE_ENTER_MODE, false, enterModeParams, 1);
  if (e1 != KANGAROO_NO_ERROR || e2 != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_ENTER_MODE failed: ");
    Serial.println(e1);
    Serial.println(e2);
    return;
  }
  
  // Set the disabled channel bitmask to 0 (tune all channels).
  long disableChannelsParams[1] = { 2 };
  e1 = kangarooZ1->systemCommand(KANGAROO_SYS_TUNE_SET_DISABLED_CHANNELS, false, disableChannelsParams, 1);
  //e2 = kangarooZ2->systemCommand(KANGAROO_SYS_TUNE_SET_DISABLED_CHANNELS, false, disableChannelsParams, 1);
  if (e1 != KANGAROO_NO_ERROR || e2 != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_SET_DISABLED_CHANNELS failed: ");
    Serial.println(e1);
    Serial.println(e2);
    return;
  }
 
  delay(500);

  // set power (go up)
  /*
  long powerCommands[1] = { 2^28 - 1 };
  e1 = kangarooZ1->systemCommand(KANGAROO_SYS_TUNE_CONTROL_OPEN_LOOP, false, powerCommands, 1);
  if (e1 != KANGAROO_NO_ERROR || e2 != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_CONTROL_OPEN_LOOP failed: ");
    Serial.println(e1);
    Serial.println(e2);
    return;
  }
  */
  
  // Begin the tune.
  long goParams[0];
  e1 = kangarooZ1->systemCommand(KANGAROO_SYS_TUNE_GO, false, goParams, 0);
  //e2 = kangarooZ2->systemCommand(KANGAROO_SYS_TUNE_GO, false, goParams, 0);
  
  if (e1 != KANGAROO_NO_ERROR || e2 != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_GO failed: ");
    Serial.println(e1);
    Serial.println(e2);
    return;
  }
  
  // Save the function we're using
  g_CurrentUpdateFunctionStr = "cmdTuneZ";
  g_CurrentUpdateFunction = &updateTuneZ;
}

void updateTuneZ(long elapsedTime) {

  if ( CurrentTime - g_CurrentCommandStartTime > 60 * 1000000 ) {

    Serial.println("Tuning Z finished");
    g_CurrentUpdateFunctionStr = "";
    g_CurrentUpdateFunction = NULL;
  }
}

// ================================================================
// ====== COMMAND: Functions to generate complex trajectories
// ================================================================

void cmdTrajectory(char** pArgs, uint8_t numArgs) {

  char buf[MOVEMENT_SEGM_MAX_ARGS][512];
  long numSegmArgs;

  // Parse into movement commands
  for (int i = 0; i < numArgs; i++) {

    numSegmArgs = parse( pArgs[i], (char**) buf, MOVEMENT_SEGM_MAX_ARGS, ',');

    // clear previous values
    memset(g_MovementSegments, 0, sizeof(long) * MOVEMENT_SEGM_MAX_ARGS);

    for (int j = 0; j < numSegmArgs; j++) {

      g_MovementSegments[i][j] = String(buf[j]).toInt();
    }
  }
  g_numMovementSegments = numArgs;

  // Save the function we're using
  g_CurrentUpdateFunctionStr = "cmdTrajectory";
  g_CurrentUpdateFunction = &updateTrajectory;

  // DEBUG
  Serial.print("Read ");
  Serial.print(g_numMovementSegments);
  Serial.println(" parameters.");
}

void updateTrajectory(long elapsedTime) {

  // -----------------------------------
  // Process movement
  // -----------------------------------

  // TODO: Detect stall... If stall detected, go to next segment

  // If Z pos is not correct, move
  if ( abs(g_CurPosZ1 - targetPositionZ) > MAX_X_ERROR ||
       abs(g_CurPosZ2 - targetPositionZ) > MAX_Z_ERROR ) {

    kangarooZ1->p(targetPositionZ / CONVERSION_Z, int(targetVelocityZ / CONVERSION_Z) );
    kangarooZ2->p(targetPositionZ / CONVERSION_Z, int(targetVelocityZ / CONVERSION_Z) );
  }

  // Did we reach our goal for the current segment?
  if ( abs(g_CurPosX - targetPositionX) > MAX_X_ERROR ) {

    // If not, let Kangaroo controllers update system state
    kangarooX->s( int( targetVelocityX / CONVERSION_X ) );

  } else {
    // If so, go to the next segment, or end program
    if (g_curMovementSegment < g_numMovementSegments) {

      if (g_MovementSegments[g_curMovementSegment][0] == MVM_TYPE_WAIT) {

        // TODO: Execute command to quickly slow motor speed. For now use simple delay
        delay( g_MovementSegments[g_curMovementSegment][1] );

      } else if (g_MovementSegments[g_curMovementSegment][0] == MVM_TYPE_LINEAR) {

        targetPositionZ    = g_MovementSegments[g_curMovementSegment][1];
        targetVelocityZ    = g_MovementSegments[g_curMovementSegment][2];
        targetPositionX    = g_MovementSegments[g_curMovementSegment][3];
        targetVelocityX    = g_MovementSegments[g_curMovementSegment][4];
      }
    } else {

      // Finished running segments, so stop updating
      g_CurrentUpdateFunction = NULL;
      g_CurrentUpdateFunctionStr = "";

      // Trigger cortex (to stop)
      // triggerCortex(true);
    }
  }
}

// ================================================================
// ====== COMMAND: Simple positioning
// ================================================================

void cmdZ(char** pArgs, uint8_t numArgs) {

  long speed = 100;
  long z     = 0;

  if (numArgs == 0 || numArgs > 2) {
    Serial.print  ("Invalid parameters (");
    Serial.print  (numArgs);
    Serial.println(" given): 'z <z pos> [<speed>]");
    return;
  }

  if (numArgs >= 1) {
    z     = String(pArgs[0]).toInt();
  }
  if (numArgs == 2) {
    speed = String(pArgs[1]).toInt();
  }

  Serial.print("Moving to Z=");
  Serial.print(z);
  Serial.print(" at speed ");
  Serial.println(speed);

  targetPositionZ = z;
  targetVelocityZ = speed;
}

void _cmdX(char** pArgs, uint8_t numArgs, bool relativeToCenter) {

  long speed = 100;
  long x     = 0;

  if (numArgs == 0 || numArgs > 2) {
    Serial.print  ("Invalid parameters (");
    Serial.print  (numArgs);
    Serial.println(" given): 'x <x pos> [<speed>]");
    return;
  }

  int inputX = String(pArgs[0]).toInt();
  if (numArgs >= 1) {
    x     = (relativeToCenter?(g_MinPosX+g_MaxPosX)/2:0) + inputX;
  }
  if (numArgs == 2) {
    speed = String(pArgs[1]).toInt();
  }

  Serial.print("Moving to X=");
  Serial.print(inputX);
  Serial.print(relativeToCenter?" (relative to center)":"");
  Serial.print(" at speed ");
  Serial.println(speed);

  targetPositionX = x;
  targetVelocityX = speed;

  g_CurrentUpdateFunction = &updateX;
  g_CurrentUpdateFunctionStr = "cmdX";
}

void cmdX(char** pArgs, uint8_t numArgs) {

  _cmdX(pArgs, numArgs, false);
}

void cmdXRelToCenter(char** pArgs, uint8_t numArgs) {

  _cmdX(pArgs, numArgs, true);
}

void updateX(long elapsedTime) {

  if (abs(g_CurPosX - targetPositionX) > MAX_X_ERROR) {
    kangarooX->p(targetPositionX / CONVERSION_X, targetVelocityX / CONVERSION_X);
  } else {
    kangarooX->p(targetPositionX / CONVERSION_X, 10 / CONVERSION_X);
    g_CurrentUpdateFunction = NULL;
    g_CurrentUpdateFunctionStr = "";
  }
}

void cmdSpeedX(char** pArgs, uint8_t numArgs) {

  Serial.println("Bouncing...");
  if (numArgs == 1) {
    long speed = String(pArgs[0]).toInt();
    for (int i = 0; i < 10; i++) {
      kangarooX->s(speed).wait();
      delay(3000);
      kangarooX->s(-speed).wait();
      delay(3000);
    }
  }
}

// ================================================================
// ====== COMMAND: Notification of whether dF is on perch and aligned correctly
// ================================================================

void cmdDfOnPerch(char** pArgs, uint8_t numArgs) {

  if (numArgs == 1) {

    g_DfReadyOnPerch = (String(pArgs[0]).toInt() > 0);

    Serial.print("Notified that dF perch status is: ");
    Serial.println(String(pArgs[0]).toInt());
  }
}

// ================================================================
// ====== COMMAND: Generate back and forth sequence with delays in
// =====           range [A,B] and absolute speeds in range [C,D],
// =====           and heights in the range [E,F]
// ================================================================

String g_TrialsMode = "";

void cmdTrials(char** pArgs, uint8_t numArgs) {
  if (true || kangarooAvailable()) {

    if (numArgs >= 1) {
      g_TrialsMode = String(pArgs[0]);
    } else {
      g_TrialsMode = String("");
    }
    
    g_CurrentUpdateFunction = updateTrials;
    g_CurrentUpdateFunctionStr = "cmdTrials";
  } else {
    sendError("Error: Can't start trials, Kangaroo not initialized.");
  }
}

long g_TimeLastTrial = 0;

void updateTrials(long elapsedTime) {

  long PRE_TRIAL_PERIOD = 5 * 1000000;

  // Only allow trial to trigger if:
  //    o No DF-ready requirement is set
  //    o The DF is in fact ready (i.e. on perch and aligned)
  //    o The trial would not fire regardless (i.e. previous trial happened too shortly before)
  //    o The final seconds of countdown to trial have already commenced, and Cortex has potentially already been notified.
  //      The dF might have flown away just prior to trial, but go through with the trial anyway.

  // If no trials for a while, and dF just became ready on perch, immediately start trial
  if ( (micros()-g_TimeLastTrial) > 25e6) {
    g_TimeUntilTrial = min( PRE_TRIAL_PERIOD, g_TimeUntilTrial);
  }
  
  if (TRIALS_REQUIRE_PERCH && !g_DfReadyOnPerch && g_TimeUntilTrial >= PRE_TRIAL_PERIOD) {
    g_TimeUntilTrial = max(g_TimeUntilTrial - elapsedTime, PRE_TRIAL_PERIOD);
  } else {
    g_TimeUntilTrial -= elapsedTime;
  }
  
  // Are we close to trial start? If so, start cortex
  if (g_TimeUntilTrial < PRE_TRIAL_PERIOD) {
    triggerCortex(true);
  }

  // Wait to start trial (so as not to habituate the dF)
  if (g_TimeUntilTrial < 0) {

    for (int i = 0; i < NUM_VELOCITY_SEGMENTS; i++) {
      if (g_CurPosX > (i == 0 ? -1000000 : g_VelocitySegmentBounds[i - 1]) && g_CurPosX < (i == NUM_VELOCITY_SEGMENTS - 1 ? 1000000 : g_VelocitySegmentBounds[i])) {
        // update target velocity
        targetVelocityX = g_VelocitySegments[i];
        // Done with loop, found speed
        break;
      }
    }
    
    // Did we reach our goal for the current segment?
    if ( abs(g_CurPosX - targetPositionX) > MAX_X_ERROR && g_TimeUntilTrial > -8e6 && 
      (abs(g_CurPosX-g_MinPosX) > 20 || targetVelocityX > 0) && 
      (abs(g_CurPosX-g_MaxPosX) > 20 || targetVelocityX < 0) ) {
      
      // Determine target velocity (will vary based on current speedups, noise, etc.)
      // Note: This if statement assumes maximum velocity boost duration is shorter than minimum time to traverse segment
      /*
      if (g_VelocityBoostTimeLeft <= 0 || !g_VelocityBoostEnabled) {
        for (int i = 0; i < NUM_VELOCITY_SEGMENTS; i++) {
          if (g_CurPosX > (i == 0 ? -1000000 : g_VelocitySegmentBounds[i - 1]) && g_CurPosX < (i == NUM_VELOCITY_SEGMENTS - 1 ? 1000000 : g_VelocitySegmentBounds[i])) {
            // After a velocity change, we boost the target velocity
            if (g_VelocityBoostEnabled) {
              g_VelocityBoostDir = g_VelocitySegments[i] - targetVelocityX;
              if (g_VelocityBoostDir != 0) {
                g_VelocityBoostTimeLeft = 50000;
                g_VelocityBoostDir /= abs(g_VelocityBoostDir);
              }
            }
            // update target velocity
            targetVelocityX = g_VelocitySegments[i];
            // Done with loop, found speed
            break;
          }
        }
      }
      */

      // Add noise
      /*
      targetVelocityX += currentVelocityNoiseX;
      targetVelocityNoiseTimeLeft -= elapsedTime;
      if (targetVelocityNoiseTimeLeft < 0) {
        // Choose new noise
        currentVelocityNoiseX = (random(0, 2) - 1) * random(0, 1000) * 0.001 * targetVelocityNoiseX;
        targetVelocityNoiseTimeLeft = random(1, 5) * 1000;
      }

      // Add velocity boost
      if (g_VelocityBoostTimeLeft > 0) {
        targetVelocityX += g_VelocityBoostDir * 1000;
      }
      */

      // Enforce maximum velocity to prevent Kangaroo malfunction
      targetVelocityX = max(min(targetVelocityX, 2000), -2000);
    
      // If not, let Kangaroo controllers update system state
      kangarooX->s( (int) (targetVelocityX + currentVelocityNoiseX) / CONVERSION_X ).wait();

    } else {

      // We reached the end of the trial...
      targetVelocityX = 0;
      // let Kangaroo controllers update system state
      kangarooX->s( 0 ).wait();
      // Save time
      g_TimeLastTrial = micros();
        
      // Wait a little bit longer, then trigger Cortex to stop saving
      Serial.println("Starting new trial in 2 seconds.");
      delay(2000);
      triggerCortex(false);

      // What direction to move in?
      long xCnt = (g_MinPosX + g_MaxPosX) / 2;

      // Go to the opposite end
      long dst = g_MinPosX + (g_MinPosX<g_MaxPosX?1:-1) * 120;
      if (abs(g_MaxPosX - g_CurPosX) > abs(g_MinPosX - g_CurPosX)) {
        dst = g_MaxPosX - (g_MinPosX<g_MaxPosX?1:-1) * 120;
      }

      long height = random(12, 18) * 25;
      long dir   = (dst - g_CurPosX) / abs(dst - g_CurPosX);
      long speed1base = (targetPositionZ * 3.3) + (random(0, 400)-200);
      long speedup = random(0,100)<=20?0:((random(0,100)<=50?1:-1)*random(300,500));

      // Use pre-programmed set of speedup/slowdown trials?
      if (g_TrialsMode.indexOf("fixed_speedup_set")!=-1) {
        int choice = random(0,3 + 0);
        if (choice == 0) {
          height = 400;
          speed1base = 1000;
          speedup = 400;
        } else if (choice == 1) {
          height = 400;
          speed1base = 1000;
          speedup = -400;
        } else if (choice == 2) {
          height = 400;
          speed1base = 1000;
          speedup = 0;
        } else {
          // Do nothing... hence the range of random(...) determine the percentage of times we use this fixed set
        }
        Serial.println("Using fixed set of speedups");
      }
      
      long speed1 = dir >  0 ? speed1base : speed1base+speedup;
      long speed2 = dir <= 0 ? speed1base : speed1base+speedup;
      
      long tWait;
      if (g_TrialIdx < 3) {
        tWait = random(30, 60) * 1000000;
      } else {
        tWait = random(60, 90) * 1000000;
      }

      // Debug mode? If so, present trials quickly for easier debugging...
      if (g_TrialsMode.indexOf("debug")!=-1) {
        tWait = 5000000;
        Serial.println("Debug mode, skipping");
      }
      
      g_TrialIdx++;
      if (g_TrialIdx > 6) { // Every 5 trials, enforce a longer wait
        g_TrialIdx = 0;
      }
      
      // DEBUG:
      //speed1 = 1000;
      //speed2 = 1600;

      // Set appropriate X speed
      g_VelocitySegments[0] = dir * speed1;
      g_VelocitySegments[1] = dir * (random(0, 100) > 0 ? speed2 : speed1);
      g_VelocitySegmentBounds[0] = - dir * 0 + (g_MinPosX + g_MaxPosX) / 2;
      targetPositionX = dst;
      
      // Set appropriate Z position
      targetPositionZ = height;
      targetVelocityZ = 200;

      // Update start time of trial
      g_TimeUntilTrial = tWait;

      // Set noise
      targetVelocityNoiseX = 0;
      if ( false && random(0, 100) > 25 ) {
        targetVelocityNoiseX = random(0, 500);
      }

      // DEBUG:
      //g_TimeUntilTrial = 10000000;
      //height=300;
      //targetVelocityNoiseX = 0;
      //g_VelocitySegments[0] = dir * 900;
      //g_VelocitySegments[0] = dir * 1600;

      //
      Serial.println("Started new trial: ");
      cmdStatus(NULL, NULL);
    }
  }
}

// ================================================================
// ====== COMMAND: Present a single trial
// ================================================================

void cmdTrial(char** pArgs, uint8_t numArgs) {

  if (numArgs != 2) {
    Serial.println("Invalid arguments. trial <height> <speed>");
    return;
  }

  startTrial(String(pArgs[0]).toInt(),200,String(pArgs[1]).toInt());
  
  g_CurrentUpdateFunction = updateTrial;
  g_CurrentUpdateFunctionStr = "cmdTrial";
}

void startTrial(int targetPosZ, int targetVelZ, int targetVelX) {
  
  targetPositionZ = targetPosZ;
  targetVelocityZ = targetVelZ;
  
  // What direction to move in?
  long xCnt = (g_MinPosX + g_MaxPosX) / 2;

  // Go to the opposite end
  targetPositionX = g_MinPosX - 580;
  if (abs(g_MaxPosX - g_CurPosX) > abs(g_MinPosX - g_CurPosX)) {
    targetPositionX = g_MaxPosX + 580;
  }
  long dir  = (targetPositionX - g_CurPosX) / abs(targetPositionX - g_CurPosX);

  g_VelocitySegments[0]  = targetVelX * dir;
}

void updateTrial(long elapsedTime) {

  // Wait until we reach the right Z height
  if ( abs(g_CurPosZ1 - targetPositionZ) < MAX_Z_ERROR ) {
    // Did we reach our goal for the current segment? /*(Build in fail-safe, in case tuning is off)*/
    if ( abs(g_CurPosX - targetPositionX) > MAX_X_ERROR /* && g_TimeUntilTrial > -8 * 1000000 */ ) {
      targetVelocityX = g_VelocitySegments[0];
    } else {
      targetVelocityX = 0;
      g_CurrentUpdateFunction = NULL;
      g_CurrentUpdateFunctionStr = "";
    }
    kangarooX->s( (int) targetVelocityX / CONVERSION_X ).wait();
  }
}

// ================================================================
// ====== COMMAND: Present trials of increasing speed at a given height
// ================================================================

void cmdTrialsIncreasingSpeed(char** pArgs, uint8_t numArgs) {
  
}

// ================================================================
// ====== COMMAND: Explore space fast
// ================================================================

void cmdExploreSpaceFast(char** pArgs, uint8_t numArgs) {

  g_TimeUntilTrial = -1;

  targetPositionZ = -100;
    
  g_CurrentUpdateFunction = updateExploreSpaceFast;
  g_CurrentUpdateFunctionStr = "cmdExploreSpaceFast";
}

void updateExploreSpaceFast(long elapsedTime) {

  g_TimeUntilTrial -= elapsedTime;

  if (g_TimeUntilTrial < 0) {

    g_TimeUntilTrial = 2000000;

    int newTargetPosZ = targetPositionZ + 100;

    //Serial.print("TargetPos=");
    //Serial.println(newTargetPosZ);
      
    if (newTargetPosZ <= 1000) {
      Serial.print("Starting new trial: z=");
      Serial.println(newTargetPosZ);
      startTrial(newTargetPosZ, 300, 1000);
    }
  } else {
    updateTrial(elapsedTime);
    if (g_CurrentUpdateFunction == NULL) {
      g_TimeUntilTrial = -1;
      g_CurrentUpdateFunction = updateExploreSpaceFast;
      g_CurrentUpdateFunctionStr = "cmdExploreSpaceFast";
    }
  }
}

// ================================================================
// ====== COMMAND: Explore a grid
// ================================================================

void cmdExploreSpace(char** pArgs, uint8_t numArgs) {

  targetPositionX = (g_MinPosX+g_MaxPosX)/2 -550;
  targetPositionZ = 0;

  targetVelocityX = 200;
  targetVelocityZ = 100;

  g_TimeUntilNextPosition = 8000000;
  
  g_CurrentUpdateFunction = updateExploreSpace;
  g_CurrentUpdateFunctionStr = "cmdExploreSpace";
}

void updateExploreSpace(long elapsedTime) {

  long g_CurPosZ = (g_CurPosZ1+g_CurPosZ2)/2;

  const int Z_INCREMENT = 50;
  const int X_INCREMENT = 50;
  
  /* If one of Z motors failed, restart motors */
  /*
  if ( abs(g_CurPosZ1-g_CurPosZ2) > 50 ) {
    cmdStatus(NULL, NULL);
    cmdStartKangaroo(NULL, NULL);
  }
  */
  
  /* Update until we arrive at position */
  if (abs(g_CurPosX - targetPositionX) > MAX_X_ERROR || 
      abs(g_CurPosZ - targetPositionZ) > 5) {
    
    kangarooX->p(targetPositionX / CONVERSION_X, targetVelocityX / CONVERSION_X);

  } else {
    /* Print position */
    Serial.println("pos,"+String(g_CurPosX-(g_MinPosX+g_MaxPosX)/2)+","+String(g_CurPosZ1)+","+String(g_CurPosZ2));
  
    /* Switch to next position if time has elapsed */
    g_TimeUntilNextPosition -= elapsedTime;
    if (g_TimeUntilNextPosition < 0) {

      g_TimeUntilNextPosition = 6000000;
      
      targetPositionX += X_INCREMENT;
      
      /* Reached the end of the line? */
      if (targetPositionX > (g_MinPosX+g_MaxPosX)/2 + 650) {
        targetPositionX = (g_MinPosX+g_MaxPosX)/2 - 650;
        targetPositionZ += Z_INCREMENT;
      }
      /* Reached the end of the whole plane? */
      if (targetPositionZ > 600) {
        targetPositionZ = 0;
        targetPositionX = 0;
      
        g_CurrentUpdateFunction = NULL;
        g_CurrentUpdateFunctionStr = "";
  
        Serial.println("finished exploring space");
      }
    }
  }
}

// ================================================================
// ====== COMMAND: Abort (currently targetting Kangaroo)
// ================================================================

void cmdAbort(char** pArgs, uint8_t numArgs) {

  Serial.println("Aborting...");

  // 1.5 sec of no packets to Kangaroo causes a timeout and aborts the program
  delay(1500);

  g_CurrentUpdateFunction = NULL;
  g_CurrentUpdateFunctionStr = "";
}

// ================================================================
// ====== COMMAND: Power down system
// ================================================================

void cmdPowerdown(char** pArgs, uint8_t numArgs) {

  targetPositionZ = 0;
  targetVelocityZ = 20;

  g_CurrentUpdateFunction = updatePowerdown;
  g_CurrentUpdateFunctionStr = "cmdPowerdown";
}

void updatePowerdown(long elapsedTime) {

  if (g_CurPosZ1 < 50 && g_CurPosZ2 < 50) {

    kangarooX->powerDown();
    
    if (kangarooZ1) {
      kangarooZ1->powerDown();
    }
    if (kangarooZ2) {
      kangarooZ2->powerDown();
    }
  }
}

void cmdNeutral(char** pArgs, uint8_t numArgs) {

  kangarooX->powerDown();
}

// ================================================================
// Route serial commands to the right function
// ================================================================

void processSerialCommand(char** pArgs, uint8_t numArgs) {

  String cmd = String( pArgs[0] );
  void (*fun)(char**, uint8_t);

  // Determine function to execute
  Serial.println(cmd);
  if (cmd == "status"            ) { fun = &cmdStatus;
  } else if (cmd == "h"          ) { fun = &cmdIdentify;
  } else if (cmd == "tune"       ) { fun = &cmdTune;
  } else if (cmd == "tunez"      ) { fun = &cmdTuneZ;
  } else if (cmd == "z"          ) { fun = &cmdZ;
  } else if (cmd == "x"          ) { fun = &cmdX;
  } else if (cmd == "xcent"      ) { fun = &cmdXRelToCenter;
  } else if (cmd == "trial"      ) { fun = &cmdTrial;
  } else if (cmd == "trials"     ) { fun = &cmdTrials;
  } else if (cmd == "explore"    ) { fun = &cmdExploreSpace;
  } else if (cmd == "explorefast") { fun = &cmdExploreSpaceFast;
  } else if (cmd == "kangaroo"   ) { fun = &cmdStartKangaroo;
  } else if (cmd == "abort"      ) { fun = &cmdAbort;
  } else if (cmd == "powerdown"  ) { fun = &cmdPowerdown;
  } else if (cmd == "neutral"    ) { fun = &cmdNeutral;
  } else if (cmd == "speedx"     ) { fun = &cmdSpeedX;
  } else if (cmd == "trajectory" ) { fun = &cmdTrajectory;
  } else if (cmd == "cortex"     ) { fun = &cmdTriggerCortex;
  } else if (cmd == "df_ready_on_perch" ) { fun = &cmdDfOnPerch;
  } else {

    // Command not recognized....
    sendError("Command not recognized.");
    Serial.println(cmd);
    return;
  }

  if (g_CurrentUpdateFunction != NULL && fun != &cmdAbort && 
    fun != &cmdStatus && fun != &cmdDfOnPerch) {
    sendError("Aborting command, other function in progress");
    return;
  }

  // Register time of function start
  g_CurrentCommandStartTime = CurrentTime;

  // Execute selected function
  fun( &(pArgs[1]), numArgs - 1 );
}

// ================================================================
// Loop
// ================================================================

int lastTargetPositionZ = 0;

void loop() {

  // -----------------------------------
  // Check that Kangaroos are operating correctly (i.e. actually on)
  // -----------------------------------

  // NOTE: By checking this, the program remains stable when Kangaroos fail / are not turned on yet
  // TODO: Implement automatic re-connect after Kangaroo power turn-off/turn-on sequence... Or is this built-in?

  bool kx = kangarooAvailable('x');
  bool kz = kangarooAvailable('z');

  if ( !kx && !kz ) {
    g_Status = "Unable to connect to either Kangaroo.";
  } else if ( !kx &&  kz ) {
    g_Status = "Unable to connect to X Kangaroo.";
  } else if (  kx && !kz ) {
    g_Status = "Unable to connect to Z Kangaroo.";
  } else {
    g_Status = "OK";
  }

  // -----------------------------------
  // Read the sensor data
  // -----------------------------------

  // IMPORTANT NOTE: These commands matter beyond simply updating sensor values.
  //                 The requests signal to the Kangaroo drivers that we wish the commands
  //                 (e.g. tuning) to continue!

  if (kx) {
    g_CurPosX  = kangarooX->getP().value()  * CONVERSION_X;
    g_CurVelX  = kangarooX->getS().value()  * CONVERSION_X;

    // Get information on bounds
    g_MinPosX = kangarooX->getMin().value() / 2;
    g_MaxPosX = kangarooX->getMax().value() / 2;
  }
  
  if (USE_KANGAROO_FOR_Z) {
    if (kz) {
      if (kangarooZ1) {
        g_CurPosZ1 = kangarooZ1->getP().value() * CONVERSION_Z;
        g_CurVelZ1 = kangarooZ1->getS().value() * CONVERSION_Z;
      }
      if (kangarooZ2) {
        g_CurVelZ2 = kangarooZ2->getS().value() * CONVERSION_Z;
        kangarooZ2->getS().value() * CONVERSION_Z;
        g_CurPosZ2 = kangarooZ2->getP().value() * CONVERSION_Z;
      }
    }
  }

  // -----------------------------------
  // Maintain Z
  // -----------------------------------

  if (USE_KANGAROO_FOR_Z) {
    if (kz) {
      if (kangarooZ1) {
        kangarooZ1->p(targetPositionZ / CONVERSION_Z, targetVelocityZ / CONVERSION_Z);
      }
      if (kangarooZ2) {
        kangarooZ2->p(targetPositionZ / CONVERSION_Z, targetVelocityZ / CONVERSION_Z);
      }
    }
  } else {
    if (targetPositionZ != lastTargetPositionZ) {
      if (!Serial2) {
        Serial2.begin(115200);
        while(!Serial2) { delay(1); }
      }
      
      lastTargetPositionZ = targetPositionZ;
      Serial2.print("position ");
      Serial2.println( int( max(0, targetPositionZ-50) * 1.2 ) );
    }
  }

  // -----------------------------------
  // Update timing variables
  // -----------------------------------

  DF_check();  // Testing the DF status
  CurrentTime = micros();
  long elapsedTime = CurrentTime - LastTime;

  // -----------------------------------
  // Stop command
  // -----------------------------------

  long STOP = digitalRead(PIN_STOP);

  if ( STOP == 1 ) {

    kangarooZ1->powerDown();
    if (kangarooZ2 != NULL) {
      kangarooZ2->powerDown();
    }
  }

  // -----------------------------------
  // Read PC inputs
  // -----------------------------------

  while ( Serial.available() > 0 )
  {
    char c = Serial.read();

    if ( c != '\n' ) {
      if (g_PCSerialBufferIdx > PC_SERIAL_BUF_LENGTH - 1) {
        // Error: Out of range
        sendError("Error: serial input buffer overflow. Resetting buffer.");
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
  // Update the currently running function
  // -----------------------------------

  // This depends on what command was executed
  if (g_CurrentUpdateFunction != NULL) {
    g_CurrentUpdateFunction(elapsedTime);
  }

  // Update time
  LastTime = CurrentTime;
}
