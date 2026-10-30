/*
  DRV8825 + NEMA 17 Stepper Diagnostic & Hardware Verification Test
  CPE3207L Thesis Hardware Testing

  Pin Map:
    DRV8825 STEP   -> ESP32 GPIO 18
    DRV8825 DIR    -> ESP32 GPIO 19
    DRV8825 ENABLE -> ESP32 GPIO 21 (LOW = Active / Holding Torque, HIGH = Sleep)
    DRV8825 RST    -> 3.3V
    DRV8825 SLP    -> 3.3V
    DRV8825 M0,M1,M2 -> 3.3V (1/32) or GND (Full Step)
    DRV8825 VMOT   -> External 12V Power Supply (+)
    DRV8825 GND    -> External 12V Supply (-) AND ESP32 GND (Shared Common GND)
*/

const int STEP_PIN   = 18;
const int DIR_PIN    = 19;
const int ENABLE_PIN = 21;

// Step delay in microseconds (Half period). 
// 1000us = 500 steps/sec (Guaranteed safe speed for ANY stepper regardless of microstepping!)
uint32_t stepDelayUs = 1000; 
bool motorDir = true; // true = CW, false = CCW
unsigned long stepCounter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, HIGH);
  digitalWrite(ENABLE_PIN, LOW); // Active LOW: Enable driver immediately

  Serial.println(F("\n======================================================="));
  Serial.println(F("     DRV8825 + NEMA 17 HARDWARE DIAGNOSTIC TEST        "));
  Serial.println(F("======================================================="));
  Serial.println(F("Pin Assignments:"));
  Serial.println(F("  STEP_PIN   : GPIO 18"));
  Serial.println(F("  DIR_PIN    : GPIO 19"));
  Serial.println(F("  ENABLE_PIN : GPIO 21 (Forced LOW -> Driver ENABLED)"));
  Serial.println(F("-------------------------------------------------------"));
  Serial.println(F("HARDWARE CHECKLIST:"));
  Serial.println(F(" 1. Feel the motor shaft RIGHT NOW:"));
  Serial.println(F("    - If shaft is LOCKED/STIFF -> 12V & Driver are ACTIVE!"));
  Serial.println(F("    - If shaft spins freely by hand -> NO POWER or NO VREF."));
  Serial.println(F(" 2. External 12V supply must be connected to VMOT & GND."));
  Serial.println(F(" 3. ESP32 GND and 12V Supply (-) MUST be tied together."));
  Serial.println(F(" 4. DRV8825 RST and SLP must be connected to 3.3V."));
  Serial.println(F("======================================================="));
  Serial.println(F("Pulsing motor continuously at safe 500 steps/second..."));
  Serial.println(F("Commands: 'SLOW', 'FAST', 'DIR', 'STOP', 'GO', '1', '2'\n"));

  delay(1500);
}

bool isStepping = true;

void loop() {
  // Read Serial Commands for Interactive Testing
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "STOP") {
      isStepping = false;
      digitalWrite(ENABLE_PIN, HIGH); // Disable driver
      Serial.println(F("[CMD] Stepper STOPPED. Driver set to SLEEP (HIGH)."));
    } else if (cmd == "GO" || cmd == "START") {
      isStepping = true;
      digitalWrite(ENABLE_PIN, LOW); // Enable driver
      delay(5);
      Serial.println(F("[CMD] Stepper STARTED. Driver ENABLED (LOW)."));
    } else if (cmd == "DIR") {
      motorDir = !motorDir;
      digitalWrite(DIR_PIN, motorDir ? HIGH : LOW);
      Serial.print(F("[CMD] Direction toggled -> "));
      Serial.println(motorDir ? F("CW (HIGH)") : F("CCW (LOW)"));
    } else if (cmd == "SLOW" || cmd == "1") {
      stepDelayUs = 2000; // 250 steps/sec (Ultra slow crawl)
      Serial.println(F("[CMD] Speed set to ULTRA-SLOW (250 steps/sec, 2000us delay)."));
    } else if (cmd == "FAST" || cmd == "2") {
      stepDelayUs = 250;  // 2000 steps/sec
      Serial.println(F("[CMD] Speed set to MEDIUM-FAST (2000 steps/sec, 250us delay)."));
    } else if (cmd == "NORM" || cmd == "3") {
      stepDelayUs = 1000; // 500 steps/sec
      Serial.println(F("[CMD] Speed set to NORMAL (500 steps/sec, 1000us delay)."));
    }
  }

  // Continuous Fail-Safe Stepping Loop
  if (isStepping) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(stepDelayUs);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(stepDelayUs);
    stepCounter++;

    // Print heartbeat every 500 steps (~1 second at 500 steps/sec)
    if (stepCounter % 500 == 0) {
      Serial.print(F("[PULSING] Total Steps Sent: "));
      Serial.print(stepCounter);
      Serial.print(F(" | Delay: "));
      Serial.print(stepDelayUs);
      Serial.print(F("us ("));
      Serial.print(1000000UL / (stepDelayUs * 2));
      Serial.println(F(" steps/sec) | Driver: ENABLED (LOW)"));
    }
  }
}
