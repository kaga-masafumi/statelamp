# Linux Raspberry Pi Agent Console

日本語版: [README.ja.md](README.ja.md)

This directory records the reproducible host setup for the Linux StateLamp
client. The target is a dedicated 800x480 console on a Raspberry Pi 4. It is an
independent client of the shared StateLamp Bridge and must not introduce
display-specific fields into the Bridge protocol.

## Target hardware

- Raspberry Pi 4, aarch64
- Elecrow 5-inch HDMI Display, PCB Rev3.3
- 800x480 HDMI panel
- XPT2046 resistive touch controller connected through the 40-pin GPIO header
- XPT2046 operated by the in-kernel `ads7846` compatible driver

The Rev3.3 touch interface uses SPI0, chip select CE1 (`cs=1`), and BCM GPIO25
for PENIRQ. Raspberry Pi Device Tree parameters use BCM GPIO numbering, not
physical header pin numbers.

## Baseline verification

Before changing configuration, record the host's OS, kernel, display mode, and
input devices locally. Do not commit machine-specific hostnames, addresses, or
credentials.

- Raspberry Pi 4 or compatible Raspberry Pi host
- 800x480 HDMI display
- A current Raspberry Pi OS or Debian-based installation
- Boot configuration: `/boot/firmware/config.txt`
- Boot command line: `/boot/firmware/cmdline.txt`
- Default systemd target: `multi-user.target`
- Display server: none; Linux virtual console on `tty1`
- DRM connector: `card1-HDMI-A-1`, connected
- Advertised HDMI mode: `800x480`
- Framebuffer virtual size: `800,480`
- Graphics overlay: `vc4-kms-v3d`
- SPI controllers: disabled
- `/dev/spidev*`: absent
- Touch input device: absent
- Standard overlay: `/boot/firmware/overlays/ads7846.dtbo`, present

The HDMI display already works at its native resolution. Do not add legacy
`hdmi_group`, `hdmi_mode`, `hdmi_cvt`, or framebuffer settings unless a later
regression demonstrates that KMS/EDID detection has failed.

## Safety rules

1. Do not run Elecrow `LCD-show` or replace the kernel.
2. Inspect and back up the live boot configuration before editing it.
3. Make one stage of changes at a time.
4. Show the proposed diff before rebooting.
5. Keep the dated backup until display and touch survive multiple reboots.
6. Never store credentials or host secrets in this repository.

All commands that write `/boot/firmware`, install packages, or reboot the host
require explicit operator approval before execution.

## Stage 1: verify the display

These checks are read-only:

```bash
cat /sys/class/drm/card1-HDMI-A-1/status
cat /sys/class/drm/card1-HDMI-A-1/modes
cat /sys/class/graphics/fb0/virtual_size
```

Expected output is `connected`, a mode containing `800x480`, and framebuffer
size `800,480`. On the physical panel, confirm that the console fills the screen
without clipping, flicker, or intermittent blanking.

## Stage 2: enable and recognize touch

### Back up the boot configuration

Choose a literal timestamp and retain it in the filename so rollback never
depends on a shell variable:

```bash
sudo cp --preserve=all /boot/firmware/config.txt \
  /boot/firmware/config.txt.before-rr050-YYYYMMDD-HHMMSS
sudo cmp /boot/firmware/config.txt \
  /boot/firmware/config.txt.before-rr050-YYYYMMDD-HHMMSS
```

### Minimal proposed Device Tree change

Append these lines below the existing `[all]` section:

```ini
# Elecrow 5-inch HDMI Display Rev3.3 resistive touch
dtparam=spi=on
dtoverlay=ads7846,cs=1,penirq=25,penirq_pull=2,speed=50000,pmax=255,xohms=150
```

This first change deliberately omits axis inversion, swapping, and coordinate
limits. Those are calibration properties, not prerequisites for device
recognition. The obsolete `keep_vref_on` parameter from older vendor examples
is not supported by the currently installed overlay and must not be added.

Before rebooting, inspect the exact change:

```bash
diff -u /boot/firmware/config.txt.before-rr050-YYYYMMDD-HHMMSS \
  /boot/firmware/config.txt
```

Then reboot only with operator approval:

```bash
sudo reboot
```

### Verify recognition after reboot

```bash
ls -l /dev/spidev* /dev/input/event* 2>/dev/null
cat /proc/bus/input/devices
journalctl -b -k --no-pager | grep -Ei 'ads7846|spi|touch'
pinctrl get 7,8,9,10,11,25
```

Expected results:

- SPI0 pins are assigned to their alternate SPI functions;
- an input device named like `ADS7846 Touchscreen` exists;
- that input device has an `eventN` handler;
- no probe, IRQ, or SPI errors appear in the kernel log.

If `evtest` is already installed, select the ADS7846 event device and touch the
four corners and center:

```bash
evtest
```

Confirm that events appear, pressure changes on press/release, both axes span a
large range, and values change consistently. Do not calibrate until these raw
events are reliable.

Alternatively, use the repository's dependency-free five-point probe. It
auto-detects the ADS7846 input device and saves raw samples plus median values
as JSON:

```bash
cd hardware/raspberry-pi
python3 touch_probe.py
```

Follow the prompts and press the center and four corners for about one second
each. The default output is `touch-probe-YYYYMMDD-HHMMSS.json`. These generated
logs are local diagnostic evidence and should be reviewed before committing,
because host paths and timestamps are included. An explicit destination can be
selected with `--output`, and `--device /dev/input/eventN` overrides automatic
device discovery.

## Stage 3: calibrate for the selected UI stack

Calibration belongs to the display server or StateLamp UI layer. The
current host has no X11 or Wayland compositor, so installing
`xinput-calibrator` now would not calibrate the Linux console and would
prematurely choose X11.

After the kiosk UI stack is selected:

- for X11, use an Xorg `InputClass` transformation/calibration matrix;
- for Wayland/libinput, use the compositor's output mapping and a libinput
  calibration matrix;
- for a direct framebuffer/DRM application reading evdev, normalize the raw
  ADS7846 ranges in the client and persist only the measured values.

Start by testing whether axes are exchanged or reversed. Only then add
`swapxy`, `invx`, `invy`, `xmin`, `xmax`, `ymin`, or `ymax`. Record the raw
corner measurements and final transform in this directory.

## Rollback

If the system still boots, restore the exact dated backup and reboot:

```bash
sudo cp --preserve=all \
  /boot/firmware/config.txt.before-rr050-YYYYMMDD-HHMMSS \
  /boot/firmware/config.txt
sudo reboot
```

If boot fails, mount the boot partition on another computer and copy the dated
backup over `config.txt`. Because Stage 2 changes only the boot configuration,
rollback does not require removing packages or restoring a kernel.

## Agent Console MVP

The first Linux Console increment is in `console/`. It serves an 800x480 web
UI and keeps Linux-only host metrics outside the shared Bridge protocol. The
server uses only the Python standard library and proxies the existing v1.2
read endpoints through the same origin:

- `/api/console/status` -> Bridge `/api/v1/status`;
- `/api/console/attention` -> Bridge `/api/v1/attention`;
- `/api/console/host` -> local temperature, load, memory, and disk metrics.

Start it for development without changing system services:

```bash
python3 hardware/raspberry-pi/console/server.py \
  --bridge-url http://127.0.0.1:18480
```

Then open `http://127.0.0.1:18880`. Set `STATELAMP_BRIDGE_URL` instead of
the command-line option when preferred. The UI polls every two seconds,
displays Bridge disconnection explicitly, and stores the most recent state
transitions in browser-local storage. This history is a display convenience,
not the future authoritative Protocol event model.

Run its dependency-free tests with:

```bash
python3 -m unittest -v hardware/raspberry-pi/console/test_server.py
```

The intended production shell is Cage plus Chromium in kiosk mode. Do not
install packages or add a systemd service until the development UI has been
verified on the physical panel and the operator approves those sudo changes.

After installing Cage and Chromium, the kiosk command itself can be tested
from an active local VT with:

```bash
hardware/raspberry-pi/console/launch-kiosk.sh
```

The launcher uses `http://127.0.0.1:18880` by default. Override it with
`STATELAMP_CONSOLE_URL` when needed. The console stylesheet hides the mouse
pointer because the production panel is touch-only.

### Boot services

The supplied systemd units assume StateLamp is deployed at `/opt/statelamp`
and that `bridge/.venv` has already been created there. The maintained unit
files are templates in `systemd/`. Replace `YOUR_USER` below with the Linux
account that owns the checkout, then install and enable the Bridge, Console,
and kiosk services for that account.
The template derives the runtime directory from the service user's UID; no
username or UID is embedded in the repository.

```bash
sudo install -d /opt/statelamp
sudo cp -a . /opt/statelamp
python3 -m venv /opt/statelamp/bridge/.venv
/opt/statelamp/bridge/.venv/bin/python -m pip install -r /opt/statelamp/bridge/requirements.txt
sudo install -m 0644 hardware/raspberry-pi/systemd/statelamp-bridge@.service /etc/systemd/system/
sudo install -m 0644 hardware/raspberry-pi/systemd/statelamp-console@.service /etc/systemd/system/
sudo install -m 0644 hardware/raspberry-pi/systemd/statelamp-kiosk@.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now statelamp-bridge@YOUR_USER.service statelamp-console@YOUR_USER.service statelamp-kiosk@YOUR_USER.service
sudo chvt 2
```

TTY1 remains available as the recovery console (`Ctrl+Alt+F1`). To disable
automatic kiosk startup without removing files:

```bash
sudo systemctl disable --now statelamp-kiosk@YOUR_USER.service statelamp-console@YOUR_USER.service statelamp-bridge@YOUR_USER.service
```

The kiosk unit activates TTY2 before starting Cage. This ordering is required:
if Cage starts while TTY1 remains active, it can wait indefinitely before
launching Chromium even though systemd reports the compositor as running.

### Raspberry Pi HDMI phantom pointers

On this Pi, the two HDMI remote-control input nodes (`vc4-hdmi-0` and
`vc4-hdmi-1`) are classified by udev as `ID_INPUT_POINTINGSTICK=1`. Cage then
advertises pointer capability and draws a cursor even though the terminal is
touch-only.

To diagnose this without installing extra packages, enumerate every event
node and compare its kernel name with the udev/input-id classification:

```bash
for event in /dev/input/event*; do
  echo "===== $event ====="
  udevadm info --query=property --name="$event" \
    | grep -E '^(DEVNAME|DEVPATH|ID_INPUT|ID_INPUT_|ID_PATH|LIBINPUT)'

  sys_path=$(udevadm info --query=path --name="$event")
  sudo udevadm test-builtin input_id "/sys$sys_path" 2>/dev/null \
    | grep '^ID_INPUT'

  event_name=$(basename "$event")
  printf 'KERNEL_NAME=%s\n' \
    "$(cat "/sys/class/input/$event_name/device/name")"
done
```

If `libinput-tools` is installed, `libinput list-devices` provides a second
view; inspect every device whose `Capabilities` contains `pointer`. Do not
assume the touchscreen is responsible merely because the cursor appears after
enabling touch. On this host the decisive output was:

```text
ADS7846 Touchscreen  ID_INPUT_TOUCHSCREEN=1
vc4-hdmi-0           ID_INPUT_POINTINGSTICK=1
vc4-hdmi-1           ID_INPUT_POINTINGSTICK=1
```

The `/proc/bus/input/devices` listing is also useful for matching kernel input
names, handlers, and event numbers, but udev properties are what reveal the
classification consumed by libinput and Cage.

After identifying the unwanted pointer devices, install the focused libinput
rule before starting the kiosk:

```bash
sudo install -m 0644 \
  hardware/raspberry-pi/udev/90-statelamp-ignore-hdmi-pointing.rules \
  /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=input
```

The rule ignores only HDMI event nodes already classified as pointing sticks.
It does not ignore the ADS7846 touchscreen or HDMI jack-switch devices.
Verify the resulting classification with:

```bash
for event in /dev/input/event*; do
  echo "=== $event ==="
  udevadm info --query=property --name="$event" \
    | grep -E 'ID_INPUT|LIBINPUT_IGNORE_DEVICE'
done
```

Expected: `ADS7846 Touchscreen` retains `ID_INPUT_TOUCHSCREEN=1`, while the
two HDMI RC event nodes retain their diagnostic classification but also gain
`LIBINPUT_IGNORE_DEVICE=1`. Restart Cage after applying the rule.

## Evidence to record

For each completed stage, add a dated note containing:

- git commit and deployed configuration diff;
- kernel version;
- physical display observation;
- input-device name and `/dev/input/eventN` mapping;
- raw values at all four corners and center;
- rotation and calibration transform;
- reboot and cold-boot result;
- rollback test or backup filename.

Do not commit the full boot configuration if it later contains unrelated or
sensitive host settings. Commit a focused diff or redacted excerpt instead.

## References

- Elecrow, *5.0 Inch HDMI Touch Screen for the Raspberry Pi User Guide*:
  <https://www.elecrow.com/download/5.0%20InchHDMITouchScreenfortheRaspberryPiUserGuide.pdf>
- Raspberry Pi firmware overlay documentation:
  <https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README>
