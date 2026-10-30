/*
  ESP32 Feeder Node - MODBUS RTU Slave Firmware (High-Speed 115200 Baud Version)
  CPE3207L Thesis Hardware & Communication Stack

  Communicates with Raspberry Pi MODBUS Master over RS-485 bus.
  - Reads sensors: HX711 (weight), Ultrasonic (water level), MQ-135 (air quality).
  - Serves MODBUS RTU Holding Registers (Slave Address #1, 115200 baud, 8N1).
  - Built-in noise-immune MODBUS RTU Function Code 0x03 handler.

  MODBUS Register Map:
    Register 0 (40001 / offset 0): Feed Weight (Grams, uint16)
    Register 1 (40002 / offset 1): NH3 Gas Level (PPM x 10, uint16)
    Register 2 (40003 / offset 2): Water Level (% x 10, uint16)
*/

#include "HX711.h"

// ---------------- Configuration ----------------
#define SLAVE_ID 1        // Unique MODBUS Slave ID for Node 1
#define RX2_PIN 16        // MAX485 RO line
#define TX2_PIN 17        // MAX485 DI line
#define DE_RE_PIN 4       // MAX485 DE/RE control

// Sensor Pins
const int HX711_DT_PIN  = 32;
const int HX711_SCK_PIN = 33;
const int TRIG_PIN      = 25;
const int ECHO_PIN      = 26;
const int MQ135_PIN     = 34;

float cal_factor = 420.0;
const float EMPTY_DISTANCE_CM = 45.0;
const float FULL_DISTANCE_CM  = 28.0;

HX711 scale;

// Shared volatile metrics between Core 0 (sensors) and Core 1 (MODBUS stack)
volatile uint16_t reg_weight_g = 0;
volatile uint16_t reg_nh3_x10   = 0;
volatile uint16_t reg_water_x10 = 0;

SemaphoreHandle_t sensorMutex;

// Standard MODBUS CRC16 Calculation
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

// Ultrasonic distance helper
float measureWaterDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(20);
  digitalWrite(TRIG_PIN, LOW);
  long durationUs = pulseIn(ECHO_PIN, HIGH, 30000);
  if (durationUs == 0) return -1.0;
  return (durationUs * 0.0343) / 2.0;
}

// Background Task on Core 0 for Sensor Acquisition
void SensorTask(void *pvParameters) {
  for (;;) {
    // 1. Read HX711 Load Cell
    float weightGrams = 0.0;
    if (scale.is_ready()) {
      scale.set_scale(cal_factor);
      weightGrams = scale.get_units(3);
      if (weightGrams < 0) weightGrams = 0;
    }

    // 2. Read Water Level Ultrasonic
    float distCm = measureWaterDistance();
    float waterPct = 0.0;
    if (distCm > 0) {
      float span = EMPTY_DISTANCE_CM - FULL_DISTANCE_CM;
      waterPct = ((EMPTY_DISTANCE_CM - distCm) / span) * 100.0;
      if (waterPct < 0.0) waterPct = 0.0;
      if (waterPct > 100.0) waterPct = 100.0;
    }

    // 3. Read MQ-135 Gas Level
    int rawADC = analogRead(MQ135_PIN);
    float pinV = rawADC * (3.3 / 4095.0);
    float nh3Ppm = pinV * 1.5 * 10.0; // Scaled proxy

    // Safely update shared MODBUS register values
    if (xSemaphoreTake(sensorMutex, portMAX_DELAY) == pdTRUE) {
      reg_weight_g = (uint16_t)weightGrams;
      reg_nh3_x10   = (uint16_t)(nh3Ppm * 10.0);
      reg_water_x10 = (uint16_t)(waterPct * 10.0);
      xSemaphoreGive(sensorMutex);
    }

    // Also stream JSON telemetry over USB Serial for direct USB cable connection
    Serial.print("{\"pen_id\":1,\"feedRemaining\":");
    Serial.print(weightGrams / 1000.0, 2);
    Serial.print(",\"waterLevel\":");
    Serial.print((int)waterPct);
    Serial.print(",\"nh3\":");
    Serial.print(nh3Ppm, 1);
    Serial.println("}");

    vTaskDelay(pdMS_TO_TICKS(500)); // Sample every 500ms
  }
}

void processModbusRequest() {
  if (Serial2.available() < 8) return;

  uint8_t raw[256];
  int rawLen = 0;
  while (Serial2.available() > 0 && rawLen < 256) {
    raw[rawLen++] = Serial2.read();
    delayMicroseconds(100);
  }

  if (rawLen < 8) return;

  // Scan incoming buffer for SLAVE_ID (1) followed by FC 0x03 & valid CRC
  int startIdx = -1;
  for (int i = 0; i <= rawLen - 8; i++) {
    if (raw[i] == SLAVE_ID && raw[i + 1] == 0x03) {
      uint16_t rxCrc = raw[i + 6] | (raw[i + 7] << 8);
      uint16_t calcCrc = calculateCRC(&raw[i], 6);
      if (rxCrc == calcCrc) {
        startIdx = i;
        break;
      }
    }
  }

  if (startIdx == -1) return; // No valid query found in buffer

  uint8_t *frame = &raw[startIdx];
  uint16_t startAddress = (frame[2] << 8) | frame[3];
  uint16_t regCount = (frame[4] << 8) | frame[5];

  if (startAddress + regCount > 3) return;

  uint16_t localWeight = 0, localNh3 = 0, localWater = 0;
  if (xSemaphoreTake(sensorMutex, (TickType_t)10) == pdTRUE) {
    localWeight = reg_weight_g;
    localNh3 = reg_nh3_x10;
    localWater = reg_water_x10;
    xSemaphoreGive(sensorMutex);
  }

  uint16_t registers[3] = { localWeight, localNh3, localWater };

  uint8_t response[256];
  uint8_t respLen = 0;

  response[respLen++] = SLAVE_ID;
  response[respLen++] = 0x03;
  response[respLen++] = regCount * 2; // Byte count

  for (uint16_t i = 0; i < regCount; i++) {
    uint16_t regVal = registers[startAddress + i];
    response[respLen++] = (regVal >> 8) & 0xFF; // MSB
    response[respLen++] = regVal & 0xFF;        // LSB
  }

  uint16_t txCrc = calculateCRC(response, respLen);
  response[respLen++] = txCrc & 0xFF;        // CRC Low
  response[respLen++] = (txCrc >> 8) & 0xFF; // CRC High

  // Transmit Response at 115200 baud
  digitalWrite(DE_RE_PIN, HIGH);
  delayMicroseconds(100);

  Serial2.write(response, respLen);
  Serial2.flush();

  delayMicroseconds(200); // Shift register flush settling at 115200 baud
  digitalWrite(DE_RE_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  sensorMutex = xSemaphoreCreateMutex();

  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW); // Set MAX485 to Receiver mode

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(MQ135_PIN, INPUT);

  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);

  // Initialize MAX485 RS-485 Serial2 at 115200 baud (8N1)
  Serial2.begin(115200, SERIAL_8N1, RX2_PIN, TX2_PIN);

  // Pin sensor polling task to Core 0 (FreeRTOS)
  xTaskCreatePinnedToCore(
    SensorTask,
    "SensorTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );

  Serial.println("\n=========================================");
  Serial.println("  ESP32 MODBUS RTU Slave Node Started    ");
  Serial.println("  Slave ID: 1 | Baud: 115200 | Pin DE/RE: 4");
  Serial.println("=========================================\n");
}

void loop() {
  processModbusRequest(); // Core 1 listens and processes MODBUS requests
  yield();
}
