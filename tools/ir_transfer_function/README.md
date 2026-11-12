# Sharp IR transfer function (ESP32)

Covers the lab requirement: *"a transfer function plot (physical quantity on the x-axis, the sensor's raw electrical output on the y-axis) based on physical measurements that you have made yourself."*

- **Sensor:** Sharp GP2Y0A21YK (10–80 cm) or GP2Y0A02YK (20–150 cm). The analog output voltage depends on the distance.
- **x-axis:** distance to the target, measured with a ruler, in cm or inches.
- **y-axis:** sensor output voltage in mV, read by the ESP32 ADC.

The y-axis uses `analogReadMilliVolts()` rather than raw counts. The ESP32 ADC is nonlinear and its counts don't map evenly to volts, so the factory-calibrated millivolts are the more accurate raw electrical output. The raw counts are still saved in the CSV.

The script also fits a model and prints the conversion's distance error. The lab asks for that error too ("report the measured error of your conversion").

## Hardware

Same wiring as the team firmware ([`config.h`](../../firmware/tsp05_main/src/config.h)):

| Sharp pin | ESP32 |
|---|---|
| VO (yellow/white) | GPIO14 |
| VCC (red) | 5V / VIN |
| GND (black) | GND |

The Sharp output stays below about 3.1 V even on a 5 V supply, so it is safe for the ESP32 pin. A 10 µF capacitor across VCC/GND near the sensor reduces noise.

## Steps

**1. Flash the logger** (a standalone program, not the team firmware):

```bash
cd tools/ir_transfer_function/esp32_ir_logger
pio run -t upload
```

Optional check: `pio device monitor` should show lines like `IR,1450,1302`, and the second number (mV) should rise as your hand gets closer. Press Ctrl+C to close it before step 3.

**2. Python setup (one time):**

```bash
cd tools/ir_transfer_function
python3 -m venv .venv && source .venv/bin/activate
pip install numpy matplotlib pyserial
ls /dev/cu.usbserial* /dev/cu.SLAB* /dev/cu.wchusb* 2>/dev/null   # find the ESP32 port
```

**3. Record.** Tape down the sensor and a ruler with 0 at the sensor face, and use white card as the target.

```bash
python3 ir_transfer.py capture --port /dev/cu.usbserial-0001 --units in --out data/ir_run1.csv
```

Move the card, type its distance, and press Enter. Repeat for each distance, then press Enter on an empty line to finish.
- Inches: `1 2 3 4 5 6 8 10 12 14 16 20 24 28 31`
- cm (leave out `--units in`): `3 5 7 10 12 15 20 25 30 35 40 50 60 70 80`

Use one unit per CSV file.

**4. Plot:**

```bash
python3 ir_transfer.py plot data/ir_run1.csv
open ir_transfer_function.png
```

**Top panel:** measured mV with ±1 std error bars and the fitted curve `mV = k / (d + d0) + c`. Points closer than the output peak are shaded and not fitted, because the sensor can't tell them apart from far points. **Bottom panel:** the distance error of the inverse conversion, with RMS and max values.

Use `--min-dist` / `--max-dist` to limit the fit to the usable range. The printed inverse formula converts IR millivolts to distance in the firmware.

**5. Reflash the team firmware afterwards:** `cd firmware/tsp05_main && pio run -e esp32dev -t upload`.
