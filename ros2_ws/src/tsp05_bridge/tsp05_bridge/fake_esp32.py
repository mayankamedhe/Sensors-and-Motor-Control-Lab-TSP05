"""Fake ESP32 for testing the bridge and GUI without hardware.

Opens a pseudo-terminal, prints its path, and speaks the same serial protocol as
firmware/tsp05_main: telemetry every 100 ms, '#' event lines, and the same commands.
Sensors are fake waveforms; a swipe happens every 5 s in SENSOR mode.

  ros2 run tsp05_bridge fake_esp32            # prints e.g. "fake ESP32 on /dev/pts/7"
  ros2 run tsp05_bridge serial_bridge --ros-args -p port:=/dev/pts/7
"""
from __future__ import annotations

import math
import os
import select
import time
import tty

STEP_DEG = 0.225          # 1/8 microstepping, like the real EasyDriver setup
STEP_SPEED_DPS = 180.0    # 30 RPM


class FakeEsp32:
    def __init__(self) -> None:
        self.sensor_mode = False
        self.tof_follow = False
        self.step_pos = 0.0
        self.step_target = 0.0
        self.servo = 70
        self.dc = 0
        self.dc_deg = 0.0      # encoder position
        self.dc_rpm = 0.0      # measured speed
        self.dc_target = None  # closed-loop position target (deg) or None
        self.dc_vel = None     # closed-loop velocity target (rpm) or None
        self.ir_filt = 1700.0
        self.ir_on = False
        self.t0 = time.monotonic()
        self.last_swipe = self.t0

    def say(self, fd: int, text: str) -> None:
        try:
            os.write(fd, (text + "\n").encode())
        except BlockingIOError:
            pass  # nobody is reading the port yet: drop the line, like a real UART would

    def command(self, fd: int, cmd: str) -> None:
        cmd = cmd.strip().lower()
        parts = cmd.split()
        if not parts:
            return
        gui_only = parts[0] in ("step", "servo", "dc", "dcdeg", "dcrpm", "move", "forward", "reverse")
        if gui_only and self.sensor_mode:
            self.say(fd, "# ignored: switch to GUI mode first (`mode gui` or press the button)")
            return
        try:
            if cmd in ("mode", "mode gui", "mode sensor"):
                self.sensor_mode = (not self.sensor_mode) if cmd == "mode" else cmd.endswith("sensor")
                self.step_target, self.dc = self.step_pos, 0
                self.say(fd, f"# MODE {'SENSOR' if self.sensor_mode else 'GUI'}")
            elif cmd in ("tof swipe", "tof follow"):
                self.tof_follow = cmd.endswith("follow")
                self.say(fd, f"# TOF CONTROL {parts[1]}")
            elif cmd == "stop":
                self.step_target, self.dc = self.step_pos, 0
                self.dc_target = self.dc_vel = None
                self.say(fd, "# stopped")
            elif parts[0] == "step":
                req = float(parts[1])
                actual = round(req / STEP_DEG) * STEP_DEG
                self.step_target = self.step_pos + actual
                self.say(fd, f"# STEP requested {req:.3f} deg -> actual {actual:.3f} deg")
            elif parts[0] == "servo":
                self.servo = max(0, min(180, round(float(parts[1]))))
                self.say(fd, f"# SERVO {self.servo} deg")
            elif parts[0] == "dc":
                self.dc = max(-255, min(255, round(float(parts[1]))))
                self.dc_target = self.dc_vel = None
                self.say(fd, f"# DC pwm {self.dc}")
            elif parts[0] == "dcdeg":
                self.dc_target, self.dc_vel = self.dc_deg + float(parts[1]), None
                self.say(fd, f"# DCDEG {float(parts[1]):+.1f} deg -> target {self.dc_target:.1f} deg")
            elif parts[0] == "dcrpm":
                self.dc_vel, self.dc_target = float(parts[1]), None
                self.say(fd, f"# DCRPM {self.dc_vel:+.1f} rpm")
            elif cmd in ("status", "help"):
                self.say(fd, f"# fake ESP32: mode={'SENSOR' if self.sensor_mode else 'GUI'}")
            else:
                self.say(fd, f"# unknown command: {cmd} (type help)")
        except (IndexError, ValueError):
            self.say(fd, f"# unknown command: {cmd} (type help)")

    def tick(self, fd: int, dt: float) -> None:
        t = time.monotonic() - self.t0
        ir_mv = 1700 + 1200 * math.sin(t * 0.7)
        flex_raw = 1600 + 60 * math.sin(t * 0.5) + 25 * math.sin(t * 13.0)  # slow bend + noise
        flex_filt = 1600 + 60 * math.sin(t * 0.5)
        bend = max(0.0, min(100.0, (flex_filt - 1540) / 1.4))
        hand = math.sin(t * 0.9) > 0
        hand_x = 1.5 + 1.5 * math.sin(t * 2.0) if hand else -1.0

        if self.sensor_mode:
            # Same as firmware ir_dc.cpp (no hysteresis): off until detected, then 90..255 as it nears.
            self.dc_target = self.dc_vel = None
            self.dc = 0 if ir_mv < 900 else round(90 + max(0.0, min(1.0, (ir_mv - 750) / (2500 - 750))) * 165)
            self.servo = round(70 + bend / 100 * 80)
            if self.tof_follow and hand:
                self.step_target = round(((hand_x / 3) - 0.5) * 180 / STEP_DEG) * STEP_DEG
            elif not self.tof_follow and time.monotonic() - self.last_swipe > 5:
                self.last_swipe = time.monotonic()
                d = 1 if int(t / 5) % 2 else -1
                self.step_target = self.step_pos + 45 * d
                self.say(fd, f"# SWIPE {'RIGHT' if d > 0 else 'LEFT'} -> stepper {45 * d:+.3f} deg")

        # IR filter + detection with hysteresis, like ir_dc.cpp
        self.ir_filt += 0.4 * (ir_mv - self.ir_filt)
        self.ir_on = self.ir_filt >= 900 or (self.ir_on and self.ir_filt >= 750)

        # DC motor: fake closed loop on a simulated encoder
        if self.dc_target is not None:
            self.dc_rpm = max(-60.0, min(60.0, 1.0 * (self.dc_target - self.dc_deg)))
            if abs(self.dc_target - self.dc_deg) < 0.5:
                self.dc_rpm, self.dc_target, self.dc = 0.0, None, 0
        elif self.dc_vel is not None:
            self.dc_rpm += 0.3 * (self.dc_vel - self.dc_rpm)
        else:
            self.dc_rpm = self.dc / 255 * 120
        self.dc_deg += self.dc_rpm * 6.0 * dt  # rpm -> deg/s
        self.dc = round(self.dc_rpm / 120 * 255) if (self.dc_target is not None or self.dc_vel is not None) else self.dc

        err = self.step_target - self.step_pos
        move = max(-STEP_SPEED_DPS * dt, min(STEP_SPEED_DPS * dt, err))
        self.step_pos += move
        moving = abs(self.step_target - self.step_pos) > 1e-6

        self.say(fd, f"sensor_mode:{int(self.sensor_mode)} tof_follow:{int(self.tof_follow)} tof_state:2 "
                     f"ir_mv:{ir_mv:.0f} ir_filt_mv:{self.ir_filt:.0f} ir_detect:{int(self.ir_on)} dc_pwm:{self.dc} "
                     f"dc_deg:{self.dc_deg:.1f} dc_rpm:{self.dc_rpm:.1f} flex_raw_mv:{flex_raw:.0f} flex_filt_mv:{flex_filt:.0f} "
                     f"bend_pct:{bend:.1f} servo_deg:{self.servo} hand:{int(hand)} hand_x:{hand_x:.2f} "
                     f"hand_mm:{150 if hand else 0} step_deg:{self.step_pos:.3f} step_moving:{int(moving)}")


def main() -> None:
    master, slave = os.openpty()
    tty.setraw(slave)
    os.set_blocking(master, False)
    print(f"fake ESP32 on {os.ttyname(slave)}  (Ctrl+C to quit)", flush=True)
    dev = FakeEsp32()
    dev.say(master, "# TSP05 integrated firmware (FAKE)")
    dev.say(master, "# MODE GUI")
    buf = b""
    last = time.monotonic()
    try:
        while True:
            ready, _, _ = select.select([master], [], [], 0.1)
            if ready:
                try:
                    buf += os.read(master, 1024)
                except OSError:
                    buf = b""
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    dev.command(master, line.decode(errors="replace"))
            now = time.monotonic()
            if now - last >= 0.1:
                dev.tick(master, now - last)
                last = now
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
