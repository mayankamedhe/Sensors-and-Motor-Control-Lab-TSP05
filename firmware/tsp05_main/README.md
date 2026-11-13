# TSP05 integrated firmware (one ESP32)

All sensors and motors in one program, as the lab requires.

| Sensor | Motor | Module | Based on |
|---|---|---|---|
| Sharp IR rangefinder (GP2Y0A02) | DC motor: off until a hand is detected (~80 cm), then faster as it gets closer (full speed at ~20 cm) | `src/ir_dc.cpp` | Mayanka's `IR_distance.ino` |
| Flex sensor | RC servo angle | `src/flex_servo.cpp` | BillyChern's `flex_servo.ino` |
| VL53L5CX hand position | Stepper: ±45° per swipe (`tof swipe`, default) or follows the hand (`tof follow`) | `src/tof_swipe.cpp`, `src/stepper_ctrl.cpp` | Mayank's `tof_swipe`, Rai's stepper controller |
| Pushbutton (debounced) | Toggles GUI ↔ SENSOR mode | `src/main.cpp` | |

Pins: [`docs/PINOUT.md`](../../docs/PINOUT.md) (matches the team wiring; the DC motor is on strapping pins, read the warning there).

## Modes

- **GUI mode** (default at power-up): motors only move on serial commands. Sensors are still read and reported.
- **SENSOR mode**: IR → DC speed (off with nothing in view; tune `DETECT_ON_MV`, `NEAR_MV`, `MIN_RUN_PWM` in `src/ir_dc.cpp`), flex → servo, swipe → stepper. Motion commands are refused.
- Switch modes with the **button** or the `mode` command. Switching stops the stepper and the DC motor.

## Serial interface (115200 baud)

Type `help`. Main commands: `mode [gui|sensor]`, `step <deg> [rpm]` (prints requested vs. actual angle),
`servo <deg>`, `dc <-255..255>` (open loop), `dcdeg <deg>` / `dcrpm <rpm>` (closed loop), `dcenc [zero]`, `dcgain ...`, `stop`, `status`, `tele on|off`, `tof swipe|follow`, `speed <rpm>` (stepper speed in SENSOR mode, default 180, max 300; moves ramp up from 60 rpm), `swipe left|right` (fake swipe for testing).

Every 100 ms the firmware prints `name:value` telemetry (`ir_mv`, `dc_pwm`, `flex_raw_mv`, `flex_filt_mv`, `servo_deg`,
`hand_x`, `step_deg`, …). The Arduino IDE **Serial Plotter** graphs it directly, including flex raw vs. filtered.
Event lines start with `#`. The ROS 2 bridge (`ros2_ws/src/tsp05_bridge`) parses these lines into topics.

## Build and flash real hardware

Step-by-step for Ubuntu and macOS, with troubleshooting: [docs/HOW_TO_RUN.md](../../docs/HOW_TO_RUN.md).

```bash
cd firmware/tsp05_main
pio run -e esp32dev -t upload
pio device monitor
```

Keep your hand away from the ToF for the first ~3 s after boot (`# TOF CALIBRATING`).

## Simulate online (wokwi.com)

Wokwi has no VL53L5CX, DC motor or EasyDriver, so the simulation substitutes:

| Real part | In the simulator |
|---|---|
| VL53L5CX hand | **Slide pot** (bottom). The outer 3% at each end = no hand; anywhere else = hand at that left-right position. *Swipe style:* drag end to end in under 1.5 s. *Follow style:* the stepper tracks the slider |
| Sharp IR | Left **knob** |
| Flex sensor | Right **knob** |
| EasyDriver | A4988 driver (full-step mode, so 1.8°/step instead of 0.225°) |
| DC motor | 3 LEDs: yellow = ENA (brightness = speed), green = IN1 (forward), red = IN2 (reverse) |
| Mode button | Blue pushbutton: toggles GUI ↔ SENSOR mode |

Steps:
1. Regenerate the paste-ready files if `src/` changed: `python3 tools/make_wokwi_web.py`.
2. Go to <https://wokwi.com/projects/new/esp32>.
3. Paste `wokwi-web/sketch.ino` over `sketch.ino`, and `wokwi-web/diagram.json` over `diagram.json`.
4. Add a file named `tsp05.h` (tab-bar ▾ → *New file…*) and paste `wokwi-web/tsp05.h` into it.
5. Add a file named `libraries.txt` containing `ESP32Servo`.
6. Press ▶. Click the serial monitor, type `mode sensor`, then turn the knobs and drag the slider.

**VS Code alternative:** install the *Wokwi Simulator* extension, run `pio run -e wokwi`, then *Wokwi: Start Simulator*.
It uses `wokwi.toml` and `diagram.json` in this folder.

## DC motor: encoder + PID (`src/dc_pid.cpp`)

GUI mode only. `dcdeg <deg>` moves the output shaft by that many degrees from where it is (PID on position, reports `# DCDEG reached at ... (error ...)`); `dcrpm <rpm>` holds a speed in either direction (feedforward + PI), `dcrpm 0` stops. `dc <pwm>`, `stop` and a mode change cancel the closed loop. Telemetry: `dc_deg`, `dc_rpm`.

Bring-up, in order (GUI mode, motor supply on):
1. **Direction:** `dc 100`, then `dc 0`. `dc_deg` must have gone **up**. If it went down, set `ENC_DIR` to -1 in `config.h` (or swap blue/purple). If it is reversed, the PID would push the wrong way: the firmware detects that, stops the motor and prints `DC STOPPED: encoder direction looks reversed`.
2. **Counts per turn:** `dcenc zero`, turn the output shaft exactly 10 turns by hand, `dcenc`, divide by 10, put it in `ENC_COUNTS_PER_REV`.
3. **Feedforward:** `dc 255`, read `dc_rpm`, put it in `RPM_AT_FULL_PWM` in `dc_pid.cpp`. Set `MIN_PWM` to the lowest PWM that turns the shaft.
4. **Tune live** (no re-flash): `dcgain pos <kp> <ki> <kd>` (default 3 0.5 0.25), `dcgain vel <kp> <ki>` (default 1.5 4). Overshoot → raise kd or lower kp; stops short → raise ki. Copy the final values into `dc_pid.cpp`.

## Known gaps (not done yet)

- DC motor PID gains and `ENC_COUNTS_PER_REV` are simulation-tuned starting values: measure and tune on the real motor (see *DC motor: encoder + PID*).
- Sensors in physical units: IR is reported in mV, not cm; the flex sensor as % of range.
- Flex calibration (`FLEX_STRAIGHT_MV`, `FLEX_BENT_MV`) is converted from Uno readings and must be **re-measured on the ESP32**.
- GUI: rqt panel in `ros2_ws/src/tsp05_gui`, tested with the fake ESP32 only.
- The team has 3 sensors; a 5-person team needs **4**.
