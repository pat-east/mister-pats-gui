# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

- Console Mode is no longer required for anything. The typeface (Akrobat) is now a one-time
  manual download straight from Fontfabric — see `INSTALL.md` — since its licence does not
  allow redistributing the font file itself; the built-in fallback typeface is used until you
  do.
- Removed a font fallback path that pointed at a desktop-Linux location never actually present
  on the MiSTer. The one guaranteed fallback is now clearly the built-in bitmap typeface,
  which needs no file at all.
- `INSTALL.md` and `README.md` brought back in line with the above, and with the box art
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
