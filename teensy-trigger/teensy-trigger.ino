
#define PIN_AMPLIFIED 4
#define PIN_TEENSY_SIGNAL 21
#define SIGNAL_DURATION_MS 1000

void setup() {

  // Init serial communication
  Serial.begin(9600);

  // Init TTL output pin
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
        Serial.write("Teensy TTL Controller"); 
        break;
      case 't':          
        
        // Write TTL
        digitalWrite(PIN_AMPLIFIED, HIGH);
        digitalWrite(PIN_TEENSY_SIGNAL, HIGH);
        delay(SIGNAL_DURATION_MS);
        digitalWrite(PIN_AMPLIFIED, LOW);
        digitalWrite(PIN_TEENSY_SIGNAL, LOW);
        
        Serial.write("!");
        break;
    }
  }
} 
