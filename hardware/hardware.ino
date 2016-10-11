void setup() {

  // Init serial communication
  Serial.begin(9600);

  // Init TTL output pin
  pinMode(3, OUTPUT);
  pinMode(7, OUTPUT);
  digitalWrite(3, LOW);
  digitalWrite(7, LOW);
  
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
        digitalWrite(3, HIGH);
        digitalWrite(7, HIGH);
        delay(2000);
        digitalWrite(3, LOW);
        digitalWrite(7, LOW);
        
        Serial.write("!");
        break;
    }
  }
} 
