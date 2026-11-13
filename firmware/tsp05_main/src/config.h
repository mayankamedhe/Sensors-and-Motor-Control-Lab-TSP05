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
// Analog sensors. GPIO14 is ADC2: fine while Wi-Fi is off (we only use USB serial).
constexpr int IR = 14;    // Sharp IR rangefinder output
constexpr int FLEX = 32;  // flex sensor divider (power the divider from 3V3, not 5V)
// L298 H-bridge for the SPG30 DC motor.
// WARNING: 0, 2 and 15 are ESP32 strapping pins. During reset/flashing ENA (15) and IN2 (0) float
// HIGH and IN1 (2) LOW, so the motor can spin until setup() runs. Keep motor power off while
// flashing. If uploads fail ("Failed to connect"), the L298 board is pulling GPIO 0 or 2 the wrong way.
constexpr int DC_ENA = 15;  // PWM
constexpr int DC_IN1 = 2;
constexpr int DC_IN2 = 0;
// SPG30E encoder (open-drain Hall outputs): 4.7-10k pull-ups to 3V3, never 5 V.
// Wires: red M+, black M- (to the L298), brown Hall Vcc (5 V), green GND, blue A, purple B.
// GPIO 16/17 are free on ESP32-WROOM DevKits (on WROVER modules they belong to PSRAM: use 4/5 instead).
constexpr int ENC_A = 16;
constexpr int ENC_B = 17;
// Pushbutton to GND (internal pull-up), toggles GUI/SENSOR mode
constexpr int BUTTON = 33;
// Simulation only: slide pot that stands in for the hand in front of the ToF
constexpr int SIM_TOF_SLIDER = 35;
}  // namespace pins

// EasyDriver microstepping. Real board default is 1/8 (MS1 = MS2 = HIGH).
#ifndef MICROSTEP
#define MICROSTEP 8
#endif

// DC encoder counts per OUTPUT-shaft turn (x4 decoding). Starting guess for the SPG30E-30K;
// measure it: `dcenc zero`, turn the output shaft 10 times by hand, `dcenc`, divide by 10.
#ifndef ENC_COUNTS_PER_REV
#define ENC_COUNTS_PER_REV 360
#endif
// +1 or -1. If `dc 100` makes dc_deg go DOWN, set this to -1 (or swap the blue and purple wires).
#ifndef ENC_DIR
#define ENC_DIR 1
#endif

enum class Mode { GUI, SENSOR };
