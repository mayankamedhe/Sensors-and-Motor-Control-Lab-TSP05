// Stepper bring-up test: ESP32 + SparkFun EasyDriver + SM-42BYG011-25
// Turns the motor 1 revolution one way, pauses, then 1 revolution back, forever.
//
// Wiring (power everything OFF before wiring):
//   ESP32 GPIO25 -> EasyDriver STEP
//   ESP32 GPIO26 -> EasyDriver DIR
//   ESP32 GND    -> EasyDriver GND (control-side GND pin)
// EasyDriver defaults: 1/8 microstepping, so 200 * 8 = 1600 steps per revolution.

const int STEP_PIN = 25;
const int DIR_PIN  = 26;

const int   MICROSTEP     = 8;                    // EasyDriver default (MS1 = MS2 = HIGH)
const long  STEPS_PER_REV = 200L * MICROSTEP;     // 1600
const float DEG_PER_STEP  = 360.0 / STEPS_PER_REV; // 0.225 deg

const unsigned int HALF_PERIOD_US = 500;          // 1000 steps/s = 0.625 rev/s

long positionSteps = 0;                           // absolute position, + = DIR HIGH

void moveSteps(long steps) {
  digitalWrite(DIR_PIN, steps >= 0 ? HIGH : LOW);
  delayMicroseconds(5);                           // DIR setup time before first STEP
  long n = labs(steps);
  for (long i = 0; i < n; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(HALF_PERIOD_US);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(HALF_PERIOD_US);
  }
  positionSteps += steps;
}

void moveDegrees(float deg) {
  long steps = lround(deg / DEG_PER_STEP);        // round to nearest achievable step
  moveSteps(steps);
  Serial.printf("requested %.3f deg -> moved %ld steps = %.3f deg | position %.3f deg\n",
                deg, steps, steps * DEG_PER_STEP, positionSteps * DEG_PER_STEP);
}

void setup() {
  Serial.begin(115200);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  delay(1000);
  Serial.println("Stepper test start");
}

void loop() {
  moveDegrees(360.0);
  delay(1000);
  moveDegrees(-360.0);
  delay(1000);
}
