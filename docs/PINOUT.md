# ESP32 system pinout

Matches the team's current wiring (2026-11-07). Source of truth in code:
[`firmware/tsp05_main/src/config.h`](../firmware/tsp05_main/src/config.h). Change both together.

| GPIO | Device/function | Notes |
|---:|---|---|
| 0 | DC motor driver IN2 | ⚠️ strapping pin |
| 2 | DC motor driver IN1 | ⚠️ strapping pin |
| 4 | VL53L5CX INT | optional, not used by the firmware yet |
| 13 | Servo signal | servo powered from an external 5–6 V supply, grounds tied together |
| 14 | Sharp IR analog output | ADC2: works only while Wi-Fi is off (we use USB serial, so fine) |
| 15 | DC motor driver ENA | ⚠️ strapping pin |
| 21 | VL53L5CX SDA | |
| 22 | VL53L5CX SCL | |
| 25 | EasyDriver STEP | |
| 26 | EasyDriver DIR | |
| 32 | Flex-sensor analog input | power the divider from **3V3**, not 5 V |
| 33 | Mode pushbutton → GND | internal pull-up, debounced in software |
| 35 | *Simulation only:* slide pot standing in for the ToF hand | unused on real hardware |

| 16 | DC encoder channel A (SPG30E **blue**) | 4.7–10 kΩ pull-up to **3.3 V**, never 5 V |
| 17 | DC encoder channel B (SPG30E **purple**) | 4.7–10 kΩ pull-up to **3.3 V**, never 5 V |

SPG30E motor cable: **red** M+ and **black** M− to the L298 outputs, **brown** Hall Vcc to 5 V, **green** GND (common with the ESP32), **blue** A → GPIO 16, **purple** B → GPIO 17. The encoder outputs are open-drain, so the pull-ups set the logic level: pulling them to 3.3 V keeps the ESP32 pins safe while the sensor runs on 5 V. GPIO 16/17 are free on ESP32-WROOM DevKits; on WROVER modules they are used by PSRAM, so use GPIO 4/5 and change `config.h`.

## ⚠️ DC motor on strapping pins 0, 2, 15

The ESP32 reads GPIO 0, 2 and 15 at reset to decide how to boot. Until `setup()` runs, and for the whole time it is being flashed, these pins are not under our control: ENA (15) and IN2 (0) tend to sit HIGH and IN1 (2) LOW.

- **The DC motor can spin during reset or upload.** Keep the motor supply off while flashing.
- **If uploads fail with "Failed to connect"**, or the board won't boot with the L298 attached, the driver board is pulling GPIO 0 or 2 to the wrong level. Fix: move ENA/IN1/IN2 to GPIO 18/19/23, and change `config.h` to match.
