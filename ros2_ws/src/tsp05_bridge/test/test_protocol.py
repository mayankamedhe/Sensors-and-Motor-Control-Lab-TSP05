import pytest

from tsp05_bridge import protocol


def test_parse_telemetry_reads_firmware_line():
    line = ("sensor_mode:1 tof_follow:0 tof_state:2 ir_mv:1520 dc_pwm:-120 flex_raw_mv:1601 flex_filt_mv:1598 "
            "bend_pct:42.5 servo_deg:104 hand:1 hand_x:1.25 hand_mm:150 step_deg:45.000 step_moving:0")
    t = protocol.parse_telemetry(line)
    assert t["sensor_mode"] == 1
    assert t["dc_pwm"] == -120
    assert t["hand_x"] == pytest.approx(1.25)
    assert t["step_deg"] == pytest.approx(45.0)
    assert len(t) == 14


@pytest.mark.parametrize("line", [
    "",
    "# MODE GUI",
    "ets Jun  8 2016 00:22:57",          # ESP32 ROM boot banner
    "rst:0x1 (POWERON_RESET),boot:0x13",
    "ir_mv:abc",
    "ir_mv:12 garbage",
])
def test_parse_telemetry_rejects_non_telemetry(line):
    assert protocol.parse_telemetry(line) is None


def test_events_and_swipes():
    assert protocol.parse_event("# MODE SENSOR") == "MODE SENSOR"
    assert protocol.parse_event("ir_mv:1") is None
    assert protocol.parse_swipe("SWIPE RIGHT -> stepper +45.000 deg") == "RIGHT"
    assert protocol.parse_swipe("SWIPE LEFT (GUI mode: stepper not moved)") == "LEFT"
    assert protocol.parse_swipe("SWIPE ignored: stepper still moving") is None
    assert protocol.parse_swipe("MODE GUI") is None


def test_commands():
    assert protocol.cmd_mode("SENSOR") == "mode sensor"
    assert protocol.cmd_mode("toggle") == "mode"
    assert protocol.cmd_tof_control("follow") == "tof follow"
    assert protocol.cmd_stepper_rel_deg(-37.5) == "step -37.500"
    assert protocol.cmd_servo_deg(89.6) == "servo 90"
    assert protocol.cmd_dc_pwm(999) == "dc 255"
    assert protocol.cmd_dc_pwm(-999) == "dc -255"
    with pytest.raises(ValueError):
        protocol.cmd_mode("fast")
    with pytest.raises(ValueError):
        protocol.cmd_tof_control("wave")
