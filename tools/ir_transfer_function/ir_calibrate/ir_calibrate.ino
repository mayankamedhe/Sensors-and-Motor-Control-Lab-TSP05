// Sharp IR rangefinder raw-output logger for the transfer-function plot.
// Board: Arduino Uno. Sharp VO -> A0, VCC -> 5V, GND -> GND.
// A 10 uF capacitor across the sensor's VCC/GND (close to the sensor) cuts the
// supply ripple caused by its ~1 kHz LED pulses, which otherwise shows up as ADC noise.
//
// Prints one raw ADC reading per line as "IR,<counts>" every 5 ms (~200 Hz).
// tools/ir_transfer_function/ir_transfer.py reads these lines; the Serial Monitor works too.

const int IR_PIN = A0;
const unsigned long SAMPLE_US = 5000;

void setup() {
  Serial.begin(115200);
  analogReference(DEFAULT);  // 5 V reference, so volts = counts * 5.0 / 1023
}

void loop() {
  static unsigned long last = 0;
  const unsigned long now = micros();
  if (now - last < SAMPLE_US) return;
  last = now;
  Serial.print("IR,");
  Serial.println(analogRead(IR_PIN));
}
