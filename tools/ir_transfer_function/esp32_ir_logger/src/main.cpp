// Sharp IR rangefinder raw-output logger for the transfer-function plot (ESP32).
//
// Wiring is the same as the team firmware (firmware/tsp05_main/src/config.h):
//   Sharp VO -> GPIO14, VCC -> 5V (VIN), GND -> GND.
// The Sharp output stays below ~3.1 V even on a 5 V supply, so it is safe for the ESP32 pin.
// GPIO14 is on ADC2, which only works while Wi-Fi is off. This sketch never starts Wi-Fi.
// A 10 uF capacitor across the sensor's VCC/GND cuts the supply ripple from its LED pulses.
//
// Prints one reading per line as "IR,<raw counts 0-4095>,<millivolts>" every 5 ms (~200 Hz).
// The millivolts come from the chip's factory ADC calibration (analogReadMilliVolts), which
// corrects the ESP32 ADC's nonlinearity, so they are the better "raw electrical output" to plot.

#include <Arduino.h>

constexpr int IR_PIN = 14;
constexpr uint32_t SAMPLE_US = 5000;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(IR_PIN, ADC_11db);  // ~0.15-3.1 V input range
}

void loop() {
  static uint32_t last = 0;
  const uint32_t now = micros();
  if (now - last < SAMPLE_US) return;
  last = now;
  const int raw = analogRead(IR_PIN);
  const int mv = analogReadMilliVolts(IR_PIN);
  Serial.printf("IR,%d,%d\n", raw, mv);
}
