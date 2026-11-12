# tsp05_bridge: ROS 2 ↔ ESP32 serial bridge

The lab guideline: *"communicate over serial with a ROS 2 node on the laptop that publishes sensor and motor state topics and subscribes to command topics."*
`serial_bridge` is that node. It is the **only** process that opens the serial port; everything else (GUI, extra nodes) talks to it through topics.

```
ESP32 (firmware/tsp05_main) ──USB serial──> serial_bridge ──topics──> foxglove_bridge ──> Foxglove GUI
```

## Build (ROS 2 Humble or Jazzy)

Works on both, with the same code: **Humble** (Ubuntu 22.04) and **Jazzy** (Ubuntu 24.04).
Below, replace `<distro>` with `humble` or `jazzy`. To check which one you have: `ls /opt/ros`.

```bash
cd ros2_ws
source /opt/ros/<distro>/setup.bash
colcon build --symlink-install
source install/setup.bash
```

## Run

| Setup | Command |
|---|---|
| Real ESP32 (Physical Linux) | `ros2 launch tsp05_bridge bridge.launch.py port:=/dev/ttyUSB0` |
| Real ESP32 (Host Mac + UTM VM) | `ros2 launch tsp05_bridge bridge.launch.py port:=socket://<mac-ip>:4000` (see docs/HOW_TO_RUN.md) |
| No hardware | Terminal 1: `ros2 run tsp05_bridge fake_esp32` (prints a `/dev/pts/N` path). Terminal 2: `ros2 launch tsp05_bridge bridge.launch.py port:=/dev/pts/N` |
| Wokwi simulator (VS Code extension) | Start the simulator, then `ros2 launch tsp05_bridge bridge.launch.py port:=rfc2217://localhost:4000` |
| With the Foxglove GUI | add `foxglove:=true` (needs `sudo apt install ros-<distro>-foxglove-bridge`) |

- **"Permission denied" on `/dev/ttyUSB0`:** `sudo usermod -aG dialout $USER`, then log out and back in.
- **Close the Arduino/PlatformIO serial monitor first.** Only one program can hold the port.
- Opening the port resets the ESP32, so expect `TOF CALIBRATING` for a few seconds.

Quick checks:
```bash
ros2 topic echo /tsp05/stepper/angle_deg
ros2 topic pub --once /tsp05/cmd/stepper_rel_deg std_msgs/msg/Float32 "{data: 37.0}"
ros2 topic pub --once /tsp05/cmd/mode std_msgs/msg/String "{data: sensor}"
```

## Topics (all under `/tsp05`)

**State (published)**

| Topic | Type | Meaning |
|---|---|---|
| `mode` | String | `GUI` or `SENSOR` |
| `connected` | Bool | serial port open (every 2 s) |
| `ir/voltage_mv`, `ir/filtered_mv` | Float32 | Sharp IR output, raw vs. EMA-filtered, mV |
| `ir/distance_cm` | Float32 | IR distance in cm (`units.py` calibration), NaN outside 20–150 cm |
| `ir/detected` | Bool | hand/obstacle detected (motor-on hysteresis in the firmware) |
| `flex/raw_mv`, `flex/filtered_mv` | Float32 | flex divider, raw vs. EMA-filtered (for the raw-vs-filtered plot) |
| `flex/bend_pct`, `flex/bend_deg` | Float32 | bend, % of calibrated range and degrees |
| `tof/state` | String | `INIT`, `CALIBRATING`, `READY`, `ERROR` |
| `tof/control` | String | `swipe` or `follow` |
| `tof/hand_present`, `tof/hand_x`, `tof/hand_mm` | Bool, Float32, Float32 | hand detection; x in columns 0–3 |
| `tof/bearing_deg`, `tof/lateral_mm` | Float32 | hand direction from the sensor axis and sideways offset (NaN = no hand) |
| `tof/swipe` | String | `LEFT` / `RIGHT`, once per swipe |
| `stepper/angle_deg`, `stepper/moving` | Float32, Bool | actual stepper angle since boot |
| `stepper/last_requested_deg`, `stepper/last_actual_deg` | Float32 | last GUI move: requested vs. actual (rounded to whole steps) |
| `servo/angle_deg` | Float32 | commanded servo angle |
| `dc/pwm` | Float32 | DC motor PWM, −255…255 (sign = direction) |
| `dc/angle_deg`, `dc/rpm` | Float32 | encoder position and speed (telemetry keys `dc_deg`, `dc_rpm`; **firmware not done yet**) |
| `log` | String | firmware event lines (mode changes, swipes, command replies) |

**Commands (subscribed).** Motion commands only act in GUI mode; the firmware replies on `log` if it ignores one.

| Topic | Type | Example |
|---|---|---|
| `cmd/mode` | String | `gui`, `sensor`, `toggle` |
| `cmd/stepper_rel_deg` | Float32 | `37.0` → moves 36.9° (nearest step), shown on `stepper/angle_deg` |
| `cmd/servo_deg` | Float32 | `0` … `180` |
| `cmd/dc_pwm` | Float32 | `-255` … `255` (open loop, for testing) |
| `cmd/dc_rel_deg` | Float32 | `90` → serial `dcdeg 90.0`: PID move relative to the encoder position (**firmware not done yet**) |
| `cmd/dc_rpm` | Float32 | `-30` → serial `dcrpm -30.0`: PID velocity, 0 = stop (**firmware not done yet**) |
| `cmd/stop` | Empty | stop stepper and DC motor |
| `cmd/tof_control` | String | `swipe` or `follow` |
| `cmd/raw` | String | any firmware serial command, e.g. `status` |

**Physical units.** The bridge converts raw telemetry into physical units in [`tsp05_bridge/units.py`](tsp05_bridge/units.py). The IR defaults are a fit to the GP2Y0A02 datasheet curve: replace them with the team's own fit from `tools/ir_transfer_function` (multiply its `k` and `c` from Uno counts by 5000/1023 to get mV). Set `FLEX_FULL_DEG` to the angle used when measuring `FLEX_BENT_MV`.

## Serial protocol

Defined in [`tsp05_bridge/protocol.py`](tsp05_bridge/protocol.py). The firmware prints one telemetry line every 100 ms of space-separated `key:number` pairs, plus event lines starting with `#`. To add a value: print a new `key:value` in `printTelemetry()` in `firmware/tsp05_main/src/main.cpp`, then map the key to a topic in `serial_bridge.py`.

Tests (no ROS needed): `python3 -m pytest src/tsp05_bridge/test`

## GUI

The control panel is the rqt plugin in [`tsp05_gui`](../tsp05_gui/README.md). One command starts the bridge and the panel:
`ros2 launch tsp05_gui system.launch.py port:=/dev/ttyUSB0`.

Foxglove also works with these topics: `foxglove:=true` (needs `ros-<distro>-foxglove-bridge`), then connect to `ws://localhost:8765`.
