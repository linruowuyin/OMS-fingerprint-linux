# OMS fingerprint module — Linux driver

Reverse engineering project for the **Jiangxi OMS Microelectronics fingerprint
module (`33a7:2388`)** that ships in several laptops and has no Linux driver.
The goal is a proper [libfprint](https://fprint.freedesktop.org/) driver so the
reader works with `fprintd`, PAM and the desktop environments.

**Status: the driver works on real hardware** — `fprintd-enroll` and
`fprintd-verify` pass, and a hardware-free umockdev regression test is included.
An upstream merge request (patch) is prepared in [`driver/upstream/`](./driver/upstream).

```
$ fprintd-enroll          → Enroll result: enroll-completed
$ fprintd-verify          → Verify result: verify-match (done)
$ meson test omsmoc       → 1/1 drivers+custom - libfprint:omsmoc OK
```

## Contents

| Document | What it covers |
|---|---|
| **[Installation](docs/INSTALL.md)** | Getting the driver working on your machine — the fastest path first |
| **[Protocol](driver/README.md)** | The reverse-engineered wire format, command semantics and hardware pitfalls |
| **[Upstreaming](driver/upstream/README.md)** | Submitting the driver to libfprint (merge request path + issue path) |
| **[Packaging](aur/README.md)** | Arch Linux package (local build, AUR submission) |
| **[Attribution](#attribution)** | Derivative-work lineage and the LGPL obligations |

## Quick start

```bash
git clone https://github.com/linruowuyin/OMS-fingerprint-linux
cd OMS-fingerprint-linux
driver/setup.sh --test        # fetch libfprint 1.94.100, build, run the umockdev test
```

That gives you a working `libfprint` with the driver in `build/`. To install it
system-wide, see **[docs/INSTALL.md](docs/INSTALL.md)**.

Requirements (Arch package names): `base-devel meson ninja glib2 glib2-devel
gusb libusb cairo pixman nss gobject-introspection umockdev`.

## The device

| | |
|---|---|
| USB ID | `33a7:2388` (Jiangxi OMS Microelectronics / OFilm) |
| Endpoints | EP1 OUT `0x01` / EP1 IN `0x81`, bulk, 64 bytes |
| Matching | **Match-on-Chip** — the chip stores up to 16 templates and does the matching, so no fingerprint algorithm is needed on the host |
| Firmware | vendor flash tooling only (not part of this repository) |

## Protocol (reverse engineered)

Two layered and completely plain text (the vendor Windows driver uses no
handshake, no key exchange and no encryption either):

```
① transport   8 byte "USBC" request header + 4 byte "USBS" acknowledgement
              (00 40 = write 64 / 80 40 = read 64 / 80 20 = poll 32)
② frames      64 byte "EF01" frames, last two data bytes = 16 bit checksum

commands      0x34 chip serial · 0x1d template count · 0x1f index table
              0x32 identify · 0x31 enroll · 0x0c delete/clear · 0x30 cancel
identify      00 01 … = finger captured (NOT a match)
              00 05 00 SS 03 e8 = matched template in slot SS
              09 05 … = no match
```

The transport, the command parameters and the response semantics (including the
timing rules and the frame checksum) are documented in
[`driver/omsmoc/omsmoc.c`](./driver/omsmoc/omsmoc.c) (file header), and in
[`driver/README.md`](./driver/README.md) together with the hardware pitfalls
(autosuspend, wedged firmware, the status of every implemented command).

## Repository layout

```
README.md                  this file
docs/
  INSTALL.md               installation guide (build, install, verify, troubleshoot)
  MAINTAINING.md           release checklist for the AUR package and the patch repo
driver/                    the libfprint driver and its tooling
  omsmoc/omsmoc.c/.h       driver source (upstream style, fpi_device + fpi_ssm)
  omsmoc/omsmoc_proto.c/.h EF01 frame codec (pure functions)
  tests/test-omsmoc-proto.c unit test for the codec (also wired into meson)
  tools/oms-smoke.c        bring-up tool (info/verify/identify/enroll/list/delete/clear)
  tests/omsmoc/            umockdev regression test + sanitized recorded traffic
  tests/sanitize-capture.py scrub device identifying data from a new recording
  upstream/                ready-to-submit upstream patch, steps and issue/request text
  libfprint-oms.patch      meson-only patch for a local build tree
  setup.sh                 fetch libfprint, apply patch, build, run the test
  README.md                protocol, command semantics, hardware notes, test coverage
aur/                       Arch package (libfprint + this driver) as a stop-gap
LICENSE                    LGPL-2.1-or-later
```

## Hardware bring-up

```bash
B=build/libfprint-build
$B/examples/oms-smoke info                 # open and print device state
$B/examples/oms-smoke verify 30 --slot 0   # press an enrolled finger
$B/examples/oms-smoke enroll 120           # press/lift 6 times
$B/examples/oms-smoke list                 # templates on the chip
$B/examples/oms-smoke delete <slot>        # delete one template
```

## Upstream status

The driver, the umockdev test case and the `hwdb` entries are ready as a single
patch in [`driver/upstream/`](./driver/upstream). It applies to upstream master,
builds there without warnings and passes the replay test. Not submitted yet.

## ⚠️ Device notes

* Never send an incomplete `USBC` sequence (header without data, or a bare
  `EF01` frame): it wedges the firmware.
* This device does **not** survive kernel autosuspend: it must stay powered
  (`power/control=on`) while a driver is being developed or used, e.g.

  ```
  ACTION=="add|change", SUBSYSTEM=="usb", ATTR{idVendor}=="33a7", ATTR{idProduct}=="2388", \\
      ATTR{power/control}="on"
  ```

  Do **not** add `TAG+="uaccess"` here: `fprintd` runs as root, so that would
  hand raw device access — including the ability to erase chip templates — to
  every local user.

* A wedged device (bulk transfers time out, enumeration still fine) recovers
  with a USB port reset, which can be done without root:

  ```bash
  python -c "import usb.core; usb.core.find(idVendor=0x33a7, idProduct=0x2388).reset()"
  ```

* Templates live in the chip only — treat them as the one irreplaceable state.

## Compliance

This is interoperability reverse engineering of hardware owned by the author.
No vendor binaries, firmware or raw captures are included in this repository;
the test recording shipped in `driver/tests/` has device identifying data
(chip serial, USB serial) removed. If you want to reproduce the captures, use
your own device and the vendor driver obtained from your hardware vendor.

## Attribution

This repository is a **derivative work** of the original driver by
**BlueApple233** — <https://github.com/BlueApple233/OMS-fingerprint-linux>
(licensed under LGPL-2.1-or-later). The original copyright notices are
retained in every source file, as required by the LGPL.

Modifications made here (2026-10-28):

| Change | Why |
|---|---|
| Split the `EF01` frame codec out of `omsmoc.c` into `omsmoc_proto.c/.h` | The codec became pure functions with no device or libfprint-private-header dependency, so it can be unit-tested without hardware |
| Added `tests/test-omsmoc-proto.c` (14 test cases) | Regression coverage for the frame codec — build, parse, checksum, bounds |
| Added `tools/oms-smoke.c` | A bring-up tool for enumerating templates on a real device |
| Added a sanitized umockdev regression test | Lets CI verify the driver without the physical sensor |
| Added AUR packaging and the upstream Driver Request text | Distribution and upstreaming |

The protocol documentation, the frame format and the driver design are the
original author's work and are reproduced here with credit.

## License

GNU Lesser General Public License 2.1 or later — see [`LICENSE`](./LICENSE),
the same license as libfprint so the driver can be merged upstream.

This repository is a derivative work of
[BlueApple233/OMS-fingerprint-linux](https://github.com/BlueApple233/OMS-fingerprint-linux),
which is also LGPL-2.1-or-later. See [Attribution](#attribution) for the
list of changes. The original copyright notices are preserved in every
source file.
