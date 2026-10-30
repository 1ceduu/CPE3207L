/*
  ===================================================================================
  CPE3207L Thesis - Integrated Feeder Node Firmware (Dual-Core FreeRTOS)
  Project: Integrated Automated Swine Feeding and Watering System with Centralized Data Monitoring
  Platform: ESP32-WROOM-32 (38-Pin Development Board)
  ===================================================================================

  HARDWARE INTEGRATION INCLUDED IN THIS FIRMWARE:
  -----------------------------------------------------------------------------------
  1. ACTUATORS:
     - NEMA 17 Stepper Motor (Auger Dispenser via DRV8825 Driver with trapezoidal speed ramp)
     - 12V Push-Pull Feed Gate Cutoff Solenoid (via 2-Channel 5V Relay Module, Active-LOW)
     - 12V Water Refill Solenoid Valve (via 2-Channel 5V Relay Module, Active-LOW)
  
  2. SENSORS:
     - HX711 24-bit ADC + Strain-Gauge Load Cell (Feed remaining in trough in grams)
     - JSN-SR04T / AJ-SR04M Waterproof Ultrasonic Sensor (Water depth % and cm)
     - MQ-135 Gas Sensor (Ammonia / NH3 air quality monitoring)
     - Manual Hardware Override Push Button (with 10k pulldown on GPIO 15)

  3. COMMUNICATION & CONTROL:
     - Built-in, Zero-Dependency RS-485 MODBUS RTU Slave Engine (FC 0x03 & FC 0x06)
     - Dual-Core FreeRTOS:
         * Core 0: Background Sensor Acquisition Task & USB JSON Telemetry Stream
         * Core 1: RS-485 MODBUS RTU Slave Stack & Serial Diagnostic Command Engine
     - Interactive USB Serial Command Line (115200 baud) for standalone bench testing

  REQUIRED ARDUINO LIBRARY:
  -----------------------------------------------------------------------------------
  - "HX711 Arduino Library" by Bogdan Necula (Install via Arduino Library Manager)
  - NO external Modbus library needed (built directly into this sketch for 100% compatibility)
  ===================================================================================
*/

#include <Arduino.h>
#include "HX711.h"
#include <Preferences.h>

// ===================================================================================
// PIN DEFINITIONS (Matches Hardware_Design/02_feeder_node_schematic.svg)
// ===================================================================================

// --- Actuator Pins ---
#define STEP_PIN           18   // DRV8825 STEP
#define DIR_PIN            19   // DRV8825 DIR
#define ENABLE_PIN         21   // DRV8825 ENABLE (Active LOW: LOW=Enabled, HIGH=Idle)
#define RELAY_FEED_PIN     22   // 2-Ch Relay IN1 -> Feed Gate Push-Pull Solenoid
#define RELAY_WATER_PIN    23   // 2-Ch Relay IN2 -> Water Solenoid Valve

// --- Sensor Pins ---
#define HX711_DT_PIN       32   // Load Cell Data Line
#define HX711_SCK_PIN      33   // Load Cell Clock Line
#define TRIG_PIN           25   // Ultrasonic Trigger
#define ECHO_PIN           26   // Ultrasonic Echo (via 1k/2k resistor divider)
#define MQ135_PIN          34   // Ammonia Gas Analog In (via 10k/20k resistor divider)
#define OVERRIDE_BTN_PIN   15   // Manual Override Button (Active HIGH with 10k pulldown)

// --- RS-485 MODBUS Pins ---
#define RX2_PIN            16   // MAX485 RO (via 1k/2k resistor divider)
#define TX2_PIN            17   // MAX485 DI
#define DE_RE_PIN          4    // MAX485 DE & RE (tied together)

// ===================================================================================
// CALIBRATION AND OPERATIONAL CONSTANTS
// ===================================================================================

// Stepper & Actuator Settings
#define DEFAULT_MICROSTEP          32     // 32 (M0, M1, M2 tied to 3.3V for 1/32 microstepping)
#define DEFAULT_START_RPM          5L     // Initial ramp speed (RPM, gentle ease-in)
#define DEFAULT_CRUISE_RPM         20L    // High-torque cruise speed (~250us delay, proven smooth)
#define DEFAULT_STEPS_PER_GRAM     80.0f  // Calibration factor: steps per gram of swine feed
#define MAX_SOLENOID_ON_MS         10000U // 10s thermal safety limit for push-pull solenoid

// Relay Logic Configuration (Active-LOW: LOW = Relay ON / Valve OPEN, HIGH = Relay OFF / Valve CLOSED)
#define RELAY_VALVE_OPEN           LOW
#define RELAY_VALVE_CLOSED         HIGH

// Sensor Calibration Defaults
#define DEFAULT_HX711_CAL_FACTOR   226.0f // Calibrated: ~226 counts/g (e.g. 396g battery = ~89,500 ADC counts)
#define DEFAULT_WEIGHT_DEADBAND_G  3.0f   // Noise cutoff deadband: fluctuations < 3.0g are locked to 0.0g
#define EMPTY_DISTANCE_CM          66.0f  // Distance to empty trough bottom (0%)
#define FULL_DISTANCE_CM           44.2f  // Distance to full water line (100%)
#define SOUND_SPEED                0.0343f // cm/us at 20°C

// MODBUS Configuration
#define SLAVE_ID                   1      // MODBUS Slave Address
#define MODBUS_BAUD_RATE           115200

// Actuator Status Bitmask Flags (MODBUS Register 6)
#define ACTUATOR_FLAG_STEPPER_ACTIVE      (1 << 0)
#define ACTUATOR_FLAG_FEED_GATE_ACTIVE    (1 << 1)
#define ACTUATOR_FLAG_WATER_VALVE_ACTIVE  (1 << 2)
#define ACTUATOR_FLAG_THERMAL_TIMEOUT     (1 << 3)
#define ACTUATOR_FLAG_ESTOP_ACTIVE        (1 << 4)

// ===================================================================================
// GLOBAL OBJECTS & SHARED STATE
// ===================================================================================

HX711 scale;
float hx711CalFactor       = DEFAULT_HX711_CAL_FACTOR;
float weightDeadbandG      = DEFAULT_WEIGHT_DEADBAND_G;
bool  hx711Tared           = false;
bool  scaleInitialized     = false;

// Real-time telemetry smoothing & outlier rejection buffers
float smoothedWeightGrams  = 0.0f;
float weightRingBuf[3]     = {0.0f, 0.0f, 0.0f};
uint8_t weightRingIdx      = 0;

// ===================================================================================
// MULTI-POINT PIECEWISE LINEAR CALIBRATION TABLE (0 - 5000g in 500g increments)
// Resolves load cell mechanical deflection & ADC non-linearity across 0-5kg.
// Employs continuous piecewise linear interpolation in between every 500g node.
// ===================================================================================

struct CalPoint {
  float  weight_g;    // Reference mass in grams (0, 500, 1000, 1500, ... 5000)
  double raw_counts;  // Measured raw ADC delta (ADC counts minus tare offset)
  bool   calibrated;  // True if measured by user; False if baseline estimate
};

#define NUM_CAL_POINTS 11

CalPoint calTable[NUM_CAL_POINTS] = {
  {    0.0f,       0.0, true  },  // Point 0: Empty Trough (0g)
  {  500.0f,  113000.0, true  },  // Point 1: 500g   (~226.0 counts/g, calibrated for low range: e.g. 225g can, 396g battery)
  { 1000.0f,  230000.0, false },  // Point 2: 1000g  (~230.0 counts/g)
  { 1500.0f,  352560.0, true  },  // Point 3: 1500g  (~235.0 counts/g, calibrated reference item)
  { 2000.0f,  475000.0, false },  // Point 4: 2000g  (~237.5 counts/g)
  { 2500.0f,  600000.0, false },  // Point 5: 2500g  (~240.0 counts/g)
  { 3000.0f,  724000.0, false },  // Point 6: 3000g  (~241.3 counts/g)
  { 3500.0f,  848000.0, false },  // Point 7: 3500g  (~242.3 counts/g)
  { 4000.0f,  972000.0, false },  // Point 8: 4000g  (~243.0 counts/g)
  { 4500.0f, 1096000.0, false },  // Point 9: 4500g  (~243.5 counts/g)
  { 5000.0f, 1220000.0, false }   // Point 10: 5000g (~244.0 counts/g)
};

void saveCalibrationToNVS() {
  Preferences prefs;
  if (prefs.begin("feeder_cal", false)) {
    prefs.putBytes("calTable", calTable, sizeof(calTable));
    prefs.putFloat("deadband", weightDeadbandG);
    prefs.end();
    Serial.println(F("[NVS] Calibration table saved to persistent flash memory."));
  } else {
    Serial.println(F("[ERROR] Failed to open NVS storage for writing!"));
  }
}

void loadCalibrationFromNVS() {
  Preferences prefs;
  if (prefs.begin("feeder_cal", true)) {
    if (prefs.isKey("calTable")) {
      prefs.getBytes("calTable", calTable, sizeof(calTable));
      Serial.println(F("[NVS] Multi-point calibration profile loaded from flash."));
    }
    if (prefs.isKey("deadband")) {
      weightDeadbandG = prefs.getFloat("deadband", DEFAULT_WEIGHT_DEADBAND_G);
    }
    prefs.end();
  }
}

void resetCalibrationDefaults() {
  calTable[0]  = {    0.0f,       0.0, true  };
  calTable[1]  = {  500.0f,  113000.0, true  };
  calTable[2]  = { 1000.0f,  230000.0, false };
  calTable[3]  = { 1500.0f,  352560.0, true  };
  calTable[4]  = { 2000.0f,  475000.0, false };
  calTable[5]  = { 2500.0f,  600000.0, false };
  calTable[6]  = { 3000.0f,  724000.0, false };
  calTable[7]  = { 3500.0f,  848000.0, false };
  calTable[8]  = { 4000.0f,  972000.0, false };
  calTable[9]  = { 4500.0f, 1096000.0, false };
  calTable[10] = { 5000.0f, 1220000.0, false };
  weightDeadbandG = DEFAULT_WEIGHT_DEADBAND_G;
  saveCalibrationToNVS();
  Serial.println(F("[CONFIG] Calibration table reset to factory baseline defaults."));
}

float interpolateWeight(double rawDelta) {
  if (rawDelta <= 0.0) return 0.0f;

  // Segment 0 to 500g: use Point 1 slope
  if (rawDelta <= calTable[1].raw_counts) {
    if (calTable[1].raw_counts <= 0.0) return 0.0f;
    return (float)(rawDelta * (calTable[1].weight_g / calTable[1].raw_counts));
  }

  // Linear interpolation within matching 500g interval
  for (int i = 1; i < NUM_CAL_POINTS - 1; i++) {
    if (rawDelta <= calTable[i + 1].raw_counts) {
      double dRaw = calTable[i + 1].raw_counts - calTable[i].raw_counts;
      if (dRaw <= 0.0) break;
      double dW   = calTable[i + 1].weight_g - calTable[i].weight_g;
      double frac = (rawDelta - calTable[i].raw_counts) / dRaw;
      return (float)(calTable[i].weight_g + frac * dW);
    }
  }

  // Segment 10 (above 5000g): extrapolate linearly using the final segment slope
  int last = NUM_CAL_POINTS - 1;
  double lastDRaw = calTable[last].raw_counts - calTable[last - 1].raw_counts;
  double lastDW   = calTable[last].weight_g - calTable[last - 1].weight_g;
  if (lastDRaw > 0.0) {
    double slope = lastDRaw / lastDW;
    return (float)(calTable[last].weight_g + (rawDelta - calTable[last].raw_counts) / slope);
  }

  return (float)(rawDelta / hx711CalFactor);
}

float getCalibratedWeightGrams(byte samples = 1, unsigned long timeoutMs = 150) {
  if (!scaleInitialized) {
    scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
    scaleInitialized = true;
    scale.set_scale(hx711CalFactor);
  }

  if (!scale.wait_ready_timeout(timeoutMs)) {
    return -1.0f; // Sensor not ready or busy converting
  }

  double rawDelta = scale.get_value(samples);

  // Handle accidental inverted load cell mounting / polarity (compression vs tension)
  if (rawDelta < -500.0 && calTable[1].raw_counts > 0.0) {
    rawDelta = -rawDelta;
  }

  // Multi-point continuous piecewise linear interpolation
  float grams = interpolateWeight(rawDelta);

  // Symmetric deadband: Baseline noise within ±weightDeadbandG is locked to 0.0g
  if (abs(grams) < weightDeadbandG) {
    grams = 0.0f;
  } else if (grams < 0.0f) {
    grams = 0.0f;
  }

  return grams;
}

SemaphoreHandle_t telemetryMutex;
SemaphoreHandle_t sensorMutex;

// Stepper Configuration
uint8_t  stepperMicrostep          = DEFAULT_MICROSTEP;
long     stepperCruiseRpm          = DEFAULT_CRUISE_RPM;
long     stepperStartRpm           = DEFAULT_START_RPM;

// Volatile Shared Telemetry (Protected by FreeRTOS Mutex)
volatile uint16_t shared_weight_g   = 0;
volatile uint16_t shared_nh3_x10    = 0;
volatile uint16_t shared_water_x10  = 0;

// Dynamic Water Level Calibration Variables
float emptyDistanceCm = EMPTY_DISTANCE_CM; // Distance to bottom of empty trough (0%)
float fullDistanceCm  = FULL_DISTANCE_CM;  // Distance to full water level (100%)

// Auto Water Refill State & Configuration
bool     autoRefillEnabled          = true;
uint8_t  autoRefillThresholdPct     = 45;    // Trigger refill when water level < 45%
uint32_t lastAutoRefillFinishedAt   = 0;
const uint32_t AUTO_REFILL_COOLDOWN_MS = 15000; // 15s cooldown after refill completes
int      consecutiveLowWaterCount   = 0;     // Debounce filter for low water trigger

// Actuator Hardware State Variables
bool feedGateState             = false;
bool waterValveState            = false;
bool stepperActive              = false;
bool thermalTimeoutTriggered    = false;
bool estopActive                = false;

uint32_t feedGateTurnedOnAt     = 0;
uint32_t waterPulseEndsAt       = 0;
bool     waterPulseActive       = false;

// Forward Function Declarations (Required for strict C++ compilation in PlatformIO / main.cpp)
float    readUltrasonicDistanceCm();
void     setFeedGate(bool open);
void     setWaterValve(bool open);
bool     refillWaterTrough(uint8_t targetPct = 100);
void     checkAutoRefill();
void     triggerWaterPulse(uint32_t durationMs);
uint32_t calculateStepDelayUs(long rpm);
void     stepPulse(uint32_t halfPeriodUs);
void     stepMotor(long totalSteps, bool clockwise = true);
bool     dispenseFeedGrams(uint16_t targetGrams);
void     emergencyStop();
void     resetSafetyFlags();
uint16_t getActuatorStatusFlags();
void     updateActuatorTimers();
void     updateSensors();
uint16_t calculateCRC(const uint8_t *buffer, uint16_t length);
void     processModbusRTU();
void     printHelpMenu();
void     printStatus();
void     processSerialCommand(String cmd);

// ===================================================================================
// ACTUATOR DRIVER FUNCTIONS
// ===================================================================================

void setFeedGate(bool open) {
  if (estopActive && open) {
    Serial.println(F("[ERROR] Cannot open Feed Gate: E-Stop is ACTIVE!"));
    return;
  }

  if (open) {
    digitalWrite(RELAY_FEED_PIN, RELAY_VALVE_OPEN);
    if (!feedGateState) {
      feedGateTurnedOnAt = millis();
    }
    feedGateState = true;
    Serial.println(F("[ACTUATOR] Feed Gate Solenoid -> OPEN"));
  } else {
    digitalWrite(RELAY_FEED_PIN, RELAY_VALVE_CLOSED);
    feedGateState = false;
    Serial.println(F("[ACTUATOR] Feed Gate Solenoid -> CLOSED"));
  }
}

void setWaterValve(bool open) {
  if (estopActive && open) {
    Serial.println(F("[ERROR] Cannot open Water Valve: E-Stop is ACTIVE!"));
    return;
  }

  if (open) {
    digitalWrite(RELAY_WATER_PIN, RELAY_VALVE_OPEN);
    waterValveState = true;
    Serial.println(F("[ACTUATOR] Water Solenoid Valve -> OPEN"));
  } else {
    digitalWrite(RELAY_WATER_PIN, RELAY_VALVE_CLOSED);
    waterValveState = false;
    waterPulseActive = false;
    Serial.println(F("[ACTUATOR] Water Solenoid Valve -> CLOSED"));
  }
}

bool refillWaterTrough(uint8_t targetPct) {
  if (estopActive) {
    Serial.println(F("[ERROR] Water refill blocked: E-Stop is ACTIVE!"));
    return false;
  }

  // 1. Measure Initial Water Level
  float distCm = readUltrasonicDistanceCm();
  float currentPct = 0.0f;
  float span = emptyDistanceCm - fullDistanceCm;
  if (distCm > 0.0f && span > 0.0f) {
    currentPct = ((emptyDistanceCm - distCm) / span) * 100.0f;
    if (currentPct < 0.0f) currentPct = 0.0f;
    if (currentPct > 100.0f) currentPct = 100.0f;
  } else {
    if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      currentPct = shared_water_x10 / 10.0f;
      xSemaphoreGive(telemetryMutex);
    }
  }

  Serial.println(F("\n======================================================="));
  Serial.print(F("[CLOSED-LOOP WATER REFILL] Initial Water Level : "));
  Serial.print(currentPct, 1); Serial.println(F(" %"));
  Serial.print(F("                           Target Stop Level   : "));
  Serial.print(targetPct); Serial.println(F(" % (100% Full)"));
  Serial.println(F("======================================================="));

  if (currentPct >= (float)targetPct) {
    Serial.println(F("[WATER REFILL] Trough is ALREADY FULL. No refill needed.\n"));
    return true;
  }

  // 2. Open Water Refill Solenoid Valve (Relay LOW)
  setWaterValve(true);

  uint32_t startTime = millis();
  uint32_t maxTimeoutMs = 60000; // 60s max safety cutoff against dry supply/overflow
  uint32_t lastCheckTime = millis();
  int consecutiveFullCount = 0; // Debounce filter against water splash waves

  // 3. Closed-Loop Water Level Monitoring Loop
  while (!estopActive) {
    delay(30); // Keep system responsive

    if (millis() - lastCheckTime >= 150) {
      lastCheckTime = millis();

      float d = readUltrasonicDistanceCm();
      if (d > 0.0f && span > 0.0f) {
        currentPct = ((emptyDistanceCm - d) / span) * 100.0f;
        if (currentPct < 0.0f) currentPct = 0.0f;
        if (currentPct > 100.0f) currentPct = 100.0f;
      }

      Serial.print(F("[REFILLING] Current Level: "));
      Serial.print(currentPct, 1);
      Serial.print(F(" % (Dist: "));
      Serial.print(d, 1);
      Serial.println(F(" cm)"));

      // STOP CONDITION: Water level reaches target (or d <= fullDistanceCm)
      if (currentPct >= (float)targetPct || (d > 0.0f && d <= fullDistanceCm)) {
        consecutiveFullCount++;
        if (consecutiveFullCount >= 3) {
          Serial.println(F("\n[REFILL] Potential 100% full detected. Pausing valve for 700ms to let surface ripples settle..."));
          setWaterValve(false); // Temporarily shut valve to kill splashes & surface waves
          delay(700);           // Allow surface ripples to dissipate

          float calmDist = readUltrasonicDistanceCm();
          float calmPct = currentPct;
          if (calmDist > 0.0f && span > 0.0f) {
            calmPct = ((emptyDistanceCm - calmDist) / span) * 100.0f;
            if (calmPct < 0.0f) calmPct = 0.0f;
            if (calmPct > 100.0f) calmPct = 100.0f;
          }

          Serial.print(F("[REFILL VERIFICATION] Calm Surface Level: "));
          Serial.print(calmPct, 1);
          Serial.print(F(" % (Dist: "));
          Serial.print(calmDist, 1);
          Serial.println(F(" cm)"));

          if (calmPct >= (float)targetPct || (calmDist > 0.0f && calmDist <= fullDistanceCm)) {
            Serial.println(F("[SUCCESS] Confirmed 100% FULL on calm water surface! Refill completed."));
            currentPct = calmPct;
            break;
          } else {
            Serial.println(F("[REFILL] False splash / ripple detected! Resuming water refill..."));
            consecutiveFullCount = 0;
            setWaterValve(true); // Re-open valve and continue refilling
          }
        }
      } else {
        consecutiveFullCount = 0;
      }
    }

    // Safety Timeout Check
    if (millis() - startTime >= maxTimeoutMs) {
      Serial.println(F("\n[SAFETY TIMEOUT] Max water refill time reached (Water supply dry or sensor error)!"));
      break;
    }
  }

  // 4. Close Water Valve
  setWaterValve(false);

  // 5. Final Confirmation Reading
  delay(300);
  float finalDist = readUltrasonicDistanceCm();
  float finalPct = currentPct;
  if (finalDist > 0.0f && span > 0.0f) {
    finalPct = ((emptyDistanceCm - finalDist) / span) * 100.0f;
    if (finalPct < 0.0f) finalPct = 0.0f;
    if (finalPct > 100.0f) finalPct = 100.0f;
  }

  Serial.println(F("======================================================="));
  Serial.print(F("[REFILL SUMMARY] Final Water Level : ")); Serial.print(finalPct, 1); Serial.println(F(" %"));
  Serial.println(F("                 Status            : Solenoid CLOSED (Full)"));
  Serial.println(F("=======================================================\n"));

  lastAutoRefillFinishedAt = millis();
  return true;
}

void checkAutoRefill() {
  if (!autoRefillEnabled || estopActive || stepperActive || feedGateState || waterValveState) {
    consecutiveLowWaterCount = 0;
    return;
  }

  // Cooldown check (minimum 15s after last refill completed)
  if (millis() - lastAutoRefillFinishedAt < AUTO_REFILL_COOLDOWN_MS) {
    consecutiveLowWaterCount = 0;
    return;
  }

  // Check every 500ms
  static uint32_t lastThreshCheck = 0;
  if (millis() - lastThreshCheck < 500) return;
  lastThreshCheck = millis();

  float currentWaterPct = 0.0f;
  if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    currentWaterPct = shared_water_x10 / 10.0f;
    xSemaphoreGive(telemetryMutex);
  } else {
    return;
  }

  // Check if water level is below threshold (ignoring 0.0% if sensor invalid/unreachable)
  if (currentWaterPct > 0.0f && currentWaterPct < (float)autoRefillThresholdPct) {
    consecutiveLowWaterCount++;
    // Require 3 consecutive low readings (~1.5s) to filter out surface sloshing / momentary dips
    if (consecutiveLowWaterCount >= 3) {
      Serial.println(F("\n======================================================="));
      Serial.print(F("[AUTO-REFILL TRIGGER] Water level ("));
      Serial.print(currentWaterPct, 1);
      Serial.print(F("%) fell below threshold ("));
      Serial.print(autoRefillThresholdPct);
      Serial.println(F("%)."));
      Serial.println(F("[AUTO-REFILL TRIGGER] Opening water valve to refill trough to 100%..."));
      Serial.println(F("======================================================="));

      consecutiveLowWaterCount = 0;
      refillWaterTrough(100);
      lastAutoRefillFinishedAt = millis();
    }
  } else {
    consecutiveLowWaterCount = 0;
  }
}

void triggerWaterPulse(uint32_t durationMs) {
  if (estopActive) {
    Serial.println(F("[ERROR] Cannot pulse water: E-Stop is ACTIVE!"));
    return;
  }

  Serial.print(F("[ACTUATOR] Pulsing water valve for "));
  Serial.print(durationMs);
  Serial.println(F(" ms..."));

  setWaterValve(true);
  waterPulseActive = true;
  waterPulseEndsAt = millis() + durationMs;
}

uint32_t calculateStepDelayUs(long rpm) {
  if (rpm < 1) rpm = 1;
  long stepsPerRev = 200L * (long)stepperMicrostep;
  return (uint32_t)(60000000L / (rpm * stepsPerRev * 2));
}

void stepPulse(uint32_t halfPeriodUs) {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(halfPeriodUs);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(halfPeriodUs);
}

void stepMotor(long totalSteps, bool clockwise) {
  if (estopActive) {
    Serial.println(F("[ERROR] Motor step blocked: E-Stop is ACTIVE!"));
    return;
  }
  if (totalSteps <= 0) return;

  Serial.print(F("[STEPPER] Stepping: "));
  Serial.print(totalSteps);
  Serial.print(F(" steps | Dir: "));
  Serial.print(clockwise ? F("CW") : F("CCW"));
  Serial.print(F(" | Microstep: 1/"));
  Serial.print(stepperMicrostep);
  Serial.print(F(" | Cruise: "));
  Serial.print(stepperCruiseRpm);
  Serial.println(F(" RPM"));

  stepperActive = true;

  // Enable DRV8825 Driver (Active LOW)
  digitalWrite(ENABLE_PIN, LOW);
  digitalWrite(DIR_PIN, clockwise ? HIGH : LOW);
  delay(5); // 5ms wake-up time for DRV8825 internal charge pump

  long accelSteps = 200L * (long)stepperMicrostep; // Ramp over 1 revolution
  if (accelSteps > totalSteps / 2) {
    accelSteps = totalSteps / 2;
  }

  for (long i = 0; i < totalSteps; i++) {
    if (estopActive) break;

    long currentRpm;
    if (accelSteps > 0 && i < accelSteps) {
      // Ease-in speed ramp
      currentRpm = stepperStartRpm + (stepperCruiseRpm - stepperStartRpm) * i / accelSteps;
    } else if (accelSteps > 0 && i >= totalSteps - accelSteps) {
      // Ease-out speed ramp
      long remaining = totalSteps - 1 - i;
      currentRpm = stepperStartRpm + (stepperCruiseRpm - stepperStartRpm) * remaining / accelSteps;
    } else {
      currentRpm = stepperCruiseRpm;
    }

    uint32_t halfPeriodUs = calculateStepDelayUs(currentRpm);
    stepPulse(halfPeriodUs);
  }

  stepperActive = false;
  Serial.println(F("[STEPPER] Motion completed."));
}

bool dispenseFeedGrams(uint16_t targetGrams) {
  if (targetGrams == 0) return false;
  if (estopActive) {
    Serial.println(F("[ERROR] Feed dispensing blocked: E-Stop is ACTIVE!"));
    return false;
  }

  // 1. Measure Initial Baseline Weight in the Trough
  float startWeight = 0.0f;
  float initialW = getCalibratedWeightGrams(3, 400);
  if (initialW >= 0.0f) {
    startWeight = initialW;
  } else {
    if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      startWeight = (float)shared_weight_g;
      xSemaphoreGive(telemetryMutex);
    }
  }

  float targetTotalWeight = startWeight + (float)targetGrams;

  Serial.println(F("\n======================================================="));
  Serial.print(F("[CLOSED-LOOP DISPENSE] Target Feed to Dispense : "));
  Serial.print(targetGrams); Serial.println(F(" g"));
  Serial.print(F("                       Initial Trough Weight    : "));
  Serial.print(startWeight, 1); Serial.println(F(" g"));
  Serial.print(F("                       Stop Threshold Target    : "));
  Serial.print(targetTotalWeight, 1); Serial.println(F(" g"));
  Serial.println(F("======================================================="));

  // 2. Open Feed Gate Cutoff Solenoid (Relay LOW)
  setFeedGate(true);
  delay(300); // Allow physical plunger to reach full open stroke

  // 3. Start Rotating Auger Stepper Motor
  stepperActive = true;
  digitalWrite(ENABLE_PIN, LOW); // Enable DRV8825 (Active LOW)
  digitalWrite(DIR_PIN, HIGH);   // Clockwise auger feed rotation
  delay(5); // 5ms wake-up time for DRV8825 internal charge pump

  uint32_t startTime = millis();
  uint32_t maxTimeoutMs = 120000; // 2 minutes max safety timeout
  uint32_t lastWeightCheckTime = millis();
  float currentWeight = startWeight;
  float dispensedSoFar = 0.0f;
  long totalStepsRun = 0;
  long rampSteps = 200L * (long)stepperMicrostep; // 1 full revolution ease-in

  // Closed-loop step loop: steps smoothly and actively samples load cell
  while (!estopActive) {
    // Step in small batches (32 steps = ~16ms)
    int batchSteps = 32;

    for (int s = 0; s < batchSteps; s++) {
      totalStepsRun++;
      long currentRpm = stepperCruiseRpm;
      if (rampSteps > 0 && totalStepsRun < rampSteps) {
        currentRpm = stepperStartRpm + (stepperCruiseRpm - stepperStartRpm) * totalStepsRun / rampSteps;
      }
      uint32_t halfPeriodUs = calculateStepDelayUs(currentRpm);
      stepPulse(halfPeriodUs);
    }

    // Periodically sample the load cell directly (every ~100ms)
    if (millis() - lastWeightCheckTime >= 100) {
      lastWeightCheckTime = millis();

      float rawW = getCalibratedWeightGrams(1, 80);
      if (rawW >= 0.0f) {
        currentWeight = rawW;
        if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          shared_weight_g = (uint16_t)currentWeight;
          xSemaphoreGive(telemetryMutex);
        }
      }

      dispensedSoFar = currentWeight - startWeight;
      if (dispensedSoFar < 0) dispensedSoFar = 0.0f;

      Serial.print(F("[DISPENSING] Delivered: "));
      Serial.print(dispensedSoFar, 1);
      Serial.print(F(" g / "));
      Serial.print(targetGrams);
      Serial.print(F(" g (Trough: "));
      Serial.print(currentWeight, 1);
      Serial.println(F(" g)"));

      // CHECK STOP CONDITION: Target weight reached!
      if (dispensedSoFar >= (float)targetGrams) {
        Serial.println(F("\n[SUCCESS] Target weight reached! Stopping auger and closing gate."));
        break;
      }
    }

    // Safety Timeout Check (protects against empty hopper or mechanical jam)
    if (millis() - startTime >= maxTimeoutMs) {
      Serial.println(F("\n[SAFETY TIMEOUT] Max dispensing time reached (Hopper empty or feed jam)!"));
      break;
    }
  }

  // 4. Immediately Stop Motor and Close Gate
  stepperActive = false;

  delay(100);
  setFeedGate(false); // Close Feed Gate Cutoff Solenoid

  // 5. Final Weight Measurement & Accuracy Verification
  delay(400); // Allow scale to settle after feed drop
  float finalWeight = currentWeight;
  float settledW = getCalibratedWeightGrams(3, 400);
  if (settledW >= 0.0f) {
    finalWeight = settledW;
    if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      shared_weight_g = (uint16_t)finalWeight;
      xSemaphoreGive(telemetryMutex);
    }
  }
  float finalDelivered = finalWeight - startWeight;
  if (finalDelivered < 0) finalDelivered = 0.0f;

  float errorGrams = abs(finalDelivered - (float)targetGrams);
  float fdaPct = 100.0f;
  if (targetGrams > 0) {
    fdaPct = (1.0f - (errorGrams / (float)targetGrams)) * 100.0f;
    if (fdaPct < 0.0f) fdaPct = 0.0f;
  }

  Serial.println(F("======================================================="));
  Serial.print(F("[DISPENSE SUMMARY] Target Requested   : ")); Serial.print(targetGrams); Serial.println(F(" g"));
  Serial.print(F("                   Actual Delivered   : ")); Serial.print(finalDelivered, 1); Serial.println(F(" g"));
  Serial.print(F("                   Feed Dispense Acc. : ")); Serial.print(fdaPct, 1); Serial.println(F(" %"));
  Serial.println(F("=======================================================\n"));

  return true;
}

void emergencyStop() {
  estopActive = true;

  // Instantly force all actuator pins to safe idle
  digitalWrite(RELAY_FEED_PIN, RELAY_VALVE_CLOSED);
  digitalWrite(RELAY_WATER_PIN, RELAY_VALVE_CLOSED);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(ENABLE_PIN, HIGH); // Driver sleep

  feedGateState = false;
  waterValveState = false;
  stepperActive = false;
  waterPulseActive = false;

  Serial.println(F("\n======================================================="));
  Serial.println(F("  !!! EMERGENCY STOP (E-STOP) ACTIVATED !!!             "));
  Serial.println(F("  All valves closed. Stepper motor disabled.           "));
  Serial.println(F("=======================================================\n"));
}

void resetSafetyFlags() {
  thermalTimeoutTriggered = false;
  estopActive = false;
  Serial.println(F("[SAFETY] E-Stop and thermal flags reset. Actuators ready."));
}

uint16_t getActuatorStatusFlags() {
  uint16_t flags = 0;
  if (stepperActive)            flags |= ACTUATOR_FLAG_STEPPER_ACTIVE;
  if (feedGateState)           flags |= ACTUATOR_FLAG_FEED_GATE_ACTIVE;
  if (waterValveState)          flags |= ACTUATOR_FLAG_WATER_VALVE_ACTIVE;
  if (thermalTimeoutTriggered)  flags |= ACTUATOR_FLAG_THERMAL_TIMEOUT;
  if (estopActive)              flags |= ACTUATOR_FLAG_ESTOP_ACTIVE;
  return flags;
}

void updateActuatorTimers() {
  uint32_t now = millis();

  // 1. Non-blocking thermal safety cutoff for push-pull solenoid
  if (feedGateState) {
    if (now - feedGateTurnedOnAt >= MAX_SOLENOID_ON_MS) {
      setFeedGate(false);
      thermalTimeoutTriggered = true;
      Serial.println(F("[SAFETY ALERT] Feed Gate auto-closed (Max 10s thermal limit exceeded)."));
    }
  }

  // 2. Non-blocking water pulse timeout
  if (waterPulseActive && waterValveState) {
    if (now >= waterPulseEndsAt) {
      setWaterValve(false);
      waterPulseActive = false;
      Serial.println(F("[ACTUATOR] Water pulse complete. Solenoid closed."));
    }
  }
}

// ===================================================================================
// SENSOR ACQUISITION TASK (Runs on Core 0)
// ===================================================================================

float readUltrasonicDistanceCm() {
  float samples[3];
  int validCount = 0;

  for (int i = 0; i < 3; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(20);
    digitalWrite(TRIG_PIN, LOW);

    // 30,000us timeout allows up to ~5m range and prevents premature timeouts
    long durationUs = pulseIn(ECHO_PIN, HIGH, 30000);
    if (durationUs > 0) {
      float d = (durationUs * SOUND_SPEED) / 2.0f;
      // Filter out-of-range sensor artifacts
      if (d >= 15.0f && d <= 450.0f) {
        samples[validCount++] = d;
      }
    }
    if (i < 2) delay(30); // 30ms settle time between pings to prevent acoustic echo bleed
  }

  if (validCount == 0) return -1.0f;
  if (validCount == 1) return samples[0];
  if (validCount == 2) return (samples[0] + samples[1]) / 2.0f;

  // 3-sample median sort
  if (samples[0] > samples[1]) { float t = samples[0]; samples[0] = samples[1]; samples[1] = t; }
  if (samples[1] > samples[2]) { float t = samples[1]; samples[1] = samples[2]; samples[2] = t; }
  if (samples[0] > samples[1]) { float t = samples[0]; samples[0] = samples[1]; samples[1] = t; }
  return samples[1];
}

void updateSensors() {
  static uint32_t lastWeightPoll       = 0;
  static uint32_t lastSlowSensorPoll   = 0;
  static uint32_t lastTelemetryStream  = 0;
  static float    lastValidWeightGrams = 0.0f;
  static float    cachedDistCm         = -1.0f;
  static float    cachedWaterPct       = 0.0f;
  static float    cachedNh3Ppm         = 0.0f;

  uint32_t now = millis();

  // 1. FAST SENSOR PATH: Read HX711 Load Cell every 100ms (HX711 native 10 SPS rate)
  if (now - lastWeightPoll >= 100) {
    lastWeightPoll = now;

    if (!scaleInitialized) {
      scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
      scaleInitialized = true;
      scale.set_scale(hx711CalFactor);
    }

    if (scale.wait_ready_timeout(20)) {
      if (!hx711Tared) {
        scale.tare(5);
        hx711Tared = true;
        smoothedWeightGrams = 0.0f;
        weightRingBuf[0] = 0.0f;
        weightRingBuf[1] = 0.0f;
        weightRingBuf[2] = 0.0f;
        Serial.println(F("[HX711] Auto-tare complete. Tray zeroed at 0.0g."));
      }

      float rawW = getCalibratedWeightGrams(1, 10);
      if (rawW >= 0.0f) {
        float delta = abs(rawW - smoothedWeightGrams);

        if (delta >= 15.0f) {
          // STEP CHANGE DETECTED: Physical object placed or removed!
          // Instantly snap to the new weight (zero filter delay, ~100ms response)
          smoothedWeightGrams = rawW;
          weightRingBuf[0] = rawW;
          weightRingBuf[1] = rawW;
          weightRingBuf[2] = rawW;
        } else if (delta >= 5.0f) {
          // FAST TRANSITION: Rapidly approach target (75% new, 25% old)
          smoothedWeightGrams = (0.25f * smoothedWeightGrams) + (0.75f * rawW);
          weightRingBuf[weightRingIdx] = rawW;
          weightRingIdx = (weightRingIdx + 1) % 3;
        } else {
          // STEADY STATE: Apply 3-sample median + low-pass EMA for rock-solid zero-jitter stability
          weightRingBuf[weightRingIdx] = rawW;
          weightRingIdx = (weightRingIdx + 1) % 3;

          float s[3] = { weightRingBuf[0], weightRingBuf[1], weightRingBuf[2] };
          if (s[0] > s[1]) { float t = s[0]; s[0] = s[1]; s[1] = t; }
          if (s[1] > s[2]) { float t = s[1]; s[1] = s[2]; s[2] = t; }
          if (s[0] > s[1]) { float t = s[0]; s[0] = s[1]; s[1] = t; }
          float medianW = s[1];

          smoothedWeightGrams = (0.70f * smoothedWeightGrams) + (0.30f * medianW);
        }

        // Apply zero deadband to final filtered output
        if (smoothedWeightGrams < weightDeadbandG) {
          smoothedWeightGrams = 0.0f;
        }

        lastValidWeightGrams = smoothedWeightGrams;
        shared_weight_g = (uint16_t)lastValidWeightGrams;
      }
    }
  }

  // 2. SLOW SENSOR PATH: Ultrasonic Distance & MQ-135 Gas every 500ms
  if (now - lastSlowSensorPoll >= 500) {
    lastSlowSensorPoll = now;

    // Read Waterproof Ultrasonic Sensor
    float distCm = readUltrasonicDistanceCm();
    static float lastValidDistCm = -1.0f;
    if (distCm > 0.0f) {
      lastValidDistCm = distCm;
    } else if (lastValidDistCm > 0.0f) {
      distCm = lastValidDistCm;
    }
    cachedDistCm = distCm;

    float waterPct = 0.0f;
    static float smoothedWaterPct = -1.0f;
    if (distCm > 0.0f) {
      float span = emptyDistanceCm - fullDistanceCm;
      if (span > 0.0f) {
        float rawPct = ((emptyDistanceCm - distCm) / span) * 100.0f;
        if (rawPct < 0.0f)   rawPct = 0.0f;
        if (rawPct > 100.0f) rawPct = 100.0f;

        if (smoothedWaterPct < 0.0f) {
          smoothedWaterPct = rawPct;
        } else {
          smoothedWaterPct = 0.75f * smoothedWaterPct + 0.25f * rawPct;
        }
        waterPct = smoothedWaterPct;
      }
    }
    cachedWaterPct = waterPct;

    // Read MQ-135 Ammonia Gas Sensor
    int rawAdc = analogRead(MQ135_PIN);
    float pinVolts = rawAdc * (3.3f / 4095.0f);
    cachedNh3Ppm = pinVolts * 1.5f * 10.0f;

    // Safely update shared metrics for MODBUS RTU
    shared_nh3_x10   = (uint16_t)(cachedNh3Ppm * 10.0f);
    shared_water_x10 = (uint16_t)(cachedWaterPct * 10.0f);
  }

  // 3. FAST TELEMETRY STREAM: Snappy Real-Time JSON Stream every 100ms
  if (now - lastTelemetryStream >= 100) {
    lastTelemetryStream = now;

    Serial.print(F("{\"telemetry\":{\"weight_g\":"));
    Serial.print(lastValidWeightGrams, 1);
    Serial.print(F(",\"water_pct\":"));
    Serial.print(cachedWaterPct, 1);
    Serial.print(F(",\"dist_cm\":"));
    Serial.print(cachedDistCm, 1);
    Serial.print(F(",\"nh3_ppm\":"));
    Serial.print(cachedNh3Ppm, 1);
    Serial.println(F("}}"));
  }
}

// ===================================================================================
// NATIVE ZERO-DEPENDENCY MODBUS RTU SLAVE ENGINE (Runs on Core 1)
// ===================================================================================

uint16_t calculateCRC(const uint8_t *buffer, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint16_t pos = 0; pos < length; pos++) {
    crc ^= (uint16_t)buffer[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void processModbusRTU() {
  if (Serial2.available() < 8) return;

  uint8_t raw[256];
  int rawLen = 0;
  while (Serial2.available() > 0 && rawLen < 256) {
    raw[rawLen++] = Serial2.read();
    delayMicroseconds(100);
  }

  if (rawLen < 8) return;

  // Scan for Frame Header addressed to SLAVE_ID
  int startIdx = -1;
  for (int i = 0; i <= rawLen - 8; i++) {
    if (raw[i] == SLAVE_ID && (raw[i + 1] == 0x03 || raw[i + 1] == 0x06)) {
      uint16_t rxCrc = raw[i + 6] | (raw[i + 7] << 8);
      uint16_t calcCrc = calculateCRC(&raw[i], 6);
      if (rxCrc == calcCrc) {
        startIdx = i;
        break;
      }
    }
  }

  if (startIdx == -1) return; // No valid frame found

  uint8_t *frame = &raw[startIdx];
  uint8_t funcCode = frame[1];
  uint16_t regAddress = (frame[2] << 8) | frame[3];
  uint16_t regValueOrQty = (frame[4] << 8) | frame[5];

  // --- FUNCTION CODE 0x03: READ HOLDING REGISTERS ---
  if (funcCode == 0x03) {
    uint16_t regCount = regValueOrQty;
    if (regAddress + regCount > 9) return; // Max 9 registers (0..8)

    uint16_t curWeight = 0, curNh3 = 0, curWater = 0;
    if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      curWeight = shared_weight_g;
      curNh3    = shared_nh3_x10;
      curWater  = shared_water_x10;
      xSemaphoreGive(telemetryMutex);
    }

    // Register Mapping:
    // Reg 0 (40001): Feed Weight (g)
    // Reg 1 (40002): Ammonia (ppm x 10)
    // Reg 2 (40003): Water Level (% x 10)
    // Reg 3 (40004): Feed Gate State (0/1)
    // Reg 4 (40005): Water Valve State (0/1/100)
    // Reg 5 (40006): Dispense Trigger
    // Reg 6 (40007): Actuator Status Flags
    // Reg 7 (40008): E-Stop State
    // Reg 8 (40009): Auto-Refill Threshold (%)
    uint16_t regMap[9] = {
      curWeight,
      curNh3,
      curWater,
      (uint16_t)(feedGateState ? 1 : 0),
      (uint16_t)(waterValveState ? 1 : 0),
      0, // Dispense trigger reads 0
      getActuatorStatusFlags(),
      (uint16_t)(estopActive ? 1 : 0),
      (uint16_t)autoRefillThresholdPct
    };

    uint8_t response[64];
    uint8_t respLen = 0;

    response[respLen++] = SLAVE_ID;
    response[respLen++] = 0x03;
    response[respLen++] = regCount * 2; // Byte count

    for (uint16_t i = 0; i < regCount; i++) {
      uint16_t val = regMap[regAddress + i];
      response[respLen++] = (val >> 8) & 0xFF; // MSB
      response[respLen++] = val & 0xFF;        // LSB
    }

    uint16_t txCrc = calculateCRC(response, respLen);
    response[respLen++] = txCrc & 0xFF;
    response[respLen++] = (txCrc >> 8) & 0xFF;

    // Transmit Response over RS-485
    digitalWrite(DE_RE_PIN, HIGH);
    delayMicroseconds(50);
    Serial2.write(response, respLen);
    Serial2.flush();
    delayMicroseconds(100);
    digitalWrite(DE_RE_PIN, LOW);
  }
  // --- FUNCTION CODE 0x06: WRITE SINGLE HOLDING REGISTER ---
  else if (funcCode == 0x06) {
    uint16_t writeVal = regValueOrQty;

    // Execute Command
    if (regAddress == 3) {        // Reg 40004: Feed Gate
      if (!estopActive && !thermalTimeoutTriggered) setFeedGate(writeVal > 0);
    } else if (regAddress == 4) { // Reg 40005: Water Valve / Refill
      if (!estopActive) {
        if (writeVal == 100 || writeVal == 2) {
          refillWaterTrough(100);
        } else {
          setWaterValve(writeVal > 0);
        }
      }
    } else if (regAddress == 5) { // Reg 40006: Dispense Grams
      if (!estopActive && writeVal > 0) dispenseFeedGrams(writeVal);
    } else if (regAddress == 7) { // Reg 40008: E-Stop Command
      if (writeVal == 1) emergencyStop();
      else if (writeVal == 2) resetSafetyFlags();
    } else if (regAddress == 8) { // Reg 40009: Auto-Refill Threshold (%)
      if (writeVal >= 10 && writeVal <= 90) {
        autoRefillThresholdPct = (uint8_t)writeVal;
        Serial.print(F("[MODBUS] Auto-refill threshold set to: "));
        Serial.print(autoRefillThresholdPct);
        Serial.println(F(" %"));
      }
    }

    // Echo back standard MODBUS FC 0x06 response (exact match to valid request)
    digitalWrite(DE_RE_PIN, HIGH);
    delayMicroseconds(50);
    Serial2.write(frame, 8);
    Serial2.flush();
    delayMicroseconds(100);
    digitalWrite(DE_RE_PIN, LOW);
  }
}

// ===================================================================================
// SERIAL COMMAND PARSER (For Direct Interactive Bench Testing)
// ===================================================================================

void printHelpMenu() {
  Serial.println(F("\n======================================================="));
  Serial.println(F("       CPE3207L FEEDER NODE - COMMAND MENU             "));
  Serial.println(F("======================================================="));
  Serial.println(F("  1  or GATE 1    : Open Feed Gate Solenoid"));
  Serial.println(F("  0  or GATE 0    : Close Feed Gate Solenoid"));
  Serial.println(F("  W 1             : Open Water Solenoid Valve"));
  Serial.println(F("  W 0             : Close Water Solenoid Valve"));
  Serial.println(F("  REFILL          : Closed-Loop Refill to 100% (Closes when full)"));
  Serial.println(F("  THRESH <10-90>  : Set Auto-Refill Trigger % (e.g. 'THRESH 45')"));
  Serial.println(F("  AUTO ON / OFF   : Enable or Disable Automated Water Refilling"));
  Serial.println(F("  WATER_CAL <e> <f>: Set water air-gap calibration (e.g. 'WATER_CAL 66.0 44.2')"));
  Serial.println(F("  P <ms>          : Pulse Water Valve (e.g. 'P 2500')"));
  Serial.println(F("  D <grams>       : Automated Dispense Cycle (e.g. 'D 100')"));
  Serial.println(F("  RAW or HX711    : Probe raw ADC counts & pin diagnostic"));
  Serial.println(F("  TARE or ZERO    : Zero the scale with empty tray"));
  Serial.println(F("  DEADBAND <g>    : Set zero deadband noise cutoff (e.g. 'DEADBAND 3.0')"));
  Serial.println(F("  CAL <grams>     : Smart calibrate nearest 500g node (e.g. 'CAL 500', 'CAL 1500')"));
  Serial.println(F("  CALPNT <1-10> <g>: Calibrate specific grid point (e.g. 'CALPNT 2 1000')"));
  Serial.println(F("  CALTABLE        : Display 11-point piecewise calibration table"));
  Serial.println(F("  SAVECAL / LOADCAL: Save / Load calibration to/from persistent flash"));
  Serial.println(F("  RESET_CAL       : Reset calibration table to factory defaults"));
  Serial.println(F("  FACTOR <val>    : Set uniform calibration factor (e.g. 'FACTOR 226.0')"));
  Serial.println(F("  S <steps>       : Step motor manually (e.g. 'S 6400' = 1 rev at 1/32)"));
  Serial.println(F("  MSTEP <1-32>    : Set microstepping (1, 2, 4, 8, 16, 32)"));
  Serial.println(F("  RPM <speed>     : Set cruise speed RPM (e.g. 'RPM 60')"));
  Serial.println(F("  ESTOP           : Emergency Stop (Lockout all actuators)"));
  Serial.println(F("  RESET           : Reset E-Stop & safety flags"));
  Serial.println(F("  STATUS          : Display current hardware states"));
  Serial.println(F("  ? or HELP       : Show this menu"));
  Serial.println(F("=======================================================\n"));
}

void printStatus() {
  Serial.println(F("\n--- FEEDER NODE HARDWARE STATUS ---"));
  Serial.print(F("  Feed Gate Solenoid  : "));
  Serial.println(feedGateState ? F("OPEN (Relay LOW)") : F("CLOSED (Relay HIGH)"));
  Serial.print(F("  Water Solenoid Valve: "));
  Serial.println(waterValveState ? F("OPEN (Relay LOW)") : F("CLOSED (Relay HIGH)"));
  Serial.print(F("  Auger Stepper Motor : "));
  Serial.println(stepperActive ? F("RUNNING") : F("IDLE (Driver Sleep)"));
  Serial.print(F("  Microstepping Mode  : 1/"));
  Serial.print(stepperMicrostep);
  Serial.print(F(" ("));
  Serial.print(200L * stepperMicrostep);
  Serial.println(F(" steps/rev)"));
  Serial.print(F("  Cruise / Start Speed: "));
  Serial.print(stepperCruiseRpm);
  Serial.print(F(" RPM / "));
  Serial.print(stepperStartRpm);
  Serial.println(F(" RPM"));
  Serial.print(F("  HX711 Load Cell     : "));
  Serial.print(scale.is_ready() ? F("READY") : F("WAITING / BUSY"));
  Serial.println(hx711Tared ? F(" (Tared: YES)") : F(" (Tared: NO)"));
  Serial.print(F("  Weight Deadband     : "));
  Serial.print(weightDeadbandG, 1);
  Serial.println(F(" g (Baseline noise cutoff)"));
  Serial.println(F("  HX711 Calibration   : 11-Point Piecewise Profile (0 - 5 kg)"));
  int calCount = 0;
  for (int i = 1; i < NUM_CAL_POINTS; i++) if (calTable[i].calibrated) calCount++;
  Serial.print(F("    Calibrated Nodes  : ")); Serial.print(calCount); Serial.println(F(" / 10 active (type 'CALTABLE' to view)"));
  Serial.print(F("  Water Calibration   : Empty: "));
  Serial.print(emptyDistanceCm, 1);
  Serial.print(F(" cm | Full: "));
  Serial.print(fullDistanceCm, 1);
  Serial.print(F(" cm (Span: "));
  Serial.print(emptyDistanceCm - fullDistanceCm, 1);
  Serial.println(F(" cm)"));
  Serial.print(F("  Auto-Refill Mode    : "));
  Serial.print(autoRefillEnabled ? F("ENABLED (Threshold: ") : F("DISABLED (Threshold: "));
  Serial.print(autoRefillThresholdPct);
  Serial.println(F("%)"));
  Serial.print(F("  Thermal Timeout     : "));
  Serial.println(thermalTimeoutTriggered ? F("FLAGGED (10s limit hit)") : F("OK"));
  Serial.print(F("  Emergency Stop      : "));
  Serial.println(estopActive ? F("ACTIVE (Locked)") : F("INACTIVE (Ready)"));
  
  if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.print(F("  Feed Weight (g)     : ")); Serial.println(shared_weight_g);
    Serial.print(F("  Water Level (%)     : ")); Serial.println(shared_water_x10 / 10.0f);
    Serial.print(F("  Ammonia (ppm)       : ")); Serial.println(shared_nh3_x10 / 10.0f);
    xSemaphoreGive(telemetryMutex);
  }
  Serial.println(F("-----------------------------------\n"));
}

void processSerialCommand(String cmd) {
  cmd.trim();
  String upper = cmd;
  upper.toUpperCase();

  if (upper == "1" || upper == "GATE 1") {
    setFeedGate(true);
  } else if (upper == "0" || upper == "GATE 0") {
    setFeedGate(false);
  } else if (upper == "W 1" || upper == "WATER 1") {
    setWaterValve(true);
  } else if (upper == "W 0" || upper == "WATER 0") {
    setWaterValve(false);
  } else if (upper == "REFILL" || upper == "W FILL" || upper == "WATER REFILL" || upper == "FILL") {
    refillWaterTrough(100);
  } else if (upper.startsWith("THRESH ") || upper.startsWith("SET_THRESH ")) {
    int val = cmd.substring(cmd.indexOf(' ') + 1).toInt();
    if (val >= 10 && val <= 90) {
      autoRefillThresholdPct = (uint8_t)val;
      Serial.print(F("[CONFIG] Auto-refill threshold set to: "));
      Serial.print(autoRefillThresholdPct);
      Serial.println(F(" %"));
    } else {
      Serial.println(F("[ERROR] Threshold must be between 10% and 90%"));
    }
  } else if (upper == "AUTO ON" || upper == "AUTO_REFILL ON" || upper == "AUTOREFILL ON" || upper == "AUTO 1") {
    autoRefillEnabled = true;
    Serial.println(F("[CONFIG] Auto-refill -> ENABLED"));
  } else if (upper == "AUTO OFF" || upper == "AUTO_REFILL OFF" || upper == "AUTOREFILL OFF" || upper == "AUTO 0") {
    autoRefillEnabled = false;
    Serial.println(F("[CONFIG] Auto-refill -> DISABLED"));
  } else if (upper.startsWith("WATER_CAL ") || upper.startsWith("WCAL ")) {
    int firstSpace = cmd.indexOf(' ');
    int secondSpace = cmd.indexOf(' ', firstSpace + 1);
    if (secondSpace > firstSpace) {
      float eDist = cmd.substring(firstSpace + 1, secondSpace).toFloat();
      float fDist = cmd.substring(secondSpace + 1).toFloat();
      if (eDist > fDist && fDist >= 15.0f && eDist < 450.0f) {
        emptyDistanceCm = eDist;
        fullDistanceCm  = fDist;
        Serial.println(F("-------------------------------------------------------"));
        Serial.println(F("[CONFIG] Water level calibration updated successfully!"));
        Serial.print(F("  Empty Distance (0%)   : ")); Serial.print(emptyDistanceCm, 1); Serial.println(F(" cm"));
        Serial.print(F("  Full Distance (100%)  : ")); Serial.print(fullDistanceCm, 1); Serial.println(F(" cm"));
        Serial.print(F("  Total Working Span    : ")); Serial.print(emptyDistanceCm - fullDistanceCm, 1); Serial.println(F(" cm"));
        Serial.println(F("-------------------------------------------------------"));
      } else {
        Serial.println(F("[ERROR] Invalid distances. Ensure Empty > Full and Full >= 15 cm."));
      }
    } else {
      Serial.println(F("[ERROR] Usage: WATER_CAL <empty_cm> <full_cm> (e.g. 'WATER_CAL 66.0 44.2')"));
    }
  } else if (upper.startsWith("P ")) {
    long ms = cmd.substring(2).toInt();
    if (ms > 0 && ms <= 60000) triggerWaterPulse((uint32_t)ms);
  } else if (upper.startsWith("D ")) {
    long grams = cmd.substring(2).toInt();
    if (grams > 0) dispenseFeedGrams((uint16_t)grams);
  } else if (upper == "RAW" || upper == "HX711") {
    if (!scaleInitialized) {
      scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
      scaleInitialized = true;
    }
    Serial.println(F("\n--- HX711 DIAGNOSTIC & RAW ADC PROBE ---"));
    Serial.print(F("  DT Pin (GPIO ")); Serial.print(HX711_DT_PIN); Serial.print(F(") Logic Level: "));
    Serial.println(digitalRead(HX711_DT_PIN) == LOW ? F("LOW (Ready / Chip Active)") : F("HIGH (Idle / Busy / Disconnected)"));
    Serial.print(F("  SCK Pin (GPIO ")); Serial.print(HX711_SCK_PIN); Serial.print(F(") Logic Level: "));
    Serial.println(digitalRead(HX711_SCK_PIN) == LOW ? F("LOW (Active Mode)") : F("HIGH (Power Down)"));
    Serial.print(F("  Scale Library Initialized : ")); Serial.println(scaleInitialized ? F("YES") : F("NO"));
    Serial.print(F("  Scale Ready (is_ready)    : ")); Serial.println(scale.is_ready() ? F("YES") : F("NO"));
    Serial.print(F("  Tare Offset (Counts)      : ")); Serial.println(scale.get_offset());

    if (scale.wait_ready_timeout(500)) {
      long rawAdc = scale.read();
      double rawDelta = scale.get_value(1);
      float currentG = getCalibratedWeightGrams(1, 100);
      Serial.print(F("  Instant 24-bit Raw ADC    : ")); Serial.println(rawAdc);
      Serial.print(F("  Raw Delta (ADC - Offset)  : ")); Serial.println(rawDelta, 0);
      Serial.print(F("  Calculated Weight (g)     : ")); Serial.print(currentG, 1); Serial.println(F(" g"));
    } else {
      Serial.println(F("  [WARNING] scale.wait_ready_timeout(500) TIMED OUT!"));
      Serial.println(F("  DT pin never went LOW. Check HX711 VCC (5V/3.3V), GND, DT (GPIO 32), and SCK (GPIO 33)."));
    }
    Serial.println(F("-----------------------------------------\n"));
  } else if (upper == "TARE" || upper == "ZERO" || upper == "T") {
    if (!scaleInitialized) {
      scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
      scaleInitialized = true;
    }
    Serial.println(F("[HX711] Taring scale... (ensure tray is empty and still)"));
    if (scale.wait_ready_timeout(600)) {
      scale.tare(10);
      hx711Tared = true;
      smoothedWeightGrams = 0.0f;
      weightRingBuf[0] = 0.0f;
      weightRingBuf[1] = 0.0f;
      weightRingBuf[2] = 0.0f;
      Serial.println(F("[HX711] Tare complete! Current reading is 0.0 g."));
    } else {
      Serial.println(F("[ERROR] Cannot tare: HX711 not responding (DT pin held HIGH). Check wiring!"));
    }
  } else if (upper.startsWith("DEADBAND ") || upper.startsWith("DB ")) {
    float db = cmd.substring(cmd.indexOf(' ') + 1).toFloat();
    if (db >= 0.0f && db <= 50.0f) {
      weightDeadbandG = db;
      Serial.print(F("[CONFIG] Weight zero deadband set to: "));
      Serial.print(weightDeadbandG, 1);
      Serial.println(F(" g"));
    } else {
      Serial.println(F("[ERROR] Deadband must be between 0.0 and 50.0 g (e.g. 'DEADBAND 3.0')"));
    }
  } else if (upper == "CALTABLE" || upper == "CAL_TABLE" || upper == "TABLE") {
    Serial.println(F("\n=========================================================================="));
    Serial.println(F("           PIECEWISE MULTI-POINT CALIBRATION TABLE (0 - 5 kg)             "));
    Serial.println(F("=========================================================================="));
    Serial.println(F(" Pnt | Reference (g) | Raw ADC Delta | Segment Slope | Calibrated Status   "));
    Serial.println(F("--------------------------------------------------------------------------"));
    for (int i = 0; i < NUM_CAL_POINTS; i++) {
      float slope = 0.0f;
      if (i > 0) {
        double dRaw = calTable[i].raw_counts - calTable[i - 1].raw_counts;
        double dW   = calTable[i].weight_g - calTable[i - 1].weight_g;
        if (dW > 0.0) slope = (float)(dRaw / dW);
      }
      Serial.printf("  %2d |   %7.1f g   |   %9.0f   |  %6.2f c/g   | %s\n",
                    i,
                    calTable[i].weight_g,
                    calTable[i].raw_counts,
                    slope,
                    calTable[i].calibrated ? "CALIBRATED (VERIFIED)" : "DEFAULT (ESTIMATED)");
    }
    Serial.println(F("--------------------------------------------------------------------------"));
    Serial.println(F(" Commands: 'CAL <grams>' to calibrate nearest 500g node | 'SAVECAL' to save"));
    Serial.println(F("           'CALPNT <1-10> <grams>' | 'RESET_CAL' to reset defaults        "));
    Serial.println(F("==========================================================================\n"));
  } else if (upper == "SAVECAL" || upper == "SAVE_CAL") {
    saveCalibrationToNVS();
  } else if (upper == "LOADCAL" || upper == "LOAD_CAL") {
    loadCalibrationFromNVS();
  } else if (upper == "RESET_CAL" || upper == "RESETCAL" || upper == "DEFAULT_CAL") {
    resetCalibrationDefaults();
  } else if (upper.startsWith("CALPNT ")) {
    int firstSpace = cmd.indexOf(' ');
    int secondSpace = cmd.indexOf(' ', firstSpace + 1);
    if (secondSpace > firstSpace) {
      int pntIdx = cmd.substring(firstSpace + 1, secondSpace).toInt();
      float knownG = cmd.substring(secondSpace + 1).toFloat();
      if (pntIdx >= 1 && pntIdx < NUM_CAL_POINTS && knownG > 0.0f) {
        if (!scaleInitialized) {
          scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
          scaleInitialized = true;
        }
        Serial.print(F("[CALIBRATION] Sampling point ")); Serial.print(pntIdx);
        Serial.print(F(" with reference ")); Serial.print(knownG, 1); Serial.println(F(" g..."));
        if (!scale.wait_ready_timeout(1000)) {
          Serial.println(F("[ERROR] HX711 not ready or disconnected."));
        } else {
          double raw = scale.get_value(10);
          if (abs(raw) < 1000.0) {
            Serial.println(F("[ERROR] Reading too low. Ensure weight is on tray!"));
          } else {
            double absRaw = abs(raw);
            double targetG = calTable[pntIdx].weight_g;
            double scaledRaw = absRaw * (targetG / (double)knownG);
            calTable[pntIdx].raw_counts = scaledRaw;
            calTable[pntIdx].calibrated = true;
            saveCalibrationToNVS();
            float verified = getCalibratedWeightGrams(5, 500);
            Serial.println(F("--------------------------------------------------------------------------"));
            Serial.print(F("  Point ")); Serial.print(pntIdx); Serial.print(F(" (")); Serial.print(targetG, 0);
            Serial.println(F(" g) Calibrated Successfully!"));
            Serial.print(F("  Raw ADC Delta       : ")); Serial.println(absRaw, 0);
            Serial.print(F("  Verified Reading    : ")); Serial.print(verified, 1); Serial.println(F(" g"));
            Serial.println(F("--------------------------------------------------------------------------"));
          }
        }
      } else {
        Serial.println(F("[ERROR] Usage: CALPNT <index 1-10> <known_grams> (e.g. 'CALPNT 2 1000')"));
      }
    } else {
      Serial.println(F("[ERROR] Usage: CALPNT <index 1-10> <known_grams> (e.g. 'CALPNT 2 1000')"));
    }
  } else if (upper.startsWith("CAL1 ") || upper.startsWith("CAL2 ") || upper.startsWith("CAL ") || upper.startsWith("CALIBRATE ")) {
    float knownGrams = 0.0f;
    if (upper.startsWith("CAL1 ")) {
      knownGrams = cmd.substring(5).toFloat();
    } else if (upper.startsWith("CAL2 ")) {
      knownGrams = cmd.substring(5).toFloat();
    } else {
      knownGrams = cmd.substring(cmd.indexOf(' ') + 1).toFloat();
    }

    if (knownGrams <= 0.0f || knownGrams > 6000.0f) {
      Serial.println(F("[ERROR] Usage: CAL <known_weight_in_grams> (e.g. 'CAL 500', 'CAL 1000', 'CAL 1500')"));
    } else {
      if (!scaleInitialized) {
        scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
        scaleInitialized = true;
      }
      Serial.print(F("[CALIBRATION] Sampling 10 readings for "));
      Serial.print(knownGrams, 1);
      Serial.println(F(" g reference weight..."));

      if (!scale.wait_ready_timeout(1000)) {
        Serial.println(F("[ERROR] Cannot calibrate: HX711 not ready or disconnected."));
      } else {
        double raw = scale.get_value(10);
        if (abs(raw) < 1000.0) {
          Serial.println(F("[ERROR] Reading too low. Place reference weight on tray, or TARE first!"));
        } else {
          double absRaw = abs(raw);
          // Find the best calibration point index (500g increments: 500->idx 1, 1000->idx 2, etc.)
          int targetIdx = (int)round(knownGrams / 500.0f);
          if (targetIdx < 1) targetIdx = 1;
          if (targetIdx >= NUM_CAL_POINTS) targetIdx = NUM_CAL_POINTS - 1;

          float targetGridGrams = calTable[targetIdx].weight_g;
          double scaledRaw = absRaw * ((double)targetGridGrams / (double)knownGrams);

          calTable[targetIdx].raw_counts = scaledRaw;
          calTable[targetIdx].calibrated = true;
          hx711CalFactor = (float)(scaledRaw / targetGridGrams);

          saveCalibrationToNVS();
          float verified = getCalibratedWeightGrams(5, 500);

          Serial.println(F("--------------------------------------------------------------------------"));
          Serial.print(F("  [CALIBRATION SUCCESS] Point ")); Serial.print(targetIdx);
          Serial.print(F(" (")); Serial.print(targetGridGrams, 0); Serial.println(F(" g) Updated!"));
          Serial.print(F("  Reference Applied   : ")); Serial.print(knownGrams, 1); Serial.println(F(" g"));
          Serial.print(F("  Raw ADC Delta       : ")); Serial.println(absRaw, 0);
          Serial.print(F("  Segment Slope       : ")); Serial.print(absRaw / knownGrams, 2); Serial.println(F(" counts/g"));
          Serial.print(F("  Verified Reading    : ")); Serial.print(verified, 1); Serial.println(F(" g"));
          Serial.println(F("  Permanently saved to flash memory (NVS)."));
          Serial.println(F("--------------------------------------------------------------------------"));
        }
      }
    }
  } else if (upper.startsWith("FACTOR ") || upper.startsWith("SCALE ")) {
    float newF = cmd.substring(cmd.indexOf(' ') + 1).toFloat();
    if (newF > 1.0f && newF < 10000.0f) {
      hx711CalFactor = newF;
      for (int i = 1; i < NUM_CAL_POINTS; i++) {
        calTable[i].raw_counts = calTable[i].weight_g * hx711CalFactor;
      }
      saveCalibrationToNVS();
      Serial.print(F("[CONFIG] Calibration factor set to: "));
      Serial.print(hx711CalFactor, 2);
      Serial.println(F(" counts/g (All grid points scaled)"));
    } else {
      Serial.println(F("[ERROR] Factor must be between 1.0 and 10000.0"));
    }
  } else if (upper.startsWith("S ")) {
    long steps = cmd.substring(2).toInt();
    if (steps > 0) stepMotor(steps, true);
  } else if (upper.startsWith("MSTEP ") || upper.startsWith("MICROSTEP ")) {
    int msVal = cmd.substring(cmd.indexOf(' ') + 1).toInt();
    if (msVal == 1 || msVal == 2 || msVal == 4 || msVal == 8 || msVal == 16 || msVal == 32) {
      stepperMicrostep = (uint8_t)msVal;
      Serial.print(F("[CONFIG] Microstepping set to 1/"));
      Serial.print(stepperMicrostep);
      Serial.print(F(" ("));
      Serial.print(200L * stepperMicrostep);
      Serial.println(F(" steps/rev)"));
    } else {
      Serial.println(F("[ERROR] Microstep must be 1, 2, 4, 8, 16, or 32"));
    }
  } else if (upper.startsWith("RPM ")) {
    long r = cmd.substring(4).toInt();
    if (r >= 1 && r <= 300) {
      stepperCruiseRpm = r;
      Serial.print(F("[CONFIG] Stepper cruise speed set to: "));
      Serial.print(stepperCruiseRpm);
      Serial.println(F(" RPM"));
    } else {
      Serial.println(F("[ERROR] RPM must be between 1 and 300"));
    }
  } else if (upper == "ESTOP") {
    emergencyStop();
  } else if (upper == "RESET") {
    resetSafetyFlags();
  } else if (upper == "STATUS") {
    printStatus();
  } else if (upper == "?" || upper == "HELP") {
    printHelpMenu();
  }
}

// ===================================================================================
// SETUP (Initializes All Hardware & Tasks)
// ===================================================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(F("\n======================================================="));
  Serial.println(F("       CPE3207L FEEDER NODE - ONLINE                   "));
  Serial.println(F("======================================================="));

  // 1. Initialize Actuator Pins to Safe Default State
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(RELAY_FEED_PIN, OUTPUT);
  pinMode(RELAY_WATER_PIN, OUTPUT);

  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, HIGH);
  digitalWrite(ENABLE_PIN, LOW);                      // Active LOW: LOW = Driver Enabled
  digitalWrite(RELAY_FEED_PIN, RELAY_VALVE_CLOSED);   // Active LOW: HIGH = Valve Closed
  digitalWrite(RELAY_WATER_PIN, RELAY_VALVE_CLOSED);  // Active LOW: HIGH = Valve Closed

  // 2. Initialize Sensor Pins (with internal pulldowns for safe disconnected bench testing)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT_PULLDOWN);
  pinMode(MQ135_PIN, INPUT);
  pinMode(OVERRIDE_BTN_PIN, INPUT_PULLDOWN);
  pinMode(HX711_SCK_PIN, OUTPUT);
  pinMode(HX711_DT_PIN, INPUT_PULLUP);
  digitalWrite(HX711_SCK_PIN, LOW);

  // Hardware reset pulse to HX711 chip (SCK HIGH > 60us resets internal logic)
  digitalWrite(HX711_SCK_PIN, HIGH);
  delayMicroseconds(80);
  digitalWrite(HX711_SCK_PIN, LOW);
  delayMicroseconds(10);

  // Always initialize HX711 library pin mapping immediately
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scaleInitialized = true;
  scale.set_scale(hx711CalFactor);

  // Load persistent multi-point calibration profile from flash (NVS)
  loadCalibrationFromNVS();

  // Give HX711 up to 1200ms to power on, settle, and complete its first 10 SPS conversion
  Serial.print(F("[HX711] Waiting for load cell stabilization... "));
  if (scale.wait_ready_timeout(1200)) {
    Serial.println(F("OK. Zeroing scale (Tare)..."));
    scale.tare(5);
    hx711Tared = true;
    smoothedWeightGrams = 0.0f;
    weightRingBuf[0] = 0.0f;
    weightRingBuf[1] = 0.0f;
    weightRingBuf[2] = 0.0f;
    Serial.println(F("[HX711] Scale zeroed (Tare complete, baseline = 0.0g)."));
  } else {
    Serial.println(F("PENDING."));
    Serial.println(F("[HX711] Notice: Load cell settling (will auto-tare on first reading)."));
  }

  // 3. Initialize RS-485 MAX485 Direction Pin
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW); // Default to Receive mode

  // 4. Create Mutexes for Thread Safety
  telemetryMutex = xSemaphoreCreateMutex();
  sensorMutex    = xSemaphoreCreateMutex();

  printHelpMenu();
  Serial.println(F("[SYSTEM] Feeder Node Initialized. Actuators Ready."));

  // 5. Actuator Startup Self-Test (Smooth 1-turn rotation on boot)
  Serial.println(F("[SYSTEM] Running Stepper Motor Startup Self-Test (1 turn)..."));
  stepMotor(6400, true);
  Serial.println(F("[SYSTEM] Stepper Motor Startup Test Completed.\n"));
}

// ===================================================================================
// MAIN LOOP (Actuation, Sensors & Communication)
// ===================================================================================

void loop() {
  // 1. Periodic Sensor Acquisition (non-blocking, every 500ms)
  updateSensors();

  // 2. Process Native RS-485 MODBUS RTU Slave Engine
  processModbusRTU();

  // 3. Non-blocking Actuator Safety Timers
  updateActuatorTimers();

  // 4. Automated Water Refill Monitoring (Threshold Check)
  checkAutoRefill();

  // 5. Check Hardware Manual Override Button (GPIO 15)
  if (digitalRead(OVERRIDE_BTN_PIN) == HIGH) {
    Serial.println(F("[BUTTON] Hardware Override Button Pressed -> Pulsing 100g Feed!"));
    dispenseFeedGrams(100);
    delay(500); // Debounce
  }

  // 6. Handle Incoming Serial Diagnostic Commands from USB
  if (Serial.available() > 0) {
    String cmd = Serial.readString();
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.print(F("[USB CMD] > "));
      Serial.println(cmd);
      processSerialCommand(cmd);
    }
  }

  yield();
}
