/*
  ESP32 Blink & Serial Verification
  
  This simple sketch verifies that:
  1. The ESP32 is powered and the computer can upload code to it.
  2. The onboard LED blinks (typically on GPIO 2).
  3. Serial communication is functioning at 115200 baud.
*/

#define LED_PIN 2 // Default onboard LED pin for most ESP32 Dev Kits

void setup() {
  // Initialize the LED pin as an output
  pinMode(LED_PIN, OUTPUT);
  
  // Initialize serial communication at 115200 bits per second
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect (only needed for native USB)
  }
  
  Serial.println("");
  Serial.println("=========================================");
  Serial.println("   ESP32 Dev Board Verification Active   ");
  Serial.println("=========================================");
}

void loop() {
  // Turn the LED on
  digitalWrite(LED_PIN, HIGH);
  Serial.println("[STATUS] LED ON - ESP32 Core is active");
  delay(1000); // Wait for 1 second
  
  // Turn the LED off
  digitalWrite(LED_PIN, LOW);
  Serial.println("[STATUS] LED OFF - ESP32 is running...");
  delay(1000); // Wait for 1 second
}
