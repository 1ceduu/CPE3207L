/*
  ActuatorController.cpp - Modular Hardware Driver Implementation
  CPE3207L Thesis Modular Architecture
*/

#include "ActuatorController.h"

ActuatorController::ActuatorController(
  uint8_t stepPin,
  uint8_t dirPin,
  uint8_t enablePin,
  uint8_t feedPin,
  uint8_t waterPin
) : _stepPin(stepPin),
    _dirPin(dirPin),
    _enablePin(enablePin),
    _feedPin(feedPin),
    _waterPin(waterPin),
    _stepsPerGram(DEFAULT_STEPS_PER_GRAM),
    _maxSolenoidOnTimeMs(DEFAULT_MAX_SOLENOID_ON_MS),
    _feedGateState(false),
    _waterValveState(false),
    _stepperActive(false),
    _thermalTimeoutTriggered(false),
    _estopActive(false),
    _feedGateTurnedOnAt(0),
    _waterPulseEndsAt(0),
    _waterPulseActive(false)
{
}

void ActuatorController::init() {
  pinMode(_stepPin, OUTPUT);
  pinMode(_dirPin, OUTPUT);
  pinMode(_enablePin, OUTPUT);
  pinMode(_feedPin, OUTPUT);
  pinMode(_waterPin, OUTPUT);

  // Safe startup states
  digitalWrite(_stepPin, LOW);
  digitalWrite(_dirPin, LOW);
  digitalWrite(_enablePin, HIGH); // ACTIVE LOW: HIGH = Stepper driver disabled/idle
  digitalWrite(_feedPin, LOW);    // Feed Gate Solenoid OFF
  digitalWrite(_waterPin, LOW);   // Water Solenoid Valve OFF

  _feedGateState = false;
  _waterValveState = false;
  _stepperActive = false;
  _thermalTimeoutTriggered = false;
  _estopActive = false;
  _waterPulseActive = false;

  Serial.println(F("[ACTUATOR] Hardware Controller Initialized successfully."));
  Serial.print(F("           Pins: STEP=")); Serial.print(_stepPin);
  Serial.print(F(" DIR=")); Serial.print(_dirPin);
  Serial.print(F(" EN=")); Serial.print(_enablePin);
  Serial.print(F(" FEED=")); Serial.print(_feedPin);
  Serial.print(F(" WATER=")); Serial.println(_waterPin);
}

void ActuatorController::update() {
  uint32_t now = millis();

  // 1. Non-blocking Thermal Safety Timeout for Feed Gate Solenoid
  if (_feedGateState) {
    if (now - _feedGateTurnedOnAt >= _maxSolenoidOnTimeMs) {
      setFeedGate(false);
      _thermalTimeoutTriggered = true;
      Serial.println(F("[SAFETY WARNING] Feed Gate Solenoid turned OFF automatically (Max continuous ON time exceeded to prevent thermal coil damage)."));
    }
  }

  // 2. Non-blocking Water Refill Pulse Timer
  if (_waterPulseActive && _waterValveState) {
    if (now >= _waterPulseEndsAt) {
      setWaterValve(false);
      _waterPulseActive = false;
      Serial.println(F("[ACTUATOR] Water pulse duration complete. Solenoid closed."));
    }
  }
}

void ActuatorController::setFeedGate(bool open) {
  if (_estopActive && open) {
    Serial.println(F("[ERROR] Cannot open Feed Gate: Emergency Stop (E-Stop) is ACTIVE!"));
    return;
  }

  if (open) {
    digitalWrite(_feedPin, HIGH);
    if (!_feedGateState) {
      _feedGateTurnedOnAt = millis();
    }
    _feedGateState = true;
    Serial.println(F("[ACTUATOR] Feed Gate Solenoid -> OPEN (HIGH)"));
  } else {
    digitalWrite(_feedPin, LOW);
    _feedGateState = false;
    Serial.println(F("[ACTUATOR] Feed Gate Solenoid -> CLOSED (LOW)"));
  }
}

void ActuatorController::setWaterValve(bool open) {
  if (_estopActive && open) {
    Serial.println(F("[ERROR] Cannot open Water Valve: Emergency Stop (E-Stop) is ACTIVE!"));
    return;
  }

  if (open) {
    digitalWrite(_waterPin, HIGH);
    _waterValveState = true;
    Serial.println(F("[ACTUATOR] Water Solenoid Valve -> OPEN (HIGH)"));
  } else {
    digitalWrite(_waterPin, LOW);
    _waterValveState = false;
    _waterPulseActive = false;
    Serial.println(F("[ACTUATOR] Water Solenoid Valve -> CLOSED (LOW)"));
  }
}

void ActuatorController::triggerWaterPulse(uint32_t durationMs) {
  if (_estopActive) {
    Serial.println(F("[ERROR] Cannot trigger water pulse: Emergency Stop is ACTIVE!"));
    return;
  }

  Serial.print(F("[ACTUATOR] Triggering water refill pulse for "));
  Serial.print(durationMs);
  Serial.println(F(" ms..."));

  setWaterValve(true);
  _waterPulseActive = true;
  _waterPulseEndsAt = millis() + durationMs;
}

uint32_t ActuatorController::calculateHalfPeriodUs(long rpm) {
  if (rpm < 1) rpm = 1;
  long stepsPerRev = (long)200 * DEFAULT_MICROSTEP;
  return (uint32_t)(60000000L / (rpm * stepsPerRev * 2));
}

void ActuatorController::stepPulse(uint32_t halfPeriodUs) {
  digitalWrite(_stepPin, HIGH);
  delayMicroseconds(halfPeriodUs);
  digitalWrite(_stepPin, LOW);
  delayMicroseconds(halfPeriodUs);
}

void ActuatorController::stepMotor(long totalSteps, bool clockwise) {
  if (_estopActive) {
    Serial.println(F("[ERROR] Stepper Motor operation blocked: Emergency Stop is ACTIVE!"));
    return;
  }
  if (totalSteps <= 0) return;

  _stepperActive = true;

  // Enable DRV8825 Stepper Driver (Active LOW)
  digitalWrite(_enablePin, LOW);
  digitalWrite(_dirPin, clockwise ? HIGH : LOW);
  delayMicroseconds(10); // Direction setup time

  long accelSteps = 200 * DEFAULT_MICROSTEP; // 1 revolution ramp up/down
  if (accelSteps > totalSteps / 2) {
    accelSteps = totalSteps / 2;
  }

  for (long i = 0; i < totalSteps; i++) {
    // Non-blocking safety check in step loop
    update();
    if (_estopActive) break;

    long currentRpm;
    if (i < accelSteps) {
      // Ease-in speed ramp: START_RPM -> CRUISE_RPM
      currentRpm = DEFAULT_START_RPM + (DEFAULT_CRUISE_RPM - DEFAULT_START_RPM) * i / accelSteps;
    } else if (i >= totalSteps - accelSteps) {
      // Ease-out speed ramp: CRUISE_RPM -> START_RPM
      long remaining = totalSteps - 1 - i;
      currentRpm = DEFAULT_START_RPM + (DEFAULT_CRUISE_RPM - DEFAULT_START_RPM) * remaining / accelSteps;
    } else {
      currentRpm = DEFAULT_CRUISE_RPM;
    }

    stepPulse(calculateHalfPeriodUs(currentRpm));
  }

  // Disable DRV8825 Stepper Driver (Active LOW -> HIGH = Idle)
  digitalWrite(_enablePin, HIGH);
  _stepperActive = false;
}

bool ActuatorController::dispenseFeedGrams(uint16_t grams) {
  if (grams == 0) return false;
  if (_estopActive) {
    Serial.println(F("[ERROR] Feed dispensing blocked: Emergency Stop is ACTIVE!"));
    return false;
  }

  Serial.print(F("[DISPENSE CYCLE] Starting automated feed dispensing sequence for "));
  Serial.print(grams);
  Serial.println(F(" grams..."));

  // 1. Open Feed Gate Cutoff Solenoid
  setFeedGate(true);
  delay(200); // Allow physical solenoid gate to reach full stroke

  // 2. Drive Stepper Motor Auger
  long totalSteps = (long)(grams * _stepsPerGram);
  Serial.print(F("[DISPENSE CYCLE] Driving auger stepper for "));
  Serial.print(totalSteps);
  Serial.println(F(" steps..."));
  stepMotor(totalSteps, true);

  // 3. Close Feed Gate Cutoff Solenoid
  delay(200);
  setFeedGate(false);

  Serial.println(F("[DISPENSE CYCLE] Feed dispensing sequence completed."));
  return true;
}

void ActuatorController::emergencyStop() {
  _estopActive = true;

  // Immediately force all actuator outputs LOW and disable stepper
  digitalWrite(_feedPin, LOW);
  digitalWrite(_waterPin, LOW);
  digitalWrite(_stepPin, LOW);
  digitalWrite(_enablePin, HIGH); // Active LOW -> Disabled

  _feedGateState = false;
  _waterValveState = false;
  _stepperActive = false;
  _waterPulseActive = false;

  Serial.println(F("\n======================================================="));
  Serial.println(F("  !!! EMERGENCY STOP (E-STOP) ACTIVATED !!!             "));
  Serial.println(F("  All actuators disengaged. Stepper driver disabled.    "));
  Serial.println(F("=======================================================\n"));
}

void ActuatorController::resetSafetyFlags() {
  _thermalTimeoutTriggered = false;
  _estopActive = false;
  Serial.println(F("[SAFETY] E-Stop and thermal safety flags reset. Actuators ready."));
}

uint16_t ActuatorController::getStatusFlags() const {
  uint16_t flags = 0;
  if (_stepperActive)            flags |= ACTUATOR_FLAG_STEPPER_ACTIVE;
  if (_feedGateState)           flags |= ACTUATOR_FLAG_FEED_GATE_ACTIVE;
  if (_waterValveState)          flags |= ACTUATOR_FLAG_WATER_VALVE_ACTIVE;
  if (_thermalTimeoutTriggered)  flags |= ACTUATOR_FLAG_THERMAL_TIMEOUT;
  if (_estopActive)              flags |= ACTUATOR_FLAG_ESTOP_ACTIVE;
  return flags;
}
