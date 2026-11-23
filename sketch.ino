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
