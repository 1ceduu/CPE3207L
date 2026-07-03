/*
  HX711 Load Cell Calibration and Testing Sketch
  
  Dependencies:
    - Install "HX711 Arduino Library" by Bogdan Necula via Arduino Library Manager.
    
  Wiring Connections:
    - HX711 VCC -> ESP32 3.3V
    - HX711 GND -> ESP32 GND
    - HX711 DT  -> ESP32 GPIO 32
    - HX711 SCK -> ESP32 GPIO 33
    
  Load Cell Connections to HX711 Breakout:
    - Red Wire   -> E+ (Excitation Positive)
    - Black Wire -> E- (Excitation Negative)
    - White Wire -> A- (Amplifier Channel A Negative)
    - Green Wire -> A+ (Amplifier Channel A Positive)
    
  Calibration Steps:
    1. Upload this sketch to the ESP32.
    2. Open the Serial Monitor (115200 baud).
    3. Make sure the load cell is empty (no weight). Press 't' in the serial monitor to tare.
    4. Place a known weight (e.g. 500g, 1000g) on the scale.
    5. Read the "Raw Value" printed in the serial monitor.
    6. Calculate your calibration factor:
       Calibration Factor = (Raw Value with weight - Tare Raw Value) / Known Weight
    7. Enter your calculated calibration factor by sending it via serial (e.g., send "1234.5") 
       or modify the CALIBRATION_FACTOR define below.
*/

#include "HX711.h"

// Pin configuration
const int DOUT_PIN = 32;
const int SCK_PIN = 33;

HX711 scale;

// Set this to your calculated calibration factor once determined
float calibration_factor = 420.0; // Default placeholder factor

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("=========================================");
  Serial.println("         HX711 Load Cell Testing         ");
  Serial.println("=========================================");

  scale.begin(DOUT_PIN, SCK_PIN);

  Serial.println("Testing connection to HX711...");
  if (scale.is_ready()) {
    Serial.println("[SUCCESS] HX711 is responsive.");
  } else {
    Serial.println("[ERROR] HX711 not found! Please check your connections.");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Taring scale... Keep load cell clear of any weight.");
  scale.set_scale();
  scale.tare(); // Reset the scale to 0
  Serial.println("[SUCCESS] Tare complete.");
  
  Serial.println("\n--- Serial Commands ---");
  Serial.println("Send 't' to Tare/Zero the scale");
  Serial.println("Send a number (e.g. '420.5') to set a new Calibration Factor");
  Serial.println("-----------------------\n");
}

void loop() {
  if (scale.is_ready()) {
    // Read raw data and calibrated data
    long raw_reading = scale.read();
    
    // Apply calibration factor to get weight
    scale.set_scale(calibration_factor);
    float weight = scale.get_units(5); // Average of 5 readings

    Serial.print("Raw: ");
    Serial.print(raw_reading);
    Serial.print(" | CalFactor: ");
    Serial.print(calibration_factor);
    Serial.print(" | Weight: ");
    Serial.print(weight, 1);
    Serial.println(" units (grams/ounces depending on calibration)");
    
  } else {
    Serial.println("[ERROR] Scale not ready.");
  }

  // Handle user inputs from Serial Monitor
  if (Serial.available() > 0) {
    char inChar = Serial.peek();
    
    if (inChar == 't' || inChar == 'T') {
      Serial.read(); // Consume character
      Serial.println("\n[ACTION] Taring scale...");
      scale.tare();
      Serial.println("[SUCCESS] Scale zeroed.\n");
    } else {
      float newFactor = Serial.parseFloat();
      if (newFactor != 0.0) {
        calibration_factor = newFactor;
        Serial.print("\n[ACTION] Updated Calibration Factor to: ");
        Serial.println(calibration_factor);
        Serial.println("");
      } else {
        Serial.read(); // Clear invalid input
      }
    }
  }

  delay(500);
}
