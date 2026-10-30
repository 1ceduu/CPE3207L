# Progress Report: August & September 2026

**Project Title:** Integrated Automated Feeding and Watering System with Centralized Data Monitoring for Commercial Piggeries  
**Course / Subject:** CPE3207L (Computer Engineering Capstone / Thesis Project)  
**Institution:** Department of Computer Engineering, University of San Carlos, Cebu City, Philippines  
**Project Group:** Group V  
**Authors:**
- Pace, Vaun Michael C.
- Parras, Paul Andrew F.
- Soriano, Lorenz Anthony L.

**Advisers:**
- Engr. Alvin Joseph S. Macapagal
- Engr. Jan Dave Q. Campañera

---

## Executive Summary

During the months of **August** and **September 2026**, the project progressed from component-level proof-of-concept testing to an integrated, modular, hardware-in-the-loop system architecture. Major accomplishments include:
1. Architectural modularization of the hardware schematics (decoupling actuation from sensing).
2. Development of the central backend services and local kiosk bridge with persistent SQLite storage.
3. Edge firmware implementation for ESP32-based Modbus RTU slave nodes.
4. Two-way interactive UI dashboard implementation with live telemetry synchronization and remote actuation control.
5. Integration of formal institutional endorsements into the thesis manuscript.

---

## 1. August 2026 Progress

### 1.1 Central Backend Architecture & Telemetry Pipeline (`RPi_Backend/`)
- **RS-485 Modbus RTU Poller (`poller.py`):**
  - Implemented multi-threaded cyclic polling routines targeting edge feeder and actuation nodes over an industrial RS-485 bus.
  - Built timeout and retry exception-handling loops to maintain communications resilience in high EMI piggery environments.
- **Analytical & Consumption Anomaly Engine (`analytical_engine.py`):**
  - Programmed automated statistical tracking for daily feed and water consumption per pen.
  - Built anomaly detection algorithms based on standard-deviation deviations to identify signs of lethargy, sickness, or pipe clogs.
- **Alert Dispatch Service (`alert_service.py`):**
  - Integrated rule-based alert routing (categorizing warnings into critical, advisory, and system levels).
  - Prepared SMS encoding and dispatch pipelines for caretaker alerts via GSM module.
- **Actuation Command Handler (`command_service.py`):**
  - Implemented thread-safe queueing for manual feed dispensing, emergency stops, and threshold modifications.
- **Data Persistence Layer (`database.py`):**
  - Initialized SQLite schema for pen configurations, time-series telemetry records, feed schedules, and audit events.
- **Host HTTP / WebSocket Bridge (`bridge_server.py`, `server.py`):**
  - Deployed local communication servers bridging raw serial data to websocket subscribers for real-time frontend visualization.

### 1.2 Firmware Development & Transducer Validation (`Test_Firmware/`)
- **Actuation Slave Protocol (`ActuationNode_MODBUS_Slave`):**
  - Configured Modbus slave holding and input registers for remote actuator control and status reporting.
- **Transducer Characterization & Calibration:**
  - **Load Cell (`HX711_LoadCell_Test`):** Calibrated dual 24-bit ADC scale factor for feed hopper rationing accuracy.
  - **Ultrasonic Water Level Sensors (`WaterLevel_AJSR04M_UART_Test`, `WaterLevel_JSNSR04T_Test`):** Evaluated both UART serial mode (AJ-SR04M) and pulse echo timing (JSN-SR04T) to verify surface reading stability under water turbulence.
  - **Air Quality Sensing (`MQ135_GasSensor_Test`):** Validated baseline analog curve and conversion calculations for ambient ammonia/harmful gas levels.
- **Multi-Sensor Edge Aggregator (`AllSensors_Integrated_Test`):**
  - Verified concurrent sampling of analog, digital, and bus sensors without timing collisions on the ESP32.

### 1.3 Documentation & Formal Endorsements
- **Institutional Endorsements:** Received and incorporated official signed defense endorsement and panel approval documentation (`GroupV_Approval_Signed.pdf`).
- **Thesis Manuscript Maintenance:** Compiled and integrated approval artifacts and front matter into the IEEE conference paper (`TimTissis.tex`).

---

## 2. September 2026 Progress

### 2.1 Hardware Modularization & Schematic Overhaul (`Hardware_Design/`)
- **Modular PCB Architecture Separation:**
  - Transitioned away from a congested single-board layout to an isolated, modular two-board topology in KiCad:
    1. **Feeder Sensor Module (`feeder_sensor_module.kicad_sch` / `.net`):** Isolated low-noise circuitry dedicated to sensitive transducer signals (HX711, ultrasonic water level, ambient gas sensors) and RS-485 transceiver.
    2. **Actuation Module (`actuation_module.kicad_sch` / `.net`):** Dedicated high-power switching board housing DRV8825 stepper motor drivers, inductive kickback protection, flyback diodes, and MOSFETs/relays for 12V push-pull water solenoids.
- **Power Delivery Hardening:**
  - Designed isolated power regulation using DC-DC buck converters to prevent motor current spikes from inducing brownouts or resets in the ESP32 logic rail.

### 2.2 Laptop Kiosk Bridge & Persistent Local Engine (`run_laptop_kiosk.py`)
- **Zero-Dependency Laptop Kiosk Server:**
  - Developed a standalone Python runtime environment enabling immediate deployment and testing on development laptops without requiring full Raspberry Pi hardware.
  - Implemented auto-detection of connected ESP32 COM ports at 115200 baud.
- **Dynamic Multi-Pen Discovery & Telemetry DB (`piggery_monitor.db`):**
  - Integrated persistent SQLite database running in **WAL (Write-Ahead Logging)** mode for high concurrency.
  - Dynamic discovery and automated registration of active pens as edge nodes come online.

### 2.3 Interactive Kiosk Dashboard Frontend (`WebsitePrototype/`)
- **Real-Time Data Integration (`app.js`, `ui.js`, `data.js`):**
  - Migrated the frontend dashboard from mock placeholders to live data streams powered by the kiosk bridge.
  - Synchronized **Screen 1 (Overview / Farm Status)** and **Screen 2 (Pen Detail / Control Screen)** in real time.
- **Pen Management & Configuration UI:**
  - Added interactive modal dialogs for modifying pen parameters (water refill threshold percentages, daily feeding targets).
  - Integrated manual override controls permitting caretakers to trigger immediate feed dispensing or water refill cycles.
- **Live Terminal / Diagnostic Console:**
  - Added an in-browser interactive terminal to inspect raw Modbus packets, force actuator test pulses, and verify line health.

### 2.4 Actuation Tuning & Stepper Driver Optimization
- **Auger Dispensing Precision (`StepperMotor_DRV8825_Test`):**
  - Tuned microstepping modes, start/stop ramp acceleration profiles, and motor current limits on the DRV8825.
  - Tested continuous and pulsed auger dispensing to ensure consistent gram-level portion control.
- **Edge-to-Cloud Modbus Map Alignment:**
  - Standardized register addressing between the ESP32 actuation firmware and the central host polling table.

---

## 3. Month-to-Month Milestone Comparison

| Project Dimension | August 2026 Deliverables | September 2026 Deliverables |
| :--- | :--- | :--- |
| **Hardware & PCB** | Breadboard prototyping, pin-health diagnostics, sensor bench validation. | Modular PCB schematics (`feeder_sensor_module` & `actuation_module` in KiCad) with isolated power rails. |
| **Edge Firmware** | Modbus RTU slave baseline; standalone test routines for HX711, ultrasonic, and MQ-135 sensors. | Stepper acceleration tuning, DRV8825 microstep calibration, standardized Modbus register map. |
| **Central Backend** | Python background services (`poller.py`, `analytical_engine.py`, `alert_service.py`, `database.py`). | Standalone kiosk runner (`run_laptop_kiosk.py`) with SQLite WAL database and auto-port discovery. |
| **Kiosk Dashboard** | Prototype HTML/CSS layouts with static mock data. | Fully reactive two-way dashboard UI (`WebsitePrototype/`) with live telemetry, modals, and manual overrides. |
| **Documentation** | Signed endorsement incorporation, approval verification, Section 7 updates. | Schematic netlist exports, hardware block diagram synchronizations, and thesis document compilations. |

---

## 4. Next Steps (Planned for October 2026)
1. **Physical PCB Fabrication & Assembly:** Manufacture and assemble the dual-board modular PCBs.
2. **Integrated Mechanical Stress Testing:** Test the motorized auger feed chute and solenoid valve under loaded hopper conditions.
3. **Multi-Pen RS-485 Network Verification:** Validate multi-drop communications over long-distance twisted-pair cabling.
4. **Final Manuscript Refinement:** Incorporate final bench test results and accuracy metrics into Chapter 5 / Results section of the thesis.
