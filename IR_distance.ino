// Pin Definitions
const int IR_SENSOR_PIN = A0;  // Sharp IR Sensor signal pin
const int MOTOR_PWM_PIN = 9;   // PWM pin to control motor speed 
const int MOTOR_IN1_PIN = 7;   // Direction pin 1
const int MOTOR_IN2_PIN = 8;   // Direction pin 2

// Sharp IR Sensor Thresholds (Analog Read Values 0 - 1023)
// Sharp sensors output HIGHER voltage (~2.5V - 3.1V) when objects are CLOSE
// and LOWER voltage (~0.4V - 0.5V) when objects are FAR away.
const int FAR_VAL  = 100;  // Threshold when hand is far (min motor speed threshold)
const int NEAR_VAL = 600;  // Threshold when hand is close (max motor speed threshold)

// Motor Speed Limits (PWM: 0 to 255)
const int MIN_SPEED = 0;   // Motor off when hand is far
const int MAX_SPEED = 255; // Full speed when hand is close

void setup() {
  Serial.begin(9600);

  // Set motor control pins as outputs
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);

  // Set initial motor direction (forward)
  digitalWrite(MOTOR_IN1_PIN, HIGH);
  digitalWrite(MOTOR_IN2_PIN, LOW);
}

void loop() {
  // Read raw analog value from Sharp IR Sensor
  int rawAnalog = analogRead(IR_SENSOR_PIN);

  // Constrain the reading within expected thresholds
  int constrainedVal = constrain(rawAnalog, FAR_VAL, NEAR_VAL);

  // Map the sensor reading to PWM speed (Higher voltage = Closer hand = Higher speed)
  int motorSpeed = map(constrainedVal, FAR_VAL, NEAR_VAL, MIN_SPEED, MAX_SPEED);

  // Output PWM signal to motor driver
  analogWrite(MOTOR_PWM_PIN, motorSpeed);

  // Print values to Serial Monitor for calibration
  Serial.print("Raw Sensor Value: ");
  Serial.print(rawAnalog);
  Serial.print("  ->  Motor Speed (PWM): ");
  Serial.println(motorSpeed);

  delay(50); // Small delay for stable readings
}