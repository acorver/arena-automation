
#define PIN_AMPLIFIED 4
#define PIN_TEENSY_SIGNAL 21
#define PIN_CLOCK_SIGNAL 1
#define SIGNAL_DURATION_MS 1000
#define MIN_COOLDOWN_MS 500 // All triggers need to be 2000 ms apart...

// variable indicating whether the system should send a TTL trigger pulse 
// upon the next available risigin sync edge
// NOTE: Currently, triggers received in the middle of the previous trigger 
//       will be ignored!! Alternatively, we could wait and send multiple triggers...
//       What is desirable?
volatile bool g_DoTrigger = true;

unsigned long g_LastTrigger = 0;

void onClockEdge() {
  
  if (g_DoTrigger) {
    
    // Write TTL pulse
    digitalWrite(PIN_AMPLIFIED, HIGH);
    digitalWrite(PIN_TEENSY_SIGNAL, HIGH);
    
    delayMicroseconds(SIGNAL_DURATION_MS * 1000);
    
    digitalWrite(PIN_AMPLIFIED, LOW);
    digitalWrite(PIN_TEENSY_SIGNAL, LOW);
    
    Serial.println("!");
    
    // Trigger done
    g_DoTrigger = false; 
    g_LastTrigger = millis();
  } 
}

void setup() {

  // Init serial communication
  Serial.begin(9600);

  // Init pins
  pinMode(PIN_CLOCK_SIGNAL, INPUT);
  pinMode(PIN_AMPLIFIED, OUTPUT);
  pinMode(PIN_TEENSY_SIGNAL, OUTPUT);
  digitalWrite(PIN_AMPLIFIED, LOW);
  digitalWrite(PIN_TEENSY_SIGNAL, LOW);

  // Attach interrupt listening for clock edge
  attachInterrupt(digitalPinToInterrupt(PIN_CLOCK_SIGNAL), onClockEdge, RISING); 
  
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
        }
        break;

    case '+':
      digitalWrite(PIN_TEENSY_SIGNAL, HIGH);
      delay(10000);
      digitalWrite(PIN_TEENSY_SIGNAL, LOW);
      break;
    }
  }
} 
