// SPG30E DC motor: quadrature Hall encoder + PID (position and velocity), GUI mode only.
// The motor output itself still goes through irdc::setPwm() (L298, ir_dc.cpp).
#pragma once
#include <Arduino.h>

namespace dcpid {

void begin();
void update();                 // call every loop(): reads the encoder, runs the PID every 20 ms
void moveRelative(float deg);  // closed-loop move, relative to the current position
void setRpm(float rpm);        // closed-loop speed, sign = direction, 0 = stop
void cancel();                 // back to open loop (does not touch the PWM)
bool active();

float angleDeg();  // output-shaft angle since boot (or since `dcenc zero`)
float rpm();       // output-shaft speed, filtered
long counts();     // raw encoder counts
void zero();
void setPosGains(float kp, float ki, float kd);
void setVelGains(float kp, float ki);
void printGains();

}  // namespace dcpid
