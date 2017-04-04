
#define PIN_AMPLIFIED 4
#define PIN_TEENSY_SIGNAL 21
#define PIN_SYNC_SIGNAL 1
#define SIGNAL_DURATION_MS 1000
#define MIN_COOLDOWN_MS 500 // All triggers need to be 2000 ms apart...

// variable indicating whether the system should send a TTL trigger pulse 
// upon the next available risigin sync edge
// NOTE: Currently, triggers received in the middle of the previous trigger 
//       will be ignored!! Alternatively, we could wait and send multiple triggers...
//       What is desirable?
bool g_DoTrigger = true;

// variable indicating if frame had already commenced at moment of trigger...
// If so, wait for the next frame, to prevent misalignment
bool g_SyncFrameCommenced = false; 

unsigned long g_LastTrigger = 0;

void setup() {

  // Init serial communication
  Serial.begin(9600);

  // Init pins
  pinMode(PIN_SYNC_SIGNAL, INPUT);
  pinMode(PIN_AMPLIFIED, OUTPUT);
  pinMode(PIN_TEENSY_SIGNAL, OUTPUT);
  digitalWrite(PIN_AMPLIFIED, LOW);
  digitalWrite(PIN_TEENSY_SIGNAL, LOW);
  
  // Confirm startup
  Serial.println("!");
}

void loop() {
  while (Serial.available()) {
    switch ((char)Serial.read()) {
      case 'h': 
        Serial.write("Teensy TTL Controller\n"); 
        break;
        
      case 't':
        if ( (millis() - g_LastTrigger) >= MIN_COOLDOWN_MS) {

          g_DoTrigger = true;
  
          // If this trigger happened when we were already halfway through a frame, wait 
          // for SYNC to go low first, and a new frame to start...
          if (digitalRead(PIN_SYNC_SIGNAL) == HIGH) {
            g_SyncFrameCommenced = true;
          } else {
            g_SyncFrameCommenced = false;
          }
        }
        break;

    case '+':
      digitalWrite(PIN_TEENSY_SIGNAL, HIGH);
      delay(10000);
      digitalWrite(PIN_TEENSY_SIGNAL, LOW);
      break;
    }
  }

  if (g_DoTrigger && digitalRead(PIN_SYNC_SIGNAL) == HIGH && !g_SyncFrameCommenced) {
    // Write TTL
    digitalWrite(PIN_AMPLIFIED, HIGH);
    digitalWrite(PIN_TEENSY_SIGNAL, HIGH);
    delay(SIGNAL_DURATION_MS);
    digitalWrite(PIN_AMPLIFIED, LOW);
    digitalWrite(PIN_TEENSY_SIGNAL, LOW);
    Serial.println("!");
    // Trigger done
    g_DoTrigger = false; 
    g_LastTrigger = millis();
  } 

  // When the trigger happened halfway through a frame, we wait for the sync to go low, and then high again...
  // So when the SYNC goes low, reset the "g_SyncFrameCommenced" variable.
  if (g_SyncFrameCommenced && digitalRead(PIN_SYNC_SIGNAL) == LOW) {
    g_SyncFrameCommenced = false;
  }
} 
