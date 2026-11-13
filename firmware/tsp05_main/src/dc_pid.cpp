#include "config.h"
#include "dc_pid.h"
#include "ir_dc.h"

namespace dcpid {
namespace {

// ---- Encoder ----
// SPG30E: 3 pulses per motor-shaft turn per channel -> 12 counts per motor turn with x4 decoding,
// times the gearbox ratio. ~360 counts per output turn is a starting guess for the 30K version.
// MEASURE IT: `mode gui`, `dcenc zero`, turn the output shaft exactly 10 turns by hand, `dcenc`,
// divide the count by 10 and put it here (config.h: ENC_COUNTS_PER_REV).
constexpr float DEG_PER_COUNT = 360.0f / ENC_COUNTS_PER_REV;

volatile int32_t encCount = 0;
volatile uint8_t encPrev = 0;
portMUX_TYPE encMux = portMUX_INITIALIZER_UNLOCKED;

// Quadrature lookup: index = (previous AB << 2) | current AB -> -1, 0 or +1.
const int8_t QDEC[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR onEdge() {
  const uint8_t cur = (digitalRead(pins::ENC_A) << 1) | digitalRead(pins::ENC_B);
  portENTER_CRITICAL_ISR(&encMux);
  encCount += ENC_DIR * QDEC[(encPrev << 2) | cur];
  encPrev = cur;
  portEXIT_CRITICAL_ISR(&encMux);
}

long readCount() {
  portENTER_CRITICAL(&encMux);
  const long c = encCount;
  portEXIT_CRITICAL(&encMux);
  return c;
}

// ---- Control ----
constexpr uint32_t PERIOD_MS = 20;         // PID rate: 50 Hz
constexpr float RPM_ALPHA = 0.3f;          // EMA on the measured speed (few counts per period)
constexpr float POS_TOL_DEG = 2.0f;        // "arrived" when within this...
constexpr float POS_TOL_RPM = 3.0f;        // ...and slower than this...
constexpr int SETTLE_PERIODS = 5;          // ...for 5 periods in a row (100 ms)
constexpr uint32_t MOVE_TIMEOUT_MS = 8000; // give up if a move never settles
// Lowest PWM that turns the motor (same deadband as MIN_RUN_PWM in ir_dc.cpp). Added on top of
// the PID output so small corrections still move the shaft.
constexpr int MIN_PWM = 70;
// Output-shaft rpm at PWM 255, for the velocity feedforward. Measure: `dc 255`, read dc_rpm.
constexpr float RPM_AT_FULL_PWM = 130.0f;

float posKp = 3.0f, posKi = 0.5f, posKd = 0.25f;  // PWM per deg, per deg*s, per deg/s
float velKp = 1.5f, velKi = 4.0f;                 // PWM per rpm, per rpm*s

enum class Ctl { OFF, POS, VEL };
Ctl ctl = Ctl::OFF;
float targetDeg = 0, targetRpm = 0, integ = 0;
float rpmFilt = 0;
long lastCount = 0;
uint32_t lastMs = 0, moveStartMs = 0;
int settled = 0;
float startErr = 0;
int wrongWay = 0;

// Safety: if the encoder counts the opposite way to the motor (A/B swapped relative to M+/M-), the
// PID pushes the wrong way and the motor runs away at full speed. Detect it and stop.
void runaway() {
  ctl = Ctl::OFF;
  irdc::setPwm(0);
  Serial.println(F("# DC STOPPED: encoder direction looks reversed. Set ENC_DIR to -1 in config.h "
                   "(or swap the blue and purple encoder wires), re-flash, and try again."));
}

void drive(float u) {
  // u: PID output in PWM units, sign = direction. Add the deadband offset so small u still turns.
  int pwm = 0;
  if (u > 0.5f) pwm = MIN_PWM + lroundf(u);
  else if (u < -0.5f) pwm = -MIN_PWM + lroundf(u);
  irdc::setPwm(constrain(pwm, -255, 255));
}

void finishMove(const char *why) {
  ctl = Ctl::OFF;
  irdc::setPwm(0);
  Serial.printf("# DCDEG %s at %.1f deg (target %.1f, error %+.1f deg)\n", why, angleDeg(), targetDeg,
                targetDeg - angleDeg());
}

}  // namespace

void begin() {
  pinMode(pins::ENC_A, INPUT_PULLUP);  // weak internal pull-ups; the external 4.7-10k to 3V3 do the real work
  pinMode(pins::ENC_B, INPUT_PULLUP);
  encPrev = (digitalRead(pins::ENC_A) << 1) | digitalRead(pins::ENC_B);
  attachInterrupt(digitalPinToInterrupt(pins::ENC_A), onEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pins::ENC_B), onEdge, CHANGE);
  lastCount = readCount();
  lastMs = millis();
}

void update() {
  const uint32_t now = millis();
  if (now - lastMs < PERIOD_MS) return;
  const float dt = (now - lastMs) / 1000.0f;
  lastMs = now;

  const long c = readCount();
  const float instRpm = (c - lastCount) * DEG_PER_COUNT / 360.0f / dt * 60.0f;
  lastCount = c;
  rpmFilt += RPM_ALPHA * (instRpm - rpmFilt);

  if (ctl == Ctl::POS) {
    const float err = targetDeg - angleDeg();
    if (fabsf(err) < POS_TOL_DEG && fabsf(rpmFilt) < POS_TOL_RPM) {
      if (++settled >= SETTLE_PERIODS) return finishMove("reached");
    } else {
      settled = 0;
    }
    if (now - moveStartMs > MOVE_TIMEOUT_MS) return finishMove("TIMEOUT");
    if (fabsf(err) > fabsf(startErr) + 90.0f) return runaway();
    // Integrate only near the target, so a long move doesn't wind up and overshoot.
    if (fabsf(err) < 15.0f) integ = constrain(integ + err * dt, -100.0f, 100.0f);
    else integ = 0;
    const float dDeg = -rpmFilt * 6.0f;  // derivative on measurement (deg/s), no kick on a new target
    const float u = fabsf(err) < POS_TOL_DEG ? 0 : posKp * err + posKi * integ + posKd * dDeg;
    drive(constrain(u, -255.0f + MIN_PWM, 255.0f - MIN_PWM));
  } else if (ctl == Ctl::VEL) {
    const float err = targetRpm - rpmFilt;
    // Spinning hard the opposite way to the command for 0.3 s -> reversed encoder.
    wrongWay = (rpmFilt * targetRpm < 0 && fabsf(rpmFilt) > 15.0f) ? wrongWay + 1 : 0;
    if (wrongWay > 15) return runaway();
    integ = constrain(integ + err * dt, -60.0f, 60.0f);
    const float ff = targetRpm / RPM_AT_FULL_PWM * 255.0f;
    irdc::setPwm(constrain(lroundf(ff + velKp * err + velKi * integ), -255L, 255L));
  }
}

void moveRelative(float deg) {
  targetDeg = angleDeg() + deg;
  startErr = deg;
  integ = 0;
  settled = 0;
  moveStartMs = millis();
  ctl = Ctl::POS;
  Serial.printf("# DCDEG %+.1f deg -> target %.1f deg\n", deg, targetDeg);
}

void setRpm(float r) {
  if (fabsf(r) < 0.5f) {
    cancel();
    irdc::setPwm(0);
    Serial.println(F("# DCRPM 0 (stopped)"));
    return;
  }
  if (ctl != Ctl::VEL) integ = 0;
  wrongWay = 0;
  targetRpm = r;
  ctl = Ctl::VEL;
  Serial.printf("# DCRPM %+.1f rpm\n", targetRpm);
}

void cancel() { ctl = Ctl::OFF; }
bool active() { return ctl != Ctl::OFF; }
float angleDeg() { return readCount() * DEG_PER_COUNT; }
float rpm() { return rpmFilt; }
long counts() { return readCount(); }

void zero() {
  portENTER_CRITICAL(&encMux);
  encCount = 0;
  portEXIT_CRITICAL(&encMux);
  lastCount = 0;
}

void setPosGains(float kp, float ki, float kd) { posKp = kp; posKi = ki; posKd = kd; printGains(); }
void setVelGains(float kp, float ki) { velKp = kp; velKi = ki; printGains(); }

void printGains() {
  Serial.printf("# DC gains: pos kp=%.3f ki=%.3f kd=%.3f | vel kp=%.3f ki=%.3f | %d counts/rev\n", posKp, posKi,
                posKd, velKp, velKi, ENC_COUNTS_PER_REV);
}

}  // namespace dcpid
