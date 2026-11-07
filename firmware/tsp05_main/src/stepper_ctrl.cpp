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

void setSpeed(float rpm) {
  rpm = fabsf(rpm);
  rpm = constrain(rpm, 0.1f, MAX_RPM);
  rpmNow = rpm;
  // STEP pulses per second = RPM x steps/rev / 60
  const float stepsPerSecond = rpmNow * STEPS_PER_REV / 60.0f;
  intervalUs = (uint32_t)(1000000.0f / stepsPerSecond);
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

  if (!continuous && --stepsRemaining <= 0) running = false;
}

void moveSteps(long steps, float rpm) {
  if (steps == 0) return;
  setDirection(steps > 0 ? 1 : -1);
  delayMicroseconds(5);  // DIR setup time before the first STEP edge
  setSpeed(rpm);
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

void runContinuous(bool forward, float rpm) {
  setDirection(forward ? 1 : -1);
  delayMicroseconds(5);
  setSpeed(rpm);
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
