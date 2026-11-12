#include "config.h"
#include "ir_dc.h"

namespace irdc {
namespace {

// Sharp GP2Y0A02YK (20-150 cm), powered from 5 V. Approximate datasheet output:
//   150 cm ~0.45 V | 100 cm ~0.75 V | 80 cm ~0.9 V | 50 cm ~1.35 V | 30 cm ~2.0 V | 20 cm ~2.5 V
// Below ~15-20 cm the output FOLDS BACK (voltage drops again as the object gets closer), so a
// hand closer than ~20 cm reads like a farther one. Keep the hand at 20 cm or more during the demo.
// The peak stays below ~2.8 V, safe for the ESP32 pin. Re-check these values with `ir_mv` / `ir_filt_mv`.
//
// Behaviour in SENSOR mode:
//   nothing in view          -> motor OFF
//   hand/obstacle detected   -> motor jumps to MIN_RUN_PWM (just above the DC motor's friction deadband)
//   hand moves closer        -> speed rises linearly to MAX_PWM at NEAR_MV
// Detection uses hysteresis (turn on at DETECT_ON_MV, off below DETECT_OFF_MV) so the motor doesn't
// chatter when the hand sits at the edge of the range.
constexpr int DETECT_ON_MV = 900;   // ~80 cm: hand/obstacle detected -> motor starts
constexpr int DETECT_OFF_MV = 750;  // ~100 cm: object gone -> motor stops
constexpr int NEAR_MV = 2500;       // ~20 cm: full speed (at or above this)
// Lowest PWM that reliably spins the SPG30 on our supply. Measure it: GUI mode, `dc <pwm>`,
// find the smallest value that turns the shaft every time, then add a small margin.
constexpr int MIN_RUN_PWM = 90;
constexpr int MAX_PWM = 255;
constexpr float IR_ALPHA = 0.4f;  // EMA on the IR voltage (Sharp updates every ~38 ms)
constexpr uint32_t SAMPLE_MS = 50;
constexpr uint32_t PWM_HZ = 1000;
constexpr uint8_t PWM_BITS = 8;

int ir = 0;           // raw mV
float irFilt = 0;     // filtered mV
bool detected = false;
int pwmSigned = 0;
uint32_t lastSampleMs = 0;

void apply(int signedPwm) {
  pwmSigned = constrain(signedPwm, -MAX_PWM, MAX_PWM);
  digitalWrite(pins::DC_IN1, pwmSigned >= 0 ? HIGH : LOW);
  digitalWrite(pins::DC_IN2, pwmSigned >= 0 ? LOW : HIGH);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pins::DC_ENA, abs(pwmSigned));
#else
  ledcWrite(15, abs(pwmSigned));  // core 2.x: high channel, away from ESP32Servo
#endif
}

// Filtered IR voltage -> PWM. 0 when nothing is detected, MIN_RUN_PWM..MAX_PWM otherwise.
int speedFromIr(float mv) {
  if (!detected && mv >= DETECT_ON_MV) detected = true;
  else if (detected && mv < DETECT_OFF_MV) detected = false;
  if (!detected) return 0;

  const float clamped = constrain(mv, (float)DETECT_OFF_MV, (float)NEAR_MV);
  const float t = (clamped - DETECT_OFF_MV) / float(NEAR_MV - DETECT_OFF_MV);  // 0 far .. 1 near
  return MIN_RUN_PWM + lroundf(t * (MAX_PWM - MIN_RUN_PWM));
}

}  // namespace

void begin() {
  pinMode(pins::DC_IN1, OUTPUT);
  pinMode(pins::DC_IN2, OUTPUT);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pins::DC_ENA, PWM_HZ, PWM_BITS);
#else
  ledcSetup(15, PWM_HZ, PWM_BITS);
  ledcAttachPin(pins::DC_ENA, 15);
#endif
  analogSetPinAttenuation(pins::IR, ADC_11db);
  ir = analogReadMilliVolts(pins::IR);
  irFilt = ir;
  apply(0);
}

void update(bool sensorMode) {
  const uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_MS) return;
  lastSampleMs = now;

  ir = analogReadMilliVolts(pins::IR);
  irFilt += IR_ALPHA * (ir - irFilt);
  const int speed = speedFromIr(irFilt);  // always update detection so the GUI shows it in both modes
  if (sensorMode) apply(speed);           // forward only
}

int setPwm(int signedPwm) {
  apply(signedPwm);
  return pwmSigned;
}

void stop() { apply(0); }

int irMv() { return ir; }
float irFiltMv() { return irFilt; }
bool irDetected() { return detected; }
int pwm() { return pwmSigned; }

}  // namespace irdc
