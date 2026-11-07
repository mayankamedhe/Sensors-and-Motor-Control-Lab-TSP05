#include "config.h"
#include "tof_swipe.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#ifndef SIM_TOF
#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>
#endif

namespace tof {
namespace {

// ---------------- Tunables (from Mayank's tested bringup/tof_swipe) ----------------
constexpr uint32_t I2C_HZ = 400000;
constexpr int W = 4;               // 4x4 zones: enough for left/right, allows a high frame rate
constexpr int RANGING_HZ = 30;
constexpr int HAND_MIN_MM = 30;
constexpr int HAND_MAX_MM = 400;   // hand must be closer than this
constexpr int BG_MARGIN_MM = 100;  // and this much closer than the background
constexpr int MIN_HAND_ZONES = 1;
constexpr float SWIPE_MIN_DX = 1.5f;  // columns travelled (range is 0..W-1)
constexpr uint32_t SWIPE_MIN_MS = 60;
constexpr uint32_t SWIPE_MAX_MS = 1500;
constexpr int GONE_FRAMES = 3;
constexpr uint32_t COOLDOWN_MS = 600;
constexpr int CALIB_FRAMES = 30;
constexpr bool MIRROR_X = false;   // tested on hardware: false gives correct LEFT/RIGHT

int16_t distMm[W * W];
uint8_t zoneStatus[W * W];
float bgMm[W * W];

QueueHandle_t swipeQueue;
portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;
Status shared = {State::INIT, false, -1.0f, 0, 0};

void setState(State s) {
  portENTER_CRITICAL(&statusMux);
  shared.state = s;
  portEXIT_CRITICAL(&statusMux);
}

bool zoneValid(uint8_t s) { return s == 5 || s == 9; }

// ---------------- Frame source: real sensor or simulated slide pot ----------------
#ifndef SIM_TOF
SparkFun_VL53L5CX imager;
VL53L5CX_ResultsData data;

bool sensorBegin() {
  Wire.begin(pins::TOF_SDA, pins::TOF_SCL);
  Wire.setClock(I2C_HZ);
  if (!imager.begin()) return false;  // uploads ~90 KB of sensor firmware: takes a few seconds
  imager.setResolution(W * W);
  imager.setRangingFrequency(RANGING_HZ);
  imager.startRanging();
  return true;
}

bool readFrame() {
  if (!imager.isDataReady()) return false;
  if (!imager.getRangingData(&data)) return false;
  for (int i = 0; i < W * W; i++) {
    distMm[i] = data.distance_mm[i];
    zoneStatus[i] = data.target_status[i];
  }
  return true;
}
#else
// Slider at either end = no hand. Dragging it across = a hand moving across the columns.
bool sensorBegin() {
  pinMode(pins::SIM_TOF_SLIDER, INPUT);
  return true;
}

bool readFrame() {
  static uint32_t lastMs = 0;
  const uint32_t now = millis();
  if (now - lastMs < 1000 / RANGING_HZ) return false;
  lastMs = now;
  const float u = analogRead(pins::SIM_TOF_SLIDER) / 4095.0f;
  for (int i = 0; i < W * W; i++) {
    distMm[i] = 1200;  // background wall
    zoneStatus[i] = 5;
  }
  if (u > 0.05f && u < 0.95f) {
    const int col = (int)lroundf((u - 0.05f) / 0.90f * (W - 1));
    for (int row = 0; row < W; row++) distMm[row * W + col] = 150;
  }
  return true;
}
#endif

void calibrateBackground() {
  float sum[W * W] = {0};
  int count[W * W] = {0};
  int frames = 0;
  while (frames < CALIB_FRAMES) {
    if (!readFrame()) { vTaskDelay(1); continue; }
    for (int i = 0; i < W * W; i++) {
      if (zoneValid(zoneStatus[i])) { sum[i] += distMm[i]; count[i]++; }
    }
    frames++;
  }
  for (int i = 0; i < W * W; i++) bgMm[i] = count[i] ? sum[i] / count[i] : 4000.0f;
}

void task(void *) {
  if (!sensorBegin()) {
    setState(State::ERROR);
    vTaskDelete(nullptr);
  }
  setState(State::CALIBRATING);
  calibrateBackground();
  setState(State::READY);

  bool tracking = false;
  float startX = 0, lastX = 0;
  uint32_t startMs = 0, lastSeenMs = 0, lastSwipeMs = 0;
  int goneCount = 0;

  for (;;) {
    if (!readFrame()) { vTaskDelay(1); continue; }
    const uint32_t now = millis();

    // --- Hand zones and their 1/distance-weighted x-position ---
    float wSum = 0, xSum = 0;
    int handZones = 0, nearestMm = 0;
    for (int row = 0; row < W; row++) {
      for (int col = 0; col < W; col++) {
        const int i = row * W + col;
        if (!zoneValid(zoneStatus[i])) continue;
        const int d = distMm[i];
        if (d < HAND_MIN_MM || d > HAND_MAX_MM) continue;
        if (d > bgMm[i] - BG_MARGIN_MM) continue;
        const int x = MIRROR_X ? (W - 1 - col) : col;
        const float w = 1.0f / d;
        xSum += w * x;
        wSum += w;
        handZones++;
        if (nearestMm == 0 || d < nearestMm) nearestMm = d;
      }
    }
    const bool present = handZones >= MIN_HAND_ZONES;
    const float x = present ? xSum / wSum : -1.0f;

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
      const uint32_t dur = lastSeenMs - startMs;
      const float dx = lastX - startX;
      if (dur >= SWIPE_MIN_MS && dur <= SWIPE_MAX_MS && fabsf(dx) >= SWIPE_MIN_DX) {
        const int8_t dir = dx > 0 ? 1 : -1;
        xQueueSend(swipeQueue, &dir, 0);
        lastSwipeMs = now;
      }
    }

    portENTER_CRITICAL(&statusMux);
    shared.handPresent = present;
    shared.handX = x;
    shared.nearestMm = nearestMm;
    shared.frames++;
    portEXIT_CRITICAL(&statusMux);
  }
}

}  // namespace

void begin() {
  swipeQueue = xQueueCreate(4, sizeof(int8_t));
  xTaskCreatePinnedToCore(task, "tof", 8192, nullptr, 1, nullptr, 0);
}

Status status() {
  portENTER_CRITICAL(&statusMux);
  const Status s = shared;
  portEXIT_CRITICAL(&statusMux);
  return s;
}

int pollSwipe() {
  int8_t dir = 0;
  if (swipeQueue && xQueueReceive(swipeQueue, &dir, 0) == pdTRUE) return dir;
  return 0;
}

void injectSwipe(int dir) {
  const int8_t d = dir > 0 ? 1 : -1;
  if (swipeQueue) xQueueSend(swipeQueue, &d, 0);
}

const char *stateName(State s) {
  switch (s) {
    case State::INIT: return "INIT";
    case State::CALIBRATING: return "CALIBRATING";
    case State::READY: return "READY";
    default: return "ERROR";
  }
}

}  // namespace tof
