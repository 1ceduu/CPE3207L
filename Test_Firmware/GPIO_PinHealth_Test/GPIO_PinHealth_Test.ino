/*
  GPIO Pin Health Test - GPIO 25 / GPIO 26
  CPE3207L Thesis Hardware Testing

  WHY THIS SKETCH EXISTS:
    A damaged GPIO and a dead sensor produce IDENTICAL symptoms in the
    ultrasonic sketches: every reading times out. This sketch removes the
    sensor from the equation and tests the ESP32 pins by themselves, so you
    know which side of the wiring to keep debugging.

    This matters here because the AJ-SR04M's ECHO line idles at 5V. If it ever
    reached GPIO 26 without the 1k/2k divider in place, the pin's input clamp
    may be blown - the pin will then read a constant value no matter what you
    connect to it.

  RUN THIS WITH THE SENSOR COMPLETELY DISCONNECTED.
    Unplug all four sensor wires. Leave only the USB cable.

  --- TEST A: ECHO pin input (GPIO 26) ---
    The sketch prints the state of GPIO 26 twice per second.
    With nothing connected it floats, so the value may drift - that is normal.

    1. Touch a jumper from GPIO 26 ("P26") to 3V3.  Expect: reads HIGH (1)
    2. Touch a jumper from GPIO 26 ("P26") to GND.  Expect: reads LOW  (0)

    Both work  -> the input pin is healthy. The fault is the sensor or wiring.
    Stuck HIGH -> pin likely damaged, or shorted to a supply rail.
    Stuck LOW  -> pin likely damaged, or shorted to ground.

    NOTE: use 3V3, never 5V, for this test. 5V is what kills these pins.

  --- TEST B: TRIG pin output (GPIO 25) ---
    The sketch toggles GPIO 25 once per second and prints what it drives.
    Put a multimeter between GPIO 25 ("P25") and GND.

    Expect: the reading alternates between ~0 V and ~3.3 V in step with the
    serial output. If it sits fixed at one voltage, the output driver is dead.

    A working TRIG must swing to ~3.3 V. The AJ-SR04M triggers reliably from
    3.3 V logic, so this is enough - the trigger does not need level shifting.
*/

const int TRIG_PIN = 25; // Output under test
const int ECHO_PIN = 26; // Input under test

bool trigState = false;
unsigned long lastToggle = 0;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT); // No pullup/pulldown - we want the bare pin

  digitalWrite(TRIG_PIN, LOW);

  Serial.println("");
  Serial.println("=========================================");
  Serial.println("      GPIO Pin Health Test               ");
  Serial.println("=========================================");
  Serial.println("DISCONNECT THE SENSOR BEFORE RUNNING THIS.");
  Serial.println("");
  Serial.println("TEST A - input GPIO 26 (P26):");
  Serial.println("  jumper P26 to 3V3 -> should read HIGH");
  Serial.println("  jumper P26 to GND -> should read LOW");
  Serial.println("  (use 3V3, NEVER 5V)");
  Serial.println("");
  Serial.println("TEST B - output GPIO 25 (P25):");
  Serial.println("  meter P25 to GND -> should alternate 0V / 3.3V");
  Serial.println("=========================================\n");

  delay(1500);
}

void loop() {
  // TEST B: toggle the trigger output once per second
  if (millis() - lastToggle >= 1000) {
    lastToggle = millis();
    trigState = !trigState;
    digitalWrite(TRIG_PIN, trigState ? HIGH : LOW);
  }

  // TEST A: sample the echo input
  int echoState = digitalRead(ECHO_PIN);

  Serial.print("GPIO26 (ECHO in): ");
  Serial.print(echoState ? "HIGH" : "LOW ");
  Serial.print("   |   GPIO25 (TRIG out): ");
  Serial.print(trigState ? "HIGH (~3.3V)" : "LOW  (~0V)");
  Serial.println();

  delay(500);
}
