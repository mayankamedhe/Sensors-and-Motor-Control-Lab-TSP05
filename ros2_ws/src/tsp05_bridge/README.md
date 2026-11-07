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
| Real ESP32 | `ros2 launch tsp05_bridge bridge.launch.py port:=/dev/ttyUSB0` |
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
| `ir/voltage_mv` | Float32 | Sharp IR output, mV |
| `flex/raw_mv`, `flex/filtered_mv` | Float32 | flex divider, raw vs. EMA-filtered (for the raw-vs-filtered plot) |
| `flex/bend_pct` | Float32 | bend, % of calibrated range |
| `tof/state` | String | `INIT`, `CALIBRATING`, `READY`, `ERROR` |
| `tof/control` | String | `swipe` or `follow` |
| `tof/hand_present`, `tof/hand_x`, `tof/hand_mm` | Bool, Float32, Float32 | hand detection; x in columns 0–3 |
| `tof/swipe` | String | `LEFT` / `RIGHT`, once per swipe |
| `stepper/angle_deg`, `stepper/moving` | Float32, Bool | actual stepper angle since boot |
| `servo/angle_deg` | Float32 | commanded servo angle |
| `dc/pwm` | Float32 | DC motor PWM, −255…255 (sign = direction) |
| `log` | String | firmware event lines (mode changes, swipes, command replies) |

**Commands (subscribed).** Motion commands only act in GUI mode; the firmware replies on `log` if it ignores one.

| Topic | Type | Example |
|---|---|---|
| `cmd/mode` | String | `gui`, `sensor`, `toggle` |
| `cmd/stepper_rel_deg` | Float32 | `37.0` → moves 36.9° (nearest step), shown on `stepper/angle_deg` |
| `cmd/servo_deg` | Float32 | `0` … `180` |
| `cmd/dc_pwm` | Float32 | `-255` … `255` |
| `cmd/stop` | Empty | stop stepper and DC motor |
| `cmd/tof_control` | String | `swipe` or `follow` |
| `cmd/raw` | String | any firmware serial command, e.g. `status` |

## Serial protocol

Defined in [`tsp05_bridge/protocol.py`](tsp05_bridge/protocol.py). The firmware prints one telemetry line every 100 ms of space-separated `key:number` pairs, plus event lines starting with `#`. To add a value: print a new `key:value` in `printTelemetry()` in `firmware/tsp05_main/src/main.cpp`, then map the key to a topic in `serial_bridge.py`.

Tests (no ROS needed): `python3 -m pytest src/tsp05_bridge/test`

## GUI (Foxglove): not built yet

Plan: `foxglove:=true`, then in Foxglove *Open connection → Foxglove WebSocket → ws://localhost:8765*, and add:
- **Plot**: `flex/raw_mv` + `flex/filtered_mv` (filter demo), `ir/voltage_mv`, `stepper/angle_deg`.
- **Gauge** or **Indicator**: `servo/angle_deg`, `dc/pwm`, `mode`, `tof/hand_present`.
- **Publish** panels (buttons): `cmd/mode`, `cmd/stepper_rel_deg`, `cmd/servo_deg`, `cmd/dc_pwm`, `cmd/stop`.
- **Log / Raw Messages**: `log`.

Export the layout (*Layout → Export*) to `ros2_ws/src/tsp05_bridge/foxglove/` so everyone uses the same one.
