# Installation

This sets up MiSTer Pat's GUI so that the device boots straight into it. Use the fresh-card
procedure for a new MiSTer Fusion installation, or the SSH procedure below for an existing setup.

The commands below install the latest *published* release. The source checkout may include
unreleased changes that are not present in the latest release assets. To install the source
version, follow [Building from source](#building-from-source).

> **What it changes.** The fresh-card installer creates `/media/fat/mister-pat`, installs the
> GUI, configures the `[MiSTer]` startup entries and restarts MiSTer. The existing-install
> procedure installs or updates the GUI files but leaves
> `MiSTer.ini` unchanged; you add the boot-path lines yourself. Uninstalling is deleting the
> GUI directory and its two INI lines — see [Uninstalling](#uninstalling).

| | |
| --- | --- |
| Release files | `MiSTer_gui`, stable `mister-gui` loader, versioned `mister-pats-gui-X.Y.Z.so`, metadata and SHA-256 checksums |
| Install directory | `/media/fat/mister-pat` |
| INI section | `[MiSTer]` in `/media/fat/MiSTer.ini` |
| Stock binary | left in place |
| Typeface | Space Grotesk, downloaded by setup or Settings → Accessibility; built-in fallback available |
| Choose an older GUI | Pin its versioned `.so` with `mister-pats-gui.so`, then restart |

## Requirements

- A MiSTer with a working SD card setup. SSH access is required for the existing-installation
  procedure below (the stock login is `root`, and unless changed the password is `1`).
- The games on a USB drive or the SD card, wherever the MiSTer already finds them
- To build from source instead of using the release: macOS or Linux with the cross-toolchain,
  see [Building from source](#building-from-source)

## Fresh MiSTer Fusion card

Follow these steps to install the GUI on a new MiSTer Fusion SD card:

1. **Prepare the card.** Use an SD card with at least 32 GB and install MiSTer Fusion. Keep the
   card connected to your computer after writing the image.
2. **Copy the installer.** Copy `tools/install-mister-pat.sh` from this project to the card's
   `/Scripts/` folder. The file should be `/Scripts/install-mister-pat.sh`.
3. **Connect MiSTer.** Insert the card, connect MiSTer to the internet, and connect a controller.
   A display capable of at least 1080p is recommended.
4. **Run MiSTer’s updater.** Start MiSTer and, from the Scripts menu, run `update.sh`. Let it
   finish and restart MiSTer if requested.
5. **Install the GUI.** From the Scripts menu, run `install-mister-pat.sh`. It installs the latest
   published GUI, configures MiSTer to start it, and restarts the system.
6. **Continue setup.** Follow the on-screen guide to learn the dashboard and prepare a small game
   sample. The guide explains how to build the full game list and prepare box art later. Core
   updates are available in Settings → Library. The installer does not download games or BIOS
   files.

If installation stops with an error, check the internet connection and run the installer again.
For an existing MiSTer setup, use the separate [SSH procedure](#existing-mister-setup-ssh).

Replace `<mister-ip>` with your MiSTer's address in every command below.

## Existing MiSTer setup (SSH)

Use this manual procedure only for an existing MiSTer setup. For a fresh MiSTer Fusion card, use
the first-time setup above; it runs the updater as part of `install-mister-pat.sh`.

### 1. Connect to your MiSTer

```sh
ssh root@<mister-ip>
```

Run steps 2 and 4 in the SSH session. Step 3 is optional and uses files on your computer. The
remaining steps are completed on the MiSTer itself.

### 2. Install or update the GUI files

On the MiSTer:

```sh
curl --proto '=https' --proto-redir '=https' --fail --location --output /tmp/install-update.sh https://raw.githubusercontent.com/pat-east/mister-pats-gui/main/tools/install-update.sh
sh /tmp/install-update.sh
```

`tools/install-update.sh` creates the application directories, verifies release metadata and
SHA-256 hashes, and installs the loader, patched MiSTer binary and first GUI library. On later
runs it adds only a newer versioned GUI library. First installation also fills in missing
system-icon BMPs and downloads Space Grotesk and its SIL Open Font License into
`/media/fat/mister-pat/fonts`. If the font host is unavailable or a downloaded file fails
validation, setup continues;
the GUI retains its built-in font fallback. The script does not change `MiSTer.ini` or reboot
the device. If it prints suggested INI entries, add them manually as described in step 4.
For a legacy installation with no active library link, run
`sh /tmp/install-update.sh --migrate-legacy` instead.

### 4. Point the boot path at it

Two keys in `/media/fat/MiSTer.ini`, in the `[MiSTer]` section. On the MiSTer:

```sh
nano /media/fat/MiSTer.ini
```

```ini
[MiSTer]
main=mister-pat/MiSTer_gui
gui=mister-pat/mister-gui
```

- **`main=`** is a stock MiSTer feature. The binary started by `/etc/inittab` reads it and
  executes the named program in its place. So the stock binary hands over to the patched one.
- **`gui=`** is the key the patch adds. The patched binary starts that program once the menu
  core is up. After a fatal GUI error it shows a crash screen and pauses automatic relaunch
  until a manual reboot; this prevents a crash loop from repeatedly writing to storage.

Relative paths resolve against `/media/fat`; absolute paths are taken as given.

Both keys are safe against typos: `main=` only switches if the file exists, and a missing
`gui=` target is reported once and then left alone. In either case you end up in the stock
menu rather than at a black screen.

> If you use per-video-mode INI files — `MiSTer_RGsB.ini`, `MiSTer_SVID.ini`,
> `MiSTer_YPbP.ini` — add both keys there as well. Each INI is read on its own.

### 5. Reboot and check

```sh
reboot
```

The GUI should appear on the television. Check that:

- the D-pad moves the focus **one** position per press
- the menu button still opens the MiSTer OSD
- a game starts with **A** and responds to the controller afterwards
- L2/R2 jump between initial letters in a long list

If something is wrong, comment out the two INI lines and reboot — you are back to your
previous setup.

### 6. Build your library, then fetch box art

When no game list exists and onboarding has not been completed, the first start opens a
controller-operated guide. It explains the main dashboard areas, the context-sensitive hints
at the bottom left, and where to find common setup options. **B** skips onboarding; that choice
is remembered so an empty library does not reopen the wizard on every boot.

The guide offers **Jump-Start**. **A** reads up to 20 game titles for each available system,
including up to 20 Arcade titles that pass the core, ROM and CRC checks. It then prepares
existing covers and looks up matching box art for those sample games. Matching artwork may be
downloaded, as the Jump-Start screen explains before you start. This creates a small sample
library for a first look at the GUI; it does not read the full collection. The system list in
progress can be scrolled with Up and Down. **B** cancels the current scan or cover work.

Afterward, the overview confirms what is ready, suggests reading the full library and preparing
its box art later, and points to controller setup. **A** starts using the GUI. Choose
**Settings → Library → Build game list** to replace the sample with the complete library. The
scan reads one system per frame to keep storage access paced. Arcade is checked in small batches
and only working games are listed; if a game is missing, **Settings → Manage Arcade → Arcade
Games** explains why.

The game list is stored in `/media/fat/mister-pat/gamesdb/`: a small `catalog.tsv` read at every
start and one `<System>.tsv` per system, read only when you open that system. The scan does not
change game files.

Choose **Settings → Library → Prepare box art** to prepare covers for the full library. The GUI
converts existing covers into five view-sized BMP variants and downloads missing artwork when
the thumbnail server has a match. It can be interrupted and resumed; completed covers stay in
place. The progress screen shows an approximate completion clock time after the first game is
processed. Budget up to about 1.7 MB per cover for the BMP variants, in addition to the existing
JPEG/PNG files. See [BOXART.md](BOXART.md) for the exact sizes and fallbacks.

If system pictures or tab icons are missing, choose **Settings → System → Download Assets**.
The GUI downloads only missing BMPs and can still display older PNG system icons while BMPs are
absent.

Rebuild the game list after adding or removing games; run Prepare box art again to fill artwork
for new games. The database scan does not alter the games on your drives.

## After a game

Starting a game releases the input devices and the screen and exits the GUI, because
otherwise the game would get no controller input. **Hold the menu button** for about a second
and a half to leave the game: the menu core is reloaded and the GUI comes back on its own.

## Updating

Open **Settings → Updates**, choose **Check now**, review the release notes, then choose
**Update now**. The download is verified before installation. Restart MiSTer to load the new
version; choosing to continue without restarting keeps the current version running until the
next restart.

If an older version is pinned, remove the pin in **Settings → Updates** to use the newest
installed version. If the update asks you to rebuild the game list, choose **Settings → Library
→ Build game list** after restarting. For manual recovery, use the
[existing setup SSH procedure](#existing-mister-setup-ssh).

### Hardware validation status

The 0.5.0 loader and versioned GUI library have been exercised on a MiSTer: installation from a
local package, repeat installation as a no-op, startup, Settings and a manual update check,
legacy migration, and a broken active library link falling back to the valid installed library
were checked. The 0.6.0 Wi-Fi setup flow and interface sound playback were also exercised on
MiSTer hardware. The
published-release download path and the GUI install/cancel path still need end-to-end testing
against a suitable newer GitHub release. Hash failures, interrupted commits, first install
without a prior version, and the loader's text fallback also remain explicit validation cases.

## Building from source

Instead of the release files:

```sh
third_party/build.sh                                        # PIC static dependencies
make                                                        # loader and versioned GUI library

git clone https://github.com/MiSTer-devel/Main_MiSTer third_party/Main_MiSTer
cd third_party/Main_MiSTer
git apply ../../patches/0001-autostart-gui.patch
make BASE=arm-unknown-linux-gnueabihf -j4                  # bin/MiSTer
cd ../..
```

`make package-release` creates the eight required GitHub assets from the release build and
the patched main binary after ELF, ABI and symbol checks. Publish those assets together under
the matching `vX.Y.Z` tag only after the hardware release checks documented in
[DISCDRIVE.md](DISCDRIVE.md).

For local development, build and deploy the current version with:

```sh
DEVICE=root@<mister-ip> make deploy
```

This builds the current release version and deploys it to MiSTer. The full build instructions are
in [README.md](../README.md#building-from-source).

## Uninstalling

Complete, in this order:

```sh
ssh root@<mister-ip> "sed -i '/^main=mister-pat/d; /^gui=mister-pat/d' /media/fat/MiSTer.ini"
ssh root@<mister-ip> 'rm -rf /media/fat/mister-pat'
ssh root@<mister-ip> reboot
```

The device then boots exactly as it did before. Nothing else was modified.

## If something goes wrong

| Symptom | Cause and remedy |
| --- | --- |
| Stock menu instead of the GUI | `main=` or `gui=` points at a file that is not there. Check the paths and that both files are executable. |
| Stock menu instead of the GUI, even though both files are right where they should be | Stock MiSTer resolves relative paths like `main=`/`gui=` against whichever storage device was last selected — persisted in `/media/fat/config/device.bin`, not necessarily the SD card. If that got switched to a USB drive (e.g. while testing a second game drive), the lookup silently fails because neither file exists there, and MiSTer falls back to itself rather than showing an error. `ssh root@<mister-ip> rm -f /media/fat/config/device.bin` and reboot to put it back on the SD card. |
| Crash screen: “GUI stopped after an error” | Read `logs/crash.log` and `logs/module-map.log` from the same run. Use `tools/symbolize-crash.sh` with the matching build artifacts, then restart MiSTer manually. |
| Loader error 70–79 on the terminal | The wrapper writes `logs/loader.log` and stops relaunches. Check the installed versioned library and `mister-pats-gui.so` link, then restart manually. |
| Black screen, device responds to SSH | Check `/media/fat/mister-pat/logs/crash.log` and the GUI/launcher processes over SSH; reboot to restore the boot path. |
| Blinking cursor in the top left | The console is in text mode. `ssh root@<mister-ip> 'chvt 1'`, or reboot. |
| Nothing on a DVI monitor | Should be detected automatically. If not, set `dvi_mode=1` in `MiSTer.ini`. Note that DVI mode carries no audio. |
| Controller moves two positions per press | A mirror input device is being read as well. It should be filtered by name; check with `grep input:` in the GUI's output. |
| Menu button does not open the OSD | The GUI owns controller input while it runs, so its exclusive grab blocks the MiSTer OSD. The grab is released on exit and before a core starts. |
| No games listed | The game list has not been built. Open Settings → Library → Build game list. |
| No box art | Covers have not been prepared yet, or the game has no match online. Open Settings → Library → Prepare box art. Existing JPEG/PNG covers remain usable without BMP variants. |
| No Arcade tab, or an Arcade tab with few games | The tab only appears when at least one Arcade game passes the check, and only games that pass are listed: core installed, ROM zip found, every CRC right. Settings → Manage Arcade → Arcade Games shows why each one fails. Then rebuild the database. |
| Systems show plain tiles or tab icons are missing | Choose Settings → System → Download Assets or rerun the setup script in step 2. Existing PNG console icons work as a fallback. |
| Text looks like a blocky, all-caps placeholder font | Space Grotesk could not be loaded, so the built-in fallback typeface is drawing. Choose Settings → Accessibility → Space Grotesk to download the font files, or rerun the setup script with network access. |
| Games on a CD-based core do not start | The core expects a different loader slot. See the table in [POC.md](POC.md) and `assets/systems.conf.example`. |

A reboot is always the safe way back, and removing the two INI lines always returns the
device to its stock behaviour.
