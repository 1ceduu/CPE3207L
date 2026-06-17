# 🐷 Construction Materials Selection Guide
## Automated Pig Feeder Module — Physical Structure
**CPE3207L – Group V | University of San Carlos**

> All prices in Philippine Peso (₱). Researched from Wilcon Depot, Regan Industrial, Circuitrocks, Shopee PH, and verified Philippine suppliers (June 2026). Always request fresh quotes before purchasing — steel prices fluctuate with global nickel/iron markets.

---

## ⚠️ Critical Disqualifications — Read First

> [!CAUTION]
> **NEVER use Galvanized Iron (GI) sheet for any feed-contact surface.** In a piggery's ammonia + urine acid + moisture environment, zinc coating degrades and leaches zinc ions into feed. This is a feed contamination hazard and violates **PNS/BAFS 282:2019** (Philippines Bureau of Agriculture and Fisheries Standards — Code of Good Animal Feeding). GI is only acceptable for the outer structural frame with no feed exposure.

> [!CAUTION]
> **NEVER use mild steel + paint/epoxy for feed-contact surfaces.** Paint chips and peels; rust particles contaminate feed. Epoxy-coated mild steel is **only acceptable for the structural frame** (Part 6).

> [!WARNING]
> **SS304 and bleach:** Frequent use of high-concentration bleach (sodium hypochlorite) can cause stress corrosion cracking in SS304 over time. Dilute bleach to ≤200 ppm and rinse thoroughly. If aggressive bleach cleaning is planned, upgrade to SS316 (+20–30% cost).

> [!WARNING]
> **Always verify HDPE is "virgin food-grade"** — never recycled HDPE. Recycled HDPE contains unknown contaminants and is NOT food-safe. Request supplier certification.

---

## 📐 System Physical Parts Overview

| Part | Description | Feed Contact? | Key Challenge |
|---|---|---|---|
| **1. Central Hopper** | 102×50×56 cm frustum, holds 90 kg feed | ✅ YES | Structural load + corrosion |
| **2. Auger Tube** | Cylindrical housing for NEMA 17 + auger screw | ✅ YES | Feed friction + hygiene |
| **3. Gate Valve Plate** | 10×10 cm sliding plate (solenoid-actuated) | ✅ YES | Wear + sliding action |
| **4. Feeder Trough ×3** | Per-pen feed deposit area (pig eats here) | ✅ YES | Pig abuse + cleaning |
| **5. Water Basin ×3** | Per-pen water containment w/ ultrasonic sensor | ✅ YES (water) | Water resistance |
| **6. Structural Frame** | Support structure elevating hopper + electronics | ❌ NO | Load bearing + corrosion |
| **7. Feed Chutes ×3** | Pipes connecting auger outlet to troughs | ✅ YES | Flow + cleanliness |

---

## 🏆 Material Suitability Scorecard

| Material | Feed-Safe? | NH₃ Resistance | PH Cost | Cebu Availability | Piggery Rating |
|---|---|---|---|---|---|
| **SS304** | ✅ Yes | ✅ Good | ₱₱₱ | ✅ Available | ⭐⭐⭐⭐⭐ |
| SS316 | ✅ Yes | ✅ Excellent | ₱₱₱₱ (+20–30%) | ⚠️ Limited | ⭐⭐⭐⭐ (overkill) |
| **GI Sheet** | ❌ NO (zinc) | ❌ Poor | ₱ | ✅ Everywhere | ⭐ — DISQUALIFIED |
| **Food-grade HDPE** | ✅ Yes | ✅ Excellent | ₱₱–₱₱₱ | ⚠️ Manila-sourced | ⭐⭐⭐⭐ |
| FRP/GRP | ✅ (food-grade resin) | ✅ Excellent | ₱₱₱ | ⚠️ Limited | ⭐⭐⭐ |
| **Mild Steel + Epoxy** | ❌ Frame only | ⚠️ Coating-dependent | ₱ | ✅ Everywhere | ⭐⭐⭐ (frame only) |
| **PVC Schedule 40** | ✅ Yes (dry feed) | ✅ Excellent | ₱ | ✅ Everywhere | ⭐⭐⭐⭐⭐ (pipes) |
| Aluminum 6061 | ✅ Yes | ⚠️ Moderate | ₱₱₱ | ⚠️ Limited | ⭐⭐ |
| Food-grade PP | ✅ Yes | ✅ Good | ₱₱ | ⚠️ Online only | ⭐⭐⭐ |

---

## Part 1 — Central Feed Hopper

**Specs:** Truncated-pyramid frustum, 102 cm × 50 cm footprint, 56 cm tall (21 cm rectangular upper + 35 cm tapered cone), hinged lid, holds 90 kg dry granular pellets.

### 🥇 Recommendation: SS304 Sheet (1.5 mm walls / 2.0 mm cone section)

**Why SS304:**
- Non-toxic, food-safe — compliant with **PNS/BAFS 282:2019** and RA 10611 (Food Safety Act)
- Passive chromium oxide layer resists NH₃ (10–100 ppm), H₂S traces, and daily hosing
- Non-porous: bacteria cannot harbor on smooth 2B-finish surface
- Industry standard for commercial livestock hoppers (Big Dutchman, San Miguel Foods use SS304 throughout)
- Service life: **10–15 years** in piggery conditions

**Recommended gauge:**
| Section | Thickness | Why |
|---|---|---|
| Rectangular walls + lid | **1.5 mm (16 gauge)** | Sufficient rigidity; reduces weight |
| Cone/frustum section | **2.0 mm (14 gauge)** | Highest abrasive wear from pellet flow; bears full 90 kg static pressure |
| Hinges / flanges | **2.0 mm** | Mechanical connection points need extra strength |

**Philippines Pricing:**

| Item | Cost |
|---|---|
| SS304 sheet (4×8 ft, 1.5 mm) — ~1 sheet needed | ₱3,240–₱6,300 |
| SS304 offcut for 2.0 mm cone section | ₱2,000–₱3,000 |
| SS304 hinges + hardware | ₱150–₱400 |
| **Materials subtotal** | **₱5,500–₱10,000** |
| TIG welding labor (8–12 hrs @ ₱100–₱150/hr, Cebu rate) | ₱3,000–₱6,000 |
| **Total (hopper only)** | **₱8,500–₱16,000** |

**Where to buy (Cebu):** Regan Industrial Sales, Cebu Metal Industries, Cebu Steel Corporation
**Where to buy (Manila/nationwide):** Power Steel Specialist Trading (powersteel.com.ph), Metalrise Enterprise (metalriseenterprise.com)

---

### 🥈 Alternative: Food-Grade HDPE Sheet (10 mm)

**Pros:** Superior chemical resistance; no welding needed (bolt + silicone assembly); students can fabricate
**Cons:** Requires external frame support under 90 kg load (prone to creep at 35°C); shorter lifespan (3–7 years); scratches over time

| Item | Cost |
|---|---|
| 10 mm HDPE sheet (4×8 ft) — 1–1.5 sheets | ₱14,640–₱21,960 |
| SS304 hardware, bolts, food-grade silicone sealant | ₱500–₱1,500 |
| **Total (HDPE hopper, no labor)** | **₱15,000–₱25,000** |

> 💡 HDPE ends up **more expensive** than SS304 in total, and requires Metro Manila shipping. SS304 is the clear overall winner.

---

## Part 2 — Auger / Screw Conveyor Tube

**Specs:** Cylindrical housing for NEMA 17 + auger screw. Carries granular pellets from hopper bottom outlet (10×10 cm) to feeder trough.

### 🥇 Recommendation: PVC Schedule 40, 4-inch Diameter

**Why PVC:**
- **Chemically resistant to NH₃ at ratings "Excellent"** (up to 60°C) — perfect for piggery
- Food-safe for dry granular feed contact — no leaching
- Smooth interior reduces pellet friction and jamming
- Universally available at every Philippine hardware store
- Easy student assembly — cut with hacksaw, join with standard PVC fittings
- Does not rust under any circumstances

| Item | Cost |
|---|---|
| 4-inch PVC Schedule 40 pipe (6m) — cut to ~1 m needed | ₱600–₱1,100 |
| End caps, fittings, support brackets | ₱200–₱500 |
| **Total** | **₱300–₱700** (your cut portion only) |

**Where to buy:** Wilcon Depot, CitiHardware, CW Home Depot, Ace Hardware, any hardware store in Cebu

> **Note:** The **auger screw/flight itself** should be SS304 or food-grade PP for feed contact. The PVC tube is the outer housing only.

---

### 🥈 Alternative: SS304 Round Tube (3-inch OD, 1.5 mm wall)
More rigid and can be welded directly to hopper outlet flange. Approximately ₱750–₱2,100 for the tube portion. Recommended if you want a permanent, professional installation; PVC is sufficient for a student prototype.

---

## Part 3 — Sliding Gate Valve Plate (Guillotine Valve)

**Specs:** Flat plate, 10×10 cm footprint + extension handle, slides to open/close hopper bottom aperture. Driven by 12V DC solenoid. Direct feed contact, sliding wear.

### 🥇 Recommendation: SS304 Plate, 3 mm Thick

**Why SS304:**
- 3 mm plate provides rigidity against bending under pellet column pressure
- Smooth 2B-finish allows reliable low-friction sliding under solenoid force
- Food-safe, corrosion-resistant
- Small piece — can be cut from SS304 sheet offcuts when ordering the hopper (zero extra sheet cost)
- Guide rails/channel should also be SS304 or HDPE (avoid GI or mild steel — will corrode and jam)

| Item | Cost |
|---|---|
| SS304 plate offcut (from hopper order) | ₱0 (use offcut) or ₱200–₱600 |
| SS304 guide rail/channel | ₱300–₱500 |
| Fabrication (cutting + grinding) | ₱500–₱1,000 |
| **Total** | **₱1,000–₱2,100** |

---

### 🥈 Alternative: Food-Grade HDPE Plate, 12 mm
Excellent low-friction sliding; even easier to cut. Risk: under sustained 90 kg pellet column pressure, 10 mm HDPE may creep/deform. Use 12 mm minimum if choosing HDPE. Cost: ₱200–₱600 (offcut from HDPE sheet if purchasing for hopper).

---

## Part 4 — Per-Pen Feeder Trough (×3)

**Specs:** U-shaped or rectangular trough per pen, pigs eat directly from this. Must withstand pig rooting/biting, daily hosing, sanitization. 3 units needed.

### 🥇 Recommendation: SS304, 1.5–2.0 mm, TIG-Welded, Radiused Internal Corners

**Why SS304:**
- **Industry standard #1** choice for Philippine commercial piggery troughs, confirmed by Jeida Farm Supply, Abellar Poultry & Livestock Equipment, RT Trading Cebu
- Non-porous: bacteria cannot harbor — critical for feed hygiene
- Withstands pig rooting and biting better than any plastic
- Easy to sanitize with piggery-grade disinfectants
- Radiused corners prevent feed waste buildup
- Lifespan: 10+ years with daily use and hosing

**Options:**

| Option | Cost |
|---|---|
| **Ready-made SS304 trough** (Shopee/Lazada, RT Trading Cebu) | ₱200–₱2,000 per unit |
| **Custom-fabricated** from local SS shop (Cebu Metal, Ferma Stainless Works) | ₱1,500–₱4,000 per unit |
| **Total for 3 troughs** | **₱600–₱12,000** |

> 💡 Check ready-made sizes first on Shopee/Lazada before custom fabrication — you may find a suitable fit and save significantly. Search: `"stainless steel pig feeder trough"`

**Where to buy (Cebu):** RT Trading (rttradingph.com), JM Poultry & Livestock Supply (jmpoultry.ph), Ferma Stainless Works, Cebu Metal Industries

---

### 🥈 Alternative: Food-Grade HDPE Sheet (10 mm, U-shaped)
Chemically superior to SS304; easier student fabrication (cut and bolt). Pigs can scratch/chew HDPE over time creating bacterial grooves. Shorter lifespan (~3–5 years). Total for 3 troughs: ₱6,900–₱13,500 — more expensive than ready-made SS304 troughs.

---

## Part 5 — Water Trough / Basin (×3)

**Specs:** Small water containment basin per pen, monitored by JSN-SR04T ultrasonic sensor mounted above. 3 units needed. Water contact only (no dry feed).

### 🥇 Recommendation: Food-Grade HDPE Ready-Made Basin/Container

**Why HDPE here (not SS304):**
- For water containment, HDPE is **ideal**: completely inert to water, cleaning chemicals, and piggery atmosphere
- Does not leach anything — fully water-safe
- Ready-made food-grade HDPE rectangular bins are **very cheap and universally available**
- Easy to drill a hole for the ultrasonic sensor probe with a waterproof rubber grommet
- Lightweight, repositionable
- This is the one part where SS304 is overkill

| Item | Cost |
|---|---|
| Ready-made HDPE/food-grade plastic rectangular basin (approx. 30×30×15 cm) | ₱200–₱800 per unit |
| Ultrasonic sensor grommet + waterproof sealant | ₱50–₱150 per unit |
| **Total for 3 basins** | **₱750–₱2,850** |

**Where to buy:** Shopee PH, Lazada PH (search: `"heavy duty food container basin"`), local hardware or agrivet stores in Cebu

---

## Part 6 — Structural Frame

**Specs:** Supports the hopper elevated above pens, holds the auger assembly and electronics enclosure. Must carry 120+ kg total (90 kg feed + hopper + auger + safety factor). **No direct feed contact.**

### 🥇 Recommendation: Mild Steel SHS (40×40 mm or 50×50 mm, 2 mm wall) + Industrial Epoxy Coating

**Why mild steel (and NOT SS304) for the frame:**
- No feed contact → zinc toxicity is not a concern → coating-protected mild steel is acceptable
- **3–4× cheaper** than an SS304 structural frame with equivalent strength
- Mild steel MIG/SMAW welding is far more available and affordable in Cebu than SS304 TIG welding
- With proper **zinc-rich epoxy primer + polyurethane topcoat**, provides 5–10 years protection in piggery atmosphere
- Re-coating in the field is straightforward when paint shows signs of wear

**Recommended coating system:**
1. Surface prep: sandblast or grind to bare metal
2. Coat 1: **Boysen Epoxy Primer** (available at Wilcon, Ace Hardware) — ₱500–₱800/L
3. Coat 2: **Boysen Epoxy Topcoat** or Polyurethane enamel (2 coats) — ₱600–₱1,200/L
4. Re-apply topcoat every **2–3 years** or when visible rust appears

> [!IMPORTANT]
> Use **SS304 bolts and nuts** at all connection joints — never mild steel fasteners. Dissimilar metal (mild steel bolt + SS part or mild steel bolt in humid air) causes galvanic corrosion that will seize or weaken joints.

| Item | Cost |
|---|---|
| Mild steel SHS 40×40mm × 2mm (6m length), ~5–8 pieces | ₱3,500–₱9,600 |
| Epoxy primer + topcoat paint | ₱1,000–₱2,000 |
| SS304 bolts/nuts/hardware | ₱500–₱1,500 |
| MIG welding labor (2–3 welder-days @ ₱500–₱800/day) | ₱1,000–₱2,400 |
| **Total Structural Frame** | **₱6,000–₱15,000** |

**Where to buy (Cebu):** Regan Industrial Sales Inc. (reganindustrial.com), Joyland Industries (Mandaue), ESC Steel Philippines (IT Park), CitiHardware, Wilcon Depot

---

### 🥈 Alternative: SS304 SHS Frame
Eliminates long-term coating maintenance. However, 3–4× more expensive (SS304 SHS ~₱1,500–₱3,000/6m vs ₱700–₱1,200 for mild steel) and requires TIG welding. Recommended only if longevity beyond 10 years is critical.

---

## Part 7 — Connecting Feed Chutes / Pipes (×3)

**Specs:** Pipes connecting auger tube outlet to each pen's feeder trough. Carries dry granular pellets. 3 delivery paths (one per pen). Must be cleanable.

### 🥇 Recommendation: PVC Schedule 40, 3-inch Diameter, Gray (Waterline Grade)

**Why PVC:**
- Same advantages as Part 2 — food-safe, NH₃ resistant, smooth interior for unobstructed pellet flow
- Compatible with all standard PVC elbows, tees, reducers (off-the-shelf at any hardware store)
- Clean by flushing or simple disassembly with union couplings
- **Available at every hardware store in the Philippines — no special sourcing needed**

| Item | Cost |
|---|---|
| 3-inch PVC Schedule 40 pipe (6m), ~3 pieces | ₱1,200–₱2,400 |
| Elbows, tees, couplings (~12 fittings) | ₱360–₱960 |
| PVC cement + primer | ₱150–₱350 |
| Support brackets | ₱200–₱400 |
| **Total (all 3 chutes)** | **₱1,700–₱3,700** |

**Where to buy:** Wilcon Depot, CitiHardware, CW Home Depot, Ace Hardware, True Value — any hardware store in Cebu

---

## 💰 Total Structural Materials Budget

| Part | Recommended Material | Budget Range |
|---|---|---|
| 1. Central Hopper | SS304 1.5/2.0 mm TIG-welded | ₱8,500–₱16,000 |
| 2. Auger Tube | PVC Schedule 40, 4-inch | ₱300–₱700 |
| 3. Gate Valve Plate | SS304, 3 mm (offcut from hopper) | ₱1,000–₱2,100 |
| 4. Feeder Trough ×3 | SS304, ready-made or custom | ₱600–₱12,000 |
| 5. Water Basin ×3 | Food-grade HDPE ready-made bins | ₱750–₱2,850 |
| 6. Structural Frame | Mild steel SHS + industrial epoxy | ₱6,000–₱15,000 |
| 7. Feed Chutes ×3 | PVC Schedule 40, 3-inch | ₱1,700–₱3,700 |
| **GRAND TOTAL** | | **₱18,850–₱52,350** |

> **Realistic mid-range estimate for a student project prototype: ~₱25,000–₱35,000**

> 💡 **Cost-saving tip:** The biggest variable is the SS304 hopper (₱8,500–₱16,000). Get at least **3 quotations** from Cebu fabricators — prices vary significantly. Cebu Metal Industries, Ferma Stainless Works, and Exotic Steel Fabrication (South Road) are your best starting points.

---

## 📋 Regulatory Context (PNS/BAFS 282:2019)

The relevant Philippine standard is **PNS/BAFS 282:2019** — *Code of Good Animal Feeding* — issued by the Bureau of Agriculture and Fisheries Standards (BAFS), enforced by the Bureau of Animal Industry (BAI):

| Requirement | Implication for Your Design |
|---|---|
| Feed-contact surfaces must be **non-toxic** | GI (zinc) and mild steel (rust) are disqualified |
| Surfaces must be **easy to clean and disinfect** | Non-porous SS304 and smooth HDPE comply; concrete fails |
| Must **not contaminate feed** biologically, chemically, or physically | PVC, SS304, HDPE all comply; painted surfaces do not |
| **No specific material prescribed by name** | SS304, food-grade HDPE, PVC, food-grade PP all meet the standard |

> The standard does not mandate SS304 specifically — but it sets requirements that SS304, food-grade HDPE, and PVC all satisfy, while GI and mild steel do NOT for feed-contact surfaces.

---

## 🏪 Key Supplier Directory

### Cebu City (Primary)
| Supplier | Products | Contact |
|---|---|---|
| **Regan Industrial Sales Inc.** | Structural steel, mild steel SHS | reganindustrial.com |
| **RT Trading** | SS piggery troughs, livestock equipment | rttradingph.com |
| **Cebu Metal Industries** | SS304 custom fabrication | Lapu-Lapu City |
| **Ferma Stainless Works** | SS304 fabrication | fermastainlessworks.com |
| **Exotic Steel Fabrication** | SS + mild steel fabrication | South Road, Cebu City |
| **Eukin Industrial Services** | SS304, conveyors | Cebu City |
| **ESC Steel Philippines** | Structural/industrial steel | IT Park, Cebu City |
| **Joyland Industries Corp.** | Industrial steel | Mandaue City |
| **JM Poultry & Livestock** | Farm/piggery equipment | jmpoultry.ph |
| **Wilcon Depot / CitiHardware** | PVC pipes, GI sheets, paint, hardware | Various Cebu locations |

### Metro Manila (For specialty materials)
| Supplier | Products | Contact |
|---|---|---|
| **Power Steel Specialist Trading** | SS304/316 sheets | powersteel.com.ph |
| **Metalrise Enterprise** | SS304, ASTM-certified | metalriseenterprise.com |
| **Panda Construction Supply** | Food-grade HDPE sheets (confirmed pricing) | pandatools.ph / (02) 8236-5500 |
| **Standard Plastics Corp.** | HDPE, PP engineering plastics | stanplas.com.ph |

### Online (Shopee / Lazada)
| Search Term | Item |
|---|---|
| `stainless steel pig feeder trough` | Ready-made SS304 troughs |
| `stainless steel 304 sheet` | SS304 for small pieces/prototyping |
| `food grade HDPE sheet` | HDPE sheet material |
| `4 inch PVC pipe schedule 40` | PVC pipe for auger tube |
| `3 inch PVC pipe` | PVC pipe for feed chutes |
| `SS304 bolt nut M8` | Stainless hardware |
| `boysen epoxy primer` | Coating for structural frame |

---

## ⚒️ Fabrication Labor Rates (Philippine, 2026)

| Service | Rate |
|---|---|
| Mild steel MIG/SMAW welding | ₱500–₱800/day |
| SS304 TIG welding (skilled) | ₱800–₱1,200/day (specialists up to ₱2,500/day) |
| HDPE plastic fabrication | ₱500–₱1,000/day (rare skill; bolt assembly preferred) |
| General fabrication helper | ₱500–₱700/day |
| Powder coating service | ₱200–₱500/kg of steel |
| Metal cutting service | ₱50–₱200 per cut |

> Always request **itemized project quotations** (not hourly rates) from fabrication shops — most quote by the whole job.

---

*Research conducted June 2026. Prices are estimates — verify with fresh supplier quotations before procurement. Philippine steel prices fluctuate with global nickel/iron ore markets.*
