/*
  AJ-SR04M Waterproof Ultrasonic Water Level Sensor Test - UART MODE
  CPE3207L Thesis Hardware Testing

  WHY THIS SKETCH EXISTS:
    The AJ-SR04M (a JSN-SR04T work-alike) selects its operating mode with a
    resistor pad on the BACK of the module, usually marked R19 (sometimes R27):

      R19 empty/open  -> Mode 0: HC-SR04 compatible TRIG/ECHO pulse mode
                         (use WaterLevel_JSNSR04T_Test instead)
      R19 = 472 (47k) -> Mode 1: AUTOMATIC UART, streams distance continuously
      R19 = 124 (120k)-> Mode 2: UART on request (host sends 0x55)
      R19 = 204 (200k)-> Mode 3: low-power UART

    In UART modes the module IGNORES the TRIG pin entirely and transmits data
    out of the ECHO pin instead. A pulseIn()-based sketch will therefore only
    ever see timeouts plus occasional short blips (UART bits misread as echoes).

    This sketch reads that UART stream. It also handles Mode 2 by sending 0x55.

  PROTOCOL (9600 baud, 8N1), 4 bytes per reading:
      Byte 0: 0xFF   (header)
      Byte 1: DATA_H (distance high byte)
      Byte 2: DATA_L (distance low byte)
      Byte 3: SUM    (checksum)
    distance_mm = (DATA_H << 8) | DATA_L
    valid if SUM == ((0xFF + DATA_H + DATA_L) & 0xFF)

  WIRING - IDENTICAL to the pulse-mode test, no rewiring needed:
    AJ-SR04M Pin     Connections
    ------------     -----------
    VCC           -> ESP32 5V (module will not work on 3.3V)
    GND           -> ESP32 GND
    ECHO (=TX)    -> 1kOhm Resistor -> ESP32 GPIO 26 -> 2kOhm Resistor -> GND
    TRIG (=RX)    -> ESP32 GPIO 25 (only used in Mode 2)

    The divider is still REQUIRED: the module's TX idles at 5V.

  CALIBRATION:
    Set EMPTY_DISTANCE_CM / FULL_DISTANCE_CM by dipstick measurement before
    trusting the level percentage. Ultrasonic reads the AIR GAP, so distance
    is LARGEST when the trough is EMPTY.
*/

const int RX_PIN = 26; // Module ECHO/TX -> ESP32 (via 1k/2k divider)
const int TX_PIN = 25; // Module TRIG/RX <- ESP32 (Mode 2 request only)

// Set true to send 0x55 request byte on TRIG pin (supports Mode 2 - R19=120k).
const bool REQUEST_MODE = true;

// --- Trough calibration (measure with a dipstick before the real test) ---
const float EMPTY_DISTANCE_CM = 45.0; // Air gap when trough is empty (0 L)
const float FULL_DISTANCE_CM  = 28.0; // Air gap when trough is full (MAX_WATER_L)
const float MAX_WATER_L       = 5.0;  // Matches WebsitePrototype/config.js MAX_WATER_L
const int   REFILL_THRESHOLD  = 40;   // Matches config.js DEFAULT_THRESHOLD (percent)

// Set true to dump every raw byte received.
const bool DEBUG_RAW = true;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  Serial.println("");
  Serial.println("=========================================");
  Serial.println("   AJ-SR04M Water Level Test (UART DIAG) ");
  Serial.println("=========================================");
  Serial.println("Target: ESP32 (38-Pin USB-C / NodeMCU ESP-32S)");
  Serial.println("RX: GPIO 26 (P26) <- module ECHO/TX via divider");
  Serial.println("TX: GPIO 25 (P25) -> module TRIG/RX");
  Serial.println("Baud: 9600 | Sending 0x55 trigger pulse");
  Serial.println("=========================================\n");
}

// Read one 4-byte frame. Returns distance in mm, or -1 on timeout/bad checksum.
int readFrameMm() {
  unsigned long deadline = millis() + 500;

  // Hunt for the 0xFF header byte
  while (millis() < deadline) {
    if (Serial2.available()) {
      int b = Serial2.read();
      if (DEBUG_RAW) {
        Serial.print("0x");
        if (b < 0x10) Serial.print("0");
        Serial.print(b, HEX);
        Serial.print(" ");
      }
      if (b == 0xFF) break;
    }
  }
  if (millis() >= deadline) return -1;

  // Collect the remaining 3 bytes
  uint8_t buf[3];
  int got = 0;
  while (got < 3 && millis() < deadline) {
    if (Serial2.available()) {
      buf[got] = (uint8_t)Serial2.read();
      if (DEBUG_RAW) {
        Serial.print("0x");
        if (buf[got] < 0x10) Serial.print("0");
        Serial.print(buf[got], HEX);
        Serial.print(" ");
      }
      got++;
    }
  }
  if (got < 3) return -1;

  uint8_t sum = (uint8_t)((0xFF + buf[0] + buf[1]) & 0xFF);
  if (sum != buf[2]) {
    Serial.println("[WARNING] Checksum mismatch - noise on the line or wrong baud");
    return -1;
  }

  return ((int)buf[0] << 8) | buf[1];
}

void loop() {
  if (REQUEST_MODE) {
    Serial2.write(0x55);
  }

  int distanceMm = readFrameMm();

  if (distanceMm < 0) {
    Serial.println("[WARNING] No valid frame. Check 5V supply, R19 mode pad, and divider.");
  } else {
    float distanceCm = distanceMm / 10.0;

    // Ultrasonic reads the air gap, so level is inverted relative to distance.
    float span = EMPTY_DISTANCE_CM - FULL_DISTANCE_CM;
    float levelPct = ((EMPTY_DISTANCE_CM - distanceCm) / span) * 100.0;
    if (levelPct < 0.0)   levelPct = 0.0;
    if (levelPct > 100.0) levelPct = 100.0;

    float litres = (levelPct / 100.0) * MAX_WATER_L;

    Serial.print("Distance: ");
    Serial.print(distanceCm, 1);
    Serial.print(" cm | Level: ");
    Serial.print(levelPct, 1);
    Serial.print("% (");
    Serial.print(litres, 2);
    Serial.print(" L)");

    if (distanceCm < 25.0) {
      Serial.print("  [BLIND ZONE - reading unreliable, raise the probe]");
    } else if (levelPct < REFILL_THRESHOLD) {
      Serial.print("  [REFILL TRIGGER - below threshold]");
    }
    Serial.println();
  }

  delay(1000);
}
