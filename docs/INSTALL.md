# Installation

How to get the OMS `33a7:2388` fingerprint reader working on your machine.

**You need:** a Linux machine with the sensor (`33a7:2388`) attached, and either
a distribution package or the build dependencies to compile it yourself.

> **Before you start:** this driver *replaces* your distribution's `libfprint`.
> It is not a separate `fprintd` plugin — the driver is compiled into the
> `libfprint-2` shared library itself. Read [Replacing your libfprint](#replacing-your-libfprint)
> before installing.

---

## Path A — Arch Linux, build the package locally

The fastest route on Arch. No AUR account needed.

```bash
git clone https://github.com/linruowuyin/OMS-fingerprint-linux
cd OMS-fingerprint-linux/aur/libfprint-omsmoc

makepkg -fC          # build (the -C clears the source cache; see below)
yes | sudo pacman -U libfprint-omsmoc-*.pkg.tar.zst
```

> `-C` is required on every rebuild. Without it `prepare()` fails because the
> patch is already applied to the cached source tree.

Then install the udev rule (the package does this for you) and restart fprintd:

```bash
sudo systemctl restart fprintd
fprintd-list          # should list your device
```

### What the package does

| | |
|---|---|
| Builds | `libfprint` 1.94.100 from the upstream tarball, with the OMS driver patched in |
| Provides | `libfprint` and `libfprint-2.so=2-64` (it *is* libfprint) |
| Conflicts | `libfprint>=1.90` — see below |
| Installs | the library, GIR/typelib, pkgconfig, `70-oms-fingerprint.rules`, and the autosuspend hwdb |

### Rebuilding after a libfprint upgrade

`pacman -Syu` will report a conflict, because the distribution's `libfprint`
replaces this package. Rebuild and reinstall:

```bash
cd OMS-fingerprint-linux/aur/libfprint-omsmoc
makepkg -fC
sudo pacman -U libfprint-omsmoc-*.pkg.tar.zst
```

### Reverting to the distribution package

```bash
sudo pacman -R libfprint-omsmoc
sudo pacman -S libfprint
sudo systemctl restart fprintd
```

---

## Path B — any distribution, build from source

This produces a working `libfprint` in `build/` without touching the system.

```bash
git clone https://github.com/linruowuyin/OMS-fingerprint-linux
cd OMS-fingerprint-linux

driver/setup.sh --test     # download libfprint 1.94.100, patch, build, run the replay test
```

### Build dependencies

| Distribution | Packages |
|---|---|
| Arch | `base-devel meson ninja glib2 glib2-devel gusb libusb cairo pixman nss gobject-introspection umockdev` |
| Debian / Ubuntu | `build-essential meson ninja-build libglib2.0-dev libgusb-dev libusb-1.0-0-dev libcairo2-dev libpixman-1-dev libnss3-dev gobject-introspection libgirepository1.0-dev umockdev` |
| Fedora | `gcc meson ninja-build glib2-devel libgusb-devel libusb1-devel cairo-devel pixman-devel nss-devel gobject-introspection-devel umockdev` |

### Trying it without installing

`setup.sh` prints the exact commands when it finishes. The short version:

```bash
B=build/libfprint-build

# Test against this build instead of the system one
sudo systemctl stop fprintd
sudo env LD_LIBRARY_PATH=$PWD/$B/libfprint /usr/lib/fprintd --no-timeout &
fprintd-enroll
fprintd-verify
```

Stop that daemon before restarting the system one:

```bash
kill %1 && sudo systemctl start fprintd
```

### Installing system-wide

`setup.sh` builds an uninstalled tree. To install it properly, use the Arch
package in Path A, or build libfprint with a prefix yourself:

```bash
cd build/libfprint-1.94.100
meson setup build --prefix=/usr --buildtype=release \
  -Ddrivers=default -Ddoc=false -Dinstalled-tests=false \
  -Dudev_rules=enabled -Dudev_hwdb=enabled
meson compile -C build
sudo meson install -C build
sudo systemctl restart fprintd
```

> **The udev rule is not optional.** The chip cannot survive kernel
> autosuspend. Without `power/control=on` the device will intermittently stop
> responding. Install this rule:
>
> ```
> # /usr/lib/udev/rules.d/70-oms-fingerprint.rules
> ACTION=="add|change", SUBSYSTEM=="usb", ATTR{idVendor}=="33a7", ATTR{idProduct}=="2388", ATTR{power/control}="on"
> ```
>
> Then `sudo udevadm control --reload && sudo udevadm trigger`.

---

## Replacing your libfprint

The driver is compiled into `libfprint-2`, so installing it means replacing the
distribution's `libfprint`. Two consequences:

**`fprintd` and `fprintd` clients** — `fprintd` links against `libfprint-2.so.2`,
so the system daemon picks the driver up automatically. No separate service to
install or enable.

**Desktop integration** — GNOME, KDE and Plasma fingerprint login all talk to
`fprintd` over D-Bus, so they keep working unchanged.

**On Arch specifically**, note that the legacy `libfprint-1` package (which
provides `libfprint=1.0`) is *not* the same thing and can coexist. That is why
the package declares `conflicts=('libfprint>=1.90')` rather than a blanket
`conflicts=('libfprint')` — a blanket conflict would remove the legacy library
too and break `fingerprint-gui`.

---

## Verifying the installation

```bash
fprintd-list                 # the device should appear
fprintd-enroll               # press and lift the finger six times
fprintd-verify               # press once
```

Expected output:

```
Enroll result: enroll-completed
Verify result: verify-match (done)
```

For a finger that is **not** enrolled, `fprintd-verify` must report
`verify-no-match`. If it reports a match for an unenrolled finger, something is
wrong — stop and check the install.

### PAM integration

`pam_fprintd` works once `fprintd-enroll` has stored a template. Before
changing `login` or `sudo` configuration, keep a root TTY open so you cannot
lock yourself out.

To test without touching system configuration:

```bash
printf 'auth       sufficient pam_fprintd.so\nauth       required   pam_deny.so\n' |
    sudo tee /etc/pam.d/oms-fprint-test
# trigger any PAM-using program, then:
sudo rm -f /etc/pam.d/oms-fprint-test
```

---

## Troubleshooting

### The device does not appear in `fprintd-list`

1. **Check the USB ID.** `lsusb | grep -i 33a7` — if it does not show
   `33a7:2388`, the driver will not match it. Some machines ship a different
   chip and need a different driver.
2. **Check the udev rule is loaded.**
   `systemd-hwdb query 'usb:v33A7p2388*'` should report `ID_AUTOSUSPEND=1`.
   `udevadm test /sys/bus/usb/devices/...` shows the rules that matched.
3. **Check the driver was actually built in.**
   `strings /usr/lib/libfprint-2.so.2.0.0 | grep -i omsmoc` should print
   something.

### The device works but randomly stops responding

This is the autosuspend problem. Confirm the rule is in place:

```bash
cat /sys/bus/usb/devices/*/power/control | grep -c on
```

Every `33a7:2388` device must report `on`. If not, the udev rule is missing or
not matching.

### The device enumerates but bulk transfers time out

The firmware is wedged. This happens when an incomplete `USBC` sequence was
sent. Reset the port — no root needed:

```bash
python3 -c "import usb.core; usb.core.find(idVendor=0x33a7, idProduct=0x2388).reset()"
```

If the device enumerates as `1234:abce` instead, it is in bootloader mode and
needs the vendor flashing tool to recover. That erases the chip, and with it
every enrolled template.

### Enrollment fails with "no match" while pressing

The chip can capture an image but reject it as a poor-quality print. Press
cleanly, with the pad of the finger flat against the sensor. Retry the stage.

### `pacman -U` aborts with a dependency conflict

The package conflicts with the distribution's `libfprint`. Answer `y` when
pacman offers to remove it, or run:

```bash
yes | sudo pacman -U libfprint-omsmoc-*.pkg.tar.zst
```

### `makepkg` fails in `prepare()`

The cached source tree already has the patch applied. Use `makepkg -fC`.

---

## Getting help

Open an issue at
<https://github.com/linruowuyin/OMS-fingerprint-linux/issues> and include:

* your distribution and version
* `lsusb | grep -i 33a7` output
* `fprintd-list` output
* `journalctl -u fprintd -n 50` after a failed attempt

If you have a device that is *not* `33a7:2388`, say so in the issue — that
information decides whether a new driver is needed.
