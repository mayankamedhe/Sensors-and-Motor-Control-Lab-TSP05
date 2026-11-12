"""ROS 2 <-> ESP32 serial bridge for the TSP05 lab.

The only node that opens the serial port. Publishes sensor and motor state from the
firmware's telemetry and forwards command topics to the firmware as serial commands.

  ros2 run tsp05_bridge serial_bridge --ros-args -p port:=/dev/ttyUSB0
  port can also be a pyserial URL, e.g. rfc2217://localhost:4000 for the Wokwi VS Code simulator.
"""
from __future__ import annotations

import serial
import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, Empty, Float32, String

import math

from tsp05_bridge import protocol, units

# telemetry key -> (topic, message type, converter)
FLOAT_TOPICS = {
    "ir_mv": "ir/voltage_mv",
    "ir_filt_mv": "ir/filtered_mv",
    "flex_raw_mv": "flex/raw_mv",
    "flex_filt_mv": "flex/filtered_mv",
    "bend_pct": "flex/bend_pct",
    "hand_x": "tof/hand_x",
    "hand_mm": "tof/hand_mm",
    "servo_deg": "servo/angle_deg",
    "dc_pwm": "dc/pwm",
    "dc_deg": "dc/angle_deg",   # encoder position (needs encoder firmware)
    "dc_rpm": "dc/rpm",         # encoder speed (needs encoder firmware)
    "step_deg": "stepper/angle_deg",
}
BOOL_TOPICS = {
    "hand": "tof/hand_present",
    "step_moving": "stepper/moving",
    "ir_detect": "ir/detected",
}
# Physical-unit topics computed here from the raw telemetry (see units.py). NaN = out of range / no hand.
DERIVED_TOPICS = ["ir/distance_cm", "flex/bend_deg", "tof/bearing_deg", "tof/lateral_mm",
                  "stepper/last_requested_deg", "stepper/last_actual_deg"]


class SerialBridge(Node):
    def __init__(self) -> None:
        super().__init__("tsp05_serial_bridge")
        self.port = self.declare_parameter("port", "/dev/ttyUSB0").value
        self.baud = int(self.declare_parameter("baud", 115200).value)
        self.ser: serial.SerialBase | None = None
        self.rx = b""
        # Calibration (physical units). Override at launch, e.g. ir_k:=... after measuring.
        self.ir_k = float(self.declare_parameter("ir_k", units.IR_K).value)
        self.ir_d0 = float(self.declare_parameter("ir_d0", units.IR_D0).value)
        self.ir_c = float(self.declare_parameter("ir_c", units.IR_C).value)
        self.flex_full_deg = float(self.declare_parameter("flex_full_deg", units.FLEX_FULL_DEG).value)

        self.float_pubs = {k: self.create_publisher(Float32, t, 10) for k, t in FLOAT_TOPICS.items()}
        self.bool_pubs = {k: self.create_publisher(Bool, t, 10) for k, t in BOOL_TOPICS.items()}
        self.derived_pubs = {t: self.create_publisher(Float32, t, 10) for t in DERIVED_TOPICS}
        self.mode_pub = self.create_publisher(String, "mode", 10)
        self.tof_state_pub = self.create_publisher(String, "tof/state", 10)
        self.tof_control_pub = self.create_publisher(String, "tof/control", 10)
        self.swipe_pub = self.create_publisher(String, "tof/swipe", 10)
        self.log_pub = self.create_publisher(String, "log", 50)
        self.connected_pub = self.create_publisher(Bool, "connected", 10)

        self.create_subscription(String, "cmd/mode", lambda m: self.send(protocol.cmd_mode, m.data), 10)
        self.create_subscription(String, "cmd/tof_control", lambda m: self.send(protocol.cmd_tof_control, m.data), 10)
        self.create_subscription(Float32, "cmd/stepper_rel_deg", lambda m: self.send(protocol.cmd_stepper_rel_deg, m.data), 10)
        self.create_subscription(Float32, "cmd/servo_deg", lambda m: self.send(protocol.cmd_servo_deg, m.data), 10)
        self.create_subscription(Float32, "cmd/dc_pwm", lambda m: self.send(protocol.cmd_dc_pwm, m.data), 10)
        self.create_subscription(Float32, "cmd/dc_rel_deg", lambda m: self.send(protocol.cmd_dc_rel_deg, m.data), 10)
        self.create_subscription(Float32, "cmd/dc_rpm", lambda m: self.send(protocol.cmd_dc_rpm, m.data), 10)
        self.create_subscription(Empty, "cmd/stop", lambda _m: self.send(protocol.cmd_stop), 10)
        self.create_subscription(String, "cmd/raw", lambda m: self.send(lambda s: s.strip(), m.data), 10)

        self.create_timer(0.01, self.poll)        # read serial at 100 Hz
        self.create_timer(2.0, self.ensure_open)  # (re)connect
        self.ensure_open()

    # ---------------- serial ----------------
    def ensure_open(self) -> None:
        connected = self.ser is not None and self.ser.is_open
        self.connected_pub.publish(Bool(data=connected))
        if connected:
            return
        try:
            self.ser = serial.serial_for_url(self.port, baudrate=self.baud, timeout=0)
            self.rx = b""
            self.get_logger().info(f"connected to {self.port} @ {self.baud}")
        except (serial.SerialException, ValueError) as e:
            self.ser = None
            self.get_logger().warn(f"cannot open {self.port}: {e} (retrying every 2 s)", throttle_duration_sec=10.0)

    def drop(self, why: Exception) -> None:
        self.get_logger().error(f"serial error on {self.port}: {why}")
        try:
            if self.ser:
                self.ser.close()
        finally:
            self.ser = None

    def send(self, build, value=None) -> None:
        try:
            line = build() if value is None else build(value)
        except ValueError as e:
            self.get_logger().warn(str(e))
            return
        if not line:
            return
        if self.ser is None:
            self.get_logger().warn(f"not connected, dropped command: {line}")
            return
        try:
            self.ser.write((line + "\n").encode())
            self.get_logger().info(f"-> {line}")
        except serial.SerialException as e:
            self.drop(e)

    def poll(self) -> None:
        if self.ser is None:
            return
        try:
            chunk = self.ser.read(max(self.ser.in_waiting, 4096))
            if chunk:
                self.rx += chunk
        except (serial.SerialException, OSError) as e:
            self.drop(e)
            return
        while b"\n" in self.rx:
            raw, self.rx = self.rx.split(b"\n", 1)
            self.handle_line(raw.decode(errors="replace").strip())
        if len(self.rx) > 4096:  # garbage without newlines (e.g. wrong baud rate)
            self.rx = b""

    # ---------------- parsing ----------------
    def handle_line(self, line: str) -> None:
        if not line:
            return
        event = protocol.parse_event(line)
        if event is not None:
            self.log_pub.publish(String(data=event))
            swipe = protocol.parse_swipe(event)
            if swipe:
                self.swipe_pub.publish(String(data=swipe))
            step = protocol.parse_step_result(event)
            if step:
                self.pub_derived("stepper/last_requested_deg", step[0])
                self.pub_derived("stepper/last_actual_deg", step[1])
            return
        t = protocol.parse_telemetry(line)
        if t is None:
            return
        for key, pub in self.float_pubs.items():
            if key in t:
                pub.publish(Float32(data=t[key]))
        for key, pub in self.bool_pubs.items():
            if key in t:
                pub.publish(Bool(data=bool(t[key])))
        self.publish_physical(t)
        if "sensor_mode" in t:
            self.mode_pub.publish(String(data="SENSOR" if t["sensor_mode"] else "GUI"))
        if "tof_follow" in t:
            self.tof_control_pub.publish(String(data="follow" if t["tof_follow"] else "swipe"))
        if "tof_state" in t:
            self.tof_state_pub.publish(String(data=protocol.TOF_STATES.get(int(t["tof_state"]), "UNKNOWN")))

    def pub_derived(self, topic: str, value: float | None) -> None:
        self.derived_pubs[topic].publish(Float32(data=math.nan if value is None else float(value)))

    def publish_physical(self, t: dict[str, float]) -> None:
        if "ir_filt_mv" in t or "ir_mv" in t:
            mv = t.get("ir_filt_mv", t.get("ir_mv"))
            self.pub_derived("ir/distance_cm", units.ir_cm(mv, self.ir_k, self.ir_d0, self.ir_c))
        if "bend_pct" in t:
            self.pub_derived("flex/bend_deg", units.flex_deg(t["bend_pct"], self.flex_full_deg))
        if "hand_x" in t:
            self.pub_derived("tof/bearing_deg", units.tof_bearing_deg(t["hand_x"]))
            if "hand_mm" in t:
                self.pub_derived("tof/lateral_mm", units.tof_lateral_mm(t["hand_x"], t["hand_mm"]))


def main() -> None:
    rclpy.init()
    node = SerialBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
