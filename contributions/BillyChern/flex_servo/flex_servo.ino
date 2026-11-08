/*
  Flex sensor -> RC servo: single-sensor bench test for an Arduino Uno R3.
  Flex sensor is a BEND sensor, not a force sensor.

  Divider: Arduino 5V -- flex sensor -- A0 -- 10 kOhm resistor -- GND
  Servo: D9 = signal. Power servo from external 5–6 V, NOT the Uno 5V.
  Connect the external supply's negative to the Arduino GND.

  Replace STRAIGHT_ADC and BENT_ADC with values measured using Serial Monitor.
  This is a temporary standalone test, not the full multi-sensor lab program.
*/
#include <Servo.h>

const byte FLEX_PIN = A0;
const byte SERVO_PIN = 9;

// Measured with the team's current sensor and wiring on Nov 6, 2026.
// Re-measure if the wiring, sensor, or bend range changes.
const int STRAIGHT_ADC = 477;
const int BENT_ADC = 521;

// Command range; actual shaft position is not independently measured.
// 150 is near the HS-311 nominal pulse limit: stop if the horn binds.
const int SERVO_MIN_DEG = 70;
const int SERVO_MAX_DEG = 150;
const float SMOOTHING = 0.15f; // 0..1: smaller values give more smoothing.

Servo servoMotor;
float filteredAdc;
unsigned long lastReportMs = 0;

void setup() {
  Serial.begin(115200);
  filteredAdc = analogRead(FLEX_PIN);
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(SERVO_MIN_DEG);
  Serial.println(F("Flex sensor bench test. Calibrate STRAIGHT_ADC and BENT_ADC."));
}

void loop() {
  const int rawAdc = analogRead(FLEX_PIN);
  filteredAdc += SMOOTHING * (rawAdc - filteredAdc);

  // Works when BENT_ADC is lower OR higher than STRAIGHT_ADC.
  float fraction = 0.0f;
  if (STRAIGHT_ADC != BENT_ADC) {
    fraction = (filteredAdc - STRAIGHT_ADC) / float(BENT_ADC - STRAIGHT_ADC);
  }
  fraction = constrain(fraction, 0.0f, 1.0f);
  const int servoCommand = SERVO_MIN_DEG +
      int(fraction * (SERVO_MAX_DEG - SERVO_MIN_DEG) + 0.5f);
  servoMotor.write(servoCommand);

  if (millis() - lastReportMs >= 100) {
    lastReportMs = millis();
    Serial.print(F("raw:")); Serial.print(rawAdc);
    Serial.print(F(" filtered:")); Serial.print(filteredAdc, 1);
    Serial.print(F(" bend_pct:")); Serial.print(fraction * 100.0f, 1);
    Serial.print(F(" servo_cmd_deg:")); Serial.println(servoCommand);
  }
  delay(20); // Simple standalone bring-up; remove blocking delay for full integration.
}
