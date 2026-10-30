/*
  ActuationModule_Test.ino - Bench Bring-Up & Interactive Diagnostic Sketch
  CPE3207L Thesis Hardware Testing

  Interactive test suite for the complete Actuation Module:
    - NEMA17 Stepper Motor (Auger Feed Screw via DRV8825)
    - Feed Gate Solenoid Valve (12V Push-Pull via IRLZ44N MOSFET)
    - Water Refill Solenoid Valve (12V DC via IRLZ44N MOSFET)

  Pin Assignments (matching feeder node schematic):
    - DRV8825 STEP   -> GPIO 18
    - DRV8825 DIR    -> GPIO 19
    - DRV8825 ENABLE -> GPIO 21 (Active LOW)
    - GATE_FEED      -> GPIO 22 (Feed Gate Solenoid)
    - GATE_WATER     -> GPIO 23 (Water Valve Solenoid)

  Serial Commands (115200 Baud):
    '1' / 'GATE 1'   - Open Feed Gate Solenoid (subject to 10s thermal safety limit)
    '0' / 'GATE 0'   - Close Feed Gate Solenoid
    'W 1'            - Open Water Solenoid Valve
    'W 0'            - Close Water Solenoid Valve
    'P <ms>'         - Pulse Water Valve for specified duration in ms (e.g. 'P 3000')
    'D <grams>'      - Execute automated dispensing sequence for target grams (e.g. 'D 250')
    'S <steps>'      - Step auger motor for manual step count (e.g. 'S 400')
    'ESTOP'          - Trigger Emergency Stop (locks out all actuators)
    'RESET'          - Reset E-Stop & safety flags
    'STATUS'         - Print current hardware status flags and states
    '?' / 'HELP'     - Show serial commands menu
*/

#include "ActuatorController.h"

ActuatorController actuators;

void printHeader() {
  Serial.println(F("\n======================================================="));
  Serial.println(F("   CPE3207L - Actuation Module Diagnostic Firmware     "));
  Serial.println(F("======================================================="));
  Serial.println(F("Pins: STEP:18 | DIR:19 | EN:21 | GATE_FEED:22 | WATER:23"));
  Serial.println(F("Thermal Safety Limit: 10s continuous ON on Feed Solenoid"));
  Serial.println(F("-------------------------------------------------------"));
}

void printHelp() {
  Serial.println(F("Available Serial Commands (115200 baud):"));
  Serial.println(F("  1  or GATE 1    : Open Feed Gate Solenoid"));
  Serial.println(F("  0  or GATE 0    : Close Feed Gate Solenoid"));
  Serial.println(F("  W 1             : Open Water Solenoid Valve"));
  Serial.println(F("  W 0             : Close Water Solenoid Valve"));
  Serial.println(F("  P <ms>          : Pulse Water Valve (e.g. 'P 2500')"));
  Serial.println(F("  D <grams>       : Run dispensing sequence (e.g. 'D 200')"));
  Serial.println(F("  S <steps>       : Step auger motor manually (e.g. 'S 800')"));
  Serial.println(F("  ESTOP           : Emergency Stop (Instant shutoff & lockout)"));
  Serial.println(F("  RESET           : Reset E-Stop and safety flags"));
  Serial.println(F("  STATUS          : Print current actuator status flags"));
  Serial.println(F("  ? or HELP       : Display this menu"));
  Serial.println(F("-------------------------------------------------------\n"));
}

void printStatus() {
  uint16_t flags = actuators.getStatusFlags();
  Serial.println(F("\n--- ACTUATOR MODULE STATUS ---"));
  Serial.print(F("  Feed Gate Solenoid  : "));
  Serial.println(actuators.isFeedGateOpen() ? F("OPEN (HIGH)") : F("CLOSED (LOW)"));
  Serial.print(F("  Water Solenoid Valve: "));
  Serial.println(actuators.isWaterValveOpen() ? F("OPEN (HIGH)") : F("CLOSED (LOW)"));
  Serial.print(F("  Auger Stepper Motor : "));
  Serial.println(actuators.isStepperActive() ? F("RUNNING") : F("IDLE (Driver Disabled)"));
  Serial.print(F("  Thermal Timeout     : "));
  Serial.println(actuators.isThermalSafetyTriggered() ? F("FLAGGED (Thermal cutoff triggered)") : F("OK"));
  Serial.print(F("  Emergency Stop      : "));
  Serial.println(actuators.isEstopActive() ? F("ACTIVE (System Locked Out)") : F("INACTIVE (Ready)"));
  Serial.print(F("  Raw Status Bitmask  : 0x"));
  Serial.println(flags, HEX);
  Serial.println(F("-------------------------------\n"));
}

void processCommand(String cmd) {
  cmd.trim();
  String upperCmd = cmd;
  upperCmd.toUpperCase();

  if (upperCmd == "1" || upperCmd == "GATE 1" || upperCmd == "OPEN FEED") {
    actuators.setFeedGate(true);
  }
  else if (upperCmd == "0" || upperCmd == "GATE 0" || upperCmd == "CLOSE FEED") {
    actuators.setFeedGate(false);
  }
  else if (upperCmd == "W 1" || upperCmd == "OPEN WATER") {
    actuators.setWaterValve(true);
  }
  else if (upperCmd == "W 0" || upperCmd == "CLOSE WATER") {
    actuators.setWaterValve(false);
  }
  else if (upperCmd.startsWith("P ") || upperCmd.startsWith("PULSE ")) {
    int spaceIdx = cmd.indexOf(' ');
    long ms = cmd.substring(spaceIdx + 1).toInt();
    if (ms > 0 && ms <= 60000) {
      actuators.triggerWaterPulse((uint32_t)ms);
    } else {
      Serial.println(F("[ERROR] Pulse duration must be between 1 and 60000 ms."));
    }
  }
  else if (upperCmd.startsWith("D ") || upperCmd.startsWith("DISPENSE ")) {
    int spaceIdx = cmd.indexOf(' ');
    long grams = cmd.substring(spaceIdx + 1).toInt();
    if (grams > 0 && grams <= 5000) {
      actuators.dispenseFeedGrams((uint16_t)grams);
    } else {
      Serial.println(F("[ERROR] Grams to dispense must be between 1 and 5000 grams."));
    }
  }
  else if (upperCmd.startsWith("S ") || upperCmd.startsWith("STEP ")) {
    int spaceIdx = cmd.indexOf(' ');
    long steps = cmd.substring(spaceIdx + 1).toInt();
    if (steps != 0) {
      bool cw = steps > 0;
      actuators.stepMotor(abs(steps), cw);
    } else {
      Serial.println(F("[ERROR] Steps must be non-zero integer (positive for CW, negative for CCW)."));
    }
  }
  else if (upperCmd == "ESTOP" || upperCmd == "STOP") {
    actuators.emergencyStop();
  }
  else if (upperCmd == "RESET" || upperCmd == "CLEAR") {
    actuators.resetSafetyFlags();
  }
  else if (upperCmd == "STATUS" || upperCmd == "STAT") {
    printStatus();
  }
  else if (upperCmd == "?" || upperCmd == "HELP") {
    printHelp();
  }
  else {
    Serial.print(F("Unknown command: '"));
    Serial.print(cmd);
    Serial.println(F("'. Type '?' or 'HELP' for options."));
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  actuators.init();
  printHeader();
  printHelp();
}

void loop() {
  actuators.update(); // Background thermal safety & pulse timing updates

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    if (input.length() > 0) {
      processCommand(input);
    }
  }
}
