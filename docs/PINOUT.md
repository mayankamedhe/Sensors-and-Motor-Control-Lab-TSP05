# ESP32 system pinout

Source of truth in code: [`firmware/tsp05_main/src/config.h`](../firmware/tsp05_main/src/config.h). Change both together.

| GPIO | Device/function | Notes |
|---:|---|---|
| 13 | Servo signal | Servo powered from an external 5–6 V supply, grounds tied together |
| 18 | DC motor driver ENA (PWM) | was GPIO 15 (strapping pin) |
| 19 | DC motor driver IN1 | was GPIO 2 (strapping pin) |
| 21 | VL53L5CX SDA | |
| 22 | VL53L5CX SCL | |
| 23 | DC motor driver IN2 | was GPIO 0 (boot button / strapping pin) |
| 25 | EasyDriver STEP | |
| 26 | EasyDriver DIR | |
| 32 | Flex-sensor analog input (ADC1) | power the divider from **3V3**, not 5 V |
| 33 | Mode pushbutton → GND | internal pull-up, debounced in software |
| 34 | Sharp IR analog output (ADC1) | was GPIO 14 (ADC2 stops working when Wi-Fi is on) |
| 35 | *Simulation only:* slide pot standing in for the ToF hand | unused on real hardware |
| 4 | VL53L5CX INT (optional) | not used by the firmware yet |

Free for the DC motor encoder (A/B): GPIO 16 and 17 are suggested. Add pull-ups to **3.3 V**, never 5 V.

**Why the DC motor pins moved:** GPIO 0, 2 and 15 are strapping pins. The level on them at reset decides whether the ESP32 boots normally or enters download mode, so an L298 input pulling on them can make the board fail to boot or fail to flash.
