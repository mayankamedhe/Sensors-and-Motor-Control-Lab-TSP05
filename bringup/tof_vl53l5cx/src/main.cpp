// VL53L5CX bring-up test for ESP32.
// Prints the 8x8 distance grid (mm) in raw array order so we can figure out
// how array indices map to the physical field of view (hand test).
// Invalid zones (target_status not 5 or 9) are printed as "----".
//
// Wiring: 3V3->3V3, GND->GND, SDA->GPIO21, SCL->GPIO22 (wire only when unpowered).

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>

static constexpr int PIN_SDA = 21;
static constexpr int PIN_SCL = 22;
static constexpr uint32_t I2C_HZ = 400000;   // 1 MHz also supported; 400 kHz is safer on breadboards
static constexpr int RESOLUTION = 8 * 8;     // 4*4 (up to 60 Hz) or 8*8 (up to 15 Hz)
static constexpr int RANGING_HZ = 15;

SparkFun_VL53L5CX imager;
VL53L5CX_ResultsData data;
int width = 0;

static bool zoneValid(uint8_t status) { return status == 5 || status == 9; }

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nVL53L5CX bring-up");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_HZ);

  Serial.println("Uploading sensor firmware (takes a few seconds)...");
  uint32_t t0 = millis();
  if (!imager.begin()) {
    Serial.println("ERROR: sensor not found at 0x29. Check wiring and 3V3 power. Halting.");
    while (true) delay(1000);
  }
  Serial.printf("Sensor ready in %lu ms\n", (unsigned long)(millis() - t0));

  imager.setResolution(RESOLUTION);
  width = (int)sqrt((double)imager.getResolution());
  imager.setRangingFrequency(RANGING_HZ);
  imager.startRanging();
  Serial.printf("Ranging: %dx%d @ %d Hz\n", width, width, RANGING_HZ);
}

void loop() {
  static uint32_t lastFrame = 0;
  if (!imager.isDataReady()) { delay(1); return; }
  if (!imager.getRangingData(&data)) return;

  uint32_t now = millis();
  int nearestIdx = -1;
  int16_t nearestMm = INT16_MAX;

  Serial.println();
  for (int row = 0; row < width; row++) {
    for (int col = 0; col < width; col++) {
      int i = row * width + col;
      if (zoneValid(data.target_status[i])) {
        Serial.printf("%5d", data.distance_mm[i]);
        if (data.distance_mm[i] < nearestMm) { nearestMm = data.distance_mm[i]; nearestIdx = i; }
      } else {
        Serial.print(" ----");
      }
    }
    Serial.println();
  }
  if (nearestIdx >= 0) {
    Serial.printf("nearest: %d mm at index %d (row %d, col %d)  dt=%lu ms\n",
                  nearestMm, nearestIdx, nearestIdx / width, nearestIdx % width,
                  (unsigned long)(now - lastFrame));
  } else {
    Serial.println("nearest: no valid zones");
  }
  lastFrame = now;
}
