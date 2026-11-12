// Sharp GP2Y0A02YK IR rangefinder -> SPG30 DC motor speed through an L298 (based on Mayanka's IR_distance.ino).
// SENSOR mode: motor off until a hand/obstacle is detected, then speeds up as it gets closer.
// Open-loop PWM for now. The lab still requires encoder feedback + PID for GUI moves.
#pragma once
#include <Arduino.h>

namespace irdc {

void begin();
void update(bool sensorMode);  // samples every 50 ms; drives the motor only in sensor mode
int setPwm(int signedPwm);     // GUI command: -255..255 (sign = direction), returns clamped value
void stop();

int irMv();          // raw
float irFiltMv();    // filtered (EMA)
bool irDetected();   // hand/obstacle in range (with hysteresis)
int pwm();  // signed

}  // namespace irdc
