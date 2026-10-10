# Installation

This sets up MiSTer Pat's GUI so that the device boots straight into it. It takes about ten
minutes and everything is done from a terminal on your computer — the MiSTer needs to be on
your network, and you need to be able to SSH into it.

The commands below install the latest *published* release. Starting with 0.4.6, releases
contain the loader, a versioned GUI library, the patched main binary and verification data.
To use the source version in that interval, follow [Building from source](#building-from-source).

> **What it changes.** The setup script creates `/media/fat/mister-pat` and installs the GUI
> files there. It only checks `MiSTer.ini` and prints any boot-path lines that need adding;
> you make those changes yourself. `/etc/inittab` and the stock MiSTer binary stay untouched.
> Uninstalling is deleting that directory and the two INI lines — see [Uninstalling](#uninstalling).

| | |
| --- | --- |
| Release files | `MiSTer_gui`, stable `mister-gui` loader, versioned `mister-pats-gui-X.Y.Z.so`, metadata and SHA-256 checksums |
| Install directory | `/media/fat/mister-pat` |
| INI section | `[MiSTer]` in `/media/fat/MiSTer.ini` |
| Stock binary | left in place |
| Typeface | Space Grotesk, downloaded by the setup script |
| Choose an older GUI | Pin its versioned `.so` with `mister-pats-gui.so`, then restart |

## Requirements

- A MiSTer with a working SD card setup, on your network, with SSH access (the stock login is
  `root`, and unless you changed it the password is `1`)
- The games on a USB drive or the SD card, wherever the MiSTer already finds them
- To build from source instead of using the release: macOS or Linux with the cross-toolchain,
  see [Building from source](#building-from-source)

Console Mode is **not required**, for anything. See
[Console Mode is not required](#console-mode-is-not-required).

Replace `<mister-ip>` with your MiSTer's address in every command below.

## Install

### 1. Connect to your MiSTer

```sh
ssh root@<mister-ip>
```

Run steps 2 and 4 in the SSH session. Step 3 is optional and uses files on your computer. The
remaining steps are completed on the MiSTer itself.

### 2. Run the install/update script

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
`sh /tmp/install-update.sh --migrate-legacy` instead. This explicitly saves the previous
executables in `.rollback/X.Y.Z/`.

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

The first start has nothing to show, so a wizard opens by itself. It lists the volumes it
found, explains what it is about to do, and waits: **A** starts the scan, **B** postpones it.

The scan reads one system per frame rather than racing through the drives. That is not
politeness — walking a large library flat out is what drops a marginally powered USB drive off
the bus, and it is the failure this whole design is built around. Expect a few minutes with a
large library, and watch the counter rather than the clock.

Arcade is part of the same scan, and the slowest part of it: for every `.mra` it checks that the
core is installed and that the ROM zip is there with the right CRCs, and lists only the games
that pass. With a few thousand `.mra` files that adds a couple of minutes. The Arcade tab appears
once there is at least one working Arcade game. If a game you expected is missing,
**Settings → Manage Arcade → Arcade Games** shows what is wrong with it.

The result lands in `/media/fat/mister-pat/gamesdb/`: a small `catalog.tsv` read at every
start, and one `<System>.tsv` per system, read only when you open that system.

Then choose **Settings → Prepare box art**. The GUI converts existing covers into five
view-sized BMP variants and downloads missing artwork when the thumbnail server has a match.
It can be interrupted and resumed; completed covers stay in place. The progress screen shows
an approximate completion clock time after the first game is processed. Budget up to about
1.7 MB per cover for the BMP variants, in addition to the existing JPEG/PNG files. See
[BOXART.md](BOXART.md) for the exact sizes and fallbacks.

If system pictures are missing, choose **Settings → Download System-Icons**. The GUI prefers
BMP icons and can still display older PNG icons while the BMPs are absent.

Rebuild the database after adding or removing games; run Prepare box art again to fill artwork
for new games. The database scan does not alter the games on your drives.

## After a game

Starting a game releases the input devices and the screen and exits the GUI, because
otherwise the game would get no controller input. **Hold the menu button** for about a second
and a half to leave the game: the menu core is reloaded and the GUI comes back on its own.

## Updating

In **Settings → Updates**, select **Check now**, then **Update now**. Confirm the release notes.
Download and hash verification run in the background. **B** cancels before the new `.so` is
published. The existing loader and patched MiSTer executable stay in place. With no version
pin, restart MiSTer to load the newest installed library. The Settings screen shows the running
version, the version selected for the next start, and whether a symlink pins that selection.
When pinned, the update check compares GitHub's version with the pinned version, even if a
higher library is already present. Downloading a newer library leaves the pin in effect.
With automatic checks enabled, Settings shows **Waiting for Internet** until a network address,
DNS and a successful GitHub HTTPS response are available; only then does it show **Checking
GitHub**. Switching automatic checks off stops the wait; manual **Check now** then remains available.

To pin an installed version manually, use a relative link in the installation directory:

```sh
cd /media/fat/mister-pat
rm -f mister-pats-gui.so.new
ln -s mister-pats-gui-0.4.6.so mister-pats-gui.so.new
mv -f mister-pats-gui.so.new mister-pats-gui.so
```

Replace `0.4.6` with the desired installed version and restart MiSTer. To return to automatic
selection of the newest installed library, remove `mister-pats-gui.so` and restart. Older
versioned libraries remain in place, so choosing one does not require another download.

The SSH installer uses the same release contract and is also available for recovery:

```sh
curl --proto '=https' --proto-redir '=https' --fail --location --output /tmp/install-update.sh https://raw.githubusercontent.com/pat-east/mister-pats-gui/main/tools/install-update.sh
sh /tmp/install-update.sh
```

The script leaves `MiSTer.ini` untouched and does not reboot. It rejects an unknown installation,
a different same-version library, and a downgrade. `--rollback` restores the boot files saved
during a legacy migration; routine GUI version changes use the optional pin above. A
development build must first be restored with `tools/deploy.sh --restore-release` from a local
checkout. Legacy `.rollback/` bundles and old versioned libraries are retained.

If the confirmation warns of a database format change, choose **Settings → Build game database**
after restarting. Updating never silently deletes or rebuilds the database.

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

`make package-release` creates the five required GitHub assets from the release build and
the patched main binary after ELF, ABI and symbol checks. Publish those assets together under
the matching `vX.Y.Z` tag only after the hardware release checks documented in `VNEXT.md`.

For local development, build the separate dev artifacts and deploy them with:

```sh
DEVICE=root@<mister-ip> make deploy
DEVICE=root@<mister-ip> tools/deploy.sh --restore-release
```

The first command backs up the installed release in `.dev-backup/`, selects an unused
numeric library slot, sets `.development-build` and restarts MiSTer. The second verifies the
backup, restores the release, removes registered dev slots and restarts. The backup is retained
and a later dev deploy reuses it only if the active release files still match its hashes. If only
the pin mode changed, it refreshes the backup before stopping the GUI. Updates are blocked
while `.development-build` exists. Both commands need a complete release installation as a
starting point. The full build instructions are in [README.md](../README.md#building-from-source).

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
| Loader error 70–79 on the terminal | The wrapper writes `logs/loader.log` and stops relaunches. Check the installed versioned libraries and any optional `mister-pats-gui.so` pin. Choose a working version or restore a legacy migration from its complete rollback bundle, then restart manually. |
| Black screen, device responds to SSH | Check `/media/fat/mister-pat/logs/crash.log` and the GUI/launcher processes over SSH; reboot to restore the boot path. |
| Blinking cursor in the top left | The console is in text mode. `ssh root@<mister-ip> 'chvt 1'`, or reboot. |
| Nothing on a DVI monitor | Should be detected automatically. If not, set `dvi_mode=1` in `MiSTer.ini`. Note that DVI mode carries no audio. |
| Controller moves two positions per press | A mirror input device is being read as well. It should be filtered by name; check with `grep input:` in the GUI's output. |
| Menu button does not open the OSD | The GUI is running with `--exclusive`. Start it without that option. |
| No games listed | The database has not been built. Settings → Build game database. |
| No box art | The scraper has not run yet, or has not reached that system. Settings → Prepare box art. Existing JPEG/PNG covers remain usable without BMP variants. |
| No Arcade tab, or an Arcade tab with few games | The tab only appears when at least one Arcade game passes the check, and only games that pass are listed: core installed, ROM zip found, every CRC right. Settings → Manage Arcade → Arcade Games shows why each one fails. Then rebuild the database. |
| Systems show plain tiles instead of console pictures | Choose Settings → Download System-Icons or rerun the setup script in step 2. Existing PNGs work as a fallback. |
| Text looks like a blocky, all-caps placeholder font | Space Grotesk could not be loaded, so the built-in fallback typeface is drawing. Rerun the setup script with network access to fetch the font files. |
| Games on a CD-based core do not start | The core expects a different loader slot. See the table in [POC.md](POC.md) and `assets/systems.conf.example`. |

A reboot is always the safe way back, and removing the two INI lines always returns the
device to its stock behaviour.

## Console Mode is not required

All four things the GUI needs are covered without it:

| What | Where |
| --- | --- |
| System catalogue | **built here**, `/media/fat/mister-pat/gamesdb/catalog.tsv` |
| Game index | **built here**, one `<System>.tsv` per system |
| Box art and backgrounds | **prepared/fetched here** — Settings → Prepare box art |
| Fonts | Space Grotesk and its license are downloaded by the setup/update script; the built-in fallback remains available |

Console Mode itself never runs once this GUI is installed: `main=` sends the boot path to the
patched binary instead. The GUI uses its own Space Grotesk files, installed alongside it.
