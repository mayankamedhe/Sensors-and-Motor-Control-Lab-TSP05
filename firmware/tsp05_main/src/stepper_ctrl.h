// Non-blocking EasyDriver stepper control (based on Rai's stepper_driver_Systems.ino).
// Call stepper::update() on every loop() pass; it emits at most one STEP pulse per call.
#pragma once
#include <Arduino.h>

namespace stepper {

constexpr long STEPS_PER_REV = 200L * MICROSTEP;
constexpr float DEG_PER_STEP = 360.0f / STEPS_PER_REV;
constexpr float DEFAULT_RPM = 180.0f;
constexpr float MAX_RPM = 300.0f;
// Moves start and end at START_RPM and ramp up/down at ACCEL_RPM_PER_S (0 -> 180 rpm in ~20 ms).
// Starting a stepper directly at high speed makes it stall and buzz; the ramp avoids that.
// If it still stalls: lower ACCEL_RPM_PER_S or the speed, or raise the EasyDriver current a little.
constexpr float START_RPM = 60.0f;
constexpr float ACCEL_RPM_PER_S = 6000.0f;

void begin();
void update();

// Relative move. Rounds to the nearest whole step and returns the actual angle that will be moved.
float moveDegrees(float deg, float rpm = DEFAULT_RPM);
void moveSteps(long steps, float rpm = DEFAULT_RPM);
// Absolute move to an angle (0 = position at boot). Retargets smoothly if already moving.
float moveToDegrees(float deg, float rpm = DEFAULT_RPM);
void runContinuous(bool forward, float rpm);
void stop();

bool isMoving();
bool isContinuous();
long positionSteps();
float positionDeg();
float currentRpm();

}  // namespace stepper
