/*
  All-Sensors Integrated Test - Single ESP32 Feeder Node
  CPE3207L Thesis Hardware Testing

  Reads all three feeder-node sensors at once and shows them together on the
  0.96" OLED and the serial monitor:
    - HX711 + load cell  -> feed weight (grams)
    - AJ-SR04M ultrasonic-> water level (cm / % / litres)
    - MQ-135             -> air quality / ammonia proxy (raw + volts)

  This is a BENCH BRING-UP sketch, not the final node firmware. It does NOT
  drive the actuators (auger, solenoids) and does NOT run the MODBUS/RS-485
  slave stack. Those belong in the dual-core firmware described in
  Integration_MODBUS_Guide.md; keep this sketch for verifying that all three
  sensors coexist on one board and read sane values simultaneously.

  Libraries (Arduino Library Manager):
    - "HX711 Arduino Library" by Bogdan Necula
    - "Adafruit SSD1306" + "Adafruit GFX Library"

  ---------------------------------------------------------------------------
  PIN MAP (matches Hardware_Design/02_feeder_node_schematic.svg)
  ---------------------------------------------------------------------------
    Sensor / signal      ESP32 GPIO   Notes
    -----------------    ----------   ----------------------------------------
    HX711 DT (DOUT)      GPIO 32
    HX711 SCK            GPIO 33
    HX711 VCC            3V3
    Ultrasonic TRIG      GPIO 25      direct
    Ultrasonic ECHO      GPIO 26      via 1k series + 2k to GND divider (5V!)
    Ultrasonic VCC       5V           needs true 5V, not 3.3V
    MQ-135 AO            GPIO 34      via 1k series + 2k to GND divider (5V!)
    MQ-135 VCC           5V
    OLED SDA             GPIO 21      I2C
    OLED SCL             GPIO 22      I2C
    OLED VCC             3V3

  ***  PIN CONFLICT WARNING  ***
    In the real feeder node, GPIO21 = DRV8825 ENABLE and GPIO22 = feed-gate
    MOSFET (see the schematic). The OLED on 21/22 is fine for THIS sensors-only
    bench test because no actuators are wired, but the display MUST move (or be
    removed) before the actuator wiring goes in. Do not carry 21/22 = I2C into
    the integrated node firmware.

  All three of the 5V sensor grounds and the divider grounds must share the
  ESP32 GND. Common ground is not optional.
*/

#include "HX711.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- Feature switches ----------------
#define USE_OLED false

// ---------------- HX711 (load cell) ----------------
const int HX711_DT_PIN  = 32;
const int HX711_SCK_PIN = 33;
float calibration_factor = 420.0; // set via serial or edit; see HX711_LoadCell_Test

HX711 scale;
bool  hx711Ready = false;

// ---------------- Ultrasonic (water level) ----------------
const int TRIG_PIN = 25;
const int ECHO_PIN = 26;
const float SOUND_SPEED = 0.0343; // cm/us at ~20C
const int SAMPLE_COUNT  = 5;

// Trough calibration - MEASURE with a dipstick, these are placeholders.
const float EMPTY_DISTANCE_CM = 45.0; // air gap when empty (0 L)
const float FULL_DISTANCE_CM  = 28.0; // air gap when full (MAX_WATER_L)
const float MAX_WATER_L       = 5.0;  // WebsitePrototype/config.js MAX_WATER_L
const int   REFILL_THRESHOLD  = 40;   // config.js DEFAULT_THRESHOLD (%)

// ---------------- MQ-135 (air quality) ----------------
const int MQ135_PIN = 34;             // ADC1_CH6, input-only pin
const float DIVIDER_RESTORE = 1.5;    // 1k/2k divider: Vsensor = Vpin * 1.5
const int   NH3_LIMIT_PPM   = 25;     // config.js NH3_LIMIT_PPM (proxy threshold)

// ---------------- OLED ----------------
#if USE_OLED
  const int  OLED_SDA  = 21;
  const int  OLED_SCL  = 22;
  const int  SCREEN_W  = 128;
  const int  SCREEN_H  = 64;    // 32 for the short module
  const uint8_t OLED_ADDR = 0x3C; // try 0x3D if blank
  Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
  bool oledReady = false;
#endif

// ---------------- Shared reading state ----------------
struct Readings {
  bool  weightValid;
  float weightGrams;

  bool  waterValid;
  float distanceCm;
  float levelPct;
  float litres;

  int   gasRaw;       // 0..4095
  float gasPinV;      // volts at the GPIO
  float gasSensorV;   // reconstructed sensor-side volts
};

// ============================================================
//  Sensor readers
// ============================================================

void readWeight(Readings &r) {
  if (hx711Ready && digitalRead(HX711_DT_PIN) == LOW) {
    scale.set_scale(calibration_factor);
    r.weightGrams = scale.get_units(1); // non-blocking sample
    r.weightValid = true;
  } else {
    r.weightValid = false;
    r.weightGrams = 0.0;
  }
}

float pingOnceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(20); // 20us: AJ-SR04M clones miss the 10us datasheet pulse
  digitalWrite(TRIG_PIN, LOW);

  long durationUs = pulseIn(ECHO_PIN, HIGH, 30000);
  if (durationUs == 0) return -1.0;
  return (durationUs * SOUND_SPEED) / 2.0;
}

void readWater(Readings &r) {
  float samples[SAMPLE_COUNT];
  int valid = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    float d = pingOnceCm();
    if (d > 0) samples[valid++] = d;
    delay(100); // settle between pings; <100ms causes echo bleed on this module
  }

  if (valid < 3) {
    r.waterValid = false;
    return;
  }
  // insertion sort, take median
  for (int i = 1; i < valid; i++) {
    float key = samples[i]; int j = i - 1;
    while (j >= 0 && samples[j] > key) { samples[j + 1] = samples[j]; j--; }
    samples[j + 1] = key;
  }
  float distanceCm = samples[valid / 2];

  float span = EMPTY_DISTANCE_CM - FULL_DISTANCE_CM;
  float levelPct = ((EMPTY_DISTANCE_CM - distanceCm) / span) * 100.0;
  if (levelPct < 0.0)   levelPct = 0.0;
  if (levelPct > 100.0) levelPct = 100.0;

  r.waterValid  = true;
  r.distanceCm  = distanceCm;
  r.levelPct    = levelPct;
  r.litres      = (levelPct / 100.0) * MAX_WATER_L;
}

void readGas(Readings &r) {
  r.gasRaw     = analogRead(MQ135_PIN);
  r.gasPinV    = r.gasRaw * (3.3 / 4095.0);
  r.gasSensorV = r.gasPinV * DIVIDER_RESTORE;
}

// ============================================================
//  Output
// ============================================================

void printSerial(const Readings &r) {
  Serial.print("Feed: ");
  if (r.weightValid) { Serial.print(r.weightGrams, 1); Serial.print(" g"); }
  else               { Serial.print("HX711 not ready"); }

  Serial.print("  |  Water: ");
  if (r.waterValid) {
    Serial.print(r.distanceCm, 1); Serial.print(" cm / ");
    Serial.print(r.levelPct, 0);   Serial.print("% / ");
    Serial.print(r.litres, 2);     Serial.print(" L");
  } else {
    Serial.print("NO ECHO");
  }

  Serial.print("  |  Gas: raw ");
  Serial.print(r.gasRaw);
  Serial.print(" (");
  Serial.print(r.gasSensorV, 2);
  Serial.print(" V)");

  // Flags
  if (r.waterValid && r.distanceCm < 25.0)          Serial.print("  [WATER BLIND ZONE]");
  else if (r.waterValid && r.levelPct < REFILL_THRESHOLD) Serial.print("  [REFILL]");
  Serial.println();
}

void sendJsonSerial(const Readings &r) {
  float feedKg = r.weightValid ? (r.weightGrams / 1000.0) : 0.0;
  if (feedKg < 0.0) feedKg = 0.0;
  float waterPct = r.waterValid ? r.levelPct : 0.0;
  float nh3Ppm = r.gasSensorV * 10.0; // Scaled proxy voltage to PPM

  Serial.print("{\"pen_id\":1,\"feedRemaining\":");
  Serial.print(feedKg, 2);
  Serial.print(",\"waterLevel\":");
  Serial.print(waterPct, 0);
  Serial.print(",\"nh3\":");
  Serial.print(nh3Ppm, 1);
  Serial.println("}");
}

void drawOLED(const Readings &r) {
#if USE_OLED
  if (!oledReady) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("FEEDER NODE - ALL");

  // Feed weight
  display.setCursor(0, 12);
  display.print("Feed : ");
  if (r.weightValid) { display.print(r.weightGrams, 0); display.println(" g"); }
  else               { display.println("HX711 err"); }

  // Water
  display.setCursor(0, 24);
  display.print("Water: ");
  if (r.waterValid) {
    display.print(r.levelPct, 0); display.print("% ");
    display.print(r.litres, 1);   display.println("L");
  } else {
    display.println("NO ECHO");
  }

  // Gas
  display.setCursor(0, 36);
  display.print("Gas  : ");
  display.print(r.gasRaw);
  display.print(" ");
  display.print(r.gasSensorV, 1);
  display.println("V");

  // Status line
  display.setCursor(0, 52);
  if (r.waterValid && r.distanceCm < 25.0)                  display.print("WATER BLIND ZONE");
  else if (r.waterValid && r.levelPct < REFILL_THRESHOLD)   display.print("WATER: REFILL");
  else if (!r.weightValid)                                  display.print("CHECK HX711");
  else                                                      display.print("OK");

  display.display();
#endif
}

// ============================================================
//  Setup / loop
// ============================================================

void handleSerialCommands() {
  // 't' tares the scale; a number sets a new calibration factor
  if (Serial.available() > 0) {
    char c = Serial.peek();
    if (c == 't' || c == 'T') {
      Serial.read();
      scale.tare();
      Serial.println("\n[ACTION] Scale tared.\n");
    } else {
      float f = Serial.parseFloat();
      if (f != 0.0) {
        calibration_factor = f;
        Serial.print("\n[ACTION] Calibration factor = ");
        Serial.println(calibration_factor);
      } else {
        Serial.read();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(MQ135_PIN, INPUT);

  Serial.println("\n=========================================");
  Serial.println("   Feeder Node - All Sensors Integrated  ");
  Serial.println("=========================================");
  Serial.println("HX711 DT/SCK: 32/33 | US TRIG/ECHO: 25/26 | MQ AO: 34");
  Serial.println("Serial: 't' = tare, number = set cal factor");

#if USE_OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setTimeOut(100); // Prevent I2C bus hang if OLED is missing or disconnected

  uint8_t targetAddr = 0;
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) {
    targetAddr = 0x3C;
  } else {
    Wire.beginTransmission(0x3D);
    if (Wire.endTransmission() == 0) {
      targetAddr = 0x3D;
    }
  }

  if (targetAddr != 0) {
    oledReady = display.begin(SSD1306_SWITCHCAPVCC, targetAddr);
    if (oledReady) {
      Serial.print("[OK] OLED ready at I2C address 0x");
      Serial.println(targetAddr, HEX);
    }
  }

  if (!oledReady) {
    Serial.println("[WARN] OLED display not detected (check SDA:21, SCL:22, VCC, GND). Continuing without OLED...");
  }
#endif

  Serial.println("[STEP] Probing HX711 Load Cell (DT:32 / SCK:33)...");
  
  pinMode(HX711_SCK_PIN, OUTPUT);
  digitalWrite(HX711_SCK_PIN, LOW);
  pinMode(HX711_DT_PIN, INPUT_PULLUP);

  // Non-blocking hardware check: DOUT line goes LOW when HX711 is powered and ready
  unsigned long tStart = millis();
  bool hxDetected = false;
  while (millis() - tStart < 250) {
    if (digitalRead(HX711_DT_PIN) == LOW) {
      hxDetected = true;
      break;
    }
    delay(10);
  }

  if (hxDetected) {
    scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
    scale.set_scale(calibration_factor);
    hx711Ready = true;
    Serial.println("[OK] HX711 Load Cell detected and ready.");
  } else {
    hx711Ready = false;
    Serial.println("[WARN] HX711 Load Cell not detected (check 3.3V/5V power, GND, DT:32, SCK:33). Continuing without weight...");
  }
  Serial.println("[STEP] Setup completed successfully. Entering main loop...\n");
  Serial.println("=========================================\n");
}

void loop() {
  Readings r;
  readWeight(r);
  readWater(r);   // ~0.5 s (5 pings x 100 ms)
  readGas(r);

  printSerial(r);
  sendJsonSerial(r);
  drawOLED(r);

  handleSerialCommands();
  delay(500);
}
