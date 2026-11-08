// Flex (bend) sensor -> HS-311 RC servo (based on BillyChern's flex_servo.ino).
#pragma once
#include <Arduino.h>

namespace flexservo {

// Servo limits in GUI mode. HS-311 nominal travel; back off if the horn binds or buzzes.
constexpr int GUI_MIN_DEG = 0;
constexpr int GUI_MAX_DEG = 180;

void begin();
void update(bool sensorMode);  // samples every 20 ms; drives the servo only in sensor mode
int setServoDeg(int deg);      // GUI command, returns the clamped angle actually commanded

int rawMv();
float filteredMv();
float bendPct();
int servoDeg();

}  // namespace flexservo
