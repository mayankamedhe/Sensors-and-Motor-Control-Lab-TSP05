# Sharp IR transfer function

Covers the lab requirement: *"a transfer function plot (physical quantity on the x-axis, the sensor's raw electrical output on the y-axis) based on physical measurements that you have made yourself."*

- **Sensor:** Sharp GP2Y0A21YK (10–80 cm) or GP2Y0A02YK (20–150 cm). The analog output voltage depends on the distance.
- **x-axis:** distance to target in cm, measured with a ruler or tape.
- **y-axis:** raw Arduino Uno ADC counts (0–1023 = 0–5 V).

The script also fits a model and prints the distance error of the conversion. The lab also asks for that error ("report the measured error of your conversion"), so one measurement session covers both requirements.

## Hardware

| Sharp pin | Uno |
|---|---|
| VO (yellow/white) | A0 |
| VCC (red) | 5V |
| GND (black) | GND |

Put a 10 µF capacitor across VCC/GND near the sensor. The sensor's LED pulses cause supply ripple, which shows up as ADC noise if you leave the capacitor out.

## Setup

```bash
arduino-cli compile --fqbn arduino:avr:uno ir_calibrate
arduino-cli upload  --fqbn arduino:avr:uno -p /dev/cu.usbmodem1101 ir_calibrate   # Linux: /dev/ttyACM0
pip install numpy matplotlib pyserial
```

(The Arduino IDE works too: open `ir_calibrate/ir_calibrate.ino` and upload it.)

## Measuring

1. Tape a ruler or tape measure to the bench with **0 at the sensor's front face**. Fix the sensor so it can't move.
2. Use a flat, matte, light-coloured target that is wider than the beam. A sheet of white card on a book works. Keep it perpendicular to the beam.
3. Run the capture and enter each distance when the target is in place:

   ```bash
   python3 ir_transfer.py capture --port /dev/cu.usbmodem1101 --out data/ir_run1.csv
   ```

   **Inches:** add `--units in` and type distances in inches. The plot and error table then use inches too.
   Inch distances for the GP2Y0A21YK: **1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 20, 24, 28, 31**.

   Suggested distances (cm) for the GP2Y0A21YK: **3, 5, 7** (below minimum range, shows the fold-back), then **10, 12, 15, 20, 25, 30, 35, 40, 50, 60, 70, 80** cm. Space them closely at short range, where the curve is steep. For the GP2Y0A02YK, go out to 150 cm.
4. Keep your hand and body out of the beam while it samples (about 1 s per point with 200 samples).

## Plotting

```bash
python3 ir_transfer.py plot data/ir_run1.csv --out ir_transfer_function.png
```

This prints the fit, the inverse conversion and an error table, then saves a two-panel PNG:
- **Top:** measured mean counts with ±1 std error bars, and the fitted curve `counts = k / (d + d0) + c`. Points closer than the output peak are shaded and not fitted, because the sensor can't tell them apart from far points.
- **Bottom:** distance error of the inverse conversion `d = k / (counts - c) - d0` at each measured point, with RMS and max values.

Use `--min-cm` / `--max-cm` to limit the fit to the usable range. Copy the printed `k`, `d0` and `c` into the firmware to convert IR readings into cm.

For a second run on a different target or day, capture to a new CSV. That gives you repeatability data to discuss.
