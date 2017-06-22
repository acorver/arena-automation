
#define PIN_TRIGGER_IN 17    // Teensy 2.0 pin F6 = Arduino Pin 17 (digital)
#define PIN_SIGNAL_OUT 20 // Teensy 2.0 pin B6 = Arduino Pin 15 (digital)
#define PIN_SIGNAL_OUT2 16 // this is the block signal
const int PIN_LEDS[] = {1, 3, 5, 7};

#define NUM_LEDS 4

#define BLINK_MICROSECONDS 5000 // Assumes 200 FPS camera rate
#define BLINK_NUM_FRAMES 5

// Switch to allow LED's to be continually on, to test Mocap thresholds
bool g_DebugState; 

void setup() {

  // Init serial communication
  Serial.begin(9600);
  
  // Init variables
  g_DebugState = false;

  // Init pins
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(PIN_LEDS[i], OUTPUT);
  }
  pinMode(PIN_TRIGGER_IN, INPUT);
  pinMode(PIN_SIGNAL_OUT, OUTPUT);
  
  // Attach interrupt for signal
  attachInterrupt(PIN_TRIGGER_IN, onTrigger, RISING);
  
  // Init signals
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(PIN_LEDS[i], LOW);
  }
  digitalWrite(PIN_SIGNAL_OUT, LOW);
}

void onTrigger() {
  
  // Turn on LED's for just 5 frames
  if (digitalRead(PIN_TRIGGER_IN) == HIGH) {
    
    // Turn all LED's on
    for (int i = 0; i < NUM_LEDS; i++) {
      digitalWrite(PIN_LEDS[i], HIGH );
    }

    // Wait 5 frames
    delayMicroseconds(BLINK_MICROSECONDS * BLINK_NUM_FRAMES); 
  } 

  // Turn all LED's off
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(PIN_LEDS[i], LOW );
  }

  // Automatically turn debug mode off
  g_DebugState = false;

  // Status output
  Serial.println("Triggered!");
}

void loop() {

  // Read serial
  if (Serial.available()) {
    
    char c = Serial.read();
    
    if (c == '+' || c == '-') {
        
      g_DebugState = (c=='+');
  
      Serial.println(g_DebugState?"Debug state":"Non-debug state");
    }
  }

  if (g_DebugState) {
    // flash the LED's !
    if ( (micros()%1000000) < 10000 ) {
      for (int i = 0; i < NUM_LEDS; i++) {
        digitalWrite(PIN_LEDS[i], HIGH);
      } 
    } else {
      for (int i = 0; i < NUM_LEDS; i++) {
        digitalWrite(PIN_LEDS[i], LOW);
      }
    }
  }
} 

