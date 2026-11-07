// Non-blocking EasyDriver stepper control (based on Rai's stepper_driver_Systems.ino).
// Call stepper::update() on every loop() pass; it emits at most one STEP pulse per call.
#pragma once
#include <Arduino.h>

namespace stepper {

constexpr long STEPS_PER_REV = 200L * MICROSTEP;
constexpr float DEG_PER_STEP = 360.0f / STEPS_PER_REV;
constexpr float DEFAULT_RPM = 30.0f;
constexpr float MAX_RPM = 120.0f;  // no acceleration ramp, so keep speeds moderate

void begin();
void update();

// Relative move. Rounds to the nearest whole step and returns the actual angle that will be moved.
float moveDegrees(float deg, float rpm = DEFAULT_RPM);
void moveSteps(long steps, float rpm = DEFAULT_RPM);
void runContinuous(bool forward, float rpm);
void stop();

bool isMoving();
bool isContinuous();
long positionSteps();
float positionDeg();
float currentRpm();

}  // namespace stepper
