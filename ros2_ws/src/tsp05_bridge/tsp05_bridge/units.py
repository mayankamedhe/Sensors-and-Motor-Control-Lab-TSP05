"""Raw firmware values -> physical units, for the GUI (lab requirement: show physical units, not volts).

No ROS imports, so it can be unit-tested without a ROS installation. After calibrating, either edit
the defaults below (then colcon build) or override them as serial_bridge parameters, e.g.

  ros2 run tsp05_bridge serial_bridge --ros-args -p port:=/dev/ttyUSB0 -p ir_k:=105486.0 -p ir_c:=-159.3
"""
from __future__ import annotations

import math

# ---------------- Sharp GP2Y0A02YK: mV -> cm ----------------
# Model (same as tools/ir_transfer_function):  mV = k / (cm + d0) + c   ->   cm = k / (mV - c) - d0
# Defaults: fit to the datasheet curve (20-150 cm, error within ~4 cm). REPLACE with the team's own
# measurements: ir_transfer.py prints k, d0, c in Uno ADC counts; multiply k and c by 5000/1023 to get mV.
IR_K = 105486.0   # mV*cm
IR_D0 = 19.53     # cm
IR_C = -159.3     # mV
IR_MIN_CM = 20.0  # usable range; closer than ~20 cm the output folds back
IR_MAX_CM = 150.0


def ir_cm(mv: float, k: float = IR_K, d0: float = IR_D0, c: float = IR_C,
          lo: float = IR_MIN_CM, hi: float = IR_MAX_CM) -> float | None:
    """Distance in cm, or None when the reading is outside the usable range."""
    if mv <= c:
        return None
    cm = k / (mv - c) - d0
    return cm if lo <= cm <= hi else None


# ---------------- Flex sensor: % of calibrated bend -> degrees ----------------
# The firmware reports 0 % at FLEX_STRAIGHT_MV and 100 % at FLEX_BENT_MV. Calibrate FLEX_BENT_MV
# with the sensor bent to FLEX_FULL_DEG (protractor), then this is a bend angle in degrees.
FLEX_FULL_DEG = 90.0


def flex_deg(bend_pct: float, full_deg: float = FLEX_FULL_DEG) -> float:
    return bend_pct / 100.0 * full_deg


# ---------------- VL53L5CX: zone column + distance -> bearing and lateral offset ----------------
# 4x4 zones across a ~45 deg horizontal field of view -> 11.25 deg per zone column.
TOF_COLUMNS = 4
TOF_FOV_DEG = 45.0


def tof_bearing_deg(hand_x: float, columns: int = TOF_COLUMNS, fov_deg: float = TOF_FOV_DEG) -> float | None:
    """Hand direction in degrees from the sensor axis (negative = left). None when there's no hand."""
    if hand_x < 0:
        return None
    return (hand_x - (columns - 1) / 2.0) * fov_deg / columns


def tof_lateral_mm(hand_x: float, hand_mm: float, columns: int = TOF_COLUMNS,
                   fov_deg: float = TOF_FOV_DEG) -> float | None:
    """Sideways hand offset from the sensor axis in mm (negative = left). None when there's no hand."""
    b = tof_bearing_deg(hand_x, columns, fov_deg)
    if b is None or hand_mm <= 0:
        return None
    return hand_mm * math.tan(math.radians(b))
