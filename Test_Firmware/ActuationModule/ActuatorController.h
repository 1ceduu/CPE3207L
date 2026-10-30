/*
  ActuatorController.h - Modular Hardware Driver for Edge Feeder Actuators
  CPE3207L Thesis Modular Architecture

  Manages high-power mechanical actuators for CPE3207L feeder nodes:
    - NEMA17 Stepper Motor (Auger Feed Dispenser via DRV8825 Driver)
    - Push/Pull Feed Gate Solenoid Valve (via IRLZ44N N-Channel MOSFET)
    - Water Refill Solenoid Valve (via IRLZ44N N-Channel MOSFET)

  Pin Assignments (matching feeder node schematic):
    - DRV8825 STEP   -> GPIO 18
    - DRV8825 DIR    -> GPIO 19
    - DRV8825 ENABLE -> GPIO 21 (ACTIVE LOW: LOW = Driver Enabled, HIGH = Idle/Disabled)
    - GATE_FEED      -> GPIO 22 (Push/Pull Solenoid MOSFET)
    - GATE_WATER     -> GPIO 23 (Water Refill Solenoid MOSFET)

  Safety Features:
    - Non-blocking thermal safety cut-off for push-pull solenoid (default 10s max continuous ON)
    - Automatic DRV8825 driver disable while idle to reduce heat and power consumption
    - Smooth trapezoidal speed ramp (ease-in, cruise, ease-out) to prevent motor stall / jammed auger
    - Emergency Stop (E-Stop) instant lockouts
*/

#ifndef ACTUATOR_CONTROLLER_H
#define ACTUATOR_CONTROLLER_H

#include <Arduino.h>

// Default Hardware Pin Definitions
#define DEFAULT_STEP_PIN       18
#define DEFAULT_DIR_PIN        19
#define DEFAULT_ENABLE_PIN     21
#define DEFAULT_GATE_FEED_PIN  22
#define DEFAULT_GATE_WATER_PIN 23

// Calibration and Constraint Defaults
#define DEFAULT_STEPS_PER_GRAM         80.0f  // Adjust based on auger physical testing
#define DEFAULT_MAX_SOLENOID_ON_MS     10000U // 10s thermal safety limit for push-pull coil
#define DEFAULT_CRUISE_RPM             60L    // Stepper cruise speed in RPM
#define DEFAULT_START_RPM              8L     // Stepper starting speed for trapezoidal ramp
#define DEFAULT_MICROSTEP              1      // 1, 2, 4, 8, 16, or 32 microsteps

// Actuator Status Bit Flags
#define ACTUATOR_FLAG_STEPPER_ACTIVE    (1 << 0)
#define ACTUATOR_FLAG_FEED_GATE_ACTIVE  (1 << 1)
#define ACTUATOR_FLAG_WATER_VALVE_ACTIVE (1 << 2)
#define ACTUATOR_FLAG_THERMAL_TIMEOUT   (1 << 3)
#define ACTUATOR_FLAG_ESTOP_ACTIVE      (1 << 4)

class ActuatorController {
public:
  ActuatorController(
    uint8_t stepPin   = DEFAULT_STEP_PIN,
    uint8_t dirPin    = DEFAULT_DIR_PIN,
    uint8_t enablePin = DEFAULT_ENABLE_PIN,
    uint8_t feedPin   = DEFAULT_GATE_FEED_PIN,
    uint8_t waterPin  = DEFAULT_GATE_WATER_PIN
  );

  // Initialize hardware GPIO pins and initial idle states
  void init();

  // Non-blocking background task (call inside main loop())
  void update();

  // Solenoid Control
  void setFeedGate(bool open);
  void setWaterValve(bool open);
  void triggerWaterPulse(uint32_t durationMs);

  // Stepper & Dispensing Routines
  bool dispenseFeedGrams(uint16_t grams);
  void stepMotor(long totalSteps, bool clockwise = true);

  // Safety & Emergency Features
  void emergencyStop();
  void resetSafetyFlags();

  // Status & Getter Queries
  bool isFeedGateOpen() const { return _feedGateState; }
  bool isWaterValveOpen() const { return _waterValveState; }
  bool isStepperActive() const { return _stepperActive; }
  bool isThermalSafetyTriggered() const { return _thermalTimeoutTriggered; }
  bool isEstopActive() const { return _estopActive; }
  uint16_t getStatusFlags() const;

  // Calibration Setters
  void setStepsPerGram(float spg) { _stepsPerGram = spg; }
  void setMaxSolenoidOnTimeMs(uint32_t ms) { _maxSolenoidOnTimeMs = ms; }

private:
  uint8_t _stepPin;
  uint8_t _dirPin;
  uint8_t _enablePin;
  uint8_t _feedPin;
  uint8_t _waterPin;

  float    _stepsPerGram;
  uint32_t _maxSolenoidOnTimeMs;

  bool _feedGateState;
  bool _waterValveState;
  bool _stepperActive;
  bool _thermalTimeoutTriggered;
  bool _estopActive;

  uint32_t _feedGateTurnedOnAt;
  uint32_t _waterPulseEndsAt;
  bool     _waterPulseActive;

  // Stepper speed calculation helper
  uint32_t calculateHalfPeriodUs(long rpm);
  void stepPulse(uint32_t halfPeriodUs);
};

#endif // ACTUATOR_CONTROLLER_H
