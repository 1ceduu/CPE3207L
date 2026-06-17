# 📋 Bill of Materials
## Integrated Automated Feeding and Watering System with Centralized Data Monitoring for Commercial Piggeries
**CPE3207L – Group V | University of San Carlos**

> All prices in Philippine Peso (₱). Sourced from Shopee PH, Lazada PH, Circuitrocks, and e-Gizmo Mechatronix (June 2026).

---

## ⚠️ Key Warnings Before You Buy

> [!CAUTION]
> **SIM800L is NOT recommended in 2026.** Globe and Smart are actively sunsetting 2G networks in the Philippines. The SIM800L is 2G-only and may fail to register or send SMS in many areas. **Use the A7670C or Air780E (4G LTE Cat-1) instead** — same AT command set, minimal code changes.

> [!WARNING]
> **Raspberry Pi prices have risen significantly.** The Pi 5 (4GB) is now ~₱5,915 at Circuitrocks. If budget is tight, the **Orange Pi 3B (4GB)** at ~₱2,500–₱3,500 is a practical alternative that runs Python, Flask, and SQLite with full touchscreen support.

> [!IMPORTANT]
> **Cebu physical stores (Bitstoc, Teknica, Manalili St.)** will cover basic hardware (wire, terminal blocks, soldering supplies) but **NOT** specialized IoT modules. Plan to order online **2–3 weeks** before your build date. Previously listed stores (Robot Garden, Handywave, Electrotek, CNC Tech, Hi-Precision, Imelda Electronics) were **not verified** as active electronics component retailers.

---

## 🛒 Complete Bill of Materials

### 🧠 Compute & Control

| # | Component | Recommended Model | Qty | Unit Price | Subtotal | Where to Buy |
|---|---|---|---|---|---|---|
| 1 | **ESP32 Dev Board** | ESP32-WROOM-32 (38-pin, USB-C) | ×4 | ₱150 | ₱600 | Shopee PH, MakerLab Electronics |
| 2 | **Central Controller SBC** | Raspberry Pi 5 (4GB) *(see alt below)* | ×1 | ₱5,915 | ₱5,915 | Circuitrocks (circuit.rocks / Lazada) |
| 2a | **Pi 5 Accessories** | Official 27W PSU + Active Cooler + 32GB microSD | ×1 set | ₱1,200 | ₱1,200 | Circuitrocks, Shopee PH |

> 💡 **Orange Pi 3B (4GB) Alternative:** ~₱2,500–₱3,500 on Shopee/Lazada. Runs Armbian Debian + Python 3 + Flask + SQLite. Saves ₱2,000–₱3,000 vs. Pi 5. Requires a **5V/3A USB-C or barrel-jack power supply** instead of the Pi 27W PSU.

---

### 🖥️ Display

| # | Component | Recommended Model | Qty | Unit Price | Subtotal | Where to Buy |
|---|---|---|---|---|---|---|
| 3 | **7" Touchscreen** | 7" HDMI Capacitive IPS (1024×600), USB touch | ×1 | ₱2,500 | ₱2,500 | Shopee PH, Lazada PH |

> ⚠️ Use HDMI + USB type (not DSI ribbon) for compatibility with both Pi and Orange Pi alternatives.

---

### ⚖️ Sensing

| # | Component | Recommended Model | Qty | Unit Price | Subtotal | Where to Buy |
|---|---|---|---|---|---|---|
| 4 | **Load Cell** | 10 kg single-point bar-type (aluminum, HX711-compatible) | ×3 | ₱100 | ₱300 | Shopee PH (often sold as kit with HX711) |
| 5 | **HX711 ADC Module** | HX711 24-bit sigma-delta amplifier | ×3 | ₱65 | ₱195 | Shopee PH, Lazada PH |
| 6 | **Waterproof Ultrasonic Sensor** | JSN-SR04T v2.0 (IP67 probe, 25–450 cm) | ×3 | ₱300 | ₱900 | Shopee PH, Circuitrocks |
| 7 | **Gas Sensor** | MQ-135 module (analog + digital output) | ×3 | ₱70 | ₱210 | Shopee PH, MakerLab Electronics |
| 13 | **Water Flow Sensor** | YF-S201 Hall-effect (1–30 L/min, 1/2" BSP) | ×1 | ₱100 | ₱100 | Shopee PH, Lazada PH |

> 💡 **Load Cell tip:** Many Shopee listings sell the 10kg load cell + HX711 as a bundle for ₱100–₱170 — better value than buying separately.

> 💡 **MQ-135 tip:** Allow a 48-hour burn-in period for new sensors. Calibrate in fresh air before deployment. Consider the **MQ-137** (ammonia-specific) if higher selectivity is needed.

---

### ⚙️ Actuation & Mechanisms

| # | Component | Recommended Model | Qty | Unit Price | Subtotal | Where to Buy |
|---|---|---|---|---|---|---|
| 8 | **Stepper Motor** | NEMA 17 17HS4401 (1.7A, 44 N·cm, 40mm body) | ×3 | ₱420 | ₱1,260 | Shopee PH, Circuitrocks |
| 9 | **Stepper Driver** | DRV8825 *(preferred over A4988)* | ×3 | ₱90 | ₱270 | Shopee PH, Lazada PH |
| 11 | **Feed Gate Solenoid** | 12V DC push-pull solenoid (5–10N, 10–15mm stroke) | ×3 | ₱200 | ₱600 | Shopee PH, Lazada PH |
| 12 | **Water Solenoid Valve** | 12V DC solenoid valve, 1/2" NPT, Normally Closed, plastic body | ×3 | ₱250 | ₱750 | Shopee PH, Lazada PH |
| 18 | **Servo Motor (Lid)** | MG90S metal gear servo *(not SG90)* | ×1 | ₱150 | ₱150 | Shopee PH, MakerLab Electronics |

> 💡 **DRV8825 over A4988:** Handles up to 2.2A, supports 1/32 microstepping for smoother auger motion, better thermal performance — only ~₱20–₱50 more. **Always add a 100µF capacitor across VCC motor pins** to prevent voltage spikes from damaging the driver.

> ⚠️ **Solenoid duty cycle warning:** Push-pull solenoids are rated for intermittent use (e.g., max 30s ON, 2 min OFF). Design control logic to prevent overheating.

> 💡 **Plastic body solenoid valve:** Choose plastic over brass body — brass corrodes with prolonged ammonia exposure.

---

### 📡 Communication

| # | Component | Recommended Model | Qty | Unit Price | Subtotal | Where to Buy |
|---|---|---|---|---|---|---|
| 10 | **RS-485 Module** | MAX485 TTL-to-RS485 half-duplex module | ×5 | ₱55 | ₱275 | Shopee PH (buy 5-pack for ~₱20/unit) |
| 14 | **LoRa Module** | Ra-02 / SX1278 (433 MHz) with SMA antenna breakout | ×2 | ₱250 | ₱500 | Shopee PH, e-Gizmo, Circuitrocks |
| 15 | **4G LTE/GSM Module** | **A7670C or Air780E Cat-1 LTE** *(NOT SIM800L)* | ×1 | ₱750 | ₱750 | e-Gizmo Mechatronix, Shopee PH |

> 💡 **Why 433 MHz LoRa:** 433 MHz achieves better wall/obstacle penetration than 868/915 MHz and is license-free for low-power use in the Philippines. Typical range: 200–500m in-building, 1–3 km open area.

> 💡 **Ra-02 note:** The Ra-02 has 2mm pitch headers — buy the version with an SMA adapter breakout PCB, otherwise it won't fit standard 2.54mm breadboards/perfboards.

---

### ⚡ Power

| # | Component | Recommended Model | Qty | Unit Price | Subtotal | Where to Buy |
|---|---|---|---|---|---|---|
| 16 | **12V Switching Power Supply** | 12V 10A SMPS (CCTV-type enclosed) | ×2 | ₱400 | ₱800 | Shopee PH, Lazada PH |
| 17 | **5V Buck Converter** | LM2596 step-down adjustable module | ×4 | ₱50 | ₱200 | Shopee PH, Lazada PH |

> 💡 **Power sizing:** Peak current draw — 3× NEMA 17 (~5.1A) + 3× feed solenoids (~1.5A) + 3× water valves (~1.5A) = ~8–10A minimum. A **12V 10A SMPS** is the right call.

> 💡 Use **separate power rails** for steppers/solenoids vs. microcontrollers + sensors. This avoids noise coupling from motor switching into sensor ADC readings.

---

### 🔧 Miscellaneous & Prototyping

| # | Component | Spec | Qty | Estimated Cost |
|---|---|---|---|---|
| 19 | Jumper wires (M-M, M-F, F-F) | 40-pin Dupont sets | several | ₱150 |
| 19 | Perfboard / Universal PCB | 5×7cm boards | 10+ | ₱200 |
| 19 | KF301 Terminal Blocks | 5mm pitch, 2/3-pin | 50-pack | ₱120 |
| 19 | Silicone wire (22 AWG) | Flexible, color-coded | per roll | ₱200 |
| 19 | Heat shrink tube set | Assorted sizes | 1 set | ₱80 |
| — | SIM card (prepaid) | Globe / Smart / DITO | ×1 | ₱40 |
| **Subtotal** | | | | **~₱790** |

---

### 📦 Enclosures & Environmental Protection

| # | Component | Spec | Qty | Unit Price | Subtotal |
|---|---|---|---|---|---|
| 20 | **IP65 Feeder Node Enclosure** | ABS plastic, rubber gasket, ~150×100×75mm | ×3 | ₱150 | ₱450 |
| 20 | **IP65 Central Unit Enclosure** | ABS plastic, rubber gasket, ~300×200×100mm | ×1 | ₱400 | ₱400 |
| 20 | **Cable Glands** | PG7 / PG9 nylon (buy 20-pack) | ×20 | ₱15 | ₱300 |
| 20 | **Desiccant Silica Gel Packs** | 5g sachets (replace every 3–6 months) | 20 pcs | ₱5 | ₱100 |
| 20 | **Acrylic Conformal Coating Spray** | ~₱200–₱300/can; coat all PCBs | ×2 cans | ₱250 | ₱500 |

> [!IMPORTANT]
> **Enclosures are non-negotiable.** Piggery air is saturated with ammonia, hydrogen sulfide, and moisture. All PCBs must be in sealed IP65 enclosures with cable glands at every wire entry. Conformal coat all PCBs before sealing. Failure to do this will result in corrosion and premature failure.

---

## 💰 Total Budget Summary

### Option A: With Raspberry Pi 5

| Category | Subtotal |
|---|---|
| Compute (ESP32 ×4 + Pi 5 + accessories) | ₱7,715 |
| Display (7" touchscreen) | ₱2,500 |
| Sensing (load cells, HX711, ultrasonic, MQ-135, flow) | ₱1,705 |
| Actuation (steppers, drivers, solenoids, servo) | ₱2,780 |
| Communication (RS-485, LoRa, 4G module) | ₱1,525 |
| Power (SMPS, buck converters) | ₱1,000 |
| Misc & Prototyping | ₱790 |
| Enclosures & Protection | ₱1,750 |
| **Subtotal** | **₱19,765** |
| Contingency / Shipping / Spares (+10%) | ₱1,977 |
| **GRAND TOTAL (Pi 5)** | **~₱21,000–₱22,000** |

### Option B: With Orange Pi 3B (Budget-Conscious)

| Swap | Savings |
|---|---|
| Orange Pi 3B (4GB) ~₱3,000 instead of Pi 5 ₱5,915 | Save ~₱2,900 |
| Generic 5V/3A USB-C PSU ~₱250 instead of Pi 27W PSU ~₱800 | Save ~₱550 |
| **Revised Grand Total** | **~₱17,500–₱18,500** |

---

## 🏪 Cebu City Sourcing Guide

| Source | Type | Best For | Notes |
|---|---|---|---|
| **Bitstoc Electronics** (Mabolo) | Physical | Basic parts only | Call ahead to check stock |
| **Teknica Electronic Parts** (Gen. Echavez St.) | Physical | Resistors, caps, basic ICs | Limited IoT modules |
| **Manalili St. / Carbon Market area** | Physical shops | Wire, terminal blocks, soldering supplies | Good for general hardware |
| **Shopee PH** (local sellers) | Online | ESP32, sensors, all IoT modules | 2–5 day shipping to Cebu |
| **Lazada / Circuitrocks store** | Online | Pi, Arduino, sensors | Official stock, reliable |
| **MakerLab Electronics** | Online (Shopee/Lazada) | Wide maker component range | Nationwide shipping |
| **e-Gizmo Mechatronix** | Online (e-gizmo.net + Shopee) | A7670C / Air780E GSM, sensors | Best for 4G LTE modules |

---

## 🔍 Quick Search Terms (Shopee / Lazada)

| Component | Search Term |
|---|---|
| ESP32 Dev Board | `ESP32 WROOM 32 dev board` |
| Raspberry Pi 5 | `Raspberry Pi 5 4GB` (Circuitrocks on Lazada) |
| Orange Pi 3B | `Orange Pi 3B 4GB` |
| 7" Touchscreen | `7 inch HDMI touchscreen monitor capacitive` |
| Load Cell + HX711 | `10kg load cell HX711 kit` |
| Ultrasonic Waterproof | `JSN-SR04T waterproof ultrasonic` |
| Gas Sensor | `MQ-135 gas sensor module` |
| Stepper Motor | `NEMA 17 stepper motor 17HS4401` |
| Stepper Driver | `DRV8825 stepper driver module` |
| RS-485 | `MAX485 RS485 TTL module` |
| Feed Solenoid | `12V push pull solenoid` |
| Water Valve | `12V solenoid valve 1/2 normally closed` |
| Flow Sensor | `YF-S201 water flow sensor` |
| LoRa Module | `Ra-02 SX1278 LoRa 433MHz` |
| 4G GSM Module | `A7670C 4G LTE module` or `Air780E` |
| 12V Power Supply | `12V 10A switching power supply CCTV` |
| Buck Converter | `LM2596 step down buck converter` |
| Servo | `MG90S metal gear servo` |
| IP65 Box | `IP65 project box waterproof enclosure ABS` |
| Cable Glands | `PG7 PG9 cable gland nylon` |

---

## ⚡ Engineering Notes for Piggery Environment

### Against Humidity & Ammonia Corrosion
- **Conformal coat all PCBs** with acrylic spray before sealing (~₱250/can on Shopee)
- **All enclosures: IP65 minimum** with rubber gaskets intact and cable glands at every penetration
- **Desiccant packs** inside every enclosure — replace every 3–6 months
- Use **plastic body** solenoid valves (brass corrodes with ammonia over time)

### Against Vibration (Stepper Motors & Auger)
- Apply **Loctite Blue** thread-locker on all mounting screws
- Add **silicone vibration isolation mounts** under ESP32 PCBs inside enclosures
- **Strain-relief** all cable connections inside enclosures (zip ties + cable clamps)

### Power System Best Practices
- Add **100µF capacitor** across DRV8825 motor power pins (prevents back-EMF damage)
- Add **flyback diodes** or **TVS diodes** on solenoid driver outputs
- Keep high-current motor wiring physically separated from sensor/MCU signal wiring
- Run **separate 12V rails** for motors/solenoids vs. logic circuitry

### MQ-135 Calibration for Piggeries
- Piggery air has **constant baseline NH₃** — always calibrate in fresh air first
- Typical piggery NH₃ levels: 10–50 ppm (alarm threshold: 20–25 ppm)
- Allow **48-hour burn-in** on new MQ sensors before trusting readings
- Plan to **replace sensors annually** in harsh piggery environments

---

*Research conducted June 2026. Prices are estimates and may vary. Verify stock and pricing on Shopee/Lazada before purchasing.*
