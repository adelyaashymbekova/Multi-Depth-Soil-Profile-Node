# Multi-Depth Soil Profile Node: Measuring Soil Moisture at Multiple Depths

**Project Progress Report**

| | |
|---|---|
| **Student Name** | Ashymbekova Adelia |
| **Group** | EEAIR-25 |
| **Subject** | Electronics Component and Circuit |
| **Topic** | Multi-Depth Soil Profile Node: measuring soil moisture at multiple depths |
| **Research question** | Can an array of multiplexed capacitive soil moisture sensors track the speed and direction of a wetting front during drip irrigation? |
| **Platform** | ESP32 DevKit V1 + CD74HC4067 multiplexer, simulated in Wokwi (wokwi.com) |
| **Language / framework** | C++ (Arduino core for ESP32), no external libraries |

---

## Table of Contents

1. Abstract
2. Introduction and Motivation
3. Objectives
4. Project Structure (files)
5. Component Specifications
6. Schematic and Wiring
7. Formulas Used in the Project
8. Working Description
9. Software Description
10. Step-by-Step Implementation in Wokwi
11. Testing and Results
12. Answer to the Research Question
13. Real-Time Applications
14. Limitations and Future Work
15. Project Progress Status
16. Conclusion
17. References
18. Appendix A: Full Source Code (`sketch.ino`)
19. Appendix B: Circuit File (`diagram.json`)
20. Appendix C: Library List (`libraries.txt`)

---

## 1. Abstract

When a drip emitter waters the soil, the water does not wet all the soil at once. It moves down layer by layer. The moving border between wet and dry soil is called the **wetting front**. This project builds a small measuring node that follows the wetting front at four depths: **5, 10, 20 and 30 cm**.

The node uses an **ESP32** and a **CD74HC4067 16-channel analog multiplexer**, so four soil moisture sensors are read through **only one ADC pin**. The program:

1. selects each sensor with the multiplexer and reads its voltage;
2. converts the raw ADC value to soil moisture in percent (two-point calibration);
3. records a time series every 2 seconds;
4. detects the moment when the wetting front reaches each depth;
5. calculates the **speed** of the front (V = Δd / Δt), the **first wet depth** and the **direction** of movement;
6. shows wet depths with LEDs and switches the (simulated) drip OFF when water reaches 30 cm.

With the demonstration data the system found that the front moved **downward at about 4 cm/min** (best-fit speed 4.11 cm/min). So the answer to the research question is **yes**: a multiplexed sensor array can track both the speed and the direction of the wetting front, if the sensors are calibrated and the sampling is fast enough.

---

## 2. Introduction and Motivation

In Kyrgyzstan and Central Asia, agriculture uses most of the fresh water. Drip irrigation saves water, but farmers often do not know **how deep** the water goes. If the water goes deeper than the plant roots (about 30 cm for vegetables), the water and the fertiliser in it are lost.

One moisture sensor shows only one soil layer. To see **how water moves**, we need several sensors at different depths. The problem is that every analog sensor needs an ADC input, and a microcontroller has only a few good ADC pins. A **multiplexer** solves this: it works like a rotary switch that connects one of 16 inputs to one output.

The question of this project is whether such a cheap system can measure not only "wet or dry", but also **how fast and in which direction** the water moves.

---

## 3. Objectives

1. Connect 4 soil moisture sensors to **one** ESP32 ADC pin through a CD74HC4067 multiplexer.
2. Use only safe **3.3 V** levels for all parts and a common ground.
3. Convert raw ADC values (0–4095) to soil moisture (0–100 %).
4. Record moisture at all depths as a time series.
5. Detect when the wetting front reaches each depth and calculate its speed.
6. Find the first wet depth and the direction (down, up or irregular).
7. Show the state with LEDs and the Serial Monitor; stop the drip when water reaches 30 cm.
8. Build and test the whole system in the Wokwi simulator.

---

## 4. Project Structure (files)

```
Soil-Profile-Node/
├── sketch.ino               -> ESP32 program: multiplexer, ADC, moisture %, wetting front analysis
├── diagram.json             -> Wokwi circuit: all parts and wires
├── libraries.txt            -> empty (no external libraries are needed)
├── images/
│   └── circuit_diagram.png  -> schematic drawing
└── README.md                -> this report
```

Internal structure of `sketch.ino`:

```
sketch.ino
├── 1. Pin assignments          (GPIO numbers for mux, ADC, LEDs, button)
├── 2. Sensor configuration     (depths, mux channels, DRY/WET calibration)
├── 3. Settings                 (sample interval, threshold, TIME_SCALE)
├── 4. Data structures          (DepthState struct, time-series ring buffer)
├── 5. selectMuxChannel(), readMuxChannelRaw(), rawToMoisturePercent()
├── 6. startIrrigationEvent(), updateLeds()
├── 7. detectWettingFront(), regressionSpeed(), frontDirection()
├── 8. printSampleLine(), printComparisonTable(), printFinalReport()
├── 9. handleButton(), handleSerial()
└── 10. setup() and loop()      (non-blocking timing with millis())
```

---

## 5. Component Specifications

### 5.1 Components used in the Wokwi simulation

| # | Component | Wokwi part type | Qty | Role in project |
|---|---|---|---|---|
| 1 | ESP32 DevKit V1 | `wokwi-esp32-devkit-v1` | 1 | Main controller, reads sensors, calculates speed |
| 2 | CD74HC4067 multiplexer | `board-cd74hc4067` | 1 | Connects one of the 4 sensors to the ADC |
| 3 | Potentiometer | `wokwi-potentiometer` | 4 | Simulates the **capacitive soil moisture sensors** at 5, 10, 20, 30 cm |
| 4 | Green LED | `wokwi-led` | 4 | "Water reached this depth" |
| 5 | Blue LED | `wokwi-led` | 1 | "Drip irrigation is ON" (represents the pump/valve) |
| 6 | Resistor 220 Ω | `wokwi-resistor` | 5 | LED current limiting |
| 7 | Push button | `wokwi-pushbutton` | 1 | Starts a new irrigation event |

### 5.2 Detailed specifications

**ESP32 DevKit V1 (ESP32-WROOM-32)**

| Parameter | Value |
|---|---|
| Processor | Dual-core Xtensa LX6, up to 240 MHz |
| Memory | 520 KB SRAM, 4 MB flash |
| Logic level | 3.3 V (pins are not 5 V tolerant) |
| ADC | 12-bit (0–4095); ADC1 = GPIO32–39 works together with Wi-Fi |
| Wireless | Wi-Fi 802.11 b/g/n, Bluetooth 4.2 / BLE |
| Used in this project | 21 % of flash, 7 % of RAM |

**CD74HC4067 16-channel analog multiplexer**

| Parameter | Value |
|---|---|
| Channels | 16 inputs (C0–C15) → 1 common output (SIG/COM) |
| Control | 4 select pins S0–S3 (binary channel number) + EN (active LOW) |
| Supply voltage | 2–6 V (3.3 V used here) |
| On-resistance | about 70 Ω at 4.5 V supply |
| Signal | analog, 0 V … VCC, works in both directions |

Channel selection (truth table for the channels used):

| S3 | S2 | S1 | S0 | Channel | Sensor |
|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | C0 | 5 cm |
| 0 | 0 | 0 | 1 | C1 | 10 cm |
| 0 | 0 | 1 | 0 | C2 | 20 cm |
| 0 | 0 | 1 | 1 | C3 | 30 cm |

**Capacitive soil moisture sensor v1.2 (real part, simulated by a potentiometer)**

| Parameter | Value |
|---|---|
| Supply | 3.3–5.5 V |
| Output | analog voltage: about 2.5 V in dry soil, about 1.2 V in water (3.3 V supply) |
| Principle | the soil is the dielectric of a capacitor; water (ε ≈ 80) raises the capacitance, so the output voltage falls |
| Advantage | no metal in contact with soil → no corrosion |

**Potentiometer (simulation of the sensor)**

| Parameter | Value |
|---|---|
| Type | 10 kΩ linear rotary |
| Output | 0–3.3 V on SIG → ADC 0–4095 |
| Use | turn **left = wetter**, turn **right = drier** (same direction as a real sensor: lower voltage = wetter) |

**LEDs + 220 Ω resistors:** green LED current = (3.3 − 2.0) V / 220 Ω ≈ **5.9 mA**, safe for an ESP32 pin (recommended ≤ 20 mA).

**Push button:** connected between GPIO4 and GND, uses the ESP32 internal pull-up, 50 ms software debounce.

### 5.3 Real hardware equivalents

| Simulation part | Real component | Notes |
|---|---|---|
| Potentiometer × 4 | Capacitive soil moisture sensor v1.2 × 4 | Must be calibrated in dry and wet soil |
| Blue LED "drip" | Relay module + 12 V solenoid valve or pump | Own power supply, flyback diode |
| Direct wires | Breadboard + jumper wires, long cable to sensors | Sensors installed in a soil column |

---

## 6. Schematic and Wiring

The circuit is defined in `diagram.json` and is drawn automatically by Wokwi. A drawing of the circuit is in `images/circuit_diagram.png`.

![Circuit diagram](images/circuit_diagram.png)

### 6.1 Block diagram

```
   SOIL PROFILE                                      ┌──────────────────────────┐
 ┌──────────────┐                                    │                          │──▶ Green LEDs (GPIO25,26,27,32)
 │ 5 cm  sensor │──C0─┐                              │         ESP32            │     "depth is wet"
 │ 10 cm sensor │──C1─┤   ┌─────────────────┐  SIG   │       DevKit V1          │
 │ 20 cm sensor │──C2─┼──▶│  CD74HC4067     │───────▶│ GPIO34 (ADC1)            │──▶ Blue LED (GPIO33)
 │ 30 cm sensor │──C3─┘   │  16:1 MUX       │        │                          │     "drip ON"
 └──────────────┘         └────────▲────────┘        │  moisture %, time series │
                                   │ S0–S3, EN       │  front arrival, speed,   │──▶ Serial Monitor
                                   └─────────────────│  direction               │
                         GPIO18,19,21,22,23          │                          │◀── Button (GPIO4)
                                                     └──────────────────────────┘
```

### 6.2 Pin connection table

| Component | Component pin | ESP32 pin | Wire colour |
|---|---|---|---|
| CD74HC4067 | VCC | 3V3 | red |
| CD74HC4067 | GND | GND | black |
| CD74HC4067 | S0 | **D18 (GPIO18)** | blue |
| CD74HC4067 | S1 | **D19 (GPIO19)** | blue |
| CD74HC4067 | S2 | **D21 (GPIO21)** | blue |
| CD74HC4067 | S3 | **D22 (GPIO22)** | blue |
| CD74HC4067 | EN | **D23 (GPIO23)** | orange |
| CD74HC4067 | SIG / COM | **D34 (GPIO34, ADC1_CH6)** | green |
| Sensor 1 (5 cm) | VCC / SIG / GND | 3V3 / mux **C0 (I0)** / GND | red / green / black |
| Sensor 2 (10 cm) | VCC / SIG / GND | 3V3 / mux **C1 (I1)** / GND | red / green / black |
| Sensor 3 (20 cm) | VCC / SIG / GND | 3V3 / mux **C2 (I2)** / GND | red / green / black |
| Sensor 4 (30 cm) | VCC / SIG / GND | 3V3 / mux **C3 (I3)** / GND | red / green / black |
| Green LED 1 (5 cm) | Anode via 220 Ω / Cathode | **D25 (GPIO25)** / GND | gold / black |
| Green LED 2 (10 cm) | Anode via 220 Ω / Cathode | **D26 (GPIO26)** / GND | gold / black |
| Green LED 3 (20 cm) | Anode via 220 Ω / Cathode | **D27 (GPIO27)** / GND | gold / black |
| Green LED 4 (30 cm) | Anode via 220 Ω / Cathode | **D32 (GPIO32)** / GND | gold / black |
| Blue LED (drip) | Anode via 220 Ω / Cathode | **D33 (GPIO33)** / GND | gold / black |
| Push button | pin 1 / pin 2 | **D4 (GPIO4)** / GND | purple / black |

*Note: on the breakout board the channels are called C0–C15; in Wokwi they are called I0–I15.*

### 6.3 Why these pins

* **GPIO34** is an ADC1 pin and input-only — ideal for an analog signal. ADC1 still works when Wi-Fi is on (ADC2 does not), which is important for a future IoT version.
* **GPIO18, 19, 21, 22, 23** are normal digital outputs that do not affect booting.
* **GPIO25, 26, 27, 32, 33** drive the LEDs; **GPIO4** has an internal pull-up for the button.
* Flash pins (GPIO6–11) are not used, and **no pin is used twice**.
* Everything runs from **3.3 V**, so the ADC input can never get more than 3.3 V.

### 6.4 Break-before-make switching

```
EN = HIGH  (all channels OFF)  →  write channel number on S0–S3  →  EN = LOW (channel ON)  →  wait 0.5 ms  →  read ADC
```

This order makes sure that two sensors are never connected together for a short moment.

---

## 7. Formulas Used in the Project

### 7.1 ADC conversion

```
V = raw × 3.3 / 4095          [V]      (12-bit ADC, 0–3.3 V)
```

Example: raw = 2010 → V = 1.62 V. One ADC step = 3.3 / 4095 ≈ 0.8 mV.
To reduce noise the program averages 16 readings: `raw_avg = (raw1 + … + raw16) / 16`.

### 7.2 Soil moisture percentage (two-point calibration)

```
Moisture (%) = (DRY − raw) / (DRY − WET) × 100        limited to 0…100 %
```

With DRY = 3000 and WET = 1200: raw = 2010 → (3000 − 2010) / 1800 × 100 = **55 %**.

⚠️ **The calibration values depend on the real sensor and the soil** (soil type, salt, temperature). Each real sensor must be measured in dry soil and in saturated soil of the same type.

### 7.3 Moisture change over time

```
ΔM = M(t) − M_baseline                       [percentage points]
R  = (M2 − M1) / (t2 − t1)                   [% per minute]
```

Example (5 cm): from 20 % to 55 % in 2 min → R = 35 / 2 = **17.5 %/min**.

### 7.4 Arrival threshold

```
M_threshold = M_baseline + 20
```

A relative threshold is used because different layers start with different moisture.

### 7.5 Arrival time with linear interpolation

```
t* = t1 + (M_threshold − M1) / (M2 − M1) × (t2 − t1)
```

Example (5 cm, baseline 20 %, threshold 40 %): between 0 min (20 %) and 2 min (55 %):
t* = 0 + (40 − 20) / (55 − 20) × 2 = **1.14 min**.

### 7.6 Wetting front speed

```
V = Δd / Δt          [cm/min]
```

* Δd = distance between two sensors (cm)
* Δt = difference of their arrival times (min)

Example: 10 cm reached at 3.20 min, 20 cm at 5.26 min → V = 10 / 2.06 = **4.86 cm/min**.

### 7.7 Best-fit (regression) speed using all sensors

```
V_fit = Σ (ti − t̄)(di − d̄) / Σ (ti − t̄)²
```

Example: t = 1.14, 3.20, 5.26, 7.36 min; d = 5, 10, 20, 30 cm → **V_fit = 4.11 cm/min**.

### 7.8 Direction

* arrivals 5 → 10 → 20 → 30 cm = **DOWNWARD** (infiltration from the dripper)
* arrivals 30 → 20 → 10 → 5 cm = **UPWARD** (capillary rise from wet soil below)
* mixed order = **IRREGULAR** (preferential flow through cracks or roots)

---

## 8. Working Description

### 8.1 How the system works (step by step)

1. **Start-up:** all LEDs flash once (lamp test). The first irrigation event starts automatically.
2. **Baseline:** the ESP32 reads every sensor 5 times and saves the average as the "dry" starting value. The threshold for each depth = baseline + 20 points. The blue LED (drip ON) turns on.
3. **Every 2 seconds** the ESP32:
   1. selects channel 0, 1, 2, 3 on the multiplexer one after another;
   2. reads each sensor on GPIO34 (16 readings averaged);
   3. converts the raw values to moisture %;
   4. calculates the rate of change (%/min) for each depth;
   5. saves the values in the time-series buffer (last 60 samples);
   6. checks if any depth has crossed its threshold;
   7. prints one data line to the Serial Monitor.
4. **When a depth becomes wet:** the program calculates the exact arrival time (interpolation), turns on that green LED, prints the speed from the previous wet depth and the direction.
5. **When the 30 cm sensor becomes wet:** the blue LED (drip) turns OFF — water has reached the bottom of the root zone, so watering should stop.
6. **When all 4 depths are wet:** the program prints a final report (arrival times, first wet depth, direction, all speeds, average and best-fit speed).
7. **Every 5 samples** a comparison table shows raw value, voltage, moisture, change since start and rate for every depth.
8. The **button** (or typing `s`) starts a new irrigation event with a new baseline.

### 8.2 Flowchart

```
          ┌──────────────┐
          │    START     │
          └──────┬───────┘
                 ▼
      Lamp test, measure BASELINE
      threshold = baseline + 20, drip ON
                 ▼
     ┌─────────────────────────────┐
     │ every 2 s: for each depth   │◀───────────────────────────┐
     │  select mux channel         │                            │
     │  read ADC (16× average)     │                            │
     │  raw → moisture %           │                            │
     └──────────────┬──────────────┘                            │
                    ▼                                           │
         moisture ≥ threshold                                   │
         and not wet before? ──No──────────────────────────────▶┤
                    │Yes                                        │
                    ▼                                           │
     arrival time (interpolation), LED ON                       │
     first depth? → print "FIRST WET DEPTH"                     │
     else → V = Δd/Δt, direction                                │
                    ▼                                           │
         30 cm wet? ──Yes──▶ drip OFF (blue LED off)            │
                    ▼                                           │
         all 4 wet? ──Yes──▶ print FINAL REPORT                 │
                    ▼                                           │
         print data line (+ table every 5 samples) ─────────────┘
```

### 8.3 Serial Monitor output (example)

```
t=  1.17 min |  5cm: 40.4%+ | 10cm: 19.2%+ | 20cm: 15.6%  | 30cm: 12.0%  | drip ON

>>> WETTING FRONT reached  5 cm at t = 1.14 min (arrival #1)
>>> FIRST WET DEPTH:  5 cm
>>> Speed from surface (0 cm) to  5 cm: 4.37 cm/min
```

`+` means the soil is getting wetter, `W` means the front has already passed this depth.

---

## 9. Software Description

### 9.1 Libraries

| Library | Purpose |
|---|---|
| Arduino core for ESP32 (built-in) | `analogRead`, `digitalWrite`, `millis`, `Serial.printf` |

No external libraries are used, so `libraries.txt` contains only comments.

### 9.2 Key programming techniques

* **Non-blocking timing with `millis()`** — the button and serial commands work at any time.
* **Struct `DepthState`** keeps all values of one depth together (moisture, baseline, threshold, arrival time, order).
* **Ring buffer** stores the last 60 samples for the "change over time" table.
* **Linear interpolation** gives a more exact arrival time than the 2-second sampling step.
* **Least-squares regression** uses all arrival points for one robust speed value.
* **All settings are constants at the top** of the file, so the node can be adapted to other depths, sensors or soils without changing the logic.
* **Serial commands:** `s` = new event, `r` = report now, `c` = CSV output for Excel, `h` = help.

### 9.3 Configuration constants

| Constant | Value | Meaning |
|---|---|---|
| `DEPTH_CM[]` | 5, 10, 20, 30 | Sensor depths (cm) |
| `DRY_RAW[]` / `WET_RAW[]` | 3000 / 1200 | Calibration (raw ADC) |
| `SAMPLE_INTERVAL_MS` | 2000 | One measurement of all depths every 2 s |
| `ADC_SAMPLES` | 16 | Readings averaged per channel |
| `RISE_THRESHOLD` | 20 | Percentage points above baseline = "front arrived" |
| `BASELINE_SAMPLES` | 5 | Readings for the baseline |
| `TIME_SCALE` | 1.0 | 1 = real time; e.g. 30 → 1 s in Wokwi = 30 s in the field |
| `HISTORY_LEN` | 60 | Samples stored in RAM |

### 9.4 Compilation check

The code was compiled for the ESP32 DevKit V1 (arduino-esp32 core 2.0.17) with all warnings enabled: **0 errors, 0 warnings**, 21 % of flash and 7 % of RAM used.

---

## 10. Step-by-Step Implementation in Wokwi

1. Open **https://wokwi.com** and sign in (free account, so the project can be saved and shared).
2. Click **ESP32 → Start from scratch** to make a new ESP32 project.
3. **`sketch.ino`:** delete the template code and paste the full code from Appendix A.
4. **`diagram.json`:** open the `diagram.json` tab, delete everything, paste Appendix B. The ESP32, the CD74HC4067, 4 potentiometers, 5 LEDs, 5 resistors and the button appear with all wires.
5. **Libraries:** nothing to install. (Optional: create `libraries.txt` and paste Appendix C.)
6. Click **Save**, then the green **▶ Start Simulation** button.
7. The Serial Monitor opens under the circuit. All LEDs flash, then the blue "drip" LED stays on.
8. **Demonstration:**
   * Turn all 4 knobs to the **right** until each depth shows about 10–20 % (dry soil). Press the **green button** to take a new baseline.
   * Turn the **5 cm** knob to the **left** (wetter) → message "WETTING FRONT reached 5 cm", green LED 1 ON.
   * After a few seconds do the same with **10 cm**, then **20 cm**, then **30 cm** → speeds and direction "DOWNWARD" are printed.
   * When 30 cm is wet → the blue LED turns OFF and the final report is printed.
   * Try the opposite order (30 → 5 cm) → direction "UPWARD".
9. Take screenshots of the circuit, the Serial Monitor and the LEDs, and add them to the report.
10. Share the project link (**Save → Share**).

**Suggested demo sequence (≈2 minutes):**

| Step | Action | Expected result |
|---|---|---|
| 1 | All knobs right (≈15 %), press button | Baselines and thresholds printed, drip ON |
| 2 | 5 cm knob left to ≈60 % | Front at 5 cm, "FIRST WET DEPTH: 5 cm", LED 1 ON |
| 3 | 10 cm knob left | Speed 5 → 10 cm, "DOWNWARD", LED 2 ON |
| 4 | 20 cm knob left | Speed 10 → 20 cm, LED 3 ON |
| 5 | 30 cm knob left | Speed 20 → 30 cm, drip OFF, final report |
| 6 | Type `c` | CSV data lines for Excel |

---

## 11. Testing and Results

The program was tested on a PC test program that replaces the ESP32 multiplexer and ADC with a software model playing these demonstration values (illustrative values, **not real field measurements**):

| Time | 5 cm | 10 cm | 20 cm | 30 cm |
|---|---|---|---|---|
| 0 min | 20 % | 18 % | 15 % | 12 % |
| 2 min | 55 % | 20 % | 16 % | 12 % |
| 4 min | 70 % | 50 % | 18 % | 13 % |
| 6 min | 75 % | 65 % | 45 % | 15 % |
| 8 min | 78 % | 70 % | 60 % | 40 % |

**Program output (final report):**

```
################ WETTING FRONT REPORT ################
Depth | Baseline | Peak   | Arrival time | Order
  5cm |   20.0%  |  77.1% |     1.14 min | 1
 10cm |   18.0%  |  68.4% |     3.20 min | 2
 20cm |   15.0%  |  55.2% |     5.26 min | 3
 30cm |   12.0%  |  32.1% |     7.36 min | 4
First depth to become wet: 5 cm
Direction of movement   : DOWNWARD (infiltration from the surface dripper)
Segment speeds (V = delta_d / delta_t):
   5 -> 10 cm : 5 cm / 2.06 min = 2.43 cm/min
  10 -> 20 cm : 10 cm / 2.06 min = 4.86 cm/min
  20 -> 30 cm : 10 cm / 2.10 min = 4.75 cm/min
Average speed  5 -> 30 cm   : 4.02 cm/min
Best-fit speed (regression): 4.11 cm/min
######################################################
```

**Check by hand:** the arrival times and speeds were also calculated by hand with the formulas from Section 7 and they are the same as the program output.

| Segment | Δd (cm) | Δt (min) | V (cm/min) |
|---|---|---|---|
| 0 → 5 cm (from dripper) | 5 | 1.14 | 4.37 |
| 5 → 10 cm | 5 | 2.06 | 2.43 |
| 10 → 20 cm | 10 | 2.06 | 4.86 |
| 20 → 30 cm | 10 | 2.10 | 4.75 |
| 5 → 30 cm (average) | 25 | 6.22 | **4.02** |

**Discussion.** The deeper the sensor, the later its moisture jumps, so the water clearly moves down. The 5 cm layer rises fast first (17.5 %/min), then slowly, because it is close to saturation — an S-shaped curve. Without interpolation (using only the first sample above the threshold: 2, 4, 6, 8 min) the average speed would be 25 / 6 = 4.17 cm/min, about 4 % different, which shows that interpolation improves the result.

---

## 12. Answer to the Research Question

**"Can an array of multiplexed capacitive soil moisture sensors track the speed and direction of a wetting front during drip irrigation?"**

* **Direction — yes.** The order in which the sensors become wet directly shows the direction (down, up or irregular).
* **Speed — yes, with limits.** V = Δd / Δt gives the speed between sensors; the regression gives one average speed (≈4 cm/min in the demo).
* **The multiplexer is not a problem.** Switching a channel takes less than 1 ms, while water needs minutes to move between sensors, so reading the sensors one after another is fast enough.
* **The accuracy is limited by:**
  * **sampling interval** — the time error of one segment is about (sample interval / travel time); with 2 s sampling and 2 min travel time it is under 2 %;
  * **sensor spacing** — we only know the front is somewhere between two sensors;
  * **calibration** — values depend on soil type and salinity;
  * **soil** — cracks and roots can make the flow irregular.

---

## 13. Real-Time Applications

1. **Smart drip irrigation:** the controller stops watering when the wetting front reaches the bottom of the root zone (e.g. 30 cm) — this saves water, electricity and fertiliser.
2. **Greenhouses and orchards in Kyrgyzstan** (Chüy valley, Issyk-Kul region): choosing the right watering time for each crop and soil.
3. **Checking the irrigation system:** if the front speed suddenly changes, a dripper may be blocked or the soil may be cracked or compacted.
4. **Protecting groundwater:** stopping water before it goes below the roots means less nitrate is washed into groundwater.
5. **Soil research:** measuring how fast water enters sand, loam or clay (soil hydraulic properties).
6. **Education:** a clear demonstration of infiltration for school and university labs.
7. **IoT farm network:** with ESP32 Wi-Fi, many nodes can send their data to a dashboard or the farmer's phone.
8. **Sports fields and parks:** watering grass only as deep as its roots.

---

## 14. Limitations and Future Work

**Limitations of the current prototype**

* **Simulated sensors:** Wokwi has no capacitive soil moisture sensor, so potentiometers are used. They give the same type of signal, but no real soil physics or noise.
* The Wokwi multiplexer is ideal (no on-resistance, no switching delay); the code still waits 0.5 ms so it also works with a real chip.
* Calibration values are typical values, not measured for a real soil.
* 4 sensors give only 4 points of the profile; one vertical column cannot see sideways flow.
* In the live demo the knobs are turned by hand, so speeds are faster than in a field (`TIME_SCALE` can correct this).

**Future improvements**

1. Build the real prototype with 4 capacitive sensors in a transparent soil column.
2. Calibrate each sensor in dry and saturated soil.
3. Add more sensors (up to 16 on one multiplexer) for better resolution.
4. Control a real relay and solenoid valve instead of the blue LED.
5. Send data via Wi-Fi (MQTT / ThingSpeak) to a phone dashboard; save data to an SD card.
6. Add a temperature sensor for temperature compensation.
7. Fit an infiltration model (e.g. Green–Ampt) to the arrival times.

---

## 15. Project Progress Status

| Task | Status |
|---|---|
| Topic study (wetting front, capacitive sensors, multiplexer) | ✅ Done |
| Component selection and specifications | ✅ Done |
| Circuit design in Wokwi (`diagram.json`) | ✅ Done |
| Schematic drawing (`images/circuit_diagram.png`) | ✅ Done |
| Multiplexer control and ADC reading | ✅ Done |
| Moisture calibration formula | ✅ Done |
| Wetting front detection, speed, direction | ✅ Done |
| LEDs, button, Serial Monitor output, CSV mode | ✅ Done |
| Testing with demonstration data | ✅ Done |
| Real prototype with capacitive sensors and soil column | ⏳ Planned |
| Calibration in real soil and real experiment | ⏳ Planned |
| Video demonstration and final analysis | ⏳ Planned |

---

## 16. Conclusion

A multi-depth soil moisture node was designed and simulated in Wokwi. An ESP32 reads four soil moisture sensors at 5, 10, 20 and 30 cm through one CD74HC4067 multiplexer and one ADC pin. The program converts the readings to moisture percentage, keeps a time series, detects when the wetting front reaches each depth, and calculates the first wet depth, the direction and the speed of the front. With the demonstration data the front moved downward at about 4 cm/min, and the program results match the hand calculations. The system answers the research question positively: a multiplexed sensor array can track the speed and direction of a wetting front. The next step is to build the real prototype and test it with real soil.

---

## 17. References

1. Texas Instruments. *CD74HC4067, CD74HCT4067 High-Speed CMOS Logic 16-Channel Analog Multiplexer/Demultiplexer — Datasheet*. https://www.ti.com/product/CD74HC4067
2. Espressif Systems. *ESP32 Series Datasheet* and *ESP32 Technical Reference Manual*.
3. Wokwi documentation: ESP32 simulation, `wokwi-potentiometer`, `wokwi-led`, `wokwi-pushbutton`. https://docs.wokwi.com
4. Hillel, D. (2004). *Introduction to Environmental Soil Physics*. Elsevier Academic Press. (infiltration and wetting front)
5. Green, W. H., Ampt, G. A. (1911). Studies on soil physics: Flow of air and water through soils. *Journal of Agricultural Science*, 4, 1–24.
6. Capacitive Soil Moisture Sensor v1.2 — product description and application notes (DFRobot SEN0193 and compatible modules).

---

## 18. Appendix A: Full Source Code (`sketch.ino`)

```cpp
/*
 * ============================================================================
 *  Multi-Depth Soil Profile Node
 *  Measuring soil moisture at 5, 10, 20 and 30 cm and tracking the
 *  wetting front during drip irrigation.
 * ============================================================================
 *
 *  Student    : Ashymbekova Adelia        Group : EEAIR-25
 *  Subject    : Electronics Component and Circuit
 *
 *  Board      : ESP32 DevKit V1 (Wokwi part: wokwi-esp32-devkit-v1)
 *  Multiplexer: CD74HC4067 16-channel analog multiplexer (Wokwi part: board-cd74hc4067)
 *  Sensors    : 4 x capacitive soil moisture sensors
 *               -> in Wokwi they are simulated with 4 potentiometers
 *
 *  Research question:
 *    Can an array of multiplexed capacitive soil moisture sensors track the
 *    speed and direction of a wetting front during drip irrigation?
 *
 *  What the program does:
 *    1. Selects one sensor at a time with the CD74HC4067 (pins S0..S3).
 *    2. Reads the analog voltage on the ESP32 ADC (GPIO34).
 *    3. Converts the raw ADC value into soil moisture (%) with a
 *       two-point calibration (dry value and wet value).
 *    4. Logs a time series every SAMPLE_INTERVAL_MS.
 *    5. Detects when the wetting front reaches each depth
 *       (moisture rises RISE_THRESHOLD points above its baseline).
 *    6. Calculates the wetting front speed V = delta_d / delta_t.
 *    7. Reports which depth became wet first and the direction of movement.
 *    8. Compares moisture changes over time (change since start + rate).
 *
 *  Controls:
 *    - Green push button (GPIO4) : start a new irrigation event
 *    - Serial Monitor commands   : s = start new event, r = report,
 *                                  c = toggle CSV output, h = help
 *
 *  No external libraries are needed (only the ESP32 Arduino core).
 * ============================================================================
 */

#include <Arduino.h>

// ---------------------------------------------------------------------------
// 1. PIN ASSIGNMENTS (no conflicts, no flash pins, no boot-strapping inputs)
// ---------------------------------------------------------------------------
const uint8_t MUX_S0_PIN  = 18;   // CD74HC4067 select bit 0 (LSB)
const uint8_t MUX_S1_PIN  = 19;   // CD74HC4067 select bit 1
const uint8_t MUX_S2_PIN  = 21;   // CD74HC4067 select bit 2
const uint8_t MUX_S3_PIN  = 22;   // CD74HC4067 select bit 3 (MSB)
const uint8_t MUX_EN_PIN  = 23;   // CD74HC4067 enable, ACTIVE LOW
const uint8_t MUX_SIG_PIN = 34;   // CD74HC4067 COM/SIG -> ADC1_CH6 (input only)

const uint8_t BUTTON_PIN   = 4;   // push button to GND (internal pull-up)
const uint8_t PUMP_LED_PIN = 33;  // blue LED: drip irrigation is ON

const uint8_t NUM_DEPTHS = 4;
// Green LEDs: ON when the wetting front has reached that depth
const uint8_t DEPTH_LED_PINS[NUM_DEPTHS] = {25, 26, 27, 32};

// ---------------------------------------------------------------------------
// 2. SENSOR CONFIGURATION
// ---------------------------------------------------------------------------
// Multiplexer channel used for each depth
const uint8_t MUX_CHANNEL[NUM_DEPTHS] = {0, 1, 2, 3};
// Installation depth of each sensor in centimetres
const float DEPTH_CM[NUM_DEPTHS] = {5.0, 10.0, 20.0, 30.0};

// Two-point calibration (raw 12-bit ADC values, 0..4095).
// A capacitive sensor gives a HIGHER voltage in DRY soil and a LOWER
// voltage in WET soil. These numbers are typical for a 3.3 V capacitive
// sensor v1.2, but they MUST be measured again for every real sensor and
// soil type (see documentation/mathematical_analysis.md).
const int DRY_RAW[NUM_DEPTHS] = {3000, 3000, 3000, 3000};  // sensor in dry soil / air
const int WET_RAW[NUM_DEPTHS] = {1200, 1200, 1200, 1200};  // sensor in saturated soil / water

// ---------------------------------------------------------------------------
// 3. MEASUREMENT AND ANALYSIS SETTINGS
// ---------------------------------------------------------------------------
const uint32_t SAMPLE_INTERVAL_MS = 2000;  // one reading of all depths every 2 s
const uint8_t  ADC_SAMPLES        = 16;    // readings averaged per channel (noise filter)
const uint16_t MUX_SETTLE_US      = 500;   // wait after switching channel
const float    RISE_THRESHOLD     = 20.0;  // % points above baseline = "front arrived"
const uint8_t  BASELINE_SAMPLES   = 5;     // readings averaged to get the baseline
const uint8_t  SUMMARY_EVERY      = 5;     // print comparison table every N samples

// TIME_SCALE lets a short simulation represent a longer field experiment.
// 1.0  -> times are real (1 s in Wokwi = 1 s in the field).
// 30.0 -> 1 s in Wokwi represents 30 s in the field (useful for a demo video).
const float TIME_SCALE = 1.0;

// Size of the time-series history kept in RAM
const uint8_t HISTORY_LEN = 60;

// ---------------------------------------------------------------------------
// 4. DATA STRUCTURES
// ---------------------------------------------------------------------------
struct DepthState {
  int   raw;            // last averaged ADC value
  float moisture;       // current moisture (%)
  float prevMoisture;   // moisture in previous sample (%)
  float baseline;       // moisture at the start of irrigation (%)
  float threshold;      // baseline + RISE_THRESHOLD (%)
  float peak;           // highest moisture during this event (%)
  float ratePerMin;     // change rate between last two samples (%/min)
  bool  frontArrived;   // true after the wetting front reached this depth
  float arrivalMin;     // arrival time in minutes after irrigation start
  int8_t arrivalOrder;  // 1 = first depth to get wet, 2 = second, ...
};

DepthState depth[NUM_DEPTHS];

// Ring buffer for the time series
float    historyMoisture[HISTORY_LEN][NUM_DEPTHS];
float    historyTimeMin[HISTORY_LEN];
uint8_t  historyHead  = 0;   // next position to write
uint8_t  historyCount = 0;   // how many valid entries

// Irrigation event state
uint32_t eventStartMs     = 0;
uint16_t eventNumber      = 0;
bool     irrigationOn     = false;
uint8_t  arrivalsCount    = 0;
bool     finalReportShown = false;

// Timing and user interface
uint32_t lastSampleMs  = 0;
uint32_t sampleCounter = 0;
float    lastSampleMin = 0.0;
bool     csvMode       = false;

// Button debouncing
bool     lastButtonReading = HIGH;
bool     buttonState       = HIGH;
uint32_t lastDebounceMs    = 0;

// Forward declarations (functions defined further below)
void printCsvHeader();
void printFinalReport();

// ---------------------------------------------------------------------------
// 5. MULTIPLEXER AND ADC FUNCTIONS
// ---------------------------------------------------------------------------

// Put a 4-bit channel number (0..15) on S0..S3
void selectMuxChannel(uint8_t channel) {
  digitalWrite(MUX_S0_PIN, (channel >> 0) & 0x01);
  digitalWrite(MUX_S1_PIN, (channel >> 1) & 0x01);
  digitalWrite(MUX_S2_PIN, (channel >> 2) & 0x01);
  digitalWrite(MUX_S3_PIN, (channel >> 3) & 0x01);
}

// Read one multiplexer channel and return the averaged raw ADC value
int readMuxChannelRaw(uint8_t channel) {
  digitalWrite(MUX_EN_PIN, HIGH);      // disable outputs while switching
  selectMuxChannel(channel);
  digitalWrite(MUX_EN_PIN, LOW);       // enable the selected channel
  delayMicroseconds(MUX_SETTLE_US);    // let the voltage settle

  analogRead(MUX_SIG_PIN);             // first reading is discarded

  uint32_t sum = 0;
  for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(MUX_SIG_PIN);
    delayMicroseconds(100);
  }
  return (int)(sum / ADC_SAMPLES);
}

// Convert raw ADC value to soil moisture percentage (two-point calibration)
// Moisture(%) = (DRY - raw) / (DRY - WET) * 100, limited to 0..100 %
float rawToMoisturePercent(int raw, uint8_t i) {
  float percent = (float)(DRY_RAW[i] - raw) * 100.0 / (float)(DRY_RAW[i] - WET_RAW[i]);
  if (percent < 0.0)   percent = 0.0;
  if (percent > 100.0) percent = 100.0;
  return percent;
}

// Convert raw ADC value to volts (for information only)
float rawToVolts(int raw) {
  return raw * 3.3 / 4095.0;
}

// Read all four depths and update moisture values
void readAllDepths() {
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    depth[i].raw = readMuxChannelRaw(MUX_CHANNEL[i]);
    depth[i].prevMoisture = depth[i].moisture;
    depth[i].moisture = rawToMoisturePercent(depth[i].raw, i);
  }
}

// Minutes since the irrigation event started (scaled by TIME_SCALE)
float minutesSinceStart() {
  return (millis() - eventStartMs) / 60000.0 * TIME_SCALE;
}

// ---------------------------------------------------------------------------
// 6. IRRIGATION EVENT CONTROL
// ---------------------------------------------------------------------------

void updateLeds() {
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    digitalWrite(DEPTH_LED_PINS[i], depth[i].frontArrived ? HIGH : LOW);
  }
  digitalWrite(PUMP_LED_PIN, irrigationOn ? HIGH : LOW);
}

// Start a new irrigation event: measure baseline and reset the analysis
void startIrrigationEvent() {
  eventNumber++;
  Serial.println();
  Serial.println(F("============================================================"));
  Serial.printf("  IRRIGATION EVENT #%u STARTED - measuring baseline...\n", eventNumber);
  Serial.println(F("============================================================"));

  // Baseline = average of several readings taken before water arrives
  float sum[NUM_DEPTHS] = {0, 0, 0, 0};
  for (uint8_t n = 0; n < BASELINE_SAMPLES; n++) {
    for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
      sum[i] += rawToMoisturePercent(readMuxChannelRaw(MUX_CHANNEL[i]), i);
    }
    delay(20);
  }

  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    depth[i].baseline     = sum[i] / BASELINE_SAMPLES;
    depth[i].threshold    = depth[i].baseline + RISE_THRESHOLD;
    depth[i].moisture     = depth[i].baseline;
    depth[i].prevMoisture = depth[i].baseline;
    depth[i].peak         = depth[i].baseline;
    depth[i].ratePerMin   = 0.0;
    depth[i].frontArrived = false;
    depth[i].arrivalMin   = -1.0;
    depth[i].arrivalOrder = 0;

    Serial.printf("  Depth %4.0f cm : baseline = %5.1f %%  ->  threshold = %5.1f %%\n",
                  DEPTH_CM[i], depth[i].baseline, depth[i].threshold);
    if (depth[i].threshold > 100.0) {
      Serial.printf("  WARNING: %2.0f cm is already very wet, the front cannot be detected there.\n",
                    DEPTH_CM[i]);
    }
  }

  historyHead = 0;
  historyCount = 0;
  arrivalsCount = 0;
  finalReportShown = false;
  sampleCounter = 0;
  lastSampleMin = 0.0;
  irrigationOn = true;
  eventStartMs = millis();
  lastSampleMs = eventStartMs;
  updateLeds();

  Serial.println(F("  Drip irrigation is ON (blue LED)."));
  Serial.println(F("  In Wokwi: turn a knob to the LEFT (counter-clockwise) = wetter soil."));
  Serial.println(F("  Wet the 5 cm knob first, then 10, 20 and 30 cm to simulate infiltration."));
  Serial.println(F("------------------------------------------------------------"));
  if (csvMode) printCsvHeader();
}

// ---------------------------------------------------------------------------
// 7. WETTING FRONT ANALYSIS
// ---------------------------------------------------------------------------

// Check every depth for the moment its moisture crosses the threshold.
// Linear interpolation between two samples gives a more exact crossing time:
//   t_cross = t_prev + (threshold - m_prev) / (m_now - m_prev) * (t_now - t_prev)
void detectWettingFront(float nowMin) {
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    if (depth[i].frontArrived) continue;
    if (depth[i].threshold > 100.0) continue;
    if (depth[i].moisture < depth[i].threshold) continue;

    float mPrev = depth[i].prevMoisture;
    float mNow  = depth[i].moisture;
    float tCross = nowMin;
    if (mNow > mPrev && mPrev < depth[i].threshold) {
      tCross = lastSampleMin + (depth[i].threshold - mPrev) / (mNow - mPrev) * (nowMin - lastSampleMin);
    }

    depth[i].frontArrived = true;
    depth[i].arrivalMin   = tCross;
    arrivalsCount++;
    depth[i].arrivalOrder = arrivalsCount;

    Serial.println();
    Serial.printf(">>> WETTING FRONT reached %2.0f cm at t = %.2f min (arrival #%u)\n",
                  DEPTH_CM[i], tCross, arrivalsCount);

    if (arrivalsCount == 1) {
      Serial.printf(">>> FIRST WET DEPTH: %2.0f cm\n", DEPTH_CM[i]);
      if (tCross > 0.0) {
        // Dripper is at the surface (0 cm), so the first speed is d / t
        Serial.printf(">>> Speed from surface (0 cm) to %2.0f cm: %.2f cm/min\n",
                      DEPTH_CM[i], DEPTH_CM[i] / tCross);
      }
    } else {
      // Speed between this depth and the depth that got wet just before it
      int8_t prev = -1;
      for (uint8_t j = 0; j < NUM_DEPTHS; j++) {
        if (depth[j].arrivalOrder == arrivalsCount - 1) prev = j;
      }
      if (prev >= 0) {
        float dDepth = DEPTH_CM[i] - DEPTH_CM[prev];
        float dTime  = depth[i].arrivalMin - depth[prev].arrivalMin;
        const char *dir = (dDepth > 0) ? "DOWNWARD" : "UPWARD";
        if (dTime > 0.001) {
          Serial.printf(">>> Segment %2.0f -> %2.0f cm: delta_d = %.0f cm, delta_t = %.2f min, V = %.2f cm/min (%s)\n",
                        DEPTH_CM[prev], DEPTH_CM[i], fabs(dDepth), dTime, fabs(dDepth) / dTime, dir);
        } else {
          Serial.printf(">>> Segment %2.0f -> %2.0f cm: both depths got wet in the same sample (too fast to resolve)\n",
                        DEPTH_CM[prev], DEPTH_CM[i]);
        }
      }
    }
    Serial.println();
  }

  // When the deepest sensor is wet, stop the drip (avoids water loss below the roots)
  if (irrigationOn && depth[NUM_DEPTHS - 1].frontArrived) {
    irrigationOn = false;
    Serial.println(F(">>> Front reached the deepest sensor: drip irrigation switched OFF (blue LED off)."));
  }

  if (arrivalsCount == NUM_DEPTHS && !finalReportShown) {
    finalReportShown = true;
    printFinalReport();
  }
}

// Least-squares slope of depth vs. arrival time = average front speed (cm/min)
//   V_fit = sum((t - t_mean)(d - d_mean)) / sum((t - t_mean)^2)
bool regressionSpeed(float &speedOut) {
  uint8_t n = 0;
  float tMean = 0, dMean = 0;
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    if (depth[i].frontArrived) { tMean += depth[i].arrivalMin; dMean += DEPTH_CM[i]; n++; }
  }
  if (n < 2) return false;
  tMean /= n;
  dMean /= n;
  float num = 0, den = 0;
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    if (!depth[i].frontArrived) continue;
    num += (depth[i].arrivalMin - tMean) * (DEPTH_CM[i] - dMean);
    den += (depth[i].arrivalMin - tMean) * (depth[i].arrivalMin - tMean);
  }
  if (den < 1e-6) return false;
  speedOut = num / den;
  return true;
}

// Direction of movement from the order in which depths became wet
const char *frontDirection() {
  if (arrivalsCount < 2) return "not enough data (need 2 wet depths)";
  bool down = true, up = true;
  float lastDepth = -1.0;
  for (uint8_t order = 1; order <= arrivalsCount; order++) {
    for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
      if (depth[i].arrivalOrder != order) continue;
      if (lastDepth >= 0) {
        if (DEPTH_CM[i] < lastDepth) down = false;
        if (DEPTH_CM[i] > lastDepth) up = false;
      }
      lastDepth = DEPTH_CM[i];
    }
  }
  if (down) return "DOWNWARD (infiltration from the surface dripper)";
  if (up)   return "UPWARD (capillary rise / water from below)";
  return "IRREGULAR (possible preferential or lateral flow)";
}

// ---------------------------------------------------------------------------
// 8. TIME SERIES AND OUTPUT
// ---------------------------------------------------------------------------

void storeHistory(float tMin) {
  historyTimeMin[historyHead] = tMin;
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    historyMoisture[historyHead][i] = depth[i].moisture;
  }
  historyHead = (historyHead + 1) % HISTORY_LEN;
  if (historyCount < HISTORY_LEN) historyCount++;
}

// Index of a sample "stepsBack" samples ago in the ring buffer
uint8_t historyIndex(uint8_t stepsBack) {
  return (historyHead + HISTORY_LEN - 1 - stepsBack) % HISTORY_LEN;
}

void printCsvHeader() {
  Serial.println(F("CSV,event,time_min,m5cm,m10cm,m20cm,m30cm,raw5,raw10,raw20,raw30"));
}

void printSampleLine(float tMin) {
  if (csvMode) {
    Serial.printf("CSV,%u,%.3f", eventNumber, tMin);
    for (uint8_t i = 0; i < NUM_DEPTHS; i++) Serial.printf(",%.1f", depth[i].moisture);
    for (uint8_t i = 0; i < NUM_DEPTHS; i++) Serial.printf(",%d", depth[i].raw);
    Serial.println();
    return;
  }
  Serial.printf("t=%6.2f min |", tMin);
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    char mark = ' ';
    if (depth[i].ratePerMin > 1.0)  mark = '+';   // getting wetter
    if (depth[i].ratePerMin < -1.0) mark = '-';   // drying / draining
    Serial.printf(" %2.0fcm:%5.1f%%%c%s|", DEPTH_CM[i], depth[i].moisture, mark,
                  depth[i].frontArrived ? "W" : " ");
  }
  Serial.println(irrigationOn ? " drip ON" : " drip OFF");
}

// Comparison of moisture changes over time for all depths
void printComparisonTable(float tMin) {
  if (historyCount == 0) {
    Serial.println(F("  (no samples recorded yet)"));
    return;
  }
  // Look back up to SUMMARY_EVERY samples for the window change
  uint8_t back = (historyCount > SUMMARY_EVERY) ? SUMMARY_EVERY : (historyCount - 1);
  uint8_t idxOld = historyIndex(back);
  float windowMin = tMin - historyTimeMin[idxOld];

  Serial.println(F("  ----------------------------------------------------------------------"));
  Serial.printf ("  CHANGE OVER TIME  (window = last %.2f min)\n", windowMin);
  Serial.println(F("  Depth | Raw  | Volt | Moist. | Change since start | Window rate | State"));
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    float change = depth[i].moisture - depth[i].baseline;
    float windowRate = (windowMin > 0) ? (depth[i].moisture - historyMoisture[idxOld][i]) / windowMin : 0.0;
    Serial.printf("  %3.0fcm | %4d | %.2f | %5.1f%% | %+7.1f points      | %+6.1f %%/min | %s\n",
                  DEPTH_CM[i], depth[i].raw, rawToVolts(depth[i].raw), depth[i].moisture,
                  change, windowRate, depth[i].frontArrived ? "WET (front passed)" : "waiting");
  }
  Serial.println(F("  ----------------------------------------------------------------------"));
}

void printFinalReport() {
  Serial.println();
  Serial.println(F("################ WETTING FRONT REPORT ################"));
  Serial.printf("Irrigation event #%u, rise threshold = %.0f %% points\n", eventNumber, RISE_THRESHOLD);
  Serial.println(F("Depth | Baseline | Peak   | Arrival time | Order"));
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    if (depth[i].frontArrived) {
      Serial.printf("%3.0fcm | %6.1f%%  | %5.1f%% | %8.2f min | %d\n", DEPTH_CM[i],
                    depth[i].baseline, depth[i].peak, depth[i].arrivalMin, depth[i].arrivalOrder);
    } else {
      Serial.printf("%3.0fcm | %6.1f%%  | %5.1f%% |   not yet    | -\n", DEPTH_CM[i],
                    depth[i].baseline, depth[i].peak);
    }
  }

  // First wet depth
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    if (depth[i].arrivalOrder == 1) Serial.printf("First depth to become wet: %.0f cm\n", DEPTH_CM[i]);
  }
  Serial.printf("Direction of movement   : %s\n", frontDirection());

  // Segment speeds in depth order (5->10, 10->20, 20->30)
  Serial.println(F("Segment speeds (V = delta_d / delta_t):"));
  for (uint8_t i = 0; i + 1 < NUM_DEPTHS; i++) {
    if (depth[i].frontArrived && depth[i + 1].frontArrived) {
      float dd = DEPTH_CM[i + 1] - DEPTH_CM[i];
      float dt = depth[i + 1].arrivalMin - depth[i].arrivalMin;
      if (fabs(dt) > 0.001) {
        Serial.printf("  %2.0f -> %2.0f cm : %.0f cm / %.2f min = %.2f cm/min\n",
                      DEPTH_CM[i], DEPTH_CM[i + 1], dd, dt, dd / dt);
      } else {
        Serial.printf("  %2.0f -> %2.0f cm : arrivals at the same time\n", DEPTH_CM[i], DEPTH_CM[i + 1]);
      }
    }
  }

  // Overall speed from the shallowest to the deepest wet sensor
  int8_t first = -1, last = -1;
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    if (!depth[i].frontArrived) continue;
    if (first < 0) first = i;
    last = i;
  }
  if (first >= 0 && last > first) {
    float dt = depth[last].arrivalMin - depth[first].arrivalMin;
    if (fabs(dt) > 0.001) {
      Serial.printf("Average speed %2.0f -> %2.0f cm   : %.2f cm/min\n", DEPTH_CM[first], DEPTH_CM[last],
                    (DEPTH_CM[last] - DEPTH_CM[first]) / dt);
    }
  }
  float vFit;
  if (regressionSpeed(vFit)) {
    Serial.printf("Best-fit speed (regression): %.2f cm/min\n", vFit);
  }
  Serial.println(F("######################################################"));
  Serial.println(F("Press the button or type 's' to start a new event."));
  Serial.println();
}

void printHelp() {
  Serial.println(F("Commands: s = start new irrigation event | r = report | c = CSV on/off | h = help"));
}

// ---------------------------------------------------------------------------
// 9. USER INPUT (button + serial)
// ---------------------------------------------------------------------------

void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) lastDebounceMs = millis();
  if (millis() - lastDebounceMs > 50 && reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW) startIrrigationEvent();   // pressed (pull-up -> LOW)
  }
  lastButtonReading = reading;
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 's': case 'S': startIrrigationEvent(); break;
      case 'r': case 'R': printComparisonTable(lastSampleMin); printFinalReport(); break;
      case 'c': case 'C':
        csvMode = !csvMode;
        Serial.printf("CSV mode %s\n", csvMode ? "ON (copy the CSV lines into Excel)" : "OFF");
        if (csvMode) printCsvHeader();
        break;
      case 'h': case 'H': printHelp(); break;
      default: break;   // ignore newline and other characters
    }
  }
}

// ---------------------------------------------------------------------------
// 10. SETUP AND LOOP
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);

  // Multiplexer control pins
  pinMode(MUX_S0_PIN, OUTPUT);
  pinMode(MUX_S1_PIN, OUTPUT);
  pinMode(MUX_S2_PIN, OUTPUT);
  pinMode(MUX_S3_PIN, OUTPUT);
  pinMode(MUX_EN_PIN, OUTPUT);
  digitalWrite(MUX_EN_PIN, HIGH);   // start disabled
  selectMuxChannel(0);

  // ADC: 12-bit resolution (0..4095). Default attenuation (11 dB) covers ~0..3.3 V
  pinMode(MUX_SIG_PIN, INPUT);
  analogReadResolution(12);

  // LEDs and button
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) pinMode(DEPTH_LED_PINS[i], OUTPUT);
  pinMode(PUMP_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println(F("Multi-Depth Soil Profile Node - ESP32 + CD74HC4067"));
  Serial.println(F("Student: Ashymbekova Adelia, EEAIR-25"));
  Serial.println(F("Depths: 5 cm (CH0), 10 cm (CH1), 20 cm (CH2), 30 cm (CH3)"));
  Serial.printf ("Sample interval: %lu ms, TIME_SCALE = %.1f, rise threshold = %.0f points\n",
                 (unsigned long)SAMPLE_INTERVAL_MS, TIME_SCALE, RISE_THRESHOLD);
  printHelp();

  // Lamp test: all LEDs on for half a second
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) digitalWrite(DEPTH_LED_PINS[i], HIGH);
  digitalWrite(PUMP_LED_PIN, HIGH);
  delay(500);

  // The first irrigation event starts automatically
  startIrrigationEvent();
}

void loop() {
  handleButton();
  handleSerial();

  uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  float tMin = minutesSinceStart();
  float dtMin = tMin - lastSampleMin;

  readAllDepths();

  // Rate of change for every depth: dM/dt in % per minute
  for (uint8_t i = 0; i < NUM_DEPTHS; i++) {
    depth[i].ratePerMin = (dtMin > 0) ? (depth[i].moisture - depth[i].prevMoisture) / dtMin : 0.0;
    if (depth[i].moisture > depth[i].peak) depth[i].peak = depth[i].moisture;
  }

  storeHistory(tMin);
  sampleCounter++;
  printSampleLine(tMin);
  detectWettingFront(tMin);
  updateLeds();

  if (!csvMode && sampleCounter % SUMMARY_EVERY == 0) printComparisonTable(tMin);

  lastSampleMin = tMin;
}
```

---

## 19. Appendix B: Circuit File (`diagram.json`)

```json
{
  "version": 1,
  "author": "Ashymbekova Adelia (EEAIR-25)",
  "editor": "wokwi",
  "parts": [
    {"type": "wokwi-esp32-devkit-v1", "id": "esp", "top": 0, "left": 0, "attrs": {}},
    {"type": "board-cd74hc4067", "id": "mux1", "top": 20, "left": 250, "attrs": {}},
    {"type": "wokwi-potentiometer", "id": "pot1", "top": -170, "left": 230, "attrs": {}},
    {"type": "wokwi-text", "id": "txt1", "top": -200, "left": 230, "attrs": {"text": "Depth 5 cm (CH0)"}},
    {"type": "wokwi-potentiometer", "id": "pot2", "top": -170, "left": 335, "attrs": {}},
    {"type": "wokwi-text", "id": "txt2", "top": -200, "left": 335, "attrs": {"text": "Depth 10 cm (CH1)"}},
    {"type": "wokwi-potentiometer", "id": "pot3", "top": -170, "left": 440, "attrs": {}},
    {"type": "wokwi-text", "id": "txt3", "top": -200, "left": 440, "attrs": {"text": "Depth 20 cm (CH2)"}},
    {"type": "wokwi-potentiometer", "id": "pot4", "top": -170, "left": 545, "attrs": {}},
    {"type": "wokwi-text", "id": "txt4", "top": -200, "left": 545, "attrs": {"text": "Depth 30 cm (CH3)"}},
    {"type": "wokwi-resistor", "id": "r1", "top": 260, "left": 220, "rotate": 90, "attrs": {"value": "220"}},
    {"type": "wokwi-led", "id": "led1", "top": 330, "left": 230, "attrs": {"color": "green"}},
    {"type": "wokwi-text", "id": "ltxt1", "top": 400, "left": 215, "attrs": {"text": "5 cm wet"}},
    {"type": "wokwi-resistor", "id": "r2", "top": 260, "left": 300, "rotate": 90, "attrs": {"value": "220"}},
    {"type": "wokwi-led", "id": "led2", "top": 330, "left": 310, "attrs": {"color": "green"}},
    {"type": "wokwi-text", "id": "ltxt2", "top": 400, "left": 295, "attrs": {"text": "10 cm wet"}},
    {"type": "wokwi-resistor", "id": "r3", "top": 260, "left": 380, "rotate": 90, "attrs": {"value": "220"}},
    {"type": "wokwi-led", "id": "led3", "top": 330, "left": 390, "attrs": {"color": "green"}},
    {"type": "wokwi-text", "id": "ltxt3", "top": 400, "left": 375, "attrs": {"text": "20 cm wet"}},
    {"type": "wokwi-resistor", "id": "r4", "top": 260, "left": 460, "rotate": 90, "attrs": {"value": "220"}},
    {"type": "wokwi-led", "id": "led4", "top": 330, "left": 470, "attrs": {"color": "green"}},
    {"type": "wokwi-text", "id": "ltxt4", "top": 400, "left": 455, "attrs": {"text": "30 cm wet"}},
    {"type": "wokwi-resistor", "id": "r5", "top": 260, "left": 540, "rotate": 90, "attrs": {"value": "220"}},
    {"type": "wokwi-led", "id": "led5", "top": 330, "left": 550, "attrs": {"color": "blue"}},
    {"type": "wokwi-text", "id": "ltxt5", "top": 400, "left": 535, "attrs": {"text": "Drip ON"}},
    {"type": "wokwi-pushbutton", "id": "btn1", "top": 260, "left": -160, "attrs": {"color": "green"}},
    {"type": "wokwi-text", "id": "title", "top": -260, "left": 0, "attrs": {"text": "Multi-Depth Soil Profile Node - ESP32 + CD74HC4067 (potentiometers = capacitive soil sensors)"}}
  ],
  "connections": [
    ["esp:TX0", "$serialMonitor:RX", "", []],
    ["esp:RX0", "$serialMonitor:TX", "", []],
    ["mux1:VCC", "esp:3V3", "red", []],
    ["mux1:GND", "esp:GND.1", "black", []],
    ["mux1:EN", "esp:D23", "orange", []],
    ["mux1:S0", "esp:D18", "blue", []],
    ["mux1:S1", "esp:D19", "blue", []],
    ["mux1:S2", "esp:D21", "blue", []],
    ["mux1:S3", "esp:D22", "blue", []],
    ["mux1:COM", "esp:D34", "green", []],
    ["pot1:VCC", "esp:3V3", "red", []],
    ["pot1:GND", "esp:GND.1", "black", []],
    ["pot1:SIG", "mux1:I0", "green", []],
    ["pot2:VCC", "esp:3V3", "red", []],
    ["pot2:GND", "esp:GND.1", "black", []],
    ["pot2:SIG", "mux1:I1", "green", []],
    ["pot3:VCC", "esp:3V3", "red", []],
    ["pot3:GND", "esp:GND.1", "black", []],
    ["pot3:SIG", "mux1:I2", "green", []],
    ["pot4:VCC", "esp:3V3", "red", []],
    ["pot4:GND", "esp:GND.1", "black", []],
    ["pot4:SIG", "mux1:I3", "green", []],
    ["esp:D25", "r1:1", "gold", []],
    ["r1:2", "led1:A", "gold", []],
    ["led1:C", "esp:GND.2", "black", []],
    ["esp:D26", "r2:1", "gold", []],
    ["r2:2", "led2:A", "gold", []],
    ["led2:C", "esp:GND.2", "black", []],
    ["esp:D27", "r3:1", "gold", []],
    ["r3:2", "led3:A", "gold", []],
    ["led3:C", "esp:GND.2", "black", []],
    ["esp:D32", "r4:1", "gold", []],
    ["r4:2", "led4:A", "gold", []],
    ["led4:C", "esp:GND.2", "black", []],
    ["esp:D33", "r5:1", "gold", []],
    ["r5:2", "led5:A", "gold", []],
    ["led5:C", "esp:GND.2", "black", []],
    ["btn1:1.l", "esp:D4", "purple", []],
    ["btn1:2.l", "esp:GND.2", "black", []]
  ],
  "dependencies": {}
}
```

---

## 20. Appendix C: Library List (`libraries.txt`)

```
# No external libraries are required.
# The project uses only the built-in ESP32 Arduino core functions
# (analogRead, digitalWrite, Serial).
```
