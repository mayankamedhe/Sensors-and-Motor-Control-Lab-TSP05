#include "config.h"
#include "stepper_ctrl.h"

namespace stepper {
namespace {

bool running = false;
bool continuous = false;
int dirSign = 1;           // +1 = DIR HIGH
long stepsRemaining = 0;
long position = 0;         // absolute position in (micro)steps since boot
float rpmNow = DEFAULT_RPM;
uint32_t intervalUs = 0;
uint32_t lastStepUs = 0;

// Speeds in STEP pulses per second, acceleration in pulses/s^2.
float vTarget = 0;  // cruise speed
float vStart = 0;   // start/stop speed
float v = 0;        // current speed
const float ACCEL_SPS2 = ACCEL_RPM_PER_S * STEPS_PER_REV / 60.0f;

float rpmToSps(float rpm) { return rpm * STEPS_PER_REV / 60.0f; }  // pulses/s = RPM x steps/rev / 60

void setSpeed(float rpm) {
  rpmNow = constrain(fabsf(rpm), 0.1f, MAX_RPM);
  vTarget = rpmToSps(rpmNow);
}

void startProfile() {  // begin a move from standstill
  vStart = min(rpmToSps(START_RPM), vTarget);
  v = vStart;
  intervalUs = (uint32_t)(1000000.0f / v);
}

// After each step: v^2 = v0^2 +/- 2*a*(1 step). Decelerate when the remaining steps are
// just enough to slow down to vStart, otherwise accelerate (or slow) towards vTarget.
void updateSpeed() {
  const float stopSteps = (v * v - vStart * vStart) / (2.0f * ACCEL_SPS2);
  if (!continuous && stepsRemaining <= stopSteps) {
    v = sqrtf(max(v * v - 2.0f * ACCEL_SPS2, vStart * vStart));
  } else if (v < vTarget) {
    v = min(vTarget, sqrtf(v * v + 2.0f * ACCEL_SPS2));
  } else if (v > vTarget) {
    v = max(vTarget, sqrtf(max(v * v - 2.0f * ACCEL_SPS2, 0.0f)));
  }
  intervalUs = (uint32_t)(1000000.0f / v);
}

void setDirection(int sign) {
  dirSign = sign >= 0 ? 1 : -1;
  digitalWrite(pins::DIR, dirSign > 0 ? HIGH : LOW);
}

}  // namespace

void begin() {
  pinMode(pins::STEP, OUTPUT);
  pinMode(pins::DIR, OUTPUT);
  digitalWrite(pins::STEP, LOW);
  digitalWrite(pins::DIR, LOW);
  setSpeed(DEFAULT_RPM);
}

void update() {
  if (!running) return;
  const uint32_t now = micros();
  if ((uint32_t)(now - lastStepUs) < intervalUs) return;
  lastStepUs = now;

  digitalWrite(pins::STEP, HIGH);
  delayMicroseconds(3);
  digitalWrite(pins::STEP, LOW);
  position += dirSign;

  if (!continuous && --stepsRemaining <= 0) {
    running = false;
    return;
  }
  updateSpeed();
}

void moveSteps(long steps, float rpm) {
  if (steps == 0) return;
  setDirection(steps > 0 ? 1 : -1);
  delayMicroseconds(5);  // DIR setup time before the first STEP edge
  setSpeed(rpm);
  startProfile();
  stepsRemaining = labs(steps);
  continuous = false;
  running = true;
  lastStepUs = micros();
}

float moveDegrees(float deg, float rpm) {
  const long steps = lroundf(deg / DEG_PER_STEP);
  moveSteps(steps, rpm);
  return steps * DEG_PER_STEP;
}

float moveToDegrees(float deg, float rpm) {
  const long target = lroundf(deg / DEG_PER_STEP);
  const long steps = target - position;
  if (steps == 0) {
    if (!continuous) stop();
  } else if (running && !continuous && (steps > 0) == (dirSign > 0)) {
    stepsRemaining = labs(steps);  // same direction: just change how far to go, keep the step timing
    setSpeed(rpm);
  } else {
    moveSteps(steps, rpm);
  }
  return target * DEG_PER_STEP;
}

void runContinuous(bool forward, float rpm) {
  setDirection(forward ? 1 : -1);
  delayMicroseconds(5);
  setSpeed(rpm);
  startProfile();
  continuous = true;
  running = true;
  lastStepUs = micros();
}

void stop() {
  running = false;
  continuous = false;
  stepsRemaining = 0;
}

bool isMoving() { return running; }
bool isContinuous() { return continuous; }
long positionSteps() { return position; }
float positionDeg() { return position * DEG_PER_STEP; }
float currentRpm() { return rpmNow; }

}  // namespace stepper
