#include "config.h"
#include "flex_servo.h"

#include <ESP32Servo.h>

// Calibration in millivolts at the ADC pin. BillyChern measured 477 (straight) and 521 (bent)
// ADC counts on the Uno with a 5 V divider. With the divider on the ESP32's 3V3, the same
// ratio gives about 1539 mV and 1681 mV. RE-MEASURE on the ESP32 (watch flex_raw_mv).
#ifndef FLEX_STRAIGHT_MV
#define FLEX_STRAIGHT_MV 1539
#endif
#ifndef FLEX_BENT_MV
#define FLEX_BENT_MV 1681
#endif

namespace flexservo {
namespace {

constexpr int SENSOR_MIN_DEG = 70;   // sensor-mode command range from BillyChern's bench test
constexpr int SENSOR_MAX_DEG = 150;
constexpr float SMOOTHING = 0.15f;   // EMA factor at a 20 ms sample period
constexpr uint32_t SAMPLE_MS = 20;

Servo servo;
int raw = 0;
float filtered = 0;
float fraction = 0;
int command = SENSOR_MIN_DEG;
uint32_t lastSampleMs = 0;

}  // namespace

void begin() {
  analogSetPinAttenuation(pins::FLEX, ADC_11db);  // full 0-3.3 V range
  raw = analogReadMilliVolts(pins::FLEX);
  filtered = raw;
  servo.setPeriodHertz(50);
  servo.attach(pins::SERVO);
  servo.write(command);
}

void update(bool sensorMode) {
  const uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_MS) return;
  lastSampleMs = now;

  raw = analogReadMilliVolts(pins::FLEX);
  filtered += SMOOTHING * (raw - filtered);

  // Works whether bending raises or lowers the voltage.
  fraction = 0.0f;
  if (FLEX_STRAIGHT_MV != FLEX_BENT_MV) {
    fraction = (filtered - FLEX_STRAIGHT_MV) / float(FLEX_BENT_MV - FLEX_STRAIGHT_MV);
  }
  fraction = constrain(fraction, 0.0f, 1.0f);

  if (sensorMode) {
    command = SENSOR_MIN_DEG + int(fraction * (SENSOR_MAX_DEG - SENSOR_MIN_DEG) + 0.5f);
    servo.write(command);
  }
}

int setServoDeg(int deg) {
  command = constrain(deg, GUI_MIN_DEG, GUI_MAX_DEG);
  servo.write(command);
  return command;
}

int rawMv() { return raw; }
float filteredMv() { return filtered; }
float bendPct() { return fraction * 100.0f; }
int servoDeg() { return command; }

}  // namespace flexservo
