# How to run the integrated firmware (Ubuntu and macOS)

This guide flashes `firmware/tsp05_main` (all sensors and motors, one program) onto the ESP32 and drives it from the serial monitor.

- **Ubuntu:** verified 2026-11-08 on Aaron's laptop.
- **macOS:** same tool, different install command and port name. Not yet verified; please update this file if something differs.

Total time the first time: about 15 minutes, mostly the toolchain download.

---

## 1. Install the tools (once)

You need **git** and **PlatformIO** (a command-line build tool for ESP32).

**Ubuntu**
```bash
sudo apt install git pipx
pipx install platformio
pipx ensurepath                 # then close and reopen the terminal
sudo usermod -aG dialout $USER  # permission to use the USB serial port; then log out and back in
```

**macOS** (needs [Homebrew](https://brew.sh))
```bash
brew install git pipx
pipx install platformio
pipx ensurepath                 # then close and reopen the terminal
```

Check: `pio --version` prints a version number.

> Prefer a GUI? Install **VS Code** + the **PlatformIO IDE** extension, open the `firmware/tsp05_main` folder, and use the ✓ (build) and → (upload) buttons in the bottom bar. The terminal steps below do the same thing.

---

## 2. Get the code (once)

```bash
git clone -b integration https://github.com/mayankamedhe/Sensors-and-Motor-Control-Lab-TSP05.git
cd Sensors-and-Motor-Control-Lab-TSP05
```

Already cloned? Update instead:
```bash
git switch integration
git pull
```

---

## 3. Find the ESP32's port

Plug the ESP32 in over USB, then:

| | Command | Typical result |
|---|---|---|
| Ubuntu | `ls /dev/ttyUSB* /dev/ttyACM*` | `/dev/ttyUSB0` |
| macOS | `ls /dev/cu.*` | `/dev/cu.usbserial-0001` (use the `cu.` one, not `tty.`) |

Nothing new appears when you plug it in?
- Try another cable. Many USB cables are charge-only.
- **macOS:** boards with a **CH340** USB chip need the [WCH CH34x driver](https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html). CP210x boards work without a driver on recent macOS.
- **Ubuntu:** `sudo apt remove brltty` if the port appears and then vanishes (a braille daemon grabs USB-serial chips).

PlatformIO usually finds the port automatically. If it picks the wrong one, add `--upload-port <port>` to the upload command and `--port <port>` to the monitor command.

---

## 4. Build and upload

> ⚠️ **Turn the motor power supplies OFF before uploading.** The DC motor is on ESP32 boot pins (GPIO 0, 2, 15) and can spin while the board resets.

```bash
cd firmware/tsp05_main          # must be this folder, not the repo root
pio run -e esp32dev -t upload
```

The first build downloads the ESP32 toolchain and takes a few minutes. It ends with `SUCCESS`.

---

## 5. Open the serial monitor

```bash
pio device monitor
```

You should see `# TSP05 integrated firmware` and `# MODE GUI`. Exit with **Ctrl+C** (needed before every upload).

---

## 6. Power on and test

1. Check every supply's **−** is connected to ESP32 **GND**.
2. Supply settings:

   | Supply | Setting |
   |---|---|
   | Stepper (EasyDriver M+) | 12 V, current limit 0.5–0.8 A |
   | Servo | 5–6 V, its own supply |
   | DC motor (L298) | 12 V |

3. Turn the supplies on. Keep your hand away from the ToF sensor until `# TOF READY` appears (about 3 s after reset).
4. Type commands in the monitor and press Enter:

| Goal | Command |
|---|---|
| Stop the scrolling telemetry | `tele off` (back on: `tele on`) |
| **Sensors drive the motors** | `mode sensor` (or press the mode button on GPIO 33) |
| Back to typed commands | `mode gui` |
| Stepper follows your hand instead of swipes | `tof follow` (back: `tof swipe`) |
| Stepper speed in sensor mode | `speed 180` (default 180, max 300) |
| Move the stepper (GUI mode) | `step 90`, `step -45`, `step 90 120` (deg, rpm) |
| Move the servo (GUI mode) | `servo 0` … `servo 180` |
| DC motor (GUI mode) | `dc 150`, `dc -150`, `stop` |
| Show everything | `status`, `help` |

In sensor mode: **IR distance → DC motor speed**, **flex bend → servo angle**, **ToF swipe → stepper ±45°**.

---

## Troubleshooting

| Error | Cause and fix |
|---|---|
| `NotPlatformIOProjectError` | You're in the repo root. `cd firmware/tsp05_main` first. **Don't run `pio project init`**; if you did, run `git clean -f -d include lib src test platformio.ini` in the repo root to undo it |
| `Could not open /dev/ttyUSB0, the port is busy` | Another program has the port open, usually the **Arduino IDE Serial Monitor** or a second `pio device monitor`. Find it with `lsof /dev/ttyUSB0` (Ubuntu, may need `sudo`) or `lsof /dev/cu.usbserial-*` (macOS), then close it |
| `Permission denied: '/dev/ttyUSB0'` (Ubuntu) | You aren't in the `dialout` group yet. Run the `usermod` line from step 1, then log out and back in |
| `Failed to connect to ESP32` | Hold the **BOOT** button on the ESP32 while the upload starts. If it keeps failing with the L298 connected, the motor driver is holding a boot pin; see [PINOUT.md](PINOUT.md) |
| Stepper buzzes or skips instead of turning | Too fast: `speed 120`. Still skipping: raise the EasyDriver current a little (TP1 ≈ 2.5 V) |
| ToF never says `READY`, or swipes aren't detected | Reset with your hand away from the sensor; check `# TOF ERROR` (wiring: SDA 21, SCL 22, 3V3, GND) |
| A sensor doesn't move its motor | `tele on` and watch its value (`ir_mv`, `flex_raw_mv`, `hand`) while you move your hand or bend the sensor |

---

## No hardware? Use the simulator

See [Simulate online (wokwi.com)](../firmware/tsp05_main/README.md#simulate-online-wokwicom). It works the same on Ubuntu and macOS, in the browser.

## ROS 2 GUI

[ros2_ws/src/tsp05_gui](../ros2_ws/src/tsp05_gui/README.md): rqt control panel + serial bridge, started with one command. It needs ROS 2 Humble or Jazzy, which run on **Ubuntu**. On a Mac: flash the ESP32 from the Mac, then plug it into an Ubuntu machine (for example the lab desktop) for the GUI.
