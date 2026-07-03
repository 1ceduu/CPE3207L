/*
  MQ-135 Gas Sensor Testing & Baseline Calibration Sketch
  
  Safety Notice:
    - The MQ-135 module must be powered by 5V ( heater requires 5V to function).
    - Its Analog Output (AO) is a 5V signal. 
    - The ESP32 pins are rated for 3.3V maximum.
    - YOU MUST USE A RESISTOR VOLTAGE DIVIDER between the MQ-135 AO pin and the ESP32 GPIO pin.
      Example: AO pin -> 1kΩ Resistor -> ESP32 GPIO 34 -> 2kΩ Resistor -> GND
    
  Wiring Connections:
    - MQ-135 VCC  -> ESP32 5V (or VIN if connected to 5V USB source)
    - MQ-135 GND  -> ESP32 GND
    - MQ-135 AO   -> 1kΩ Resistor -> ESP32 GPIO 34 (ADC1_CH6)
    - ESP32 GPIO 34 -> 2kΩ Resistor -> GND (Common Ground)

  Sensor Preheating (Burn-In):
    - For new MQ-135 sensors, let it run continuously for 24-48 hours. 
    - The sensor will get warm to the touch; this is normal because of the internal heater.
    
  Calibration Steps:
    1. Upload this sketch.
    2. Place the sensor in fresh outdoor air (or clean indoor air).
    3. Note down the stabilized "Rs" value.
    4. Since fresh air has about 400ppm CO2 and minimal ammonia, we divide Rs by a scaling factor
       (typically Rs/R0 = 3.6 in clean air for MQ-135) to find R0. Or simply record Rs in fresh air
       as your baseline R0 (relative clean-air baseline).
    5. Test response by bringing a small ammonia source (glass cleaner) near the sensor.
*/

const int MQ135_PIN = 34;      // ESP32 ADC pin connected to divider output
const float RL_VALUE = 10.0;   // Load resistor value on the module (typically 10 kΩ)
const float VCC_VAL = 5.0;     // Supply voltage to the MQ sensor (5.0V)

// Enter your clean air baseline R0 here once calculated
float R0_clean_air = 10.0;     // Default placeholder R0 in kΩ

void setup() {
  Serial.begin(115200);
  pinMode(MQ135_PIN, INPUT);
  
  Serial.println("");
  Serial.println("=========================================");
  Serial.println("         MQ-135 Gas Sensor Test          ");
  Serial.println("=========================================");
  Serial.println("Allow sensor heater to warm up for 5-10 minutes before testing.");
}

void loop() {
  int rawADC = analogRead(MQ135_PIN);
  
  // ESP32 12-bit ADC reads 0 to 4095. Max voltage on pin is 3.3V.
  float espPinVoltage = rawADC * (3.3 / 4095.0);
  
  // Reconstruct the original 5V sensor voltage before the 2/3 divider (1k & 2k resistors)
  // V_esp = V_sensor * (2k / (1k + 2k)) = V_sensor * (2/3)
  // V_sensor = V_esp * 1.5
  float sensorVoltage = espPinVoltage * 1.5;
  
  // Avoid division by zero if sensor voltage is extremely low or high
  if (sensorVoltage < 0.1) sensorVoltage = 0.1;
  if (sensorVoltage > 4.9) sensorVoltage = 4.9;

  // Calculate Sensor Resistance (Rs) using the voltage divider formula:
  // Rs = RL * (Vcc - Vout) / Vout
  float Rs = RL_VALUE * (VCC_VAL - sensorVoltage) / sensorVoltage;

  // Calculate the ratio Rs/R0
  float ratio = Rs / R0_clean_air;

  // Estimate Ammonia (NH3) concentration in PPM using curve fit power equation
  // For MQ-135, NH3 curve is approximately: PPM = 102.2 * (Rs/R0)^(-2.47)
  float ppmNH3 = 102.2 * pow(ratio, -2.47);

  // Print results
  Serial.print("Raw ADC: ");
  Serial.print(rawADC);
  Serial.print(" | V_pin: ");
  Serial.print(espPinVoltage, 2);
  Serial.print("V | V_sensor: ");
  Serial.print(sensorVoltage, 2);
  Serial.print("V | Rs: ");
  Serial.print(Rs, 2);
  Serial.print(" kOhm | NH3 Est: ");
  Serial.print(ppmNH3, 1);
  Serial.println(" ppm");

  delay(1000);
}
