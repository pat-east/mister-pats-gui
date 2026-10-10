# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.6] - 2026-10-10

This release exercises the new installer and versioned GUI package ahead of 0.5.0.

### Added

- **Versioned GUI update path** — a stable loader selects a versioned GUI library; Settings can check,
  confirm, download and install a verified release in the background. The SSH installer shares
  the release format and keeps rollback bundles. A separate dev deploy can restore its saved
  release. The 0.4.6-to-0.5.0 update will be validated on MiSTer hardware.

### Changed

- **Interface typeface** — replaced Akrobat with Space Grotesk. The install/update script
  downloads the font files and their SIL Open Font License; the existing font fallbacks remain.
- **Grid box-art scrolling** — artwork file checks and loading run on the image worker instead
  of the render path, keeping navigation responsive as new tiles enter view.
- **Systems tile backgrounds** — image-backed tiles use black behind their BMP icons for a
  more consistent edge and background color.

### Fixed

- **Verified HTTPS on MiSTer** — the SSH installer passes the available CA bundle explicitly
  to `curl`, matching the GUI downloader.
- **Repeated dev deploys** — a release backup left by `--restore-release` can be reused after
  its link target and all active files have been checked against the saved hashes.
- **Wi-Fi startup checks** — the automatic GitHub check waits for a network address, DNS and a
  successful HTTPS response before showing "Checking GitHub"; network failures retry quietly
  until the connection returns or the check is switched off. A published release older than the
  selected GUI reports no newer version instead of an asset error.
- **Library updates** — routine updates publish only a verified versioned `.so`. The loader
  selects the newest installed version unless an optional symlink pins another one; Settings
  shows the running and selected versions. With a pin, update checks compare against its
  selected version even when a higher library remains installed.

## Planned — 0.7.0

- **Usability improvements**, especially the Arcade tab's entry points and individual game
  items.
- **Group language and revision variants of one game.** Show a single entry for a title with
  regional or revision variants, while preserving each ROM and letting the player choose which
  version to launch.

## [0.4.0] - 2026-10-09

### Added

- **Show box art setting** — hides game covers and background artwork across Home, Games,
  Favorites and Arcade, making it possible to compare menu responsiveness without artwork
  loading and drawing.
- **View-sized BMP box art** — *Prepare box art* creates uncompressed variants for Home
  (178 px), Arcade (148 px), Grid (245 px), Boxart small (128 px), and List detail (up to
  689×624 px). The GUI loads the matching variant without resizing it during loading; JPEGs
  remain as fallbacks. The variants preserve each cover's aspect ratio. At the largest bounds
  they can add up to about 1.7 MB per cover.
- **Artwork loading worker** — a single bounded worker decodes requested covers while the
  interface continues to handle input. Its queue holds at most 32 requests; the cache is
  limited to 120 entries and 32 MiB. When a BMP variant is absent, existing JPEG/PNG
  artwork remains available as a fallback.
- **System icon download** — Settings can fetch missing BMP icons from the public repository;
  existing PNG icons remain usable until their BMP counterparts are present.
- **Install/update script** — downloads the latest release and missing system icons, creates
  application directories, and prints needed `MiSTer.ini` changes without editing the file.
- **Crash screen and log** — fatal GUI errors pause automatic relaunch and display a restart
  instruction; details are appended to `logs/crash.log` when available.

### Changed

- **Game presentations** — removed Boxart large and Compact. List, Grid, and Boxart small
  remain; saved defaults for removed presentations fall back to Grid.
- **Splash duration and icon loading** — reduced the initial wait from two seconds to one.
  The progress bar advances toward 25% during that second, then advances as system icons are
  loaded sequentially; the splash remains until loading is complete.
- **System icons** — loaded from opaque BMP files, avoiding PNG decompression and per-pixel
  source-alpha blending. The repository assets are 163×163 for the normal 1080p tile;
  PNGs remain the fallback when a BMP is absent.
- **Box-art ETA** — uses elapsed time since the scrape began and the displayed
  processed/total game counts to project a completion clock time in the MiSTer's time zone.
  Different amounts of work per game can still move the estimate during a run.
- **Development deploy** — stops the GUI launcher before replacing binaries, then reboots,
  intended to resolve the earlier two-attempt handoff. Its complete run is not yet verified;
  the latest attempt was interrupted when the device was powered off.

### Fixed

- **Home screen could start without its games drawn** — the initial contents are now painted
  explicitly after startup or refresh, without requiring a controller action to trigger a
  screen update.
- **Static-thread startup crash** — the build now explicitly links the pthread condition
  functions needed by libstdc++'s static-initialization guard.

### Performance findings

- **Box art disabled:** the user reports that browsing feels very snappy, with good to very
  good response.
- **Menu responsiveness:** Home and warm Systems were already fluid earlier in development;
  the user now reports that the current menus look and feel very good overall and considers
  smoother, faster menu control complete for 0.4.0.
- **Earlier PNG/BMP icon benchmark:** with 300×300 PNGs and 160×160 BMPs, warm decode plus
  scaling was 12.4–12.8% faster for BMP. Canvas drawing differed by only 0.6–0.7%, within
  measurement noise; the first uncached pass showed no consistent gain. The newer 163×163
  native-size path has not yet been benchmarked.

### Device observations

- **Box-art preparation on MiSTer:** the 11,502-entry run completed. The user reports that
  browsing feels better and has approved the current version for commit.
- **ETA update:** built locally and manually deployed after that run. Its accuracy during a
  subsequent full scrape has not yet been observed.

## [0.3.0] - 2026-10-07

### Added

- **Networking details in Settings** — shows the current Ethernet and Wi-Fi IPv4 addresses.
- **A box art miss audit tool** — compares missed titles with the Libretro thumbnail index.

### Changed

- **Arcade group browsing** — manufacturer and category previews are alphabetical and use
  simple text entries with game counts instead of game-style tiles.
- **Box art matching** — recognizes more regional, subtitle, series-name, and catalogue-title
  variations while leaving ambiguous matches unresolved; Arcade titles retain revision-aware
  exact matching.
- **Favorites** — adding or removing a favorite now requires holding X for two seconds, with a
  progress indicator and cancellation if the selection changes.
- **Font fallback sizing** — the built-in bitmap typeface uses smaller, proportional sizing;
  game labels and details adapt when the external TrueType fonts are unavailable.
- **Status-bar spacing** — separates the date from the clock and the IP address from the
  network label.
- **Console output** — removes routine startup and shutdown messages while retaining the
  persistent debug log and error output.

### Fixed

- **System files mistaken for games** — known boot media, BIOS entries, and MiSTer utilities
  are excluded from game listings, favorites, and scraping.
- **Controller diagrams** — corrects the displayed physical positions of the X and Y buttons.

### Verified

- **Font fallback on MiSTer** — tested with external font files absent; the interface remains
  usable with the built-in typeface. The installed font files were restored after the test.
- **Updated box art scraper on MiSTer** — a scrape was completed; see [BOXART.md](docs/BOXART.md)
  for the available audit data and its limits.

## [0.2.0] - 2026-09-30

Arcade. Until now this GUI only knew systems that have one directory of games and one core;
MiSTer's arcade library has neither, and needed its own treatment all the way from the scan to
the box art. Design, measurements and the real-hardware case study behind it are in
[ARCADE.md](docs/ARCADE.md).

### Added

- **Arcade as a system** — `Systems -> Arcade`, with favourites, history, the Games tab and all
  five presentations like any other. A game is launched by handing its `.mra` straight to
  `load_core`; MiSTer's own arcade loader does the rest.
- **A library of the games that actually work.** The game database records an Arcade game only
  when its core is installed, its ROM zip is found and every part's CRC matches — the check
  MiSTer itself effectively makes, using MiSTer's own search order (every USB drive first, the
  SD card last) rather than a guessed one. Each game is listed once, however many drives hold a
  copy. A full copy on the card and another on a drive is normal, and so is a library where
  most of the catalogue has no ROMs behind it.
- **An Arcade tab** with three rows — Games, Manufacturers, Categories. The cursor starts on a
  row's title, and confirming it opens the whole list; Right steps onto the tiles, where a game
  starts and a manufacturer or category opens just its games. Back from a tile returns to the
  title first. Settings -> *Show Arcade menu item* turns the tab off; it only appears when the
  library has Arcade games in the first place.
- **Manufacturer and category lists** in the game database, written by the same scan: more
  files in the row format a system's own list already uses, so a group opens in the ordinary
  game list with nothing Arcade-specific in it. Spellings that differ only in case or spacing
  ("Beat 'Em Up", "Beat 'em Up") are one group. A game with no manufacturer or category is
  filed under "(Unknown)" / "(Uncategorized)", not left out.
- **Box art for Arcade**, from the `MAME` set on the libretro thumbnail server, matched by the
  whole title — brackets included — so each ROM revision gets its own cover. Saved in each
  drive's own `_Arcade/media`, next to the games on that drive. Box art only for now; whether
  MAME's gameplay screenshots make a pleasant background has not been looked at.
- **Settings -> Manage Arcade -> Arcade Games**, a diagnostic table of every `.mra` on every
  drive: whether its core is installed, its ROM zip found and its CRCs correct, and which zip
  it asks for. Confirming a row launches the game. This is the screen that explains why a
  game is *missing* from the library above.
- **A distinct Mega CD icon.** It used to be the same gamepad as Mega Drive, because the icon
  set it came from ships the same picture under both names; Mega CD now has the pad with a disc.
- **`--press`**, a command-line option that feeds button presses to the interface before
  anything else (`--press down,confirm,wait:30`), so a screen several presses deep — the
  controller input test, say — can be captured with `--dump` without holding a controller.
  A capture no longer takes the console from a GUI that is already running.
- **Host-side tests** for the new parts: zip central-directory reading, `.mra` parsing and
  both lookups, the Arcade scan end to end against scratch volumes with real zip fixtures, and
  the thumbnail matching against the server's real names.

### Changed

- The game database format is now version 4. The first start after updating asks for a
  rebuild; until then the GUI falls back to whatever Console Mode left behind, as it always
  did without a database.
- A machine holding only arcade games can now be scanned; the scan used to stop at "no game
  directories found next to an installed core".

### Fixed

- The opt-in update check announced "v0.1.8 available" to someone already running a later
  version: it compared the release tags for being different, not for being newer. It now
  compares them number by number.
- Arcade cores were only found when named `Arcade-<name>`, which almost none are — MiSTer also
  accepts the bare `<name>`, followed by `.` or `_`. Every core outside the one the case study
  began with therefore showed up as not installed.

## [0.1.8] - 2026-09-27

### Added

- **Controller management, in Settings → Controllers** — this project's largest feature to
  date, and its first that writes files outside its own `mister-pat/` directory. Fully
  controller-first, no keyboard required anywhere in it:
  - **List** connected controllers, showing the vendor/product MiSTer itself reports (not
    `lsusb`'s), whether a mapping/deadzone is already set, and whether two pads currently
    share one mapping.
  - **Input test** — every button, D-pad direction, trigger and stick shown live, drawn as a
    schematic pad rather than a bare word list. Read-only: nothing here writes anything, and
    no button does anything but light up. Leaves after holding Start 5 seconds, or on its own
    after 10 seconds with no input at all — for a sub-device (a DualShock's own "Motion
    Sensors" or "Touchpad" node, say) that has no Start to hold at all.
  - **Deadzone** — reads and writes MiSTer's own `deadzone=` line in `MiSTer.ini`. Every write
    is preceded by a timestamped backup into `/media/fat/mister-pat/mister-ini-backups/`
    (capped at 30), then an atomic replace — this file must never be allowed to end up broken.
  - **Button mapping wizard** — writes MiSTer's own binary `input_<vid>_<pid>_v3.map` file.
    Start is captured first and doubles as the wizard's own safety net for the rest of the
    run (hold 5s: abort; press once: skip a button that mis-fired; 1 minute idle: skip a
    button this pad does not have, with a visible countdown). Each step draws a schematic pad
    diagram highlighting the one relevant button, direction or stick, rather than a bare
    letter whose physical position depends on which pad convention you grew up with.
  - **Bluetooth pairing** — only offered where an adapter is present; reuses stock MiSTer's
    own `btpair` helper rather than reimplementing pairing, and offers the button-mapping
    wizard right away for a newly paired, still-unmapped pad.
  - See [CONTROLLER.md](docs/CONTROLLER.md) for the full design and, further down, the trail of
    real hardware findings below that shaped it along the way.

### Fixed

Every one of these was found by actually running the wizard against real pads, not by reading
source alone — see CONTROLLER.md for the full trace on each:

- **MiSTer's own A/B/X/Y `.map` slots follow SNES-style positions** (right=A, bottom=B, top=X,
  left=Y), not the Xbox letters most modern pads are physically labelled with. Writing
  straight into the same-named slot silently swapped X and Y (and A and B) on a real PS4 pad.
- **The OSD "confirm" slot and the OSD "open from inside a game" combo are unrelated slots.**
  An earlier assumption fed one captured button into both, which overrode a pad's correct A
  button mapping with an unrelated one — "confirm with A" stopped working, only the actual
  Menu button did. Now three separate, correctly-scoped prompts, matching stock MiSTer.
- **A D-pad-only pad (a Retro-Bit Saturn-style pad) reports its D-pad as a hat axis, not as
  keys** — the wizard's capture never saw it at all. Now reproduces the synthetic code
  MiSTer's own `input.cpp` builds for exactly this case.
- **Button/axis capture was level-triggered, not edge-triggered** — a button held a moment too
  long (including the very button used to confirm the previous step) could silently answer
  several steps in one stampede. Every capture is now edge-triggered, locked from answering a
  second step once it has answered one, and Start's own trailing release is specifically
  guarded against being misread as a tap-to-skip on the step right after it.
- **Stick-axis capture was far too sensitive**, and measured against the wrong thing (a stick's
  full span, which a resting stick can only ever travel half of). Now requires 75% of the
  axis's own half-range, measured from wherever it sat when the step began.
- **The input test's 10-second idle timeout never fired** for a trigger or a motion-sensor
  axis — first because "distance from centre" reads a trigger (which rests at one end, not
  the middle) as permanently deflected, then, after that fix, because a live sensor keeps
  drifting even lying still, which eventually outran a one-time rest snapshot anyway. Fixed
  with a rest baseline that continuously chases the current reading while nothing is
  happening, and freezes the moment something is.

### Verified

- **PS4 controller (DualShock 4)** — wired and over Bluetooth; pairing survives a reboot with
  no re-pairing needed.
- **Retro-Bit Sega Saturn pad** — works well once switched to X-Input mode.

## [0.1.7] - 2026-09-26

### Fixed

- **A pad's D-pad could move the selection twice per press.** Some controllers — an Xbox 360
  pad through a wireless receiver, among them — report the D-pad both as a hat (`ABS_HAT0X`/
  `ABS_HAT0Y`) and as separate `BTN_DPAD_*` key events for the same physical press; acting on
  both moved the cursor twice. Each device is now probed for a hat once, at connect time, and
  `BTN_DPAD_*` is ignored on any device that has one.

### Added

- **Settings → "Start MiSTer Core"** exits this GUI and hands the screen back to MiSTer's own
  stock menu — the place a controller actually gets configured (`Define joystick buttons`),
  and otherwise not reachable from here at all if the usual way in (a keyboard's F12, or a
  case's own hardware OSD button) does not work for a given setup. A matching change to the
  patched main binary stops it from relaunching the GUI a few seconds later; a reboot is what
  brings it back.

### Verified

- **Xbox 360 Wireless Receiver for Windows, with two Xbox 360 controllers paired to it** — now
  tested and working, including the D-pad fix above. A drifting stick on this pad is a Stock
  MiSTer `deadzone=` setting, not something this GUI touches; match it against the vendor/product
  MiSTer itself reports for the pad (`cat /proc/bus/input/devices`), which can differ from what
  `lsusb` shows for the receiver.

## [0.1.6] - 2026-09-24

### Fixed

- **A rescan from the interactive wizard could hang forever at "Writing the catalogue …".**
  `LibraryScan` only kept stepping itself while its own `running()` said so, and that check
  did not count the Writing state — so the moment scanning finished and state moved to
  Writing, nothing ever drove it forward again, and the catalogue write it exists to run
  never happened. Likely never caught before because every earlier scan this project's own
  testing did went through the `--scan` command-line path instead, which drives its own loop
  and was never affected. Writing the catalogue is also no longer one large blocking call:
  it happens one system's line at a time, the same paced way scanning itself already did,
  with the progress bar now showing real numbers instead of just the moving dot.
- **A second rescan could fail outright** with "cannot move the previous database aside".
  The box art scraper's index cache lived inside `gamesdb/`, the exact directory `GameDatabase`
  treats as one disposable, atomically swapped unit — so it rode along into `gamesdb.old` on
  the first rebuild, and cleanup only knew about the `.tsv` files that actually belong there.
  Left behind, it meant `gamesdb.old` could never truly be emptied, and every rebuild after
  the first failed the same way. The cache now lives next to `gamesdb/`, not inside it, and
  cleanup no longer stops at files it does not recognise.
- **Disc-based systems could report games that do not exist.** A CD system's BIOS commonly
  sits in region folders right next to the actual games — `MegaCD/Europe`, `/Japan`, `/USA`,
  each holding nothing but `cd_bios.rom` — and a scan counted every folder under a disc-based
  system as a game without looking inside it, so these were catalogued as three MegaCD "games"
  on a library that has none. A folder now has to actually contain a disc image to count.
- Favourites on Home were listed in whatever order they happened to be added in, rather than
  alphabetically — the order itself carries no information the way Recently played's does, so
  finding one on a list that has grown got harder for no reason as it grew.

### Added

- **An opt-in check for a newer release**, Settings → *Check for updates on GitHub* (off by
  default — the only network access in this project that is not something already asked for
  in the moment, like the scraper is). Runs once, briefly, a few seconds after starting; a
  newer version found shows next to the version number in the corner rather than as a
  separate notification.

## [0.1.5] - 2026-09-24

### Added

- **Several external drives are now supported properly**, end to end. A system's directory
  used to be baked into the database as an absolute path at scan time; a drive moving to a
  different `/media/usbN` after a reboot — which MiSTer does not promise will not happen —
  left it stale even though every individual game's own path re-resolved correctly around it,
  so box art (and, for anything not itself re-resolved live, launching) silently broke for
  that drive alone. A system's directory is now stored and resolved exactly the way a game's
  path already was: root id plus what is relative to it, re-resolved at load time rather than
  trusted as a fixed string. Verified on hardware against a real two-drive setup, including
  unmounting and remounting both volumes swapped with no reboot in between.
- **The ConsoleMode cache index is also multi-drive-safe now.** It previously resolved every
  cached path against `/media/usb0` unconditionally, so a second attached volume's games would
  get paths pointing at the first one and fail to launch. Each cache file is ConsoleMode's own
  record of one specific volume, named after that volume's filesystem serial or USB signature
  — nothing about the name says which mount point it belongs to, and trusting it would repeat
  the exact mistake the drive-resolution fix above exists to avoid. Each file is instead
  matched to a mount point by checking that a spread sample of the paths it records are
  actually there, the same principle applied twice.
- **A reusable error modal** — a centred, dismissible dialog for a failure the user needs to
  actually notice, as opposed to a passing status line in the bottom bar. Wired up first for a
  game that fails to start (drive not responding, disc image missing, no core for the system);
  meant to be reached for again the next time something needs the same treatment.
- **A default presentation, settable from Settings** — Grid, List, Boxart large, Boxart small
  or Compact, applied to Systems, Games and Favorites immediately and remembered afterwards.

### Fixed

- A root's fallback search (see 0.1.4) checked which mount points existed only once, at
  process construction — before a slower second drive had necessarily finished enumerating.
  It now re-scans immediately before that search runs, rather than trusting a snapshot that
  could be older than the drive it was looking for.

### Changed

- The database format bumped to account for the above (a system's directory is no longer an
  absolute path on disk). An existing database is detected as outdated and rebuilt rather than
  misread — expect one rescan after updating.

## [0.1.4] - 2026-09-23

### Fixed

- **Box art (and occasionally whole games) could silently point at the wrong drive for an
  entire session**, most visible as every tile under Systems and the Games tab showing no
  artwork at all while Home and Favorites looked fine. Root cause: `GameDatabase` remembers
  which mount point each drive was on by recording one game directory as a "probe" and
  checking it is still there. Right after boot, a USB drive that has not finished mounting
  yet looks exactly like a drive that is not there at all — and the fallback search this
  triggered would go looking for that same probe directory on *every* mount point, including
  `/media/fat`, where a not-yet-installed system's own empty scaffolding folder (created by
  Console Mode for every known core, whether or not anything lives there) could satisfy the
  same directory-exists check. A whole drive's worth of games would then silently, permanently
  resolve against `/media/fat` instead — right paths in the catalogue, wrong prefix put in
  front of them, for the rest of the session. Fixed two ways: the original recorded location
  now gets real time to finish mounting (up to ~12 s, paid only once, only by a root that
  genuinely is not ready yet) before anything looks elsewhere for it, and the fallback search
  itself now requires a candidate to actually have a handful of files in it, not just exist.
- `Image::scaledTo()` had no same-size fast path — asking for the size an image already is
  still ran the full per-pixel bilinear resample, measured on-device at ~34 ms for nothing.
  Getting the same pixels back now costs a copy instead.

### Added

- **A persistent, leveled log** at `/media/fat/mister-pat/logs/debug.log` (INFO / WARN /
  ERROR, timestamped, rotates itself at 512 KB) — startup info, library and database load
  results, root-drive resolution problems, scan and scraper progress and failures. Lives on
  the SD card rather than in `/tmp`, specifically so it survives the reboot that a boot-time
  bug is likely to be followed by. Nothing personally identifying is ever written to it —
  local file paths under your own game/artwork directories, nothing else — so a report can
  come with a copy of this file attached.
- **A startup splash** — briefly shows this project's own logo (embedded in the binary, not a
  loose asset file) with a progress bar, before anything touches a game drive. While it is up,
  every attached USB slot (`usb0`–`usb5`) gets a cheap, spaced-out directory read, which gives
  a slow-to-enumerate drive a head start on being ready by the time the real work begins.
- **A second, smaller artwork variant** (`<name>-sm.jpg`, longest edge 300px) written by the
  scraper alongside the existing full-size copy, generated from the same single download.
  Every presentation except Boxart large and the List view's detail panel — which is most of
  the time a tile is actually on screen — decodes a third of the pixels for it. Already-scraped
  libraries get the small copy filled in locally, from the full-size file already on disk, no
  network access needed.

### Changed

- **Opening a system or the Games tab no longer resolves artwork for anything but what is
  about to be drawn.** Every entry starts as a stub — path and display name, no `stat()` calls
  — built for the whole 10,500-game library in one go, in a handful of milliseconds. The
  expensive part (finding the actual box art and background files, up to four `stat()` calls
  each) now happens per tile, the moment it is about to be rendered, with a small time budget
  per frame so a letter jump across a library that has never scrolled there before still
  cannot turn into a hitch.



### Added

- A shared `LoadingIndicator` — title, progress bar, status/count line, a moving dot so a big
  directory never looks stuck — now used everywhere a wait is unavoidable: the database scan,
  the box art scraper, opening a large system, and the "Games" tab. One look instead of a
  different ad-hoc message per screen.

### Changed

- **Opening a system or the "Games" tab no longer blocks.** Resolving a game's artwork paths
  costs real `stat()` calls — thousands of them for a big system, tens of thousands for the
  whole library at once — and doing all of it before the first frame could render is what made
  opening NES (2,882 games) or Mega Drive (976) look like a hang. It now happens a slice at a
  time across frames, the same way image decoding already did.
- **"Games" reveals entries as they resolve, already in the right order**, rather than showing
  a loading screen until every system has been merged and sorted. The display name a path will
  get costs no I/O — see `Library::nameFor` — so the whole list can be put in its final order
  up front, cheaply, before any of the expensive work starts. The header's count reflects the
  final total immediately too, instead of climbing as more of the list resolves.

### Fixed

- The loading indicator's moving dot was pinned to the bottom of its drawing area, which
  collided with the count line whenever a screen (Systems, Games) didn't also have a status
  line above it. It is now placed a fixed distance below whatever text was drawn last.
- `SystemsScreen`'s new loading panel never cleared the canvas before drawing over it — each
  frame's text and dot painted on top of the last instead of replacing it, smearing into an
  unreadable mess while a scan was in progress.

## [0.1.2] - 2026-09-23

### Fixed

- **CD-based systems could not load a disc at all** — PlayStation, Saturn, Mega CD,
  TurboGrafx-16 CD and Neo Geo CD. The table that picks the MGL disk-slot type for these
  systems was compared against the system name case-sensitively (`"PLAYSTATION"` against the
  actual `"PlayStation"`), so it silently never matched; every one of them fell back to the
  generic ROM file slot instead. The core would start and even boot its BIOS, but never
  received the disc as a mount event, so nothing loaded. The comparison is case-insensitive
  now, and `"Sega CD"` was corrected to this project's own `"Mega CD"` naming while in there.
- Returning from a detail view (or any other full redraw) could leave the Systems or Home tab
  blank, or showing only a partial, stale set of tiles. Both screens skip their own repaint
  when nothing they track has changed, as an optimisation — but neither knew the app had
  already wiped the canvas out from under them for a reason of its own. They are told now.
- A failed artwork decode was cached as failed forever. A drive that had not finished
  settling right after boot could turn real, readable box art into a permanent blank for the
  rest of the session; failures are retried after a few seconds instead.
- Launching a disc-based game could silently hand the core a folder instead of the actual
  disc image if the drive briefly did not respond at the exact moment of launch — most likely
  right after opening a large system's list, itself a burst of reads. It now retries for a
  few seconds and fails with a visible message rather than starting a core with nothing
  behind it.

## [0.1.1] - 2026-09-23

### Changed

- Console Mode is no longer required for anything. The typeface was a one-time manual download
  under its original license restrictions; see the installation instructions. The built-in
  fallback typeface is used until the external font files are installed.
- Removed a font fallback path that pointed at a desktop-Linux location never actually present
  on the MiSTer. The one guaranteed fallback is now clearly the built-in bitmap typeface,
  which needs no file at all.
- `docs/INSTALL.md` and `README.md` brought back in line with the above, and with the box art
  scraper being the GUI's own rather than Console Mode's.

## [0.1.0] - 2026-09-22

### Added

- Initial public release: a tiled, box-art-first frontend for the MiSTer FPGA, written in
  plain C++ for the device's own Linux side.
- Its own game database — a scan builds a catalogue of systems and a per-system game index,
  so opening a console afterwards reads one small file and nothing else.
- Its own box art scraper, from Settings, pulling from the libretro thumbnail server.
- Five presentations per system (list, large/small box art, grid, compact), with letter-jump
  navigation on the shoulder buttons.
- A Home screen with recently played and favourites.
- *Manage systems*, in Settings — hide the systems you do not use, or hide the Games tab
  entirely for a library large enough that browsing it flat stops being useful.
- Console icons for systems without their own art.
- Boots straight into the GUI via a small patch to the MiSTer main binary, which also teaches
  it to recognise a DVI-only display even when the video mode is pinned in the INI.
