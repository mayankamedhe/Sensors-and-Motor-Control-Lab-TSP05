/*
  ESP32 + EasyDriver serial stepper controller

  Wiring:
    ESP32 GPIO 25 -> EasyDriver STEP
    ESP32 GPIO 26 -> EasyDriver DIR
    ESP32 GND     -> EasyDriver GND

  EasyDriver defaults to 1/8 microstepping:
    200 full steps/revolution × 8 = 1600 pulses/revolution
*/

const uint8_t STEP_PIN = 25;
const uint8_t DIR_PIN  = 26;

const long STEPS_PER_REV = 1600;

bool motorRunning = false;
bool continuousMode = false;

long stepsRemaining = 0;
long completedSteps = 0;

float currentRPM = 30.0;
unsigned long stepIntervalUs = 0;
unsigned long lastStepTimeUs = 0;

String serialCommand;

// ------------------------------------------------------------

void setSpeedRPM(float rpm)
{
  rpm = abs(rpm);

  if (rpm < 0.1) {
    rpm = 0.1;
  }

  currentRPM = rpm;

  // STEP pulses per second = RPM × steps/rev ÷ 60
  float stepsPerSecond = (currentRPM * STEPS_PER_REV) / 60.0;

  stepIntervalUs =
      (unsigned long)(1000000.0 / stepsPerSecond);
}

void setDirection(bool forward)
{
  digitalWrite(DIR_PIN, forward ? HIGH : LOW);
}

void stopMotor()
{
  motorRunning = false;
  continuousMode = false;
  stepsRemaining = 0;

  Serial.println("Motor stopped");
}

void moveSteps(long steps, float rpm)
{
  if (steps == 0) {
    Serial.println("Number of steps cannot be zero");
    return;
  }

  setDirection(steps > 0);
  setSpeedRPM(rpm);

  stepsRemaining = abs(steps);
  completedSteps = 0;
  continuousMode = false;
  motorRunning = true;
  lastStepTimeUs = micros();

  Serial.print("Moving ");
  Serial.print(steps);
  Serial.print(" microsteps at ");
  Serial.print(currentRPM);
  Serial.println(" RPM");
}

void startContinuous(bool forward, float rpm)
{
  setDirection(forward);
  setSpeedRPM(rpm);

  completedSteps = 0;
  continuousMode = true;
  motorRunning = true;
  lastStepTimeUs = micros();

  Serial.print("Continuous movement: ");
  Serial.print(forward ? "forward" : "reverse");
  Serial.print(" at ");
  Serial.print(currentRPM);
  Serial.println(" RPM");
}

// ------------------------------------------------------------

void updateMotor()
{
  if (!motorRunning) {
    return;
  }

  unsigned long now = micros();

  if ((unsigned long)(now - lastStepTimeUs) >= stepIntervalUs) {
    lastStepTimeUs = now;

    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(3);
    digitalWrite(STEP_PIN, LOW);

    completedSteps++;

    if (!continuousMode) {
      stepsRemaining--;

      if (stepsRemaining <= 0) {
        motorRunning = false;

        Serial.print("Movement complete: ");
        Serial.print(completedSteps);
        Serial.println(" microsteps");
      }
    }
  }
}

// ------------------------------------------------------------

void printStatus()
{
  Serial.println("--- Motor status ---");

  Serial.print("State: ");
  Serial.println(motorRunning ? "RUNNING" : "STOPPED");

  Serial.print("Mode: ");
  Serial.println(continuousMode ? "CONTINUOUS" : "POSITION");

  Serial.print("Speed: ");
  Serial.print(currentRPM);
  Serial.println(" RPM");

  Serial.print("Steps completed: ");
  Serial.println(completedSteps);

  if (!continuousMode) {
    Serial.print("Steps remaining: ");
    Serial.println(stepsRemaining);
  }
}

void printHelp()
{
  Serial.println();
  Serial.println("Available commands:");
  Serial.println("  move <steps> <rpm>");
  Serial.println("  forward <rpm>");
  Serial.println("  reverse <rpm>");
  Serial.println("  stop");
  Serial.println("  status");
  Serial.println("  help");
  Serial.println();
  Serial.println("Examples:");
  Serial.println("  move 1600 30");
  Serial.println("  move -800 20");
  Serial.println("  forward 40");
  Serial.println("  reverse 15");
  Serial.println("  stop");
  Serial.println();
}

// ------------------------------------------------------------

void processCommand(String command)
{
  command.trim();
  command.toLowerCase();

  if (command.length() == 0) {
    return;
  }

  if (command == "stop") {
    stopMotor();
    return;
  }

  if (command == "status") {
    printStatus();
    return;
  }

  if (command == "help") {
    printHelp();
    return;
  }

  long steps;
  float rpm;

  if (sscanf(command.c_str(), "move %ld %f", &steps, &rpm) == 2) {
    moveSteps(steps, rpm);
    return;
  }

  if (sscanf(command.c_str(), "forward %f", &rpm) == 1) {
    startContinuous(true, rpm);
    return;
  }

  if (sscanf(command.c_str(), "reverse %f", &rpm) == 1) {
    startContinuous(false, rpm);
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(command);
  Serial.println("Type 'help' for available commands.");
}

// ------------------------------------------------------------

void setup()
{
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);

  Serial.begin(115200);
  Serial.setTimeout(20);

  setSpeedRPM(30);

  delay(500);

  Serial.println();
  Serial.println("ESP32 EasyDriver controller ready");
  Serial.println("Type 'help' for available commands");
}

void loop()
{
  updateMotor();

  while (Serial.available()) {
    char incoming = Serial.read();

    if (incoming == '\n' || incoming == '\r') {
      if (serialCommand.length() > 0) {
        processCommand(serialCommand);
        serialCommand = "";
      }
    } else {
      serialCommand += incoming;

      // Prevent unlimited input growth
      if (serialCommand.length() > 100) {
        serialCommand = "";
        Serial.println("Command too long");
      }
    }
  }
}