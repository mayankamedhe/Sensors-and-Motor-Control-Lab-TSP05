# VL53L5CX bring-up (ESP32)

Standalone test that checks the ToF imager works and lets us work out how the zone order maps to the physical view. This is not the team's main firmware.

**Wiring (power off while wiring):** 3V3→3V3, GND→GND, SDA→GPIO 21, SCL→GPIO 22.

```bash
pio run                      # build
pio run -t upload            # flash (ESP32 on USB)
pio device monitor           # view output (Ctrl+C to exit)
```

**Hand test:** put a hand in the top-left of the sensor's view, about 15 cm away, and note which `row`/`col` the `nearest:` line reports. Repeat for the top-right and bottom-left corners. Record the mapping in CONTEXT.md.
