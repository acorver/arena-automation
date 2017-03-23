
#define PIN_SYNC_IN 17    // Teensy 2.0 pin F6 = Arduino Pin 17 (digital)
#define PIN_SIGNAL_OUT 20 // Teensy 2.0 pin B6 = Arduino Pin 15 (digital)
#define PIN_SIGNAL_OUT2 16 // this is the block signal

//#define PIN_SYNC_IN 4    // Teensy 2.0 pin F6 = Arduino Pin 17 (analog)
//#define PIN_SIGNAL_OUT 6 // Teensy 2.0 pin B6 = Arduino Pin 15 (analog)

const int PIN_LEDS[] = {1, 3, 5, 7};

#define NUM_LEDS 4

#define COUNTER_MAX 16 // 4 LEDs, i.e. 2^4 states

#define PULSE_WIDTH 384.61538461 // i.e. 1000 ms / 2600 Hz sampling frequency = ~0.384 ms = 384 usecs

// Teensy 3.* only (Teensy 2.* doesn't support sufficiently high-resolution PWM)
// #define PULSE_OUT_HIGH 255
// #define PULSE_OUT_MIDDLE 100

// Teensy 2.* only (Used to manually simulate PWM cycle)
#define PWM_SAMPLING_FOLD_INCREASE 5

// Current counter
unsigned int nCounter;

void setup() {

  // Init serial communication
  // Serial.begin(9600);

  // Init variables
  nCounter = 0;

  // Init pins
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(PIN_LEDS[i], OUTPUT);
  }
  pinMode(PIN_SYNC_IN, INPUT);
  pinMode(PIN_SIGNAL_OUT, OUTPUT);
  
  // Teensy 3.* only (Teensy 2.* doesn't support sufficiently high-resolution PWM)
  //analogWriteFrequency(PIN_SIGNAL_OUT, 26000);

  // Init signals
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(PIN_LEDS[i], LOW);
  }
  digitalWrite(PIN_SIGNAL_OUT, LOW);
  
  // Teensy 3.* only (Teensy 2.* doesn't support sufficiently high-resolution PWM)
  //analogWrite(PIN_SIGNAL_OUT, LOW);
  
  // Confirm startup
  // Serial.println("Teensy Sync Running!");
}

void loop() {

  // Wait for positive edge of incoming sync signal
  while (1) {
    if (digitalRead(PIN_SYNC_IN)==HIGH) {
      break;
    }
  }

  // Increase Counter
  nCounter += 1;
  if (nCounter == COUNTER_MAX) {
    nCounter = 0;  
  }

  // Turn LEDs on
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(PIN_LEDS[i], (nCounter >> i) & 1 );
  }
  
  // Determine pulse voltages
  // Teensy 3.* only (Teensy 2.* doesn't support sufficiently high-resolution PWM)
  //unsigned int v = PULSE_OUT_MIDDLE + ((nCounter >> 2) & 1) * (PULSE_OUT_HIGH-PULSE_OUT_MIDDLE);
  
  // Rate of PWM (1.0f = 100% duty cycle, 0.5f = 50%)
  float fPWM = 1.0f - 0.5f * ((nCounter >> 2) & 1);
  
  for (int i = 0; i < (1+(nCounter & 3)); i++) {
    
    // Give PWM higher sampling frequency
    for (int j = 0; j < PWM_SAMPLING_FOLD_INCREASE; j++) {
      digitalWrite(PIN_SIGNAL_OUT, HIGH);
      delayMicroseconds(PULSE_WIDTH * fPWM / PWM_SAMPLING_FOLD_INCREASE);
      digitalWrite(PIN_SIGNAL_OUT, LOW);
      delayMicroseconds(PULSE_WIDTH * (1.0f - fPWM) / PWM_SAMPLING_FOLD_INCREASE );
    }
    
    digitalWrite(PIN_SIGNAL_OUT, LOW);
    delayMicroseconds(PULSE_WIDTH);
    
    // Teensy 3.* only (Teensy 2.* doesn't support sufficiently high-resolution PWM)
    //analogWrite(PIN_SIGNAL_OUT, v);
    //delayMicroseconds(PULSE_WIDTH);
    //analogWrite(PIN_SIGNAL_OUT, 0);
    //delayMicroseconds(PULSE_WIDTH);
  }
} 

