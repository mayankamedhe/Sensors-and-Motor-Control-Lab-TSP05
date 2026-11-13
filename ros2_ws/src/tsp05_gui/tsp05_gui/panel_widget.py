"""TSP05 control panel: shows sensor/motor state and sends motor commands.

Talks only to the ROS 2 topics of tsp05_bridge (/tsp05/...), never to the serial port.
ROS callbacks run on rqt's executor thread, so every update goes through a Qt signal
and is applied on the GUI thread.
"""
from __future__ import annotations

import collections
import math
import time

from python_qt_binding.QtCore import QPointF, Qt, QTimer, Signal
from python_qt_binding.QtGui import QColor, QFont, QPainter, QPen
from python_qt_binding.QtWidgets import (
    QComboBox, QDoubleSpinBox, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QPlainTextEdit,
    QProgressBar, QPushButton, QSizePolicy, QSlider, QSpinBox, QVBoxLayout, QWidget,
)
from std_msgs.msg import Bool, Empty, Float32, String

NS = "/tsp05/"
FLOAT_TOPICS = [
    "ir/voltage_mv", "ir/filtered_mv", "ir/distance_cm",
    "flex/raw_mv", "flex/filtered_mv", "flex/bend_pct", "flex/bend_deg",
    "tof/hand_x", "tof/hand_mm", "tof/bearing_deg", "tof/lateral_mm",
    "servo/angle_deg", "dc/pwm", "dc/angle_deg", "dc/rpm",
    "stepper/angle_deg", "stepper/last_requested_deg", "stepper/last_actual_deg",
]
BOOL_TOPICS = ["tof/hand_present", "stepper/moving", "connected", "ir/detected"]
DC_MAX_RPM = 60  # SPG30E-60K output shaft tops out around 60-70 rpm at full PWM
STRING_TOPICS = ["mode", "tof/state", "tof/control", "tof/swipe", "log"]


class LivePlot(QWidget):
    """Minimal scrolling line plot (no extra dependencies)."""

    COLORS = [QColor("#e4572e"), QColor("#2e86ab"), QColor("#3bb273"), QColor("#7768ae")]

    def __init__(self, title: str, series: list[str], window_s: float = 10.0, unit: str = "") -> None:
        super().__init__()
        self.title, self.series, self.window_s, self.unit = title, series, window_s, unit
        self.data = {s: collections.deque() for s in series}
        self.setMinimumHeight(150)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

    def add(self, name: str, value: float) -> None:
        now = time.monotonic()
        d = self.data[name]
        d.append((now, value))
        while d and now - d[0][0] > self.window_s:
            d.popleft()

    def paintEvent(self, _event) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        left, right, top, bottom = 52, 8, 20, 18
        p.fillRect(0, 0, w, h, QColor("#ffffff"))
        p.setPen(QColor("#333333"))
        p.drawText(left, 14, self.title)

        values = [v for d in self.data.values() for _, v in d]
        if not values:
            p.drawText(left, h // 2, "waiting for data…")
            return
        lo, hi = min(values), max(values)
        pad = max((hi - lo) * 0.1, 1.0)
        lo, hi = lo - pad, hi + pad
        now = time.monotonic()
        pw, ph = w - left - right, h - top - bottom

        p.setPen(QPen(QColor("#dddddd")))
        for i in range(5):
            y = top + ph * i / 4
            p.drawLine(left, int(y), w - right, int(y))
            p.setPen(QColor("#666666"))
            p.drawText(2, int(y) + 4, f"{hi - (hi - lo) * i / 4:.0f}")
            p.setPen(QPen(QColor("#dddddd")))
        p.setPen(QColor("#666666"))
        p.drawText(left, h - 4, f"last {self.window_s:.0f} s  {self.unit}")

        for idx, (name, d) in enumerate(self.data.items()):
            color = self.COLORS[idx % len(self.COLORS)]
            p.setPen(QPen(color, 2))
            pts = [QPointF(left + pw * (1 - (now - t) / self.window_s), top + ph * (hi - v) / (hi - lo)) for t, v in d]
            for a, b in zip(pts, pts[1:]):
                p.drawLine(a, b)
            p.drawText(w - right - 130, top + 14 + 14 * idx, f"— {name}")


class PanelWidget(QWidget):
    msg = Signal(str, object)  # (topic, value), emitted from ROS threads

    def __init__(self, node) -> None:
        super().__init__()
        self.setObjectName("Tsp05PanelWidget")
        self.setWindowTitle("TSP05 Control Panel")
        self.node = node
        self.mode = "?"
        self.last_msg_time = 0.0

        self.pub_mode = node.create_publisher(String, NS + "cmd/mode", 10)
        self.pub_tof = node.create_publisher(String, NS + "cmd/tof_control", 10)
        self.pub_step = node.create_publisher(Float32, NS + "cmd/stepper_rel_deg", 10)
        self.pub_servo = node.create_publisher(Float32, NS + "cmd/servo_deg", 10)
        self.pub_dc = node.create_publisher(Float32, NS + "cmd/dc_pwm", 10)
        self.pub_dc_deg = node.create_publisher(Float32, NS + "cmd/dc_rel_deg", 10)
        self.pub_dc_rpm = node.create_publisher(Float32, NS + "cmd/dc_rpm", 10)
        self.pub_stop = node.create_publisher(Empty, NS + "cmd/stop", 10)
        self.pub_raw = node.create_publisher(String, NS + "cmd/raw", 10)

        self.subs = []
        for t in FLOAT_TOPICS:
            self.subs.append(node.create_subscription(Float32, NS + t, lambda m, t=t: self.msg.emit(t, m.data), 10))
        for t in BOOL_TOPICS:
            self.subs.append(node.create_subscription(Bool, NS + t, lambda m, t=t: self.msg.emit(t, m.data), 10))
        for t in STRING_TOPICS:
            self.subs.append(node.create_subscription(String, NS + t, lambda m, t=t: self.msg.emit(t, m.data), 50))

        self._build_ui()
        self.msg.connect(self._on_msg)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._tick)
        self.timer.start(100)

    # ---------------- UI ----------------
    def _build_ui(self) -> None:
        big = QFont()
        big.setPointSize(14)
        big.setBold(True)

        # Top bar: connection, mode, ToF style, stop
        self.conn = QLabel("● no data")
        self.mode_label = QLabel("MODE: ?")
        self.mode_label.setFont(big)
        b_gui = QPushButton("GUI mode")
        b_sensor = QPushButton("SENSOR mode")
        b_gui.clicked.connect(lambda: self.pub_mode.publish(String(data="gui")))
        b_sensor.clicked.connect(lambda: self.pub_mode.publish(String(data="sensor")))
        self.tof_combo = QComboBox()
        self.tof_combo.addItems(["swipe", "follow"])
        self.tof_combo.activated.connect(lambda i: self.pub_tof.publish(String(data=self.tof_combo.itemText(i))))
        b_stop = QPushButton("STOP motors")
        b_stop.setStyleSheet("background:#d62828;color:white;font-weight:bold;padding:6px 14px;")
        b_stop.clicked.connect(lambda: self.pub_stop.publish(Empty()))
        top = QHBoxLayout()
        for wdg in (self.conn, self.mode_label, b_gui, b_sensor, QLabel("ToF → stepper:"), self.tof_combo):
            top.addWidget(wdg)
        top.addStretch(1)
        top.addWidget(b_stop)

        # Sensors
        self.ir_bar, self.ir_val = self._bar(20, 150), QLabel("– cm")
        self.flex_bar, self.flex_val = self._bar(0, 100), QLabel("– °")
        self.hand_bar, self.hand_val = self._bar(0, 300), QLabel("–")
        self.tof_state = QLabel("ToF: –")
        sg = QGridLayout()
        sg.addWidget(QLabel("<b>IR distance</b> (Sharp GP2Y0A02, 20–150 cm → DC motor)"), 0, 0, 1, 2)
        sg.addWidget(self.ir_bar, 1, 0)
        sg.addWidget(self.ir_val, 1, 1)
        sg.addWidget(QLabel("<b>Flex bend angle</b> (→ servo)"), 2, 0, 1, 2)
        sg.addWidget(self.flex_bar, 3, 0)
        sg.addWidget(self.flex_val, 3, 1)
        sg.addWidget(QLabel("<b>ToF hand position</b> (→ stepper)"), 4, 0, 1, 2)
        sg.addWidget(self.hand_bar, 5, 0)
        sg.addWidget(self.hand_val, 5, 1)
        sg.addWidget(self.tof_state, 6, 0, 1, 2)
        self.flex_plot = LivePlot("Flex: raw vs filtered", ["raw", "filtered"], unit="mV")
        self.ir_plot = LivePlot("IR: raw vs filtered", ["raw", "filtered"], unit="mV")
        sv = QVBoxLayout()
        sv.addLayout(sg)
        sv.addWidget(self.flex_plot)
        sv.addWidget(self.ir_plot)
        sensors = QGroupBox("Sensors")
        sensors.setLayout(sv)

        # Stepper
        self.step_val = QLabel("– °")
        self.step_val.setFont(big)
        self.step_moving = QLabel("")
        self.step_spin = QDoubleSpinBox()
        self.step_spin.setRange(-3600, 3600)
        self.step_spin.setDecimals(1)
        self.step_spin.setValue(90)
        self.step_spin.setSuffix(" °")
        b_move = QPushButton("Move")
        b_move.clicked.connect(lambda: self._step(self.step_spin.value()))
        quick = QHBoxLayout()
        for d in (-90, -45, 45, 90):
            b = QPushButton(f"{d:+d}°")
            b.clicked.connect(lambda _c, d=d: self._step(d))
            quick.addWidget(b)
        self.speed_spin = QSpinBox()
        self.speed_spin.setRange(10, 300)
        self.speed_spin.setValue(180)
        self.speed_spin.setSuffix(" rpm")
        b_speed = QPushButton("Set sensor-mode speed")
        b_speed.clicked.connect(lambda: self.pub_raw.publish(String(data=f"speed {self.speed_spin.value()}")))
        stg = QGridLayout()
        stg.addWidget(QLabel("Actual angle:"), 0, 0)
        stg.addWidget(self.step_val, 0, 1)
        stg.addWidget(self.step_moving, 0, 2)
        self.step_last = QLabel("Last move: –")
        stg.addWidget(self.step_last, 4, 0, 1, 3)
        stg.addWidget(QLabel("Relative move:"), 1, 0)
        stg.addWidget(self.step_spin, 1, 1)
        stg.addWidget(b_move, 1, 2)
        stg.addLayout(quick, 2, 0, 1, 3)
        stg.addWidget(self.speed_spin, 3, 1)
        stg.addWidget(b_speed, 3, 2)
        self.stepper_box = QGroupBox("Stepper (rounded to the nearest step; shows the actual angle)")
        self.stepper_box.setLayout(stg)

        # Servo
        self.servo_val = QLabel("– °")
        self.servo_val.setFont(big)
        self.servo_slider = QSlider(Qt.Horizontal)
        self.servo_slider.setRange(0, 180)
        self.servo_target = QLabel("90 °")
        self.servo_slider.setValue(90)
        self.servo_slider.valueChanged.connect(lambda v: self.servo_target.setText(f"{v} °"))
        self.servo_slider.sliderReleased.connect(lambda: self._servo(self.servo_slider.value()))
        sq = QHBoxLayout()
        for d in (0, 90, 180):
            b = QPushButton(f"{d}°")
            b.clicked.connect(lambda _c, d=d: (self.servo_slider.setValue(d), self._servo(d)))
            sq.addWidget(b)
        svg = QGridLayout()
        svg.addWidget(QLabel("Commanded angle:"), 0, 0)
        svg.addWidget(self.servo_val, 0, 1)
        svg.addWidget(self.servo_slider, 1, 0)
        svg.addWidget(self.servo_target, 1, 1)
        svg.addLayout(sq, 2, 0, 1, 2)
        self.servo_box = QGroupBox("Servo (drag and release, or click a limit)")
        self.servo_box.setLayout(svg)

        # DC motor: closed loop (encoder + PID) position and velocity, plus raw PWM for testing
        self.dc_pos = QLabel("– °")
        self.dc_pos.setFont(big)
        self.dc_speed = QLabel("– rpm")
        self.dc_speed.setFont(big)
        self.dc_deg_spin = QDoubleSpinBox()
        self.dc_deg_spin.setRange(-3600, 3600)
        self.dc_deg_spin.setDecimals(1)
        self.dc_deg_spin.setValue(90)
        self.dc_deg_spin.setSuffix(" °")
        b_dc_move = QPushButton("Move")
        b_dc_move.clicked.connect(lambda: self.pub_dc_deg.publish(Float32(data=float(self.dc_deg_spin.value()))))
        self.dc_rpm_spin = QDoubleSpinBox()
        self.dc_rpm_spin.setRange(-DC_MAX_RPM, DC_MAX_RPM)
        self.dc_rpm_spin.setDecimals(0)
        self.dc_rpm_spin.setValue(30)
        self.dc_rpm_spin.setSuffix(" rpm")
        b_dc_run = QPushButton("Run")
        b_dc_run.clicked.connect(lambda: self.pub_dc_rpm.publish(Float32(data=float(self.dc_rpm_spin.value()))))
        self.dc_val = QLabel("–")
        self.dc_slider = QSlider(Qt.Horizontal)
        self.dc_slider.setRange(-255, 255)
        self.dc_target = QLabel("0")
        self.dc_slider.valueChanged.connect(lambda v: self.dc_target.setText(f"{v:+d}"))
        self.dc_slider.sliderReleased.connect(lambda: self.pub_dc.publish(Float32(data=float(self.dc_slider.value()))))
        b_dc0 = QPushButton("Stop DC")
        b_dc0.clicked.connect(lambda: (self.dc_slider.setValue(0), self.pub_dc.publish(Float32(data=0.0))))
        dg = QGridLayout()
        dg.addWidget(QLabel("Encoder position:"), 0, 0)
        dg.addWidget(self.dc_pos, 0, 1)
        dg.addWidget(QLabel("Speed:"), 0, 2)
        dg.addWidget(self.dc_speed, 0, 3)
        dg.addWidget(QLabel("Relative move (− = reverse):"), 1, 0)
        dg.addWidget(self.dc_deg_spin, 1, 1, 1, 2)
        dg.addWidget(b_dc_move, 1, 3)
        dg.addWidget(QLabel("Velocity (− = reverse):"), 2, 0)
        dg.addWidget(self.dc_rpm_spin, 2, 1, 1, 2)
        dg.addWidget(b_dc_run, 2, 3)
        dg.addWidget(QLabel("<i>Open-loop PWM test:</i>"), 3, 0)
        dg.addWidget(self.dc_slider, 3, 1, 1, 2)
        dg.addWidget(self.dc_target, 3, 3)
        dg.addWidget(QLabel("Applied PWM:"), 4, 0)
        dg.addWidget(self.dc_val, 4, 1)
        dg.addWidget(b_dc0, 4, 3)
        self.dc_box = QGroupBox("DC motor (encoder + PID: relative degrees or velocity)")
        self.dc_box.setLayout(dg)

        self.gui_hint = QLabel("")
        self.gui_hint.setStyleSheet("color:#b5651d;font-weight:bold;")
        mv = QVBoxLayout()
        mv.addWidget(self.gui_hint)
        mv.addWidget(self.stepper_box)
        mv.addWidget(self.servo_box)
        mv.addWidget(self.dc_box)
        mv.addStretch(1)
        motors = QGroupBox("Motors")
        motors.setLayout(mv)

        # Log
        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(200)
        self.log.setMaximumHeight(110)

        cols = QHBoxLayout()
        cols.addWidget(sensors, 1)
        cols.addWidget(motors, 1)
        root = QVBoxLayout(self)
        root.addLayout(top)
        root.addLayout(cols)
        root.addWidget(QLabel("Firmware events"))
        root.addWidget(self.log)
        self._set_mode("?")

    @staticmethod
    def _bar(lo: int, hi: int) -> QProgressBar:
        b = QProgressBar()
        b.setRange(lo, hi)
        b.setTextVisible(False)
        return b

    # ---------------- commands ----------------
    def _step(self, deg: float) -> None:
        self.pub_step.publish(Float32(data=float(deg)))

    def _servo(self, deg: int) -> None:
        self.pub_servo.publish(Float32(data=float(deg)))

    # ---------------- state ----------------
    def _set_mode(self, mode: str) -> None:
        self.mode = mode
        gui = mode == "GUI"
        self.mode_label.setText(f"MODE: {mode}")
        self.mode_label.setStyleSheet("color:#1b7f3b;" if gui else "color:#b5651d;" if mode == "SENSOR" else "")
        for box in (self.stepper_box, self.servo_box, self.dc_box):
            box.setEnabled(gui)
        self.gui_hint.setText("" if gui else "Sensors are driving the motors. Click “GUI mode” to command them here.")

    def _on_msg(self, topic: str, value) -> None:
        self.last_msg_time = time.monotonic()
        if topic == "ir/voltage_mv":
            self._ir_mv = value
            self.ir_plot.add("raw", value)
        elif topic == "ir/filtered_mv":
            self.ir_plot.add("filtered", value)
        elif topic == "ir/detected":
            self._ir_detected = bool(value)
        elif topic == "ir/distance_cm":
            mv = getattr(self, "_ir_mv", math.nan)
            det = " · DETECTED" if getattr(self, "_ir_detected", False) else ""
            if math.isnan(value):
                self.ir_bar.setValue(150)
                self.ir_val.setText(f"out of range ({mv:.0f} mV){det}")
            else:
                self.ir_bar.setValue(int(value))
                self.ir_val.setText(f"{value:.1f} cm ({mv:.0f} mV){det}")
        elif topic == "flex/raw_mv":
            self.flex_plot.add("raw", value)
        elif topic == "flex/filtered_mv":
            self.flex_plot.add("filtered", value)
        elif topic == "flex/bend_pct":
            self.flex_bar.setValue(int(value))
        elif topic == "flex/bend_deg":
            self.flex_val.setText(f"{value:.1f} °")
        elif topic == "tof/hand_x":
            self.hand_bar.setValue(int(value * 100) if value >= 0 else 0)
            self._hand_x = value
        elif topic == "tof/hand_mm":
            self._hand_mm = value
        elif topic == "tof/bearing_deg":
            self._bearing = value
        elif topic == "tof/lateral_mm":
            if math.isnan(value) or getattr(self, "_hand_x", -1) < 0:
                self.hand_val.setText("no hand")
            else:
                self.hand_val.setText(f"{getattr(self, '_bearing', math.nan):+.1f}°, "
                                      f"{getattr(self, '_hand_mm', math.nan):.0f} mm away, {value:+.0f} mm sideways")
        elif topic == "tof/state":
            self.tof_state.setText(f"ToF: {value}")
        elif topic == "tof/control":
            if self.tof_combo.currentText() != value:
                self.tof_combo.setCurrentText(value)
        elif topic == "tof/swipe":
            self.log.appendPlainText(f"[swipe] {value}")
        elif topic == "stepper/angle_deg":
            self.step_val.setText(f"{value:.3f} °")
        elif topic == "stepper/last_requested_deg":
            self._step_req = value
        elif topic == "stepper/last_actual_deg":
            self.step_last.setText(f"Last move: requested {getattr(self, '_step_req', math.nan):+.3f}° "
                                   f"→ actual {value:+.3f}°")
        elif topic == "stepper/moving":
            self.step_moving.setText("moving…" if value else "")
        elif topic == "servo/angle_deg":
            self.servo_val.setText(f"{value:.0f} °")
        elif topic == "dc/pwm":
            self.dc_val.setText(f"{value:+.0f}")
        elif topic == "dc/angle_deg":
            self.dc_pos.setText(f"{value:.1f} °")
        elif topic == "dc/rpm":
            self.dc_speed.setText(f"{value:+.1f} rpm")
        elif topic == "mode":
            if value != self.mode:
                self._set_mode(value)
        elif topic == "log":
            self.log.appendPlainText(value)

    def _tick(self) -> None:
        fresh = time.monotonic() - self.last_msg_time < 1.0
        self.conn.setText("● ESP32 data" if fresh else "● no data (is serial_bridge running?)")
        self.conn.setStyleSheet("color:#1b7f3b;" if fresh else "color:#d62828;")
        self.flex_plot.update()
        self.ir_plot.update()

    def shutdown(self) -> None:
        self.timer.stop()
        for s in self.subs:
            self.node.destroy_subscription(s)
        for p in (self.pub_mode, self.pub_tof, self.pub_step, self.pub_servo, self.pub_dc, self.pub_dc_deg,
                  self.pub_dc_rpm, self.pub_stop, self.pub_raw):
            self.node.destroy_publisher(p)
