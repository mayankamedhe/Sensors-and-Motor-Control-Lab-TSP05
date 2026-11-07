#include "config.h"
#include "ir_dc.h"

namespace irdc {
namespace {

// Mayanka's Uno thresholds were 100 (far) and 600 (near) ADC counts at a 5 V reference,
// i.e. about 490 mV and 2930 mV. Using millivolts keeps them valid on the ESP32's 12-bit ADC.
// The Sharp output stays below ~3.1 V even with the sensor on 5 V, so it is safe for the ESP32 pin.
constexpr int FAR_MV = 490;    // hand far  -> motor off
constexpr int NEAR_MV = 2930;  // hand near -> full speed
constexpr int MIN_SPEED = 0;
constexpr int MAX_SPEED = 255;
constexpr uint32_t SAMPLE_MS = 50;
constexpr uint32_t PWM_HZ = 1000;
constexpr uint8_t PWM_BITS = 8;

int ir = 0;
int pwmSigned = 0;
uint32_t lastSampleMs = 0;

void apply(int signedPwm) {
  pwmSigned = constrain(signedPwm, -MAX_SPEED, MAX_SPEED);
  digitalWrite(pins::DC_IN1, pwmSigned >= 0 ? HIGH : LOW);
  digitalWrite(pins::DC_IN2, pwmSigned >= 0 ? LOW : HIGH);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pins::DC_ENA, abs(pwmSigned));
#else
  ledcWrite(15, abs(pwmSigned));  // core 2.x: high channel, away from ESP32Servo
#endif
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
  apply(0);
}

void update(bool sensorMode) {
  const uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_MS) return;
  lastSampleMs = now;

  ir = analogReadMilliVolts(pins::IR);
  if (sensorMode) {
    // Higher voltage = closer hand = faster, forward only (same as the Uno sketch).
    const int mv = constrain(ir, FAR_MV, NEAR_MV);
    apply(map(mv, FAR_MV, NEAR_MV, MIN_SPEED, MAX_SPEED));
  }
}

int setPwm(int signedPwm) {
  apply(signedPwm);
  return pwmSigned;
}

void stop() { apply(0); }

int irMv() { return ir; }
int pwm() { return pwmSigned; }

}  // namespace irdc
