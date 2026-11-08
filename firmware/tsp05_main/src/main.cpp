// TSP05 Sensors & Motor Control Lab: integrated firmware for one ESP32.
//
//   Sharp IR      -> DC motor speed  (ir_dc.cpp,       from Mayanka's IR_distance.ino)
//   Flex sensor   -> RC servo angle  (flex_servo.cpp,  from BillyChern's flex_servo.ino)
//   VL53L5CX      -> stepper, two styles (`tof swipe` / `tof follow`):
//                    swipe:  each left/right swipe turns it +/-45 deg (tof_swipe.cpp, from Mayank's tof_swipe)
//                    follow: the stepper angle tracks the hand's left-right position
//                    (stepper_ctrl.cpp, from Rai's stepper controller)
//   Pushbutton    -> toggles GUI / SENSOR mode (debounced in software)
//
// Serial (115200): type `help`. Telemetry is printed every 100 ms as `name:value` pairs,
// which the Arduino IDE Serial Plotter can graph. Event lines start with `#`.
#include "config.h"
#include "flex_servo.h"
#include "ir_dc.h"
#include "stepper_ctrl.h"
#include "tof_swipe.h"

namespace {

constexpr float SWIPE_DEG = 45.0f;  // stepper move per swipe (CONTEXT.md open question 11)
// Follow mode: hand at the far left of the ToF view -> -FOLLOW_SPAN_DEG/2, far right -> +FOLLOW_SPAN_DEG/2.
constexpr float FOLLOW_SPAN_DEG = 180.0f;
constexpr float FOLLOW_ALPHA = 0.3f;         // EMA on hand x per frame: smooths ToF jitter
constexpr float FOLLOW_DEADBAND_DEG = 2.0f;  // ignore target changes smaller than this
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t TELEMETRY_MS = 100;

Mode mode = Mode::GUI;  // start in GUI mode so nothing moves on its own at power-up
enum class TofControl { SWIPE, FOLLOW };
TofControl tofControl = TofControl::SWIPE;
// Stepper speed when the ToF drives it (swipe and follow). Change at runtime with `speed <rpm>`.
// There is no acceleration ramp: if the motor buzzes or skips instead of turning, lower it.
float sensorRpm = 60.0f;
bool telemetryOn = true;
String line;

const char *modeName() { return mode == Mode::GUI ? "GUI" : "SENSOR"; }

void setMode(Mode m) {
  mode = m;
  // Leaving either mode: stop the motors that run continuously so nothing keeps going unattended.
  stepper::stop();
  irdc::stop();
  while (tof::pollSwipe() != 0) {}  // drop swipes that happened before the switch
  Serial.printf("# MODE %s\n", modeName());
}

// ---------------- Debounced pushbutton ----------------
void updateButton() {
  static int stable = HIGH, lastRead = HIGH;
  static uint32_t changedMs = 0;
  const int r = digitalRead(pins::BUTTON);
  if (r != lastRead) {
    lastRead = r;
    changedMs = millis();
  }
  if (r != stable && millis() - changedMs >= DEBOUNCE_MS) {
    stable = r;
    if (stable == LOW) setMode(mode == Mode::GUI ? Mode::SENSOR : Mode::GUI);  // on press
  }
}

// ---------------- Serial commands ----------------
void printHelp() {
  Serial.println(F("# Commands (any mode):"));
  Serial.println(F("#   mode [gui|sensor]   no argument = toggle (same as the button)"));
  Serial.println(F("#   stop                stop stepper and DC motor"));
  Serial.println(F("#   status | help | tele on|off"));
  Serial.println(F("#   tof swipe|follow    ToF -> stepper style: +/-45 deg per swipe, or follow the hand"));
  Serial.println(F("#   speed <rpm>         stepper speed in SENSOR mode (GUI: step <deg> <rpm>)"));
  Serial.println(F("#   swipe left|right    fake a ToF swipe (acts in SENSOR mode, swipe style)"));
  Serial.println(F("# Commands (GUI mode only):"));
  Serial.println(F("#   step <deg> [rpm]    relative stepper move, e.g. step 37 or step -90 20"));
  Serial.println(F("#   move <steps> <rpm>  forward <rpm>  reverse <rpm>"));
  Serial.println(F("#   servo <deg>         0-180"));
  Serial.println(F("#   dc <pwm>            -255..255, sign = direction"));
}

void printStatus() {
  const tof::Status t = tof::status();
  Serial.printf("# mode=%s tof_control=%s | stepper pos=%.3f deg (%ld steps) %s rpm=%.1f | servo=%d deg | dc_pwm=%d | "
                "ir=%d mV | flex=%d mV (%.1f%%) | tof=%s frames=%lu\n",
                modeName(), tofControl == TofControl::FOLLOW ? "follow" : "swipe", stepper::positionDeg(), stepper::positionSteps(),
                stepper::isMoving() ? "MOVING" : "IDLE", stepper::currentRpm(), flexservo::servoDeg(),
                irdc::pwm(), irdc::irMv(), flexservo::rawMv(), flexservo::bendPct(), tof::stateName(t.state),
                (unsigned long)t.frames);
}

bool guiOnly() {
  if (mode == Mode::GUI) return true;
  Serial.println(F("# ignored: switch to GUI mode first (`mode gui` or press the button)"));
  return false;
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;
  const char *c = cmd.c_str();
  float a, b;
  long n;
  char word[16];

  if (cmd == "help") printHelp();
  else if (cmd == "status") printStatus();
  else if (cmd == "stop") { stepper::stop(); irdc::stop(); Serial.println(F("# stopped")); }
  else if (cmd == "mode") setMode(mode == Mode::GUI ? Mode::SENSOR : Mode::GUI);
  else if (cmd == "mode gui") setMode(Mode::GUI);
  else if (cmd == "mode sensor") setMode(Mode::SENSOR);
  else if (cmd == "tele on") telemetryOn = true;
  else if (cmd == "tele off") telemetryOn = false;
  else if (cmd == "tof swipe") { tofControl = TofControl::SWIPE; stepper::stop(); Serial.println(F("# TOF CONTROL swipe")); }
  else if (sscanf(c, "speed %f", &a) == 1) {
    sensorRpm = constrain(a, 1.0f, stepper::MAX_RPM);
    Serial.printf("# SPEED %.1f rpm (sensor-driven stepper)\n", sensorRpm);
  }
  else if (cmd == "tof follow") { tofControl = TofControl::FOLLOW; stepper::stop(); Serial.println(F("# TOF CONTROL follow")); }
  else if (sscanf(c, "swipe %15s", word) == 1) tof::injectSwipe(strcmp(word, "right") == 0 ? 1 : -1);
  else if (sscanf(c, "step %f %f", &a, &b) == 2) {
    if (guiOnly()) Serial.printf("# STEP requested %.3f deg -> actual %.3f deg\n", a, stepper::moveDegrees(a, b));
  } else if (sscanf(c, "step %f", &a) == 1) {
    if (guiOnly()) Serial.printf("# STEP requested %.3f deg -> actual %.3f deg\n", a, stepper::moveDegrees(a));
  } else if (sscanf(c, "move %ld %f", &n, &b) == 2) {
    if (guiOnly()) { stepper::moveSteps(n, b); Serial.printf("# MOVE %ld steps\n", n); }
  } else if (sscanf(c, "forward %f", &b) == 1) {
    if (guiOnly()) stepper::runContinuous(true, b);
  } else if (sscanf(c, "reverse %f", &b) == 1) {
    if (guiOnly()) stepper::runContinuous(false, b);
  } else if (sscanf(c, "servo %f", &a) == 1) {
    if (guiOnly()) Serial.printf("# SERVO %d deg\n", flexservo::setServoDeg(lroundf(a)));
  } else if (sscanf(c, "dc %f", &a) == 1) {
    if (guiOnly()) Serial.printf("# DC pwm %d\n", irdc::setPwm(lroundf(a)));
  } else Serial.printf("# unknown command: %s (type help)\n", c);
}

void readSerial() {
  while (Serial.available()) {
    const char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (line.length()) handleCommand(line);
      line = "";
    } else if (line.length() < 100) {
      line += ch;
    }
  }
}

// ---------------- Sensor -> stepper ----------------
// Follow style: once per new ToF frame, point the stepper at the (smoothed) hand x.
// When the hand leaves, the stepper holds its last position.
void handleFollow() {
  static uint32_t lastFrame = 0;
  static bool tracking = false;
  static float xSmooth = 0, lastTarget = 0;
  while (tof::pollSwipe() != 0) {}  // swipes are not used in this style

  const tof::Status t = tof::status();
  if (t.frames == lastFrame) return;
  lastFrame = t.frames;
  if (mode != Mode::SENSOR || !t.handPresent) {
    tracking = false;
    return;
  }
  if (!tracking) {  // hand just appeared: start the filter at its position
    tracking = true;
    xSmooth = t.handX;
    lastTarget = stepper::positionDeg();
  }
  xSmooth += FOLLOW_ALPHA * (t.handX - xSmooth);
  const float target = (xSmooth / (tof::COLUMNS - 1) - 0.5f) * FOLLOW_SPAN_DEG;
  if (fabsf(target - lastTarget) < FOLLOW_DEADBAND_DEG) return;
  lastTarget = stepper::moveToDegrees(target, sensorRpm);
}

void handleSwipes() {
  if (tofControl == TofControl::FOLLOW) {
    handleFollow();
    return;
  }
  const int dir = tof::pollSwipe();
  if (dir == 0) return;
  if (mode != Mode::SENSOR) {
    Serial.printf("# SWIPE %s (GUI mode: stepper not moved)\n", dir > 0 ? "RIGHT" : "LEFT");
    return;
  }
  if (stepper::isMoving()) {
    Serial.println(F("# SWIPE ignored: stepper still moving"));
    return;
  }
  const float actual = stepper::moveDegrees(dir * SWIPE_DEG, sensorRpm);
  Serial.printf("# SWIPE %s -> stepper %+.3f deg\n", dir > 0 ? "RIGHT" : "LEFT", actual);
}

void printTelemetry() {
  static uint32_t lastMs = 0;
  static tof::State lastTofState = tof::State::INIT;
  const uint32_t now = millis();
  if (now - lastMs < TELEMETRY_MS) return;
  lastMs = now;

  const tof::Status t = tof::status();
  if (t.state != lastTofState) {
    lastTofState = t.state;
    Serial.printf("# TOF %s%s\n", tof::stateName(t.state),
                  t.state == tof::State::CALIBRATING ? " (keep your hand away from the sensor)" : "");
  }
  if (!telemetryOn) return;
  Serial.printf("sensor_mode:%d tof_follow:%d ir_mv:%d dc_pwm:%d flex_raw_mv:%d flex_filt_mv:%.0f bend_pct:%.1f "
                "servo_deg:%d hand:%d hand_x:%.2f hand_mm:%d step_deg:%.3f\n",
                mode == Mode::SENSOR, tofControl == TofControl::FOLLOW, irdc::irMv(), irdc::pwm(), flexservo::rawMv(), flexservo::filteredMv(),
                flexservo::bendPct(), flexservo::servoDeg(), t.handPresent, t.handX, t.nearestMm,
                stepper::positionDeg());
}

}  // namespace

void setup() {
  Serial.setTxBufferSize(1024);  // printing must not block, or the stepper pulses stutter
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n# TSP05 integrated firmware"));
#ifdef SIM_TOF
  Serial.println(F("# SIMULATION build: ToF replaced by the slide pot (drag it end to end = swipe)"));
#endif

  pinMode(pins::BUTTON, INPUT_PULLUP);
  stepper::begin();
  flexservo::begin();  // attach the servo before the DC PWM so they get separate LEDC channels
  irdc::begin();
  tof::begin();        // background task on core 0; sensor firmware upload takes a few seconds

  printHelp();
  Serial.printf("# MODE %s\n", modeName());
}

void loop() {
  stepper::update();
  updateButton();
  readSerial();
  const bool sensorMode = mode == Mode::SENSOR;
  irdc::update(sensorMode);
  flexservo::update(sensorMode);
  handleSwipes();
  printTelemetry();
}
