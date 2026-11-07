// VL53L5CX swipe-gesture detection (based on Mayank's bringup/tof_swipe).
// Runs in its own FreeRTOS task on core 0 so that slow I2C frame reads
// (and the multi-second firmware upload at boot) never stall the stepper on core 1.
#pragma once
#include <Arduino.h>

namespace tof {

constexpr int COLUMNS = 4;  // 4x4 zone mode: hand x is reported in 0..COLUMNS-1

enum class State { INIT, CALIBRATING, READY, ERROR };

struct Status {
  State state;
  bool handPresent;
  float handX;      // column 0..W-1, -1 when no hand
  int nearestMm;    // nearest hand zone, 0 when no hand
  uint32_t frames;
};

void begin();            // starts the background task; returns immediately
Status status();
int pollSwipe();         // +1 = RIGHT, -1 = LEFT, 0 = no new swipe (consumes one event)
void injectSwipe(int dir);  // test hook: pretend a swipe happened
const char *stateName(State s);

}  // namespace tof
