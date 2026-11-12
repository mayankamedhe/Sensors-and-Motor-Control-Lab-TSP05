import math

import pytest

from tsp05_bridge import units


def test_ir_cm_matches_datasheet_curve():
    for cm, mv in [(20, 2500), (50, 1350), (80, 900), (120, 600)]:
        assert units.ir_cm(mv) == pytest.approx(cm, abs=4.0)


def test_ir_cm_out_of_range():
    assert units.ir_cm(3500) is None   # closer than 20 cm (or fold-back)
    assert units.ir_cm(100) is None    # beyond 150 cm / nothing in view
    assert units.ir_cm(-500) is None


def test_flex_and_tof():
    assert units.flex_deg(50) == pytest.approx(45.0)
    assert units.tof_bearing_deg(1.5) == pytest.approx(0.0)
    assert units.tof_bearing_deg(3.0) == pytest.approx(16.875)
    assert units.tof_bearing_deg(-1) is None
    assert units.tof_lateral_mm(3.0, 200) == pytest.approx(200 * math.tan(math.radians(16.875)))
    assert units.tof_lateral_mm(-1, 200) is None
