# Research and Proof of Concept

This document is the technical record behind MiSTer Pat's GUI: how a MiSTer actually boots, where
a custom frontend can insert itself, and what we proved on real hardware rather than merely read
about. It covers the reverse-engineered interfaces — the `main=` hook, the framebuffer, the
`/dev/MiSTer_cmd` FIFO, `.mgl` files, the FPGA manager — together with the experiments that
confirmed them and the mistakes that taught us the rules. The scope of the project is a
**frontend**: no FPGA core of our own, no Verilog; the actual loading of core plus game is
delegated to existing MiSTer mechanisms wherever possible. Everything documented here was
**verified on the real device** unless explicitly marked as untested or assumed; deviations from
the public documentation are called out where we found them. For installing and running the
result see [INSTALL.md](INSTALL.md), for the interface design [GUI.md](GUI.md), and for
measurements [PERFORMANCE.md](PERFORMANCE.md).

## Test device

- Access: `ssh root@192.168.64.163` (public key deposited, no password needed)
- `Linux MiSTer 6.18.38-MiSTer #2 SMP armv7l GNU/Linux`, Buildroot
- The device already runs **Console Mode** as its frontend instead of the stock GUI — a very
  useful precedent, see [The `main=` hook](#the-main-hook-the-real-path).

## Target platform

| Component | Value |
| --- | --- |
| SoC | Intel/Altera Cyclone V SoC (DE10-Nano) |
| CPU | Dual-core ARM Cortex-A9 @ 800 MHz |
| Architecture | ARMv7-A, hard float (`armhf`) |
| FPU/SIMD | VFPv3-D32, NEON |
| Kernel | **6.18.38-MiSTer** (the public documentation says 5.15.1 — outdated) |
| libc | glibc 2.31 (Buildroot) |
| libstdc++ | `libstdc++.so.6.0.28` → max. `GLIBCXX_3.4.28` |
| Binary format | ELF 32-bit LSB, ARM EABI5, hard-float ABI |

Important: this is 32-bit ARM. An Apple Silicon Mac is also ARM, but **AArch64/Darwin** — a
completely different target (64 vs. 32 bit, Mach-O vs. ELF, macOS vs. Linux). Compiling natively
is therefore *not* possible; a cross toolchain is mandatory. The host being ARM does not help.

## Storage layout (verified)

```
/dev/loop8      on /           type ext4    (rw,noatime,nodiratime)
/dev/mmcblk0p1  on /media/fat  type exfat   (rw,noatime,nodiratime,sync,dirsync,fmask=0022,dmask=0022)
/dev/sda1       on /media/usb0 type vfat
/media/fat/linux/bluetooth on /var/lib/bluetooth type ext4   (loop0)
```

Corrections to the public documentation:

- The main partition is **exFAT**, not FAT32.
- The root filesystem is **not on a partition of its own**; it is an image file *on* the exFAT
  partition: `/media/fat/linux/linux.img` (393,216,000 bytes), mounted as `/` through a loop
  device (`/dev/loop8`). `/etc/inittab` therefore lives inside that image.
- `fmask=0022,dmask=0022`: everything on `/media/fat` is automatically `0755`. Copied binaries
  are **immediately executable**; `chmod +x` is a no-op there.
- Symlinks work on this exFAT partition (tested).

## Boot chain

`/etc/inittab` (BusyBox init) inside the root image, verified on the device:

```
::sysinit:/bin/mount -t proc proc /proc
::sysinit:/bin/mkdir -p /dev/pts /dev/shm
::sysinit:/bin/mount -a
::sysinit:/media/fat/MiSTer &
::sysinit:/etc/resync &
::sysinit:/sbin/swapon -a
...
::sysinit:/etc/init.d/rcS
console::respawn:/sbin/agetty --nohostname -L  console  xterm
console::respawn:/sbin/agetty --nohostname -L tty1 linux
```

Important details:

- **`::sysinit:`, not `::respawn:`** — the main binary is **not** restarted automatically when it
  crashes or exits. init runs sysinit entries once. A replacement application therefore has to be
  robust by itself or bring its own watchdog.
- The `agetty` entries are `respawn` — the login console comes back, the GUI does not.

### The `main=` hook (the real path)

The stock binary `/media/fat/MiSTer` reads the INI key `main=` and **execs the program named
there**. That is exactly how Console Mode hooks itself in — `/media/fat/MiSTer.ini`:

```ini
[MiSTer]
main=ConsoleMode/MiSTer_ConsoleMode
```

That is why `/proc/<pid>/exe` points at `MiSTer_ConsoleMode` with PPID 1 (exec keeps the PID and
PPID of the process started by init). The stock binary itself contains not a single Console Mode
reference — the path comes exclusively from the INI.

Implementation in [`user_io.cpp:1515`](https://github.com/MiSTer-devel/Main_MiSTer/blob/master/user_io.cpp#L1515):

```c
const char *main = getFullPath(cfg.main);
if (strcasecmp(main, getappname()) && FileExists(main))
{
    printf("Current exec is %s, core requires exec %s\n", getappname(), main);
    app_restart(path, xml, main);
}
```

- INI key table: `cfg.cpp:141` → `{ "MAIN", &cfg.main, STRING, 0, 1023 }`
- Default: `cfg.cpp:615` → `strcpy(cfg.main, "MiSTer")`
- The path is relative to `/media/fat` (via `getFullPath`)

**Built-in safety net:** `FileExists(main)`. If `main=` points at a file that does not exist, the
stock GUI simply carries on. The intervention is thus a single INI line — trivially reversible
over SSH or with the SD card in the Mac.

**Consequence for us:** no swapping of the boot binary, no patching of `/etc/inittab` inside the
root image, no symlinks. Just `main=mister-pat/<our-binary>` in `MiSTer.ini`.

Rejected alternatives:

- *Overwriting `/media/fat/MiSTer`* — works, but is more invasive and collides with regular
  MiSTer updates.
- *Patching `/etc/inittab`* — it lives in the loop image; needlessly laborious.
- *Symlinking the binary* — technically possible (exFAT supports symlinks), but the `main=` hook
  is the intended mechanism and comes with the `FileExists` fallback.

## Video: how pixels reach the TV

The TV picture comes from the FPGA, not from the ARM. `printf` ends up on `/dev/console` and is
**not** visible on the television. There is, however, a real Linux framebuffer:

```
/dev/fb0   name = MiSTer_fb   1920x1080   32 bpp   stride = 7680   smem_len = 8294400
channels: R offset=16 len=8   G offset=8 len=8   B offset=0 len=8   (XRGB in the 32-bit word)
```

Architecture:

- The FPGA scaler reads the framebuffer out of DDR3 at `FB_ADDR = 0x20000000 + 32 MiB`
  (`video.cpp:38`); the kernel driver `MiSTer_fb` maps the same region as `/dev/fb0`.
- The scaler is configured **by the main binary** over SPI through `/dev/mem`
  (`spi_uio_cmd_cont(UIO_SET_FBUF)`, `video.cpp:~4290`). Console Mode accordingly holds `/dev/mem`
  open as fd 3 and does not use `/dev/fb0` at all.
- The overlay is only visible when `video_fb_state()` is true — i.e. when the main binary has
  enabled the framebuffer.

### Enabling the framebuffer

`/dev/MiSTer_cmd` is a **FIFO** (`prw-r--r--`) read by the running main binary (fd 11 of the
Console Mode process). It accepts (`video.cpp:4182` ff.):

| Command | Meaning |
| --- | --- |
| `fb_cmd0 <fmt> <rb> <div>` | Full screen, resolution = current video mode / `div` (1–4) |
| `fb_cmd1 <fmt> <rb> <width> <height>` | Explicit size, integer-scaled and centred |
| `fb_cmd2 <fmt> <rb> <div>` | Same as `fb_cmd0` |

`fmt`: `8888` (32bpp), `1555`, `565`, `8` (palette). `rb` = swap red/blue.

**But:** `video_cmd()` is entirely gated by `if (video_fb_state())` — the commands reconfigure an
already active framebuffer, they do not switch it on.

It is switched on only by the main binary itself (`cfg.fb_terminal` must be 1, which it is on the
test device):

- **F9** in the menu core → `video_chvt(1); video_fb_enable(!video_fb_state())` (`menu.cpp:1369`)
- **Scripts menu** → `video_chvt(2); video_fb_enable(1)`, the script runs under `agetty` on tty2
  (`menu.cpp:7603`)
- **Doc viewer** (`.pdf`/`.txt`) → likewise, launches `/media/fat/linux/pdfviewer`
  (`menu.cpp:3410`)

This is also why `/media/fat/linux/pdfviewer` and `glow` work: they are started from those paths
and then write to `/dev/fb0`.

**For our own GUI this means:** as long as a main binary is running that has configured the
scaler, we can simply paint into `/dev/fb0` — proven in PoC 2. If we become the `main=` binary
ourselves, it is an open question whether the overlay is still active or has to be requested by
us. Programming the FPGA itself is explicitly not our goal (see the scope note in the
introduction); the route goes through delegation to the existing mechanisms.

## Frontend interface (verified)

The way to start cores and games **without working on the FPGA ourselves**.

### `/dev/MiSTer_cmd` — the complete command set

The FIFO is **created and read by the main binary itself**
([`input.cpp:5141`](https://github.com/MiSTer-devel/Main_MiSTer/blob/master/input.cpp#L5141)):

```c
unlink(CMD_FIFO);
mkfifo(CMD_FIFO, 0666);
pool[NUMDEV+1].fd = open(CMD_FIFO, O_RDWR | O_NONBLOCK | O_CLOEXEC);
```

Dispatcher: [`input.cpp:6228–6268`](https://github.com/MiSTer-devel/Main_MiSTer/blob/master/input.cpp#L6228).
Maximum 1023 bytes, a trailing `\n` is stripped. There are exactly five commands:

| Command | Effect |
| --- | --- |
| `fb_cmd0 <fmt> <rb> <div>` / `fb_cmd2 …` | Framebuffer mode, resolution = video mode / `div` (1–4) |
| `fb_cmd1 <fmt> <rb> <width> <height>` | Framebuffer mode, explicit size |
| `video_mode <modeline>` | Custom modelines only |
| **`load_core <path>`** | **`.rbf` → `fpga_load_rbf()`; `.mra`/`.mgl` → `xml_load()`** |
| `screenshot [scaled] [path]` | Screenshot |
| `volume mute\|unmute\|0..7` | Volume |

No `load_game`, no `mount`, no further FIFO or socket.

**`load_core` is our lever** — verified on the device:

```sh
echo "load_core /tmp/poc_random.mgl" > /dev/MiSTer_cmd
```

Result: the core switched to SNES, the running main binary restarted with our MGL as `argv[2]`,
and the game ran. **And the USB storage device stayed mounted** — MiSTer's own reconfiguration
does not knock it off the bus, whereas our incomplete loader sequence did (see
[Incident during the first live test](#incident-during-the-first-live-test-usb-storage-lost)).
That is a weighty argument for delegation.

### The decisive consequence

`fpga_load_rbf()` ends in `app_restart()` — the FIFO-reading process re-execs itself. And **only
the process that created the FIFO reads it.** The architectural question follows directly:

- If the stock binary keeps running and our GUI runs alongside it, `load_core` works — the stock
  binary handles core loading and game start completely.
- If *we* are the `main=` binary, the stock binary has been exec'd away. Then nobody reads the
  FIFO and we would have to implement `fpga_load_rbf()` ourselves — exactly the FPGA programming
  we wanted to avoid.

### `.mgl` (MiSTer Game Loader)

Parser: `support/arcade/mra_loader.cpp:1330ff` (`scan_mgl`), structure in `mra_loader.h:30-56`.

**A proven template.** Console Mode starts games via MGL itself and drops the most recent one at
`/media/fat/.LASTLAUNCH.mgl` — a guaranteed-working example straight from the device:

```xml
<mistergamedescription>
	<rbf>_Console/NES</rbf>
	<file delay="2" type="f" index="1" path="../../../../../media/usb0/games/NES/Addams Family, The (E) [!].nes"/>
</mistergamedescription>
```

Two corrections to the obvious assumption follow from it:

- **`index="1"`, not `0`.** The index is the number of the file option in the core menu. For NES
  and SNES it is 1; for other cores it has to be checked — see the table under "Slot per core".
- **The path is relative** and climbs with `../../../../../` up to the root instead of being
  absolute. Five levels are enough starting from `/media/fat/games/<System>`. Absolute paths are
  permitted according to the code, but did not work for us — the relative form is the proven one.

Produced by us and successfully started on the device (the game ran):

```xml
<mistergamedescription>
	<rbf>_Console/SNES</rbf>
	<file delay="2" type="f" index="1" path="../../../../../media/usb0/games/SNES/Michael Andretti's IndyCar Challenge (USA).zip/Michael Andretti's IndyCar Challenge (USA).smc"/>
</mistergamedescription>
```

- `<rbf>` is a directory plus prefix, not a complete file — the match is by prefix
  (`<prefix>.` or `<prefix>_`), taking the alphabetically greatest hit.
- For `<file>`, **all four attributes are mandatory**, otherwise the entry is discarded.
  `type=f` = ROM/file, `type=s` = disk slot, `index` = number of the F/S option in the core's
  confstr, `delay` in seconds.
- `<reset delay="n" [hold="n"]/>` is optional. At most 6 entries.
- Paths are absolute with a leading `/`, otherwise relative to `HomeDir()`.

**Important:** MGL playback is a state machine layered on top of the OSD menu (`menu.cpp`) — it
"types" the core menu navigation. Whoever replaces `HandleUI` loses MGL. All the more reason to
leave the loading to the stock binary.

**Archives: the path must point *into* the zip.** `FileIsZipped()` (`file_io.cpp:198`) looks for
`.zip` in the path and splits it into archive path and inner path. `isPathRegularFile`
(`file_io.cpp:302`) says so explicitly: *"If there's no path into the zip file, don't bother
opening it, we're a 'directory'"*. An MGL entry with `path="…/Game.zip"` is therefore treated as
a directory and loads nothing — the correct form is `path="…/Game.zip/rom.sfc"`.

This matters to us because practically the whole library is packed: in the test device's SNES
directory there are 786 `.zip` files against a single `.sfc`. Our GUI therefore has to know the
inner file name, i.e. read the zip's central directory itself — MiSTer uses miniz for that
(`mz_zip_*` in `file_io.cpp`).

#### Slot per core

Which slot a core expects is stated in its Verilog source in the `CONF_STR`, and `menu.cpp:2116`
matches `type` and `index` precisely against those entries. If nothing matches, MGL playback
aborts (`menu.cpp:2376`) — the core starts, the game does not load. That was exactly the failure
with PlayStation: we sent `type="f"`, while the core expects `type="s"`.

| System | `CONF_STR` | `type` | `index` |
| --- | --- | --- | --- |
| PlayStation | `H7S1,CUECHD,Load CD` | `s` | 1 |
| Saturn | `S0,CUECHD,Insert Disc` | `s` | 0 |
| Sega CD | `S0,CUECHD,Insert Disk` | `s` | 0 |
| TurboGrafx-16 CD | `S0,CUECHD,Insert CD` | `s` | 0 |
| Neo Geo CD | `S1,CUECHD,Load CD Image` | `s` | 1 |
| 3DO | `S0,CUEISO,Insert Disk` | `s` | 0 |
| TurboGrafx-16 | `FS0,PCEBIN,Load TurboGrafx` | `f` | 0 |
| Nintendo 64 | `FS1,N64z64n64v64,Load` | `f` | 1 |

Two reading traps: the `S` in `FS1` is not part of the number, it announces save support
(`menu.cpp:2110` skips one optional `S` and one optional `C`). And prefixes such as `H7` are
visibility conditions that are cut off before evaluation — the index stays 1. The values ship as
defaults in `Library.cpp` and can be overridden via
[`assets/systems.conf.example`](assets/systems.conf.example) → `/media/fat/mister-pat/systems.conf`.

#### CD games are folders, not files

In the Console Mode index every line carries a fourth field that marks a folder:

```
PLAYSTATION	3D Lemmings	games/PSX/3D Lemmings	1
ATARI 2600	3-D Genesis (Prototype).a26	games/Atari2600/3-D Genesis (Prototype).a26	0
```

A CD game exists as a folder of a `.cue` file plus data tracks; the core has to be handed the
image file inside it. Its name does **not** reliably follow the folder name:

```
games/PSX/3D Lemmings/3D Lemmings.cue                                  identical
games/PSX/007 - The World Is Not Enough/007 - ... (USA).cue            different
```

Deriving it is therefore out. `Launcher::resolveDisc` reads the folder once at startup and picks
by preference `.cue`, `.chd`, `.iso`, `.ccd` — the `.cue` ahead of its own `.bin` tracks, and
with several images the alphabetically first one, i.e. disc 1. That is a single `readdir` on one
folder and therefore no risk to the storage device.

### The stock binary's CLI

`main.cpp:62-73`: exactly two arguments, `argv[1]` = core/RBF path, `argv[2]` = XML path. Both
are only passed on to `user_io_init()`.

**`main.cpp` does not load `argv[1]` into the FPGA** — there is no `fpga_load_rbf` there. The core
is already in the FPGA at startup (put there by U-Boot or by the predecessor instance). So
`MiSTer core.rbf` loads *nothing*; the argument is informational (HomeDir, config names, ROM
lookup). This matches `app_restart` (`fpga_io.cpp:620-647`):
`execl(appname, appname, path, xml, NULL)`.

### `main=` can be set per INI section

INI sections are matched against the **core name** (`cfg.cpp:241-245`): `[MiSTer]` always
applies, plus `[arcade]`, `[arcade_vertical]` and the respective core name (with wildcard
support). `main=` can thus be restricted to individual cores instead of applying globally.

## Build toolchain

### Our route (macOS arm64 host)

Prebuilt cross toolchain via the Homebrew tap
[messense/macos-cross-toolchains](https://github.com/messense/homebrew-macos-cross-toolchains),
built for `aarch64-darwin`:

```sh
brew tap messense/macos-cross-toolchains
brew install messense/macos-cross-toolchains/arm-unknown-linux-gnueabihf
```

This installs GCC 15.2.0 (crosstool-NG 1.28.0) as
`arm-unknown-linux-gnueabihf-{gcc,g++,strip,readelf,…}` into `/opt/homebrew/bin` — no PATH setup
needed. Prerequisite: an accepted Xcode license (`sudo xcodebuild -license accept`).

### Official route (x86_64 Linux hosts only)

`Main_MiSTer` uses `gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf` (target
`arm-none-linux-gnueabihf`), C99 for C and C++14 for C++. Not usable on an M1 Mac. The stock
binary on the device is built with it (`GCC: (GNU Toolchain for the A-profile Architecture
10.2-2020.11 (arm-10.16)) 10.2.1 20201103`).

Libraries linked by the original, for orientation: libc, libstdc++, libm, librt, libpthread,
Imlib2, libfreetype, libbz2, libpng16, libz, Bluetooth lib.

### Two pitfalls

1. **The toolchain builds for ARMv5TE/VFPv2 by default**, not for ARMv7-A. Without explicit flags
   the binary says `Tag_CPU_arch: v5TE`, `Tag_FP_arch: VFPv2`, no NEON. It runs on the Cortex-A9
   (backwards compatible) but gives away performance and SIMD. Hence always:

   ```
   -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard
   ```

2. **glibc version skew.** Our toolchain (GCC 15.2) ships a far newer glibc/libstdc++ than the
   target system (glibc 2.31, `libstdc++.so.6.0.28` → max. `GLIBCXX_3.4.28`). Harmless for a
   hello world (it only needs `GLIBC_2.4`), but as soon as the real C++ standard library is used,
   missing `GLIBCXX_*` symbols threaten at runtime. Remedy: **link statically** (`-static`), or at
   least `-static-libstdc++ -static-libgcc`. It costs size (417K instead of 5.5K), which is
   irrelevant on a 400 MB partition.

   To check what a binary demands:

   ```sh
   arm-unknown-linux-gnueabihf-readelf -V <binary> | grep -oE "GLIBC_[0-9.]+|GLIBCXX_[0-9.]+"
   ```

Note for zsh: `$FLAGS` is not word-split automatically — write `${=FLAGS}` or spell the flags out
directly.

## Proof of concept

Sources under [`poc/`](poc/), deployed to `/media/fat/mister-pat` on the device.

### PoC 1 — cross-compile and execute ✅

[`poc/hello.cpp`](poc/hello.cpp): a minimal C++ program printing one line and its own PID.

```sh
cd poc
arm-unknown-linux-gnueabihf-g++ -std=gnu++14 -O2 \
  -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb -static -o hello_static hello.cpp
arm-unknown-linux-gnueabihf-strip hello_static
scp hello_static root@192.168.64.163:/media/fat/mister-pat/
ssh root@192.168.64.163 /media/fat/mister-pat/hello_static
```

Result on real hardware:

```
Hello from a future MiSTer replacement, built on an M1 Mac.
PID=1677
```

ABI attributes of the finished binary — an exact match for the Cortex-A9:

```
Tag_CPU_name: "7-A"   Tag_CPU_arch: v7   Tag_CPU_arch_profile: Application
Tag_FP_arch: VFPv3    Tag_Advanced_SIMD_arch: NEONv1   Tag_ABI_VFP_args: VFP registers
```

Both the static (417K) and the dynamic (5.5K) build run, exit code 0.

*Side note:* running locally under emulation does not work on macOS — Homebrew's `qemu` only ships
`qemu-system-*`, no user-mode `qemu-arm` (it is not built on Darwin), and Apple Silicon cannot
execute 32-bit ARM natively. Real hardware is the more meaningful test anyway.

### PoC 2 — framebuffer ✅

[`poc/fbhello.cpp`](poc/fbhello.cpp): mmaps `/dev/fb0` and paints text with an embedded 8x8 bitmap
font.

```sh
arm-unknown-linux-gnueabihf-g++ -std=gnu++14 -O2 \
  -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb -static -o fbhello fbhello.cpp
```

Runs without errors and writes into the mapping:

```
fb0: 1920x1080 @ 32 bpp, line_length=7680, smem_len=8294400, id=MiSTer_fb
channels: R off=16 len=8  G off=8 len=8  B off=0 len=8
painted 1920x1080
```

**And it is visible and legible on the television** (verified on the device). That proves the
complete video path: a binary of our own, cross-compiled on an M1 Mac, paints pixels onto the TV —
without any system change, without touching the boot path, reversible at any time.

State of the device at the time: Console Mode as frontend, `fb_terminal=1`, active VT `tty1`,
`vtcon1` (fbcon) bound to `fb0`. The framebuffer overlay was already active — the assumption that
F9 is required first did not hold.

Two findings from this PoC:

- **`/dev/fb0` is a shared buffer.** The running frontend redraws on every controller input and
  overwrites our output — visible as bleed-through. A single paint pass is therefore gone again
  almost immediately; a visible test needs a redraw loop. Once we are the `main=` binary
  ourselves, that competitor no longer exists.
- **Without double buffering it flickers badly.** Drawing straight into the visible buffer
  (clear → draw) tears visibly. Solution: render offscreen and blit once per frame with a single
  `memcpy`.

### PoC 3 — "Press Start to Play" with game launch ✅

[`poc/startscreen.cpp`](poc/startscreen.cpp) plus [`poc/gfx.h`](poc/gfx.h) (the renderer as a
component of its own: offscreen canvas, one `memcpy` per frame, range-based glyph table).

Done and verified on the device:

- **Rendering** — "PRESS START TO PLAY", pulsing, 1920×1080
- **Controller input** over evdev, all 7 devices detected, `EVIOCGRAB` exclusive (Console Mode
  keeps the devices open but does *not* grab them exclusively — our grab succeeds)
- **Game selection** — 788 SNES and 369 NES ROMs found, random pick
- **Zip resolution** ([`poc/zip.h`](poc/zip.h)) — reads the central directory itself and finds the
  ROM name inside the archive. Necessary because 786 of 788 SNES games are packed.
- **MGL generation** with the correct index and the relative path form
- **Game launch** — handed to the FIFO via `load_core`, the game ran on the device

```
chosen:     /media/usb0/games/SNES/Michael Andretti's IndyCar Challenge (USA).zip
rom in mgl: …(USA).zip/Michael Andretti's IndyCar Challenge (USA).smc
```

The chain selection → MGL → launch is thereby fully proven.

**Important side finding: input devices are not stable.** A wireless controller falls asleep and
then reappears as a *new* `event*` node. Scanning once at startup is not enough — that is exactly
why the gamepad was missing in the first test run. The GUI has to rescan periodically (currently
every second) or watch `/dev/input` with inotify.

### Bitstream loader ✅

[`poc/fpga.h`](poc/fpga.h) — a port of the FPGA manager sequence from Main_MiSTer/U-Boot
(GPL-2.0+; the project is going to be published as open source anyway).

Done in two stages to avoid freezing the machine:

**1. Read-only probe** ([`poc/fpgaprobe.cpp`](poc/fpgaprobe.cpp)) — validates the mapping and
register offsets before anything is written:

```
stat = 0x00000054   mode = 4 (USER_MODE)   msel = 0x0A
ctrl = 0x000002C0   gpi[31] = 0            initdone = 1
```

The values match exactly what `fpgamgr_program_init` would compute from `msel=0x0A` (bit 3 set →
32-bit configuration width; `msel & 3 == 2` → CDRATIO x8 → `ctrl = 0x2C0`). That confirmed the
layout.

**2. Writing** ([`poc/loadcore.cpp`](poc/loadcore.cpp)) — successful on the device:

```
before: mode=4 ready=1
bitstream: 4446024 bytes
load_rbf returned 0
after:  mode=4 ready=1
```

Afterwards `/tmp/CORENAME` = `SNES`, process
`MiSTer_ConsoleMode /media/fat/_Console/SNES_20260823.rbf /tmp/poc_random.mgl` with PPID 1. No
freeze, no power cycle needed.

Important implementation details:

- **Mapping:** a single `mmap` of `/dev/mem` over `0xFF000000` with length `0x01000000`; registers
  are addressed as `&base[(addr & 0xFFFFFF) >> 2]`.
- **Bridges:** before writing, `bridges(false)` (system manager `fpgaintfgrp_module`=0, SDR
  `+0x5080`=0, reset manager `brg_mod_reset`=7, NIC301 `remap`=1), afterwards `bridges(true)`.
  This is the dangerous part — get it wrong and the bus hangs.
- **Data transfer:** Main_MiSTer uses inline assembly with `ldmia`/`stmia` for 32-byte bursts. We
  write a simple 32-bit loop to the data register (`0xFFB90000`) instead — semantically
  unambiguous and fast enough at 4.4 MB.
- **`.rbf` header:** the SNES core has *no* MiSTer-specific header; the check for the magic
  `"MiSTer"` is still needed, because other cores do have one.
- **Handover:** `fork` + `setsid` before the `execl`, otherwise the new main binary dies with the
  SSH session.

**Precondition:** the running frontend process has to be terminated first. Otherwise it talks to a
core that was swapped out from under it, and two main binaries fight over the FPGA and the inputs.

#### Incident during the first live test: USB storage lost

On the first live run, the USB drive holding the game library disappeared from the bus. The
temporal correlation is unambiguous (failure at uptime 3683 s, core load immediately before).
Kernel log:

```
usb 1-1.1.1: reset high-speed USB device number 9
usb 1-1.1.1: device descriptor read/64, error -110
usb 1-1.1.1: USB disconnect, device number 9
device offline error, dev sda
usb 1-1.1-port1: attempt power cycle
usb 1-1.1-port1: unable to enumerate USB device
```

`error -110` is a timeout — the device stops responding, even after the hub's power cycle
attempts. No data corruption, an enumeration failure; physically unplugging and replugging it, or
a power cycle, restores it. All other USB devices stayed connected.

Suspected cause: an FPGA reconfiguration changes the current draw abruptly and a marginally
powered USB hub sags. A known MiSTer issue — but we made it more likely, because our port was
**incomplete**.

**Three deviations from the original sequence that need correcting:**

1. **`fpga_core_reset(1)` is missing.** Main_MiSTer calls it before `do_bridge(0)` and before
   `socfpga_load()` (`fpga_io.cpp:591`, `:623`). We skipped it.
2. **`fpga_core_reset(0)` practically does not exist.** In the whole repo only `(1)` is ever
   called. The function sets bit 30 (`0x40000000`) in the GPO for `reset=1` and bit 31
   (`0x80000000`) for `reset=0` — so it is not the assert/release pair one would expect.
3. **`fpga_gpo_read()` returns a cached copy**, not the register (`fpga_io.cpp:518`; the real
   `readl` is commented out). MiSTer tracks the GPO state itself because the register does not
   read back reliably. A separate process without that history has to read the register — the
   probe reports `0x80000042` there, which looks plausible, but according to the original it
   cannot be relied upon.

**Lesson:** do not port this sequence selectively. It is proven on exactly this hardware; every
omitted step is a risk whose effect cannot be assessed.

### PoC 4 — our own binary as `main=` ✅

This is the one that turned a companion process into a boot path, and it is what ships today.

Rather than write a `main=` binary from scratch — which would mean taking on FPGA core
loading, video initialisation and input handling, all of it already solved — the project forks
Main_MiSTer and adds a single INI key:

```ini
main=mister-pat/MiSTer_gui      # the stock binary hands over to our fork
gui=mister-pat/mister-gui       # our fork starts the frontend
```

The patch is small on purpose (see [`patches/`](patches/)): a `gui[1024]` field in the config
struct, a `GUI` INI key, and a block in `menu.cpp` that starts the configured program once the
menu core is up. It starts it through MiSTer's own scripts mechanism — writing `/tmp/script`
and triggering it — which is what puts the framebuffer overlay on screen in the first place.

Three details that were not obvious:

- **A restart cooldown is required.** Without it, a frontend that fails to start spins in a
  loop. The patch retries every five seconds and gives up entirely if the file does not exist,
  reporting it once.
- **The overlay has to be re-enabled when the OSD closes.** `menu.cpp:1407` disables it
  unconditionally on `KEY_F12|UPSTROKE`; the patch puts it back if our session is still alive.
- **A long press on the menu button loads `menu.rbf`**, which is the way back from a running
  game. Without it there is no route home, because starting a game exits the frontend.

Way back at any point: remove the two INI lines. `main=` only switches if the file exists, so
a wrong path lands in the stock menu rather than at a black screen.

## Integrating with a running MiSTer environment

The frontend runs **alongside** the MiSTer main binary, not in its place. Six rules follow from
that, all of them worked out on the device — each one was a bug first.

> These were learned while running next to Console Mode, before the boot path of PoC 4 existed.
> Five of them still apply verbatim, because the frontend still runs beside a main binary —
> ours now rather than Console Mode's. Rule 1 is the exception: the script session that puts up
> the overlay is now started by our own fork, so there is no foreign renderer left to suspend.
> It is kept here because it explains *why* the overlay works the way it does.

**1. The framebuffer overlay is not ours.** It is put up by the main binary as soon as a script
session is running (`video_fb_enable(1)` in the scripts path, `menu.cpp:7603`). Console Mode starts
its renderer exactly that way:

```
MiSTer_ConsoleMode           (main binary, FIFO)
 └─ /bin/bash /tmp/script    (MiSTer's scripts mechanism)
     └─ ConsoleMode_arm      (renderer, holds /dev/fb0)
```

If you terminate the renderer, the script session ends and the main binary switches the overlay
**off** — your own application keeps running but is invisible. The right move is to only
**suspend** the renderer (`kill -STOP`) and resume it later. That is exactly what
[`tools/run-gui.sh`](tools/run-gui.sh) does.

**2. The console has to go into graphics mode.** Otherwise the kernel draws the cursor and
terminal output into the same framebuffer. `KDSETMODE`/`KD_GRAPHICS` on `/dev/tty0`.

**3. Restore the console mode you found, do not assume "text".** Console Mode leaves the console in
graphics mode already; whoever hard-sets `KD_TEXT` on exit leaves a blinking cursor in the top
left. So read `KDGETMODE` beforehand.

**4. Open the console with `O_NOCTTY`.** After `setsid` the process is a session leader without a
controlling terminal; a plain `open("/dev/tty0", O_RDWR)` then makes the console our terminal, and
because we are not in its foreground process group the kernel kills us with `SIGTTOU` — without a
single log line. `SIGHUP`, `SIGTTOU` and `SIGTTIN` are ignored on top of that.

**5. Do not grab inputs exclusively.** `EVIOCGRAB` locks out the MiSTer main binary, and only it
can open the OSD — the Xbox button then does nothing. Console Mode likewise does not grab
exclusively. Exclusive mode is available via `--exclusive`, but off by default.

**6. Skip `MiSTer virtual input`.** The main binary mirrors the inputs it reads onto a uinput
device of its own. Reading both counts every button press twice — the focus jumps two positions.
The device is recognised by its name and skipped.

On top of that comes the behaviour at game launch: as soon as a core is loaded, the application
releases the input devices and the console and **exits** (`Context::standDown`). If it did not,
the game would get no controls.

## Architecture decision

The frontend research yields two routes, and they are mutually exclusive.

### Route A — companion process (fits the scope)

The stock binary keeps running, our GUI runs next to it as a process of its own (started e.g. from
`/media/fat/linux/user-startup.sh`).

- Drawing into `/dev/fb0` — proven in PoC 2
- Core and game launch via `echo "load_core …" > /dev/MiSTer_cmd`
- MGL works, because the stock binary keeps its menu state machine
- **No FPGA code, no intervention in the boot path** — nothing can render the interface unusable
- Open point: the stock UI still exists underneath and draws into the same framebuffer on input
  (the bleed-through we observed). Has to be settled in practice.

### Route B — becoming the `main=` binary

We replace the main binary entirely.

- Full control over the screen, no competitor in the framebuffer
- But: nobody reads the FIFO any more, so we would have to implement `fpga_load_rbf()` ourselves —
  exactly the FPGA programming that the scope rules out
- On top of that we lose MGL, because its playback sits on `menu.cpp`/`HandleUI`
- Mitigation possible: set `main=` only in the menu core's section, so the stock binary still
  takes over for game cores
- To be clarified along the way, should this route ever become relevant: Main_MiSTer is under the
  GPL — taking over code would have licensing consequences for our project

### Decision: Route B

Full replacement was chosen. Two findings make it considerably cheaper than initially feared:

**1. Core loading is manageable.** `socfpga_load()` (`fpga_io.cpp:347`) is ~30 lines plus helpers —
a port of the socfpga FPGA manager driver from U-Boot, pure register accesses on mapped `/dev/mem`:

```c
fpgamgr_program_init();
fpgamgr_program_write(rbf_data, rbf_size);
fpgamgr_program_poll_cd();
fpgamgr_program_poll_initphase();
fpgamgr_program_poll_usermode();
```

Framed by `do_bridge(0)` / `do_bridge(1)` (`fpga_io.cpp:378`). An alternative, not yet examined:
the kernel FPGA manager (`/sys/class/fpga_manager/fpga0`) exists on the device and could do the
same with considerably less code.

`.rbf` files can carry a **16-byte MiSTer-specific header** (magic `"MiSTer"`, bitstream size as a
`uint32` at offset 12, data from offset 16) — it has to be stripped when handing the bitstream to
a different loader.

**2. MGL stays usable.** Verified in `user_io.cpp:1550`:

```c
if (xml && isXmlName(xml) == 2) mgl_parse(xml);
```

The stock binary plays back a `.mgl` when it is passed as `argv[2]`.

### The resulting plan

1. Our GUI becomes the `main=` binary and replaces everything the user sees while browsing.
2. On core selection: load the bitstream ourselves (`socfpga_load` equivalent or the kernel FPGA
   manager).
3. Then hand over to the stock binary via `exec`, with the core path as `argv[1]` and optionally a
   `.mgl` as `argv[2]` — it takes care of the in-game OSD, ROM mounting and MGL playback.

That keeps the scope sustainable: we build the interface, not a reimplementation of Main_MiSTer.

**Important to keep in mind:** setting `main=` globally means we are the main binary *during* a
game as well — then we would have to implement core communication (`spi_uio`), the OSD, ROM/disk
mounting and input mapping ourselves, i.e. practically all of Main_MiSTer. To avoid that, `main=`
belongs in the menu core's section, not in `[MiSTer]`.

Kernel interfaces present on the device, should they ever become relevant:
`/sys/class/fpga_manager/fpga0` (`ff706000.fpgamgr`, state `operating`) as well as the
`fpga_bridge` and `fpga_region` classes.

## Sources

- [MiSTer-devel/Main_MiSTer](https://github.com/MiSTer-devel/Main_MiSTer) — source code of the stock GUI
- [MiSTer-devel/Linux_Image_creator_MiSTer](https://github.com/MiSTer-devel/Linux_Image_creator_MiSTer) — `create_img.sh`, builds the root image
- [MkDocs_MiSTer — ARM Cross-Compiling](https://mister-devel.github.io/MkDocs_MiSTer/developer/mistercompile/)
- [MkDocs_MiSTer — INI Settings](https://mister-devel.github.io/MkDocs_MiSTer/advanced/ini/)
- [messense/homebrew-macos-cross-toolchains](https://github.com/messense/homebrew-macos-cross-toolchains)
