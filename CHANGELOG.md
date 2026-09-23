# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
