/*
  MAX485 TTL-to-RS485 Half-Duplex Communication Sketch
  
  Safety Notice:
    - The MAX485 module is powered by 5V.
    - Its RO (Receiver Output) pin outputs a 5V signal when active.
    - The ESP32 pins are rated for 3.3V maximum.
    - YOU MUST USE A RESISTOR VOLTAGE DIVIDER between the MAX485 RO pin and the ESP32 RX2 pin.
      Example: RO pin -> 1kΩ Resistor -> ESP32 RX2 (GPIO 16) -> 2kΩ Resistor -> GND
    - The DI (Driver Input) is driven by the ESP32 TX2 pin (3.3V). The MAX485 accepts 3.3V logic high,
      so TX2 can be connected directly to DI.
      
  Wiring Connections:
    - MAX485 VCC    -> ESP32 5V (or VIN)
    - MAX485 GND    -> ESP32 GND (Common Ground)
    - MAX485 RO     -> 1kΩ Resistor -> ESP32 RX2 (GPIO 16)
    - ESP32 GPIO 16 -> 2kΩ Resistor -> GND
    - MAX485 DI     -> ESP32 TX2 (GPIO 17)
    - MAX485 DE & RE (tied together) -> ESP32 GPIO 4 (Direction Control pin)
    
  RS-485 Bus Connections:
    - MAX485 A Pin  -> Connected to other RS-485 device Pin A (non-inverting)
    - MAX485 B Pin  -> Connected to other RS-485 device Pin B (inverting)
    - GND           -> It is recommended to connect the GND of both devices with a third wire
                       to prevent common-mode voltage damage.
                       
  How to Test:
    Option 1: Connect a USB-to-RS485 adapter from your PC to the A/B pins of the MAX485. Use a serial
              terminal program (like PuTTY or Arduino Serial Monitor) on your PC at 9600 baud.
              Type a message and send it. The ESP32 will receive it and echo it back.
    Option 2: Use two ESP32s, each wired to a MAX485 module. Program one as a transmitter and one as
              a receiver/echo.
*/

#define RX2_PIN 16
#define TX2_PIN 17
#define DE_RE_PIN 4 // Pin used to control transmit/receive mode

void setup() {
  // Debug serial console to PC
  Serial.begin(115200);
  
  // RS-485 serial communication (Serial2)
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  
  // Set the DE & RE pin as output
  pinMode(DE_RE_PIN, OUTPUT);
  
  // Initialize in RECEIVE mode (DE/RE = LOW)
  digitalWrite(DE_RE_PIN, LOW);
  
  Serial.println("");
  Serial.println("=========================================");
  Serial.println("         MAX485 RS-485 Echo Test         ");
  Serial.println("=========================================");
  Serial.println("Listening for messages on RS-485 A/B bus at 9600 baud...");
}

void loop() {
  // Check if data has arrived from the RS-485 bus
  if (Serial2.available() > 0) {
    // Read the incoming message
    String incomingMsg = Serial2.readStringUntil('\n');
    incomingMsg.trim(); // Remove whitespace and newlines
    
    // Print the received message to the PC Debug Console
    Serial.print("[RS-485 IN] Received: \"");
    Serial.print(incomingMsg);
    Serial.println("\"");
    
    // Echo the message back to the RS-485 bus
    Serial.println("[RS-485 OUT] Echoing back...");
    
    // 1. Switch MAX485 to TRANSMIT mode (DE/RE = HIGH)
    digitalWrite(DE_RE_PIN, HIGH);
    delay(5); // Small delay to let the pin state settle
    
    // 2. Transmit the data
    Serial2.print("ECHO: ");
    Serial2.println(incomingMsg);
    
    // 3. Wait for transmission to complete before switching back to receive
    Serial2.flush(); 
    delay(5); // Small delay for final stop bits to clear the transmitter
    
    // 4. Switch MAX485 back to RECEIVE mode (DE/RE = LOW)
    digitalWrite(DE_RE_PIN, LOW);
    
    Serial.println("[STATUS] Switched back to Receive Mode.");
  }

  // Optional: Send data from PC keyboard over the RS-485 bus (for debugging)
  if (Serial.available() > 0) {
    String localInput = Serial.readStringUntil('\n');
    localInput.trim();
    
    Serial.print("[LOCAL] Sending to RS-485: \"");
    Serial.print(localInput);
    Serial.println("\"");
    
    // Switch to transmit mode
    digitalWrite(DE_RE_PIN, HIGH);
    delay(5);
    
    // Send message
    Serial2.println(localInput);
    
    // Wait and switch back to receive
    Serial2.flush();
    delay(5);
    digitalWrite(DE_RE_PIN, LOW);
  }
}
