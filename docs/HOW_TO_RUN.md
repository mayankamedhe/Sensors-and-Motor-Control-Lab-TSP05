# How to run the TSP05 system (Ubuntu and macOS)

- **Part A (Ubuntu or macOS):** flash `firmware/tsp05_main` (all sensors and motors, one program) onto the ESP32 and drive it from the serial monitor.
- **[Part B (Ubuntu only)](#part-b-ros-2-and-the-gui-ubuntu):** install ROS 2, then run the serial bridge and the rqt control panel.

# Part A: Firmware

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
git clone https://github.com/mayankamedhe/Sensors-and-Motor-Control-Lab-TSP05.git
cd Sensors-and-Motor-Control-Lab-TSP05
git switch main          # the repo's default branch is an old one, so switch explicitly
```

Already cloned? Update instead:
```bash
git switch main
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

---

# Part B: ROS 2 and the GUI (Ubuntu)

The laptop side: [`tsp05_bridge`](../ros2_ws/src/tsp05_bridge/README.md) talks to the ESP32 over USB (or network socket), and [`tsp05_gui`](../ros2_ws/src/tsp05_gui/README.md) is the control panel.
ROS 2 needs **Ubuntu**. On a Mac:
- **Option 1 (Physical Ubuntu):** flash the ESP32 from the Mac (Part A), then plug it into an Ubuntu machine (e.g. lab desktop).
- **Option 2 (UTM VM on Mac):** keep the ESP32 plugged into the Mac, and forward serial over the virtual network to the VM (no USB passthrough needed!). See [Step 9b](#9b-mac--utm-vm-setup-network-serial-forwarding) below.

Total time the first time: about 30 minutes, mostly the ROS 2 download (~2 GB). Do Part A steps 1–2 first (git, the `dialout` group, the code).

## 7. Install ROS 2 (once)

Pick the version that matches your Ubuntu (`lsb_release -rs`):

| Ubuntu | ROS 2 | Use `<distro>` = |
|---|---|---|
| 24.04 | Jazzy | `jazzy` |
| 22.04 | Humble | `humble` |

Add the ROS 2 package source (from the [official install guide](https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html)):
```bash
sudo apt install software-properties-common
sudo add-apt-repository universe
sudo apt update && sudo apt install curl -y
export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F'"' '{print $4}')
curl -L -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo ${UBUNTU_CODENAME:-${VERSION_CODENAME}})_all.deb"
sudo dpkg -i /tmp/ros2-apt-source.deb
```

Install ROS 2 (includes rqt), the build tool, and the serial library. Replace `<distro>`:
```bash
sudo apt update
sudo apt install ros-<distro>-desktop ros-dev-tools python3-serial
```

Check: `source /opt/ros/<distro>/setup.bash && ros2 --help` prints the ROS 2 help.

## 8. Build the workspace (once, and after every `git pull`)

```bash
cd ros2_ws                                  # from the repo root
source /opt/ros/<distro>/setup.bash
colcon build --symlink-install
```

It ends with `Summary: 2 packages finished`.

## 9. Start the bridge and the GUI (Physical Ubuntu)

1. Flash the ESP32 (Part A) and plug it into this machine. **Close every serial monitor** (`pio device monitor`, Arduino IDE): only one program can use the port.
2. In a new terminal:
   ```bash
   cd ros2_ws
   source /opt/ros/<distro>/setup.bash
   source install/setup.bash
   ros2 launch tsp05_gui system.launch.py port:=/dev/ttyUSB0
   ```
3. The control panel opens. Wait for the green **● ESP32 data** and **ToF: READY**, then turn on the motor supplies.
4. Use **GUI mode** to command the motors from the panel, **SENSOR mode** to let the sensors drive them.

---

## 9b. Mac + UTM VM Setup (Network serial forwarding, no USB passthrough)

If you are running Ubuntu inside a UTM VM on your Mac, USB passthrough can often be unstable or tricky to configure. You can leave the ESP32 plugged directly into your Mac and forward the serial port over TCP to the VM:

### On your Mac (macOS Terminal):
1. Find your ESP32's serial port:
   ```bash
   ls /dev/cu.*
   # e.g., /dev/cu.usbserial-0001
   ```
2. Make sure any serial monitor is closed (`pio device monitor`, Arduino IDE).
3. Start the TCP serial bridge (listens on port 4000):
   * **Option A: Using `socat`** (`brew install socat`):
     ```bash
     socat -d -d TCP-LISTEN:4000,reuseaddr,fork GOPEN:/dev/cu.usbserial-0001,raw,echo=0,ispeed=115200,ospeed=115200
     ```
     *(Note: On macOS, `socat` requires `ispeed` and `ospeed` rather than `b115200`)*.

### In the UTM Ubuntu VM:
1. Find your Mac host IP address:
   * **UTM default (Shared Network):** `192.168.64.1` (verify with `ip route | grep default`).
   * **Tailscale / VPN:** use your Mac's Tailscale IP (e.g. `100.73.140.108`).
2. Run the system launch pointing to the socket:
   ```bash
   cd ros2_ws
   source /opt/ros/humble/setup.bash
   source install/setup.bash
   ros2 launch tsp05_gui system.launch.py port:=socket://<mac-ip>:4000
   ```
   *(Or launch them in separate terminals: `ros2 launch tsp05_bridge bridge.launch.py port:=socket://<mac-ip>:4000` in Terminal 1, and `ros2 run tsp05_gui tsp05_gui` in Terminal 2).*

---

**No hardware?** Terminal 1: `ros2 run tsp05_bridge fake_esp32` (prints a `/dev/pts/N` path). Terminal 2: the launch command with `port:=/dev/pts/N`.

| Problem | Fix |
|---|---|
| `Package 'tsp05_gui' not found` | Run step 8, and `source install/setup.bash` in this terminal |
| Panel shows **● no data** | Wrong port (`ls /dev/ttyUSB*`), a serial monitor still has it open, or the ESP32 isn't flashed |
| `Permission denied` / `cannot open /dev/ttyUSB0` | `dialout` group (Part A step 1), then log out and back in |
| `No module named 'serial'` | `sudo apt install python3-serial` |
| Panel window doesn't appear over SSH | Run it on the machine's own screen, or use `ssh -X` |

**Tested:** Jazzy on Ubuntu 24.04 with `fake_esp32`. Not yet tested with the real ESP32, or on Humble.
