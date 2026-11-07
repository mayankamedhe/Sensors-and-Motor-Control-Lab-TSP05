// Sharp IR rangefinder -> SPG30 DC motor speed through an L298 (based on Mayanka's IR_distance.ino).
// Open-loop PWM for now. The lab still requires encoder feedback + PID for GUI moves.
#pragma once
#include <Arduino.h>

namespace irdc {

void begin();
void update(bool sensorMode);  // samples every 50 ms; drives the motor only in sensor mode
int setPwm(int signedPwm);     // GUI command: -255..255 (sign = direction), returns clamped value
void stop();

int irMv();
int pwm();  // signed

}  // namespace irdc
