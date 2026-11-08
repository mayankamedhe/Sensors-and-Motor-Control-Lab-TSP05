// VL53L5CX swipe-gesture test for ESP32.
//
// Detects a hand swiping left->right or right->left in front of the sensor and
// prints "SWIPE,LEFT" / "SWIPE,RIGHT". onSwipe() is where the stepper command goes.
//
// How it works:
//  1. Calibration: the first ~1 s records the background distance of each zone
//     (desk, wires, wall). KEEP YOUR HAND AWAY during "Calibrating...".
//  2. Each frame, a zone counts as "hand" if it is valid, within HAND_MIN..HAND_MAX mm,
//     and at least BG_MARGIN_MM closer than its background.
//  3. Hand x-position = weighted average column of the hand zones (closer = more weight).
//  4. When the hand appears we remember its start x; when it disappears we compare
//     with its last x. A big enough change in a short enough time = a swipe.
//
// Wiring: 3V3->3V3, GND->GND, SDA->GPIO21, SCL->GPIO22 (wire only when unpowered).

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>

// ---------------- Tunables ----------------
static constexpr int      PIN_SDA        = 21;
static constexpr int      PIN_SCL        = 22;
static constexpr uint32_t I2C_HZ         = 400000;
static constexpr int      W              = 4;      // 4x4 zones: enough for left/right, allows high frame rate
static constexpr int      RANGING_HZ     = 30;     // 4x4 supports up to 60 Hz
static constexpr int      HAND_MIN_MM    = 30;
static constexpr int      HAND_MAX_MM    = 400;    // hand must be closer than this
static constexpr int      BG_MARGIN_MM   = 100;    // and this much closer than the background
static constexpr int      MIN_HAND_ZONES = 1;      // zones needed to say "hand present"
static constexpr float    SWIPE_MIN_DX   = 1.5f;   // columns travelled (range is 0..W-1)
static constexpr uint32_t SWIPE_MIN_MS   = 60;
static constexpr uint32_t SWIPE_MAX_MS   = 1500;   // slower than this = hover, not a swipe
static constexpr int      GONE_FRAMES    = 3;      // frames without hand = swipe finished
static constexpr uint32_t COOLDOWN_MS    = 600;    // ignore new swipes right after one
static constexpr int      CALIB_FRAMES   = 30;
static constexpr uint32_t STATUS_MS      = 500;    // status print interval (keeps Serial light)
static constexpr bool     MIRROR_X       = false;  // flip if LEFT/RIGHT come out reversed

// ---------------- State ----------------
SparkFun_VL53L5CX imager;
VL53L5CX_ResultsData data;
float bgMm[W * W];

static bool zoneValid(uint8_t status) { return status == 5 || status == 9; }

void onSwipe(int dir) {  // dir: +1 = RIGHT, -1 = LEFT
  Serial.printf("SWIPE,%s\n", dir > 0 ? "RIGHT" : "LEFT");
  // TODO (integration with Aaron's stepper): e.g. stepper.move(dir * STEPS_PER_SWIPE);
}

static void calibrateBackground() {
  float sum[W * W] = {0};
  int count[W * W] = {0};
  Serial.println("Calibrating background, keep your hand away...");
  int frames = 0;
  while (frames < CALIB_FRAMES) {
    if (!imager.isDataReady()) { delay(1); continue; }
    if (!imager.getRangingData(&data)) continue;
    for (int i = 0; i < W * W; i++) {
      if (zoneValid(data.target_status[i])) { sum[i] += data.distance_mm[i]; count[i]++; }
    }
    frames++;
  }
  Serial.print("Background (mm):");
  for (int i = 0; i < W * W; i++) {
    bgMm[i] = count[i] ? sum[i] / count[i] : 4000.0f;  // never valid = treat as far away
    if (i % W == 0) Serial.print("\n ");
    Serial.printf("%6.0f", bgMm[i]);
  }
  Serial.println("\nReady. Swipe your hand left or right ~10-30 cm in front of the sensor.");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nVL53L5CX swipe test");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_HZ);

  Serial.println("Uploading sensor firmware (takes a few seconds)...");
  if (!imager.begin()) {
    Serial.println("ERROR: sensor not found at 0x29. Check wiring and 3V3 power. Halting.");
    while (true) delay(1000);
  }
  imager.setResolution(W * W);
  imager.setRangingFrequency(RANGING_HZ);
  imager.startRanging();
  Serial.printf("Ranging: %dx%d @ %d Hz\n", W, W, RANGING_HZ);

  calibrateBackground();
}

void loop() {
  static bool tracking = false;
  static float startX = 0, lastX = 0;
  static uint32_t startMs = 0, lastSeenMs = 0, lastSwipeMs = 0, lastStatusMs = 0;
  static int goneCount = 0;

  if (!imager.isDataReady()) { delay(1); return; }
  if (!imager.getRangingData(&data)) return;
  uint32_t now = millis();

  // --- Find hand zones and their weighted x-position ---
  float wSum = 0, xSum = 0;
  int handZones = 0;
  int nearestMm = 0;
  for (int row = 0; row < W; row++) {
    for (int col = 0; col < W; col++) {
      int i = row * W + col;
      if (!zoneValid(data.target_status[i])) continue;
      int d = data.distance_mm[i];
      if (d < HAND_MIN_MM || d > HAND_MAX_MM) continue;
      if (d > bgMm[i] - BG_MARGIN_MM) continue;
      int x = MIRROR_X ? (W - 1 - col) : col;
      float w = 1.0f / d;
      xSum += w * x;
      wSum += w;
      handZones++;
      if (nearestMm == 0 || d < nearestMm) nearestMm = d;
    }
  }
  bool present = handZones >= MIN_HAND_ZONES;
  float x = present ? xSum / wSum : -1;

  // --- Swipe state machine ---
  if (present) {
    if (!tracking && now - lastSwipeMs > COOLDOWN_MS) {
      tracking = true;
      startX = x;
      startMs = now;
    }
    if (tracking) {
      lastX = x;
      lastSeenMs = now;
      goneCount = 0;
    }
  } else if (tracking && ++goneCount >= GONE_FRAMES) {
    tracking = false;
    uint32_t dur = lastSeenMs - startMs;
    float dx = lastX - startX;
    if (dur >= SWIPE_MIN_MS && dur <= SWIPE_MAX_MS && fabsf(dx) >= SWIPE_MIN_DX) {
      onSwipe(dx > 0 ? +1 : -1);
      lastSwipeMs = now;
    } else {
      Serial.printf("(not a swipe: dx=%.2f cols, %lu ms)\n", dx, (unsigned long)dur);
    }
  }

  // --- Light status output ---
  if (now - lastStatusMs >= STATUS_MS) {
    lastStatusMs = now;
    if (present)
      Serial.printf("hand: x=%.2f zones=%d nearest=%d mm\n", x, handZones, nearestMm);
  }
}
