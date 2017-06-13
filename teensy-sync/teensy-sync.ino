
#define PIN_TRIGGER_IN 17    // Teensy 2.0 pin F6 = Arduino Pin 17 (digital)
#define PIN_SIGNAL_OUT 20 // Teensy 2.0 pin B6 = Arduino Pin 15 (digital)
#define PIN_SIGNAL_OUT2 16 // this is the block signal
const int PIN_LEDS[] = {1, 3, 5, 7};

#define NUM_LEDS 4

#define BLINK_MICROSECONDS 5000 // Assumes 200 FPS camera rate
#define BLINK_NUM_FRAMES 5

// Indicator for how many frames the LED's were just on
int g_nNumFramesOn;

void setup() {

  // Init serial communication
  // Serial.begin(9600);

  // Init variables
  g_nNumFramesOn = 0;

  // Init pins
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(PIN_LEDS[i], OUTPUT);
  }
  pinMode(PIN_TRIGGER_IN, INPUT);
  pinMode(PIN_SIGNAL_OUT, OUTPUT);
  
  // Init signals
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(PIN_LEDS[i], LOW);
  }
  digitalWrite(PIN_SIGNAL_OUT, LOW);
}

void loop() {

  // Turn on LED's for just 5 frames
  if (digitalRead(PIN_TRIGGER_IN) == HIGH && g_nNumFramesOn == 0) {
    // Turn all LED's on
    for (int i = 0; i < NUM_LEDS; i++) {
      digitalWrite(PIN_LEDS[i], HIGH );
    }
    delayMicroseconds(BLINK_MICROSECONDS * BLINK_NUM_FRAMES); 
    for (int i = 0; i < NUM_LEDS; i++) {
      digitalWrite(PIN_LEDS[i], LOW );
    }
    g_nNumFramesOn ++;
  }

  if (digitalRead(PIN_TRIGGER_IN) == LOW) {
    g_nNumFramesOn = 0;
  }
} 

