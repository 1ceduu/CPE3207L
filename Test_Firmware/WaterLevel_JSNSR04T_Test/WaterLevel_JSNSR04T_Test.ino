/*
  JSN-SR04T / HC-SR04 Waterproof Ultrasonic Water Level Sensor Test
  CPE3207L Thesis Hardware Testing

  Component Info:
    - Sensor: JSN-SR04T v2.0 (Waterproof Ultrasonic Module) or HC-SR04
    - Microcontroller: ESP32-WROOM-32 / NodeMCU ESP-32S (38-pin Type-C Dev Board)

  IMPORTANT HARDWARE NOTES:
    1. VCC MUST be connected to 5V (VIN or 5V rail). JSN-SR04T will NOT operate properly on 3.3V.
    2. ECHO pin outputs a 5V logic signal.
       YOU MUST USE A VOLTAGE DIVIDER (1kΩ series + 2kΩ to GND) on ECHO before connecting to GPIO 26!
       Wiring ECHO directly to a GPIO will damage the ESP32.
    3. MINIMUM DISTANCE WARNING: JSN-SR04T has a blind zone of ~20 cm to 25 cm.
       Objects closer than 20 cm will read incorrectly. Mount the probe so that a FULL
       trough still leaves more than 25 cm of air gap to the sensor face.

  Wiring Setup (matches Hardware_Design/02_feeder_node_schematic.svg):
    JSN-SR04T Pin    Connections
    -------------    -----------
    VCC           -> ESP32 5V (pin labelled "5V" on the 38-pin header)
    GND           -> ESP32 GND
    TRIG          -> ESP32 GPIO 25 (silkscreen "P25", direct connection ok)
    ECHO          -> 1kΩ Resistor -> ESP32 GPIO 26 ("P26") -> 2kΩ Resistor -> GND

  OPTIONAL 0.96" OLED (SSD1306, I2C) - BENCH TESTING ONLY:
    OLED Pin         Connections
    --------         -----------
    VCC           -> ESP32 3V3   (NOT 5V - these modules are 3.3V parts)
    GND           -> ESP32 GND
    SDA           -> ESP32 GPIO 21 ("P21")
    SCL           -> ESP32 GPIO 22 ("P22")

    Set USE_OLED to false to compile without the display (sketch still runs on
    serial alone). Requires libraries "Adafruit SSD1306" + "Adafruit GFX Library"
    from the Arduino Library Manager.

    NOTE: GPIO22/23 are assigned to the MAX485 in the final thesis design
    (02_feeder_node_schematic.svg). That collision does not matter for this
    standalone bench test, but the OLED cannot stay on GPIO22 in the real node.

  CALIBRATION BEFORE TESTING:
    Set EMPTY_DISTANCE_CM and FULL_DISTANCE_CM below by measuring with a dipstick.
    Ultrasonic sensors read the AIR GAP, so distance is largest when the trough is EMPTY.
*/

// ---------------- OLED configuration ----------------
#define USE_OLED true

#if USE_OLED
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>

  const int  OLED_SDA    = 21;
  const int  OLED_SCL    = 22;
  const int  SCREEN_W    = 128;
  const int  SCREEN_H    = 64;   // use 32 if you have the short 128x32 module
  const uint8_t OLED_ADDR = 0x3C; // try 0x3D if the display stays blank

  Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
  bool oledReady = false;
#endif

const int TRIG_PIN = 25; // Trigger pin (Outputs 10us pulse)
const int ECHO_PIN = 26; // Echo pin (Inputs pulse duration, via 2/3 divider)

// Speed of sound in air at 20°C: 343 m/s = 0.0343 cm/us
const float SOUND_SPEED = 0.0343;

// --- Trough calibration (measure these with a dipstick before running the real test) ---
const float EMPTY_DISTANCE_CM = 45.0; // Air gap when trough is empty (0 L)
const float FULL_DISTANCE_CM  = 28.0; // Air gap when trough is full (MAX_WATER_L)
const float MAX_WATER_L       = 5.0;  // Matches WebsitePrototype/config.js MAX_WATER_L
const int   REFILL_THRESHOLD  = 40;   // Matches config.js DEFAULT_THRESHOLD (percent)

// Median filtering: a single ping is too noisy for the +/-1 cm pass criterion.
const int SAMPLE_COUNT = 5;

// Set true to print the raw echo pulse width. Use this when every reading times
// out - it separates "module never fires" (always 0 us) from "module fires but
// the echo is lost or absorbed" (short/erratic values).
const bool DEBUG_RAW = false;

// DIAGNOSTIC MODE: shows raw pulse width, sample spread, and hit count on the
// OLED instead of the level bar. Use this while doing the known-distance sweep
// and while setting the calibration constants. Set false for normal operation.
const bool DIAG_MODE = true;

// Populated by readDistanceOnce/readDistanceMedian for the diagnostic display.
long  lastRawUs   = 0;   // pulse width of the most recent ping
float lastSpread  = 0.0; // max-min across valid samples, in cm
int   lastValid   = 0;   // how many of SAMPLE_COUNT pings returned an echo

// Read one echo, returns distance in cm, or -1.0 on timeout/out-of-range.
float readDistanceOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // 20 us rather than the datasheet's 10 us: AJ-SR04M clones are unreliable
  // at 10 us and frequently ignore the trigger entirely.
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(20);
  digitalWrite(TRIG_PIN, LOW);

  // Timeout 30000 us => ~5 m one-way maximum range
  long durationUs = pulseIn(ECHO_PIN, HIGH, 30000);
  lastRawUs = durationUs;

  if (DEBUG_RAW) {
    Serial.print("  [raw: ");
    Serial.print(durationUs);
    Serial.println(" us]");
  }

  if (durationUs == 0) {
    return -1.0;
  }
  // Distance = (Time * Speed of Sound) / 2 (for round trip)
  return (durationUs * SOUND_SPEED) / 2.0;
}

// Median of SAMPLE_COUNT pings. Returns -1.0 if fewer than 3 valid readings.
float readDistanceMedian() {
  float samples[SAMPLE_COUNT];
  int valid = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    float d = readDistanceOnce();
    if (d > 0) {
      samples[valid++] = d;
    }
    delay(100); // AJ-SR04M needs ~100 ms to settle; 60 ms causes echo bleed
  }

  lastValid = valid;

  if (valid < 3) {
    lastSpread = 0.0;
    return -1.0;
  }

  // Insertion sort (tiny array), then take the middle value
  for (int i = 1; i < valid; i++) {
    float key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = key;
  }

  // Sorted, so spread is simply last minus first. A large spread means the
  // echoes disagree with each other - the reading is not trustworthy no
  // matter how clean the median looks.
  lastSpread = samples[valid - 1] - samples[0];

  return samples[valid / 2];
}

// Draw one frame. distanceCm < 0 means "no valid reading".
void updateDisplay(float distanceCm, float levelPct, float litres) {
#if USE_OLED
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("AJ-SR04M WATER TEST");

  if (distanceCm < 0) {
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.println("NO ECHO");
    display.setTextSize(1);
    display.setCursor(0, 42);
    if (DIAG_MODE) {
      display.print("hits ");
      display.print(lastValid);
      display.print("/");
      display.println(SAMPLE_COUNT);
      display.print("raw ");
      display.print(lastRawUs);
      display.println(" us");
    } else {
      display.println("Check probe cable,");
      display.println("aim at hard surface");
    }
    display.display();
    return;
  }

  // Distance, large
  display.setTextSize(2);
  display.setCursor(0, 14);
  display.print(distanceCm, 1);
  display.println(" cm");

  if (DIAG_MODE) {
    // Calibration view: the numbers that tell you whether to trust the reading
    display.setTextSize(1);
    display.setCursor(0, 34);
    display.print("raw ");
    display.print(lastRawUs);
    display.println(" us");

    display.setCursor(0, 44);
    display.print("spread ");
    display.print(lastSpread, 1);
    display.println(" cm");

    display.setCursor(0, 54);
    display.print("hits ");
    display.print(lastValid);
    display.print("/");
    display.print(SAMPLE_COUNT);
    if (lastSpread > 2.0) {
      display.print("  UNSTABLE");
    } else if (distanceCm < 25.0) {
      display.print("  BLIND?");
    }
    display.display();
    return;
  }

  // Level and volume
  display.setTextSize(1);
  display.setCursor(0, 34);
  display.print("Level ");
  display.print(levelPct, 1);
  display.print("%  ");
  display.print(litres, 2);
  display.println(" L");

  // Progress bar
  int barW = (int)((levelPct / 100.0) * (SCREEN_W - 4));
  display.drawRect(0, 45, SCREEN_W, 10, SSD1306_WHITE);
  display.fillRect(2, 47, barW, 6, SSD1306_WHITE);

  // Status line
  display.setCursor(0, 57);
  if (distanceCm < 25.0) {
    display.print("BLIND ZONE");
  } else if (levelPct < REFILL_THRESHOLD) {
    display.print("REFILL TRIGGER");
  } else {
    display.print("OK");
  }

  display.display();
#endif
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);

#if USE_OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledReady) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("AJ-SR04M WATER TEST");
    display.println("");
    display.println("Starting...");
    display.display();
  } else {
    Serial.println("[WARNING] OLED not found at the configured address.");
    Serial.println("          Try 0x3D, check SDA=21 / SCL=22, or set USE_OLED false.");
  }
#endif

  Serial.println("");
  Serial.println("=========================================");
  Serial.println("   JSN-SR04T Water Level Sensor Test     ");
  Serial.println("=========================================");
  Serial.println("Target: ESP32 (38-Pin USB-C / NodeMCU ESP-32S)");
  Serial.println("Trig Pin: GPIO 25 (P25) | Echo Pin: GPIO 26 (P26)");
  Serial.println("Note: JSN-SR04T blind zone is ~20 cm.");
  Serial.print("Calibration: empty=");
  Serial.print(EMPTY_DISTANCE_CM, 1);
  Serial.print(" cm, full=");
  Serial.print(FULL_DISTANCE_CM, 1);
  Serial.println(" cm");
  Serial.print("Refill threshold: ");
  Serial.print(REFILL_THRESHOLD);
  Serial.println("%");
  Serial.println("=========================================\n");
}

void loop() {
  float distanceCm = readDistanceMedian();

  if (distanceCm < 0) {
    Serial.println("[WARNING] No echo received / Out of range (> 4.5m or < 20cm)");
    updateDisplay(-1.0, 0.0, 0.0);
  } else {
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
      Serial.print("  [BLIND ZONE - reading unreliable, lower the probe]");
    } else if (levelPct < REFILL_THRESHOLD) {
      Serial.print("  [REFILL TRIGGER - below threshold]");
    }
    Serial.println();

    updateDisplay(distanceCm, levelPct, litres);
  }

  delay(1000); // Poll every 1 second
}
