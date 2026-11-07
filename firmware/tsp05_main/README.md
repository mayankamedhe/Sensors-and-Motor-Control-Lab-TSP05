# TSP05 integrated firmware (one ESP32)

All sensors and motors in one program, as the lab requires.

| Sensor | Motor | Module | Based on |
|---|---|---|---|
| Sharp IR rangefinder | DC motor speed (L298) | `src/ir_dc.cpp` | Mayanka's `IR_distance.ino` |
| Flex sensor | RC servo angle | `src/flex_servo.cpp` | BillyChern's `flex_servo.ino` |
| VL53L5CX swipe gesture | Stepper ±45° per swipe | `src/tof_swipe.cpp`, `src/stepper_ctrl.cpp` | Mayank's `tof_swipe`, Rai's stepper controller |
| Pushbutton (debounced) | Toggles GUI ↔ SENSOR mode | `src/main.cpp` | |

Pins: [`docs/PINOUT.md`](../../docs/PINOUT.md). Several changed from the individual sketches; check before wiring.

## Modes

- **GUI mode** (default at power-up): motors only move on serial commands. Sensors are still read and reported.
- **SENSOR mode**: IR → DC speed, flex → servo, swipe → stepper. Motion commands are refused.
- Switch modes with the **button** or the `mode` command. Switching stops the stepper and the DC motor.

## Serial interface (115200 baud)

Type `help`. Main commands: `mode [gui|sensor]`, `step <deg> [rpm]` (prints requested vs. actual angle),
`servo <deg>`, `dc <-255..255>`, `stop`, `status`, `tele on|off`, `swipe left|right` (fake swipe for testing).

Every 100 ms the firmware prints `name:value` telemetry (`ir_mv`, `dc_pwm`, `flex_raw_mv`, `flex_filt_mv`, `servo_deg`,
`hand_x`, `step_deg`, …). The Arduino IDE **Serial Plotter** graphs it directly, including flex raw vs. filtered.
Event lines start with `#`. A future ROS 2 bridge can parse the same lines.

## Build and flash real hardware

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
| VL53L5CX hand | **Slide pot** (bottom left). Drag it from one end to the other in under 1.5 s = one swipe |
| Sharp IR | Left **knob** |
| Flex sensor | Right **knob** |
| EasyDriver | A4988 driver (full-step mode, so 1.8°/step instead of 0.225°) |
| DC motor | 3 LEDs: yellow = ENA (brightness = speed), green = IN1, red = IN2 |

Steps:
1. Regenerate the paste-ready files if `src/` changed: `python3 tools/make_wokwi_web.py`.
2. Go to <https://wokwi.com/projects/new/esp32>.
3. Paste `wokwi-web/sketch.ino` over `sketch.ino`, and `wokwi-web/diagram.json` over `diagram.json`.
4. Add a file named `tsp05.h` (tab-bar ▾ → *New file…*) and paste `wokwi-web/tsp05.h` into it.
5. Add a file named `libraries.txt` containing `ESP32Servo`.
6. Press ▶. Click the serial monitor, type `mode sensor`, then turn the knobs and drag the slider.

**VS Code alternative:** install the *Wokwi Simulator* extension, run `pio run -e wokwi`, then *Wokwi: Start Simulator*.
It uses `wokwi.toml` and `diagram.json` in this folder.

## Known gaps (not done yet)

- DC motor: open-loop PWM only. The lab needs **encoder + PID** for GUI position and velocity moves.
- Sensors in physical units: IR is reported in mV, not cm; the flex sensor as % of range.
- Flex calibration (`FLEX_STRAIGHT_MV`, `FLEX_BENT_MV`) is converted from Uno readings and must be **re-measured on the ESP32**.
- ROS 2 bridge and GUI.
- The team has 3 sensors; a 5-person team needs **4**.
