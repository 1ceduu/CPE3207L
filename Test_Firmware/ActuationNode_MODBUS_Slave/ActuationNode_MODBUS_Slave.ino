/*
  ESP32 Actuation Node - MODBUS RTU Slave Firmware
  CPE3207L Thesis Modular Architecture

  Dedicated controller for high-power mechanical actuators:
    - NEMA17 Stepper Motor (Auger Feed Dispenser via DRV8825 Driver)
    - Push/Pull Feed Gate Solenoid Valve (via IRLZ44N MOSFET)
    - Water Refill Solenoid Valve (via IRLZ44N MOSFET)

  Communicates with Raspberry Pi MODBUS Master over RS-485 bus (Slave Address #10, 9600 baud, 8N1).

  MODBUS Holding Registers (Read & Write / Commandable by Raspberry Pi):
    Register 0 (40001 / offset 0): Feed Gate Solenoid State (0 = Closed, 1 = Open)
    Register 1 (40002 / offset 1): Water Solenoid Valve State (0 = Closed, 1 = Open)
    Register 2 (40003 / offset 2): Dispense Feed Target (Grams; >0 triggers automated dispense cycle)
    Register 3 (40004 / offset 3): Actuator Status Bitmask Flags (Read-Only Status)
    Register 4 (40005 / offset 4): Emergency Stop Command (0 = Normal, 1 = Trigger E-Stop, 2 = Reset E-Stop)

  Hardware Pins:
    - MAX485 RO (RX)  -> 1k/2k Divider -> ESP32 GPIO 16 (RX2)
    - MAX485 DI (TX)  -> ESP32 GPIO 17 (TX2)
    - MAX485 DE & RE  -> ESP32 GPIO 4 (Direction Control)
    - DRV8825 STEP    -> GPIO 18
    - DRV8825 DIR     -> GPIO 19
    - DRV8825 ENABLE  -> GPIO 21 (ACTIVE LOW: LOW = Driver Enabled, HIGH = Disabled)
    - GATE_FEED (MOSFET)  -> GPIO 22
    - GATE_WATER (MOSFET) -> GPIO 23
*/

#include <ModbusRTU.h>
#include "ActuatorController.h"

#define SLAVE_ID 10       // Unique MODBUS Slave ID for Actuation Node
#define RX2_PIN 16
#define TX2_PIN 17
#define DE_RE_PIN 4

ModbusRTU mb;
ActuatorController actuators;

void setup() {
  Serial.begin(115200);

  // Initialize Actuator Controller Hardware Pins & Default Safe States
  actuators.init();

  // Initialize MODBUS RS-485 Serial2
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  mb.begin(&Serial2, DE_RE_PIN);
  mb.slave(SLAVE_ID);

  // Add Holding Registers (offsets 0, 1, 2, 3, 4)
  mb.addHreg(0, 0); // Feed Gate Solenoid State Command (0 = Closed, 1 = Open)
  mb.addHreg(1, 0); // Water Solenoid Valve State Command (0 = Closed, 1 = Open)
  mb.addHreg(2, 0); // Stepper Dispense Target Trigger (Grams)
  mb.addHreg(3, 0); // Actuator Status Flags (Bitmask)
  mb.addHreg(4, 0); // E-Stop Command (0 = Normal, 1 = Trigger, 2 = Reset)

  Serial.println(F("\n========================================="));
  Serial.println(F("  ESP32 Actuation Node MODBUS Slave      "));
  Serial.println(F("  Slave ID: 10 | Baud: 9600 | Pin DE/RE: 4"));
  Serial.println(F("=========================================\n"));
}

void loop() {
  mb.task(); // Process incoming MODBUS RTU requests from Raspberry Pi
  actuators.update(); // Background thermal safety & pulse timing checks

  // Handle Emergency Stop Register Command (Register 4)
  uint16_t estopCmd = mb.Hreg(4);
  if (estopCmd == 1 && !actuators.isEstopActive()) {
    actuators.emergencyStop();
  } else if (estopCmd == 2 && actuators.isEstopActive()) {
    actuators.resetSafetyFlags();
    mb.Hreg(4, 0); // Reset command register back to normal state
  }

  // Read current commanded solenoid states from MODBUS holding registers
  uint16_t feedGateCmd  = mb.Hreg(0);
  uint16_t waterValveCmd = mb.Hreg(1);
  uint16_t dispenseGramsCmd = mb.Hreg(2);

  // Apply Solenoid States if not overridden by thermal timeout or E-Stop
  if (!actuators.isEstopActive()) {
    if (feedGateCmd != actuators.isFeedGateOpen() && !actuators.isThermalSafetyTriggered()) {
      actuators.setFeedGate(feedGateCmd > 0);
    }
    if (waterValveCmd != actuators.isWaterValveOpen()) {
      actuators.setWaterValve(waterValveCmd > 0);
    }
  }

  // Check for Automated Dispense Feed Trigger Command (Register 2)
  if (dispenseGramsCmd > 0 && !actuators.isEstopActive()) {
    bool success = actuators.dispenseFeedGrams(dispenseGramsCmd);
    if (success) {
      mb.Hreg(2, 0); // Clear dispense trigger register once executed
    }
  }

  // Sync current hardware status flags back to MODBUS Register 3 (Read-Only for Master)
  mb.Hreg(3, actuators.getStatusFlags());

  yield();
}
