// Shared pin map and constants for the integrated TSP05 firmware.
// Keep docs/PINOUT.md in sync with this file.
#pragma once
#include <Arduino.h>

namespace pins {
// VL53L5CX ToF imager (I2C, 3.3 V)
constexpr int TOF_SDA = 21;
constexpr int TOF_SCL = 22;
// EasyDriver stepper
constexpr int STEP = 25;
constexpr int DIR = 26;
// HS-311 RC servo signal (servo powered from an external 5-6 V supply)
constexpr int SERVO = 13;
// Analog sensors: must be ADC1 pins (GPIO32-39). ADC2 pins stop working when Wi-Fi is on.
constexpr int IR = 34;    // Sharp IR rangefinder output
constexpr int FLEX = 32;  // flex sensor divider (power the divider from 3V3, not 5V)
// L298 H-bridge for the SPG30 DC motor
constexpr int DC_ENA = 18;  // PWM
constexpr int DC_IN1 = 19;
constexpr int DC_IN2 = 23;
// Pushbutton to GND (internal pull-up), toggles GUI/SENSOR mode
constexpr int BUTTON = 33;
// Simulation only: slide pot that stands in for the hand in front of the ToF
constexpr int SIM_TOF_SLIDER = 35;
}  // namespace pins

// EasyDriver microstepping. Real board default is 1/8 (MS1 = MS2 = HIGH).
#ifndef MICROSTEP
#define MICROSTEP 8
#endif

enum class Mode { GUI, SENSOR };
