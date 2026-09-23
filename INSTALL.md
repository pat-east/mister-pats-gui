# Installation

This sets up MiSTer Pat's GUI so that the device boots straight into it.

> **What it changes.** One new directory on the SD card, `/media/fat/mister-pat`, and two
> lines added to `MiSTer.ini`. No existing file is rewritten, `/etc/inittab` is untouched,
> and the stock MiSTer binary stays exactly where it is. Uninstalling is deleting that
> directory and those two lines — see [Uninstalling](#uninstalling).

## Requirements

- A MiSTer with a working SD card setup and SSH access
- To build from source: macOS or Linux with the cross-toolchain, see
  [README.md](README.md#building-from-source)

Console Mode is **not required**, for anything. If you already have it installed, this GUI
picks up its typeface automatically and nothing further needs doing. If you do not, see
[Step 0](#step-0--the-typeface-if-you-want-it) below for the one manual step that replaces it.

## Step 0 — The typeface, if you want it

This GUI builds its own catalogue of systems, its own index of games, and fetches its own box
art — a wizard walks through the first two on first start (see
[Step 5](#step-5--build-the-game-database)), and box art is a button in Settings once the
database exists (**Settings → Fetch box art**). None of that needs Console Mode.

The one thing still outside the GUI is its typeface, Akrobat. It cannot be bundled here —
Fontfabric's free-font licence permits using it in your own designs but not redistributing the
font files themselves — so getting it is a manual, one-time step:

1. Download it from Fontfabric directly: <https://www.fontfabric.com/fonts/akrobat/>
2. Copy `Akrobat-Bold.ttf` and `Akrobat-SemiBold.ttf` onto the device:
   ```sh
   scp Akrobat-Bold.ttf Akrobat-SemiBold.ttf root@<ip>:/media/fat/mister-pat/fonts/
   ```
   (`mkdir -p /media/fat/mister-pat/fonts` first if the directory does not exist yet.)

Skip this and everything still works — the GUI falls back to a built-in typeface. If Console
Mode happens to be on the same SD card, its own copy of Akrobat is found automatically and
this step is not needed either way.

Replace `<ip>` with your MiSTer's address in every command below.

## Step 1 — Build

```sh
third_party/build.sh                                        # static zlib, libpng, freetype
make                                                        # build/mister-gui

git clone https://github.com/MiSTer-devel/Main_MiSTer third_party/Main_MiSTer
cd third_party/Main_MiSTer
git apply ../../patches/0001-autostart-gui.patch
make                                                        # MiSTer, the patched main binary
cd ../..
```

The patch does two things to the MiSTer main binary. It adds one INI key, `gui=`, and it
decouples DVI detection from `video_mode` — upstream only works out whether a display is DVI
while it is also deriving the video mode from the EDID, so setting `video_mode` in the INI
switches that detection off as a side effect, and a monitor on a DVI adapter stays dark.
Everything else about the binary is unchanged, which is the point: core loading, ROM mounting
and the in-game OSD keep working exactly as before.

## Step 2 — Copy the files

Everything lives in one directory that belongs solely to this application.

```sh
ssh root@<ip> 'mkdir -p /media/fat/mister-pat'

scp build/mister-gui root@<ip>:/media/fat/mister-pat/
scp third_party/Main_MiSTer/MiSTer root@<ip>:/media/fat/mister-pat/MiSTer_gui
ssh root@<ip> 'chmod +x /media/fat/mister-pat/mister-gui /media/fat/mister-pat/MiSTer_gui'
```

Then the console icons, once, about 2.8 MB:

```sh
tar czf /tmp/icons.tgz -C assets icons
scp /tmp/icons.tgz root@<ip>:/tmp/
ssh root@<ip> 'tar xzf /tmp/icons.tgz -C /media/fat/mister-pat/ --no-same-owner && rm /tmp/icons.tgz'
```

`--no-same-owner` is required because exFAT has no concept of file ownership and `tar` would
otherwise abort.

## Step 3 — Point the boot path at it

Two keys in `/media/fat/MiSTer.ini`, in the `[MiSTer]` section:

```ini
main=mister-pat/MiSTer_gui
gui=mister-pat/mister-gui
```

- **`main=`** is a stock MiSTer feature. The binary started by `/etc/inittab` reads it and
  executes the named program in its place. So the stock binary hands over to our patched one.
- **`gui=`** is the key the patch adds. The patched binary starts that program once the menu
  core is up, and restarts it if it ever exits while the menu core is still running.

Relative paths resolve against `/media/fat`; absolute paths are taken as given.

Both keys are safe against typos: `main=` only switches if the file exists, and a missing
`gui=` target is reported once and then left alone. In either case you end up in the stock
menu rather than at a black screen.

> If you use per-video-mode INI files — `MiSTer_RGsB.ini`, `MiSTer_SVID.ini`,
> `MiSTer_YPbP.ini` — add both keys there as well. Each INI is read on its own.

## Step 4 — Reboot and check

```sh
ssh root@<ip> reboot
```

The GUI should appear on the television. Check that:

- the D-pad moves the focus **one** position per press
- the menu button still opens the MiSTer OSD
- a game starts with **A** and responds to the controller afterwards
- L2/R2 jump between initial letters in a long list

If something is wrong, comment out the two INI lines and reboot — you are back to your
previous setup.

## Step 5 — Build the game database

The first start has nothing to show, so a wizard opens by itself. It lists the volumes it
found, explains what it is about to do, and waits: **A** starts the scan, **B** postpones it.

The scan reads one system per frame rather than racing through the drives. That is not
politeness — walking a large library flat out is what drops a marginally powered USB drive off
the bus, and it is the failure this whole design is built around. Expect a few minutes with a
large library, and watch the counter rather than the clock.

The result lands in `/media/fat/mister-pat/gamesdb/`: a small `catalog.tsv` read at every
start, and one `<System>.tsv` per system, read only when you open that system.

Run it again whenever you add or remove games: **Settings → Build game database**. Nothing on
your drives is written or changed, either time.

## After a game

Starting a game releases the input devices and the screen and exits the GUI, because
otherwise the game would get no controller input. **Hold the menu button** for about a second
and a half to leave the game: the menu core is reloaded and the GUI comes back on its own.

## Uninstalling

Complete, in this order:

```sh
ssh root@<ip> "sed -i '/^main=mister-pat/d; /^gui=mister-pat/d' /media/fat/MiSTer.ini"
ssh root@<ip> 'rm -rf /media/fat/mister-pat'
ssh root@<ip> reboot
```

The device then boots exactly as it did before. Nothing else was modified.

## If something goes wrong

| Symptom | Cause and remedy |
| --- | --- |
| Stock menu instead of the GUI | `main=` or `gui=` points at a file that is not there. Check the paths and that both files are executable. |
| Black screen, device responds to SSH | `ssh root@<ip> killall mister-gui` — the patched binary restarts it after a few seconds. |
| Blinking cursor in the top left | The console is in text mode. `ssh root@<ip> 'chvt 1'`, or reboot. |
| Nothing on a DVI monitor | Should be detected automatically. If not, set `dvi_mode=1` in `MiSTer.ini`. Note that DVI mode carries no audio. |
| Controller moves two positions per press | A mirror input device is being read as well. It should be filtered by name; check with `grep input:` in the GUI's output. |
| Menu button does not open the OSD | The GUI is running with `--exclusive`. Start it without that option. |
| No games listed | The database has not been built. Settings → Build game database. |
| No box art | The scraper has not run yet, or has not reached that system. Settings → Fetch box art. |
| Text looks like a blocky, all-caps placeholder font | No Akrobat file was found, so the built-in fallback typeface is drawing instead — this is a working state, not a bug. See [Step 0](#step-0--the-typeface-if-you-want-it) if you want the real typeface. |
| Games on a CD-based core do not start | The core expects a different loader slot. See the table in [POC.md](POC.md) and `assets/systems.conf.example`. |

A reboot is always the safe way back, and removing the two INI lines always returns the
device to its stock behaviour.

## Console Mode is not required

All four things the GUI needs are covered without it:

| What | Where |
| --- | --- |
| System catalogue | **built here**, `/media/fat/mister-pat/gamesdb/catalog.tsv` |
| Game index | **built here**, one `<System>.tsv` per system |
| Box art and backgrounds | **fetched here** — Settings → Fetch box art |
| Fonts | downloaded once by hand — [Step 0](#step-0--the-typeface-if-you-want-it) — or the built-in fallback if you skip that |

If Console Mode happens to already be on the SD card, its copy of Akrobat is picked up
automatically and nothing changes; if it is not, nothing is missing that this GUI cannot get
on its own. Console Mode itself never runs either way once this GUI is installed: `main=`
sends the boot path to the patched binary instead, and only Console Mode's font file, if it is
there, is ever read.
