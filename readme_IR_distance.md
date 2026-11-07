Pin mapping between Arduino setup and the ESP32:

# ESP32 Pin Mapping Quick Reference

## Pin Comparison Table

| Component | Arduino Pin | ESP32 GPIO Pin | Function |
| --- | --- | --- | --- |
| **Sharp IR Sensor Signal (VO)** | `A0` | **GPIO 34** | Analog distance input (ADC1) |
| **Sharp IR VCC / GND** | `5V` / `GND` | **5V (VIN)** / **GND** | Sensor power & ground |
| **L298N ENA** | `Pin 9` (PWM) | **GPIO 25** | Motor PWM speed control |
| **L298N IN1** | `Pin 7` | **GPIO 26** | Motor direction pin 1 |
| **L298N IN2** | `Pin 8` | **GPIO 27** | Motor direction pin 2 |
| **SPG30 Motor Power** | `OUT1` / `OUT2` | **OUT1** / **OUT2** | L298N motor output terminals |
| **Encoder Signals** | `Pin 2` / `Pin 3` | *Unused* | Disconnected for basic speed control |