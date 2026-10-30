/*
  Solenoid Push-Pull Valve Test - Single ESP32 Feeder Node
  CPE3207L Thesis Hardware Testing

  Target Actuator: 12V DC Push-Pull Solenoid Valve (Feed Gate Cutoff)
  
  Schematic Reference: 
    - Hardware_Design/02_feeder_node_schematic.svg
    - Hardware_Design/04_power_and_driver_detail.svg

  Hardware Wiring Diagram:
    +-----------------------------------------------------------------------+
    | ESP32 GPIO 22  ----[ 220R Resistor ]----+                             |
    |                                         |                             |
    |                                      [Gate]                           |
    | ESP32 GND     ----+----------------[ IRLZ44N ] MOSFET                |
    |                   |                  [Source]                         |
    |               [ 10kR ]                  |                             |
    |               Pulldown                  v GND (Common Actuator Rail)  |
    |                   |                                                   |
    |                  GND                                                  |
    |                                                                       |
    | +12V SMPS --------+-----------------------+                           |
    |                   |                       |                           |
    |                   +----[ Cathode (Line) ] |                           |
    |                            1N4007         | [ Solenoid 12V Coil ]     |
    |                        Flyback Diode      |                           |
    |                   +----[ Anode        ]   |                           |
    |                   |                       |                           |
    |                   +-----------------------+                           |
    |                   |                                                   |
    |              [ Drain ]                                                |
    +-----------------------------------------------------------------------+

  ⚠️ SAFETY WARNING:
    Push-pull solenoids draw ~0.5A to 1.5A at 12V and heat up rapidly.
    Do NOT keep the solenoid continuously powered for more than 10-15 seconds.
    This firmware enforces a safety timeout (default 10s) to prevent coil thermal damage.

  Serial Menu Commands:
    '1' or 'ON'     - Turn ON solenoid continuously (subject to safety timeout)
    '0' or 'OFF'    - Turn OFF solenoid immediately
    'P' or 'PULSE'  - Trigger a 1-second pulse open
    'C' or 'CYCLE'  - Run a 5-cycle endurance test (1s ON, 3s OFF)
    '?' or 'HELP'   - Show commands menu
*/

// Pin Assignments matching feeder node schematic
const int SOLENOID_PIN = 22; // GPIO 22: Feed Gate Solenoid MOSFET Gate (GPIO 23 for Water Valve)

// Safety Constraints
const unsigned long MAX_ON_TIME_MS = 10000; // Max 10s continuous ON safety limit

// State variables
bool solenoidState = false;
unsigned long solenoidTurnedOnAt = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Wait for Serial Monitor

  // Ensure pin starts LOW to prevent unintended power-on actuation
  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW);
  solenoidState = false;

  printHeader();
  printHelp();
}

void loop() {
  // Check for safety timeout
  if (solenoidState) {
    unsigned long elapsed = millis() - solenoidTurnedOnAt;
    if (elapsed >= MAX_ON_TIME_MS) {
      turnOffSolenoid("SAFETY TIMEOUT EXCEEDED (Max 10s continuous ON limit reached)");
    }
  }

  // Handle incoming Serial commands
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();

    if (input.length() > 0) {
      processCommand(input);
    }
  }
}

void turnOnSolenoid(const char* reason) {
  digitalWrite(SOLENOID_PIN, HIGH);
  solenoidState = true;
  solenoidTurnedOnAt = millis();
  Serial.print(F("[SOLENOID] -> ENGAGED (HIGH) | Reason: "));
  Serial.println(reason);
}

void turnOffSolenoid(const char* reason) {
  digitalWrite(SOLENOID_PIN, LOW);
  solenoidState = false;
  Serial.print(F("[SOLENOID] -> DISENGAGED (LOW) | Reason: "));
  Serial.println(reason);
}

void triggerPulse(unsigned long durationMs) {
  Serial.print(F("[PULSE] Triggering "));
  Serial.print(durationMs);
  Serial.println(F(" ms actuation pulse..."));

  turnOnSolenoid("Pulse Start");
  delay(durationMs);
  turnOffSolenoid("Pulse End");
}

void runEnduranceCycle(int count, unsigned long onTimeMs, unsigned long offTimeMs) {
  Serial.println(F("\n=========================================="));
  Serial.print(F("Starting Solenoid Cycle Test (Count: "));
  Serial.print(count);
  Serial.println(F(")"));
  Serial.println(F("=========================================="));

  for (int i = 1; i <= count; i++) {
    Serial.print(F("Cycle "));
    Serial.print(i);
    Serial.print(F(" / "));
    Serial.println(count);

    turnOnSolenoid("Cycle Actuation");
    delay(onTimeMs);
    turnOffSolenoid("Cycle Rest");
    
    if (i < count) {
      Serial.print(F("Cooling down for "));
      Serial.print(offTimeMs / 1000);
      Serial.println(F(" seconds..."));
      delay(offTimeMs);
    }
  }

  Serial.println(F("[CYCLE] Endurance cycle test completed successfully!\n"));
}

void processCommand(String cmd) {
  if (cmd == "1" || cmd == "ON" || cmd == "OPEN") {
    if (!solenoidState) {
      turnOnSolenoid("User Command");
    } else {
      Serial.println(F("[WARN] Solenoid is ALREADY ON."));
    }
  } 
  else if (cmd == "0" || cmd == "OFF" || cmd == "CLOSE") {
    if (solenoidState) {
      turnOffSolenoid("User Command");
    } else {
      Serial.println(F("[WARN] Solenoid is ALREADY OFF."));
    }
  } 
  else if (cmd == "P" || cmd == "PULSE") {
    triggerPulse(1000); // 1 sec pulse
  } 
  else if (cmd.startsWith("P ") || cmd.startsWith("PULSE ")) {
    int spaceIdx = cmd.indexOf(' ');
    long ms = cmd.substring(spaceIdx + 1).toInt();
    if (ms > 0 && ms <= 10000) {
      triggerPulse(ms);
    } else {
      Serial.println(F("[ERROR] Pulse duration must be between 1 and 10000 ms."));
    }
  }
  else if (cmd == "C" || cmd == "CYCLE" || cmd == "BURST") {
    runEnduranceCycle(5, 1000, 3000); // 5 cycles, 1s ON, 3s OFF
  } 
  else if (cmd == "?" || cmd == "HELP") {
    printHelp();
  } 
  else {
    Serial.print(F("Unknown command: '"));
    Serial.print(cmd);
    Serial.println(F("'. Type '?' or 'HELP' for options."));
  }
}

void printHeader() {
  Serial.println(F("\n======================================================="));
  Serial.println(F("   CPE3207L - Solenoid Push-Pull Valve Test Firmware   "));
  Serial.println(F("======================================================="));
  Serial.print(F("Target Pin : GPIO "));
  Serial.println(SOLENOID_PIN);
  Serial.print(F("Max Safety : "));
  Serial.print(MAX_ON_TIME_MS / 1000);
  Serial.println(F(" seconds continuous limit"));
  Serial.println(F("-------------------------------------------------------"));
}

void printHelp() {
  Serial.println(F("Available Serial Commands (115200 baud):"));
  Serial.println(F("  1  or ON        : Energize solenoid valve"));
  Serial.println(F("  0  or OFF       : De-energize solenoid valve"));
  Serial.println(F("  P  or PULSE     : Trigger 1-second pulse open"));
  Serial.println(F("  P <ms>          : Trigger custom pulse duration (e.g., 'P 2500')"));
  Serial.println(F("  C  or CYCLE     : Run 5-cycle endurance test (1s ON / 3s OFF)"));
  Serial.println(F("  ?  or HELP      : Display this menu"));
  Serial.println(F("-------------------------------------------------------\n"));
}
