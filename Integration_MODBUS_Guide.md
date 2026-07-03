# MODBUS RTU Integration & Communication Guide

This guide explains how to connect your individual sensor nodes (ESP32) to the central controller (Raspberry Pi kiosk) over an **RS-485 serial bus** using the **MODBUS RTU protocol**. 

---

## 1. Network Topology (RS-485 Bus)
MODBUS RTU is a **Master-Slave** protocol. In this project:
- **Master**: Raspberry Pi (polls data from the nodes).
- **Slaves**: Up to 3x ESP32 Feeder Nodes (each has a unique ID, e.g., `1`, `2`, and `3`).

### Bus Wiring Setup
- All device **A pins** connect together to the A line.
- All device **B pins** connect together to the B line.
- Place a **120Ω termination resistor** at the two physical ends of the RS-485 bus (e.g., at the Pi end and the last ESP32 node end). This prevents signal reflections.
- **Common Ground**: Connect the GND of the Raspberry Pi and all ESP32 MAX485 modules together using a spare wire in your bus cable to prevent common-mode voltage differences.

---

## 2. MODBUS Register Mapping
Since MODBUS communicates using 16-bit integer registers, floating-point sensor values must be scaled (e.g. multiplied by 100) before sending, and divided on the receiving (Pi) end.

| Register Address | Offset | Type | Description | Unit / Scaling |
|---|---|---|---|---|
| **30001** (or 40001) | `0` | Input / Holding | Feed Trough Weight | Grams (integer) |
| **30002** (or 40002) | `1` | Input / Holding | MQ-135 Gas Reading | PPM × 10 (e.g., `250` = 25.0 ppm) |
| **30003** (or 40003) | `2` | Input / Holding | Water Level / Ultrasonic | cm × 10 (e.g., `123` = 12.3 cm) |
| **40001** | `0` | Holding | Feed Gate Solenoid State | `0` = Closed, `1` = Open |
| **40002** | `1` | Holding | Water Valve State | `0` = Closed, `1` = Open |

---

## 3. ESP32 MODBUS Slave Firmware Template
Use the popular Arduino library **`Modbus-Arduino` by Yaacov** (or search `ModbusRTU` by `emelianov` in Arduino Library Manager).

Here is a standard dual-core design template where:
- **Core 0** reads the sensors (HX711, MQ-135).
- **Core 1** handles MODBUS RTU requests to prevent lag/timing mismatches.

```cpp
#include <ModbusRTU.h>
#include "HX711.h"

#define SLAVE_ID 1 // Unique ID for this feeder node (1, 2, or 3)
#define RX2_PIN 16
#define TX2_PIN 17
#define DE_RE_PIN 4

// Sensor pin definitions
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;
const int MQ135_PIN = 34;

HX711 scale;
ModbusRTU mb;

// Shared variables for Core 0 & Core 1
volatile uint16_t shared_weight = 0;
volatile uint16_t shared_ammonia = 0;
SemaphoreHandle_t mutex;

void setup() {
  Serial.begin(115200);
  
  // Create mutex for thread-safe shared variables
  mutex = xSemaphoreCreateMutex();
  
  // 1. Initialize Sensors
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(420.0); // Replace with your calibration factor
  scale.tare();
  
  pinMode(MQ135_PIN, INPUT);

  // 2. Initialize RS-485 MODBUS
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  mb.begin(&Serial2, DE_RE_PIN);
  mb.slave(SLAVE_ID);
  
  // Register MODBUS Holding Registers (offsets 0, 1, 2)
  mb.addHreg(0, 0); // Weight
  mb.addHreg(1, 0); // Gas
  mb.addHreg(2, 0); // Ultrasonic (stubbed)

  // Start sensor reading task on Core 0
  xTaskCreatePinnedToCore(
    SensorReadTask,    /* Task function */
    "SensorTask",      /* Name of task */
    4096,              /* Stack size */
    NULL,              /* Parameter to pass */
    1,                 /* Task priority */
    NULL,              /* Task handle */
    0                  /* Pin to Core 0 */
  );
}

void loop() {
  // Core 1 executes Modbus tasks
  if (xSemaphoreTake(mutex, (TickType_t) 10) == pdTRUE) {
    mb.Hreg(0, shared_weight);
    mb.Hreg(1, shared_ammonia);
    xSemaphoreGive(mutex);
  }
  
  mb.task();
  yield();
}

// Sensor polling runs concurrently on Core 0
void SensorReadTask(void * pvParameters) {
  for(;;) {
    // 1. Read scale
    float weight = 0;
    if (scale.is_ready()) {
      weight = scale.get_units(5);
    }
    
    // 2. Read MQ-135
    int rawADC = analogRead(MQ135_PIN);
    float voltage = rawADC * (3.3 / 4095.0) * 1.5;
    float Rs = 10.0 * (5.0 - voltage) / voltage;
    float ppmNH3 = 102.2 * pow((Rs / 10.0), -2.47); // Assuming R0 = 10.0

    // Update shared variables safely
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      shared_weight = (weight > 0) ? (uint16_t)weight : 0;
      shared_ammonia = (uint16_t)(ppmNH3 * 10.0); // Scale up by 10 to keep decimal
      xSemaphoreGive(mutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(500)); // Read sensors every 500ms
  }
}
```

---

## 4. Raspberry Pi MODBUS Master Python Backend
On your Raspberry Pi, install the lightweight **`minimalmodbus`** library:
```bash
pip3 install minimalmodbus
```

### Python Script to Query Node Data
```python
#!/usr/bin/env python3
import minimalmodbus
import serial
import time

# Configure RS-485 Serial Port (USB-to-RS485 adapter on Pi is typically /dev/ttyUSB0)
# If using Pi's onboard UART (GPIO 14/15), it is /dev/ttyAMA0 or /dev/ttyS0
PORT = '/dev/ttyUSB0' 
BAUDRATE = 9600

# Create instrument objects for each feeder node
node1 = minimalmodbus.Instrument(PORT, slaveaddress=1)
node1.serial.baudrate = BAUDRATE
node1.serial.bytesize = 8
node1.serial.parity = serial.PARITY_NONE
node1.serial.stopbits = 1
node1.serial.timeout = 0.5
node1.mode = minimalmodbus.MODE_RTU

# Enable automatic Transmit/Receive GPIO control if using a GPIO pin for DE/RE
# node1.serial.rs485_mode = serial.rs485.RS485Settings(rts_level_for_tx=True, rts_level_for_rx=False, delay_before_tx=0, delay_before_rx=0)

print("Starting MODBUS Master Polling loop...")

while True:
    try:
        # Read two registers starting from address 0 (Weight and MQ-135)
        # function code 3 (Read Holding Registers)
        data = node1.read_registers(registeraddress=0, number_of_registers=2, functioncode=3)
        
        weight = data[0]
        ammonia = data[1] / 10.0 # Scale back down to float
        
        print(f"[NODE 1] Weight: {weight} g | Ammonia: {ammonia:.1f} ppm")
        
    except minimalmodbus.NoResponseError:
        print("[NODE 1] Timeout - No response from device.")
    except minimalmodbus.ModbusException as e:
        print(f"[NODE 1] MODBUS Error: {e}")
        
    time.sleep(2.0) # Poll every 2 seconds
```

---

## 5. Integrating with the Kiosk Dashboard
To link this real-time data to your kiosk display:
1. **Database Storage**: The Python script writes the polled values into your project SQLite database (`piggery_monitor.db`).
2. **Flask Backend**: A Flask web application reads from the SQLite database and serves an API endpoint (e.g., `/api/telemetry`).
3. **Kiosk UI**: The fullscreen dashboard webpage on the Raspberry Pi uses JavaScript `fetch()` inside a `setInterval()` loop to update the interface in real time every few seconds.
