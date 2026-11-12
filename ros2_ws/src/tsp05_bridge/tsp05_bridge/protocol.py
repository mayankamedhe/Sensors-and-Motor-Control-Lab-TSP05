"""Text protocol between the ESP32 firmware (firmware/tsp05_main) and the ROS 2 bridge.

ESP32 -> laptop, one message per line:
  telemetry  space-separated `key:number` pairs, every 100 ms, e.g.
             "sensor_mode:1 ir_mv:1520 servo_deg:110 step_deg:45.000"
  event      a line starting with '#', e.g. "# SWIPE RIGHT -> stepper +45.000 deg"
  other      boot messages from the ESP32 ROM etc. (ignored)

laptop -> ESP32: the firmware's serial commands, one per line ("step 37.5", "mode sensor", ...).
No ROS imports here, so it can be unit-tested without a ROS installation.
"""
from __future__ import annotations

import re

TOF_STATES = {0: "INIT", 1: "CALIBRATING", 2: "READY", 3: "ERROR"}
SWIPE_RE = re.compile(r"^#\s*SWIPE\s+(LEFT|RIGHT)\b")
STEP_RE = re.compile(r"^#\s*STEP requested\s+(-?[\d.]+)\s*deg\s*->\s*actual\s+(-?[\d.]+)\s*deg")


def parse_telemetry(line: str) -> dict[str, float] | None:
    """Return {key: value} for a telemetry line, or None if the line isn't telemetry."""
    line = line.strip()
    if not line or line.startswith("#"):
        return None
    out: dict[str, float] = {}
    for token in line.split():
        key, sep, value = token.partition(":")
        if not sep or not key:
            return None
        try:
            out[key] = float(value)
        except ValueError:
            return None
    return out or None


def parse_event(line: str) -> str | None:
    """Return the text of an event line without the leading '#', or None."""
    line = line.strip()
    if not line.startswith("#"):
        return None
    return line[1:].strip()


def parse_swipe(event: str) -> str | None:
    """Return 'LEFT' or 'RIGHT' if the event line reports a swipe."""
    m = SWIPE_RE.match("# " + event)
    return m.group(1) if m else None


def parse_step_result(event: str) -> tuple[float, float] | None:
    """(requested, actual) degrees from "STEP requested 37.000 deg -> actual 37.125 deg", else None."""
    m = STEP_RE.match("# " + event)
    return (float(m.group(1)), float(m.group(2))) if m else None


# ---------------- Commands ----------------
def cmd_mode(value: str) -> str:
    value = value.strip().lower()
    if value in ("gui", "sensor"):
        return f"mode {value}"
    if value in ("toggle", ""):
        return "mode"
    raise ValueError(f"mode must be gui, sensor or toggle, got {value!r}")


def cmd_tof_control(value: str) -> str:
    value = value.strip().lower()
    if value not in ("swipe", "follow"):
        raise ValueError(f"tof control must be swipe or follow, got {value!r}")
    return f"tof {value}"


def cmd_stepper_rel_deg(deg: float) -> str:
    return f"step {deg:.3f}"


def cmd_servo_deg(deg: float) -> str:
    return f"servo {round(deg)}"


def cmd_dc_pwm(pwm: float) -> str:
    return f"dc {round(max(-255.0, min(255.0, pwm)))}"


def cmd_dc_rel_deg(deg: float) -> str:
    """DC motor: move this many degrees from the current encoder position (closed loop, PID)."""
    return f"dcdeg {deg:.1f}"


def cmd_dc_rpm(rpm: float) -> str:
    """DC motor: hold this output-shaft speed, sign = direction (closed loop, PID). 0 = stop."""
    return f"dcrpm {rpm:.1f}"


def cmd_stop() -> str:
    return "stop"
