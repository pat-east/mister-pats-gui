# Arcade Support — Analysis, Design and What Was Built

Design document for the "Arcade" item on the [README roadmap](README.md#roadmap).
Written the same way [CONTROLLER.md](CONTROLLER.md) was — analysis and a proposed design before
any code changes — because Arcade genuinely does not fit the one-system-one-core model the rest
of this GUI is built on, and that needs to be settled before anything gets built. It was then
kept as the record of what got built and how it differed from the plan: sections headed
*Implemented* say what exists; the rest is the reasoning they came from, marked where it has
since been carried out. Everything
below that cites MiSTer's own source was confirmed by reading it
(`third_party/Main_MiSTer/file_io.cpp`, `support/arcade/mra_loader.cpp`, `input.cpp`); the case
study was confirmed live, on real hardware, in the session this document was written from.

**Status, as of 0.2.0.** Built:

- the [diagnostic table](#implemented-the-diagnostic-table), which also launches a game;
- [Arcade as a real system](#implemented-arcade-as-a-real-system) — a library of only the games
  that work, each once, with manufacturer and category lists in the game database;
- the [Arcade tab](#implemented-the-arcade-tab);
- [box art](#implemented-box-art) from the `MAME` thumbnail set.

What has been seen on a real MiSTer: the diagnostic table and launching from it; the scan, the
database it writes and the list it gives; the Arcade tab and a completed run of the updated box
art scraper. The later box-art matching audit is documented in [BOXART.md](BOXART.md); its
checked-in miss list is from September 30, 2026, and does not include fetched/skipped/missing
totals from the subsequent device run.

Not built: telling HDD/CHD-based games apart ([Requirement 4](#requirements)), Manage Cores /
Manage MRAs, DIP-switch editing, and backgrounds for Arcade. See [Open questions](#open-questions)
and [Non-goals](#non-goals-for-v1).

- [Case study: Killer Instinct](#case-study-killer-instinct)
- [Why Arcade doesn't fit the existing model](#why-arcade-doesnt-fit-the-existing-model)
- [Facts this design relies on](#facts-this-design-relies-on) — the MRA format, the two
  independent root lookups that caused the case study's own bug, RBF version selection, HDD/CHD
  games, the command FIFO's vocabulary
- [What already existed in this codebase](#what-already-exists-in-this-codebase) — before any
  of this was built
- [Requirements](#requirements) — and which are met
- [Implemented: the diagnostic table](#implemented-the-diagnostic-table) — Settings -> Manage
  Arcade -> Arcade Games
- [Implemented: Arcade as a real system](#implemented-arcade-as-a-real-system) — the scan, the
  database lists, launching — and [the tab](#implemented-the-arcade-tab) and [box
  art](#implemented-box-art) built on it
- [Proposed architecture](#proposed-architecture) — phased: a lean v1, then metadata
- [Scraper and box art](#scraper-and-box-art) — checked live against the real server: which
  platform, a real wrong-cover bug in the existing matching, and where scraped art has to live
- [Decision: a dedicated Arcade tab](#decision-a-dedicated-arcade-tab) — Arcade as a system
  *and* its own tab, three browsable dimensions, with mockups
- [Decision: extending "Build game database" for
  Arcade](#decision-extending-build-game-database-for-arcade) — the mechanism behind the tab
  above: manufacturer/category as more files in the same row format, not new data, with flow
  diagrams
- [Where cores and MRAs would come from, if this ever fetches
  them](#where-cores-and-mras-would-come-from-if-this-ever-fetches-them) — how
  `Update_All_MiSTer` actually works
- [Decision: Manage Cores and Manage MRAs](#decision-manage-cores-and-manage-mras) — starting
  from the ROMs you own rather than the whole catalog, the shared sync mechanism, and the
  complete end-to-end path, with a mockup
- [Non-goals for v1](#non-goals-for-v1)
- [Open questions](#open-questions) — most now answered, with where

## Case study: Killer Instinct

The session this document came from started as plain troubleshooting: Killer Instinct (Arcade),
core installed, would not start. The MiSTer was reachable over SSH
(`192.168.64.128`), which made it possible to actually find out why instead of guessing — and
the answer turned out to be two unrelated, genuine bugs, not one.

**Background.** Killer Instinct's real arcade board stores its music and sound on an IDE hard
drive, not mask ROMs — unusual for MAME-era arcade hardware, but real. The MiSTer core (by Keith
Conger, [MiSTer-devel/Arcade-KillerInstinct_MiSTer](https://github.com/MiSTer-devel/Arcade-KillerInstinct_MiSTer),
first booting August 2026, still under active development at the time of writing) reflects that:
its MRA ships the program/graphics ROMs the normal way, but the hard drive has to be supplied
separately, converted from MAME's `.chd` into a raw sector image with `chdman extracthd`, and
manually selected once from the OSD on first launch.

**Everything about the installation was actually correct.** Core RBF byte-identical to the
official release. ROM zip CRCs byte-identical to what the MRA asks for. The CHD correctly
converted to a raw `.img` the right size. And it still would not start — a message flashed for
under a second and the menu came straight back, too fast to read.

**Bug 1: the core loader was searching the wrong drive.** Triggering the same load over SSH
(`echo "load_core /media/fat/_Arcade/Killer Instinct (v1.5d).mra" > /dev/MiSTer_cmd`, repeatable
without touching a controller) made it possible to catch the error. It named
`/media/usb0` — a path nothing had pointed at. The MiSTer had a USB drive mounted
(`/media/usb0`) holding an independent, fully correct copy of the same library. The **ROM zip
search always checks mounted USB drives before the SD card**, regardless of which drive the
`.mra` itself was loaded from — see [Facts this design relies
on](#facts-this-design-relies-on) below for exactly why, confirmed by reading the actual source
rather than guessed from symptoms. Once the files on both drives were made consistent, this
stopped mattering — but for a library split unevenly across two drives, as this one apparently
was, it would not have.

**Bug 2 (context, not separately hit here): the HDD mount is a real manual step, not a bug, but
an easy one to mistake for a broken install.** The core's own README says the raw image "must be
selected" from the OSD on first launch, and does not auto-mount from the `.mra` the way a CD
image does from an `.mgl`. Nothing in this codebase's launch path today does that, or could,
without a materially bigger feature — see [HDD/CHD-based games](#hddchd-based-games-no-automated-mount-found)
and [Non-goals](#non-goals-for-v1).

Neither of these would have been found by staring at file listings. The CRCs matched. The RBF
matched. The only way to actually see what MiSTer itself was doing was to trigger the exact same
load it runs internally and watch what it did — which is also, not coincidentally, close to what
a real implementation of this feature needs to be able to explain to a user when a game will not
start.

## Why Arcade doesn't fit the existing model

Every system this GUI already handles — `SystemCatalog`, `CoreIndex`, `Library` (`src/SystemCatalog.h`,
`src/CoreIndex.h`, `src/Library.h`) — is built on one assumption: **one directory holds one
system's games, and one core (one `.rbf`) plays all of them.** `GameSystem::core` is a single
path; `Launcher::launchGame` builds one `.mgl` naming that one core plus a ROM path and hands it
to `load_core`.

Arcade breaks that assumption in every direction at once:

- **One folder (`_Arcade`), thousands of unrelated games.** `_Arcade/*.mra` is not "the games of
  one system" the way `PSX/*.chd` is — it is MiSTer's entire arcade library, every manufacturer
  and every decade, in one flat directory (Console Mode's own section-file model, which this
  project's `Library.cpp` already partly leans on for other systems, has no real equivalent
  here).
- **Each game names its own core.** An `.mra`'s `<rbf>` tag is the core for *that game only* —
  sometimes several games share one core (Killer Instinct and Killer Instinct 2 both use
  `KillerInstinct.rbf`), sometimes a game has its own. There is no single "Arcade core" to put in
  `GameSystem::core`.
- **A game is a small XML descriptor, not a ROM file.** An `.mra` describes how to assemble the
  actual ROM from a MAME-format zip (which is not shipped with the core, has to be sourced
  separately, and can be the wrong revision) — the file this GUI would list is a recipe, not the
  game data itself.
- **Launching is a different code path entirely**, confirmed in `support/arcade/mra_loader.cpp`
  (`xml_load`): an `.mra` given to `load_core` is parsed by MiSTer's own arcade loader, not the
  generic `.mgl` reader `Launcher.cpp` already drives for every other system. There is no way to
  wrap an `.mra`'s core inside an `.mgl` and get `.mgl`'s automatic disk-mount (`type="s"`) for
  free — they are two separate, non-composable loading mechanisms.
- **Per-game settings exist that no other system has** — DIP switches, encoded per game in the
  MRA itself and persisted separately (see below) — and, for a handful of boards, an actual
  storage device (the HDD/CHD case) rather than just a ROM.

None of this means Arcade cannot fit into this GUI's existing UI (tabs, grid, favourites,
history) — see [Proposed architecture](#proposed-architecture) below, where the answer is closer
to "yes, easily" than the list above might suggest. It means the *data model* underneath needs a
second shape, not a bigger `GameSystem`.

## Facts this design relies on

All confirmed by reading MiSTer's own source this session — `third_party/Main_MiSTer` is already
vendored into this repo for the boot-patch build, so this was read directly, not looked up
secondhand.

### The MRA format

An `.mra` is XML (`<misterromdescription>`): a display `<name>`, `<year>`, `<manufacturer>`,
`<category>`, the `<rbf>` this game needs, a `<buttons>` list, an optional `<switches>` block
(DIP switches, bit-packed), and one or more `<rom index="N" zip="...">` blocks, each listing
named parts with a CRC32 MiSTer checks the actual zip contents against before trusting them. A
real, minimal example (Killer Instinct's own, `_Arcade/Killer Instinct (v1.5d).mra`):

```xml
<rom index="1" zip="kinst.zip">
  <part name="ki-l15d.u98" crc="7b65ca3d"/>
  <part name="u10-l1"      crc="b6cc155f"/>
  <!-- … -->
</rom>
```

The `zip` attribute can name more than one candidate, `|`-separated, tried in order — useful for
a set that shipped under more than one MAME romset name over time.

### Two independent root lookups — the case study's actual bug

This is the fact that mattered most this session, and it is not documented anywhere obvious —
found only by reading `support/arcade/mra_loader.cpp` and `file_io.cpp` side by side.

**The core (`.rbf`) is looked up relative to wherever the `.mra` itself was loaded from.**
`xml_load` calls `set_arcade_root(path)` with the exact path handed to `load_core`; that function
derives `arcade_root` by truncating the path at its `/_Arcade` (or other `/_*`) segment —
literally "what volume did this `.mra` come from." `get_rbf` then searches
`<arcade_root>/cores` for a file starting `Arcade-<rbfname>` **or just `<rbfname>`** (the bare
form is what nearly every real core uses, e.g. `Alpha68k_20221223.rbf`; an earlier version of this
project looked for the prefixed form alone and so found almost no cores outside Killer Instinct —
found only when the first library rebuild listed two games), each followed by `.` or `_`, and — worth its own note, see
[RBF version selection](#rbf-version-selection) below — picks the lexicographically **last**
match, which is how a dated release file wins over an undated one automatically. **This part
stays on the same drive the `.mra` was loaded from.** It is not the part that broke.

**The ROM zip is looked up through a completely different, global, drive-order search that
ignores where the `.mra` came from entirely.** The same function that resolves `mame_root`
(`findGamesDir`, `file_io.cpp`) is used everywhere in MiSTer for "find the `games/` directory
relative to *some* storage root" — CD-based console cores use the identical function for the
same reason. Its actual order, `findPrefixDir` in `file_io.cpp`:

1. `/media/usb0/games` through `/media/usb5/games`, in that order — **checked first, every
   time**, USB drive by USB drive, regardless of whether anything is actually plugged in there
2. a network share, then a CIFS mount
3. `/media/fat/games` — **checked last**

There is no configuration flag for this order and no per-core opt-out; it is a hardcoded loop in
`file_io.cpp`. So a game loaded straight from the SD card (`arcade_root = /media/fat`, core found
correctly on the SD card) can still have its ROM zip resolved against `/media/usb0/games/mame/…`
if a USB drive happens to be mounted and happens to have a `games/mame` directory at all — found,
not guessed, by triggering the exact same load over SSH and reading the resulting error, which
named `/media/usb0` even though the `.mra` used was explicitly the one on `/media/fat`.

**Worth flagging directly: this project's own `GameDatabase.cpp` has a comment that reads as
contradicting this**, and should not be read as a correction to it — the two are about different
things. `kMountPoints` there lists `/media/fat` before `usb0..usb7` with the comment "in the
order MiSTer itself would use them" — that is about the order MiSTer **numbers** mount points as
drives are detected (device enumeration), a fact `GameDatabase`'s own scan does not actually
depend on since it walks every root regardless of order. It is not, and was not written to
describe, the order `findPrefixDir` **searches** an already-mounted set of roots when a relative
directory like `games/mame` could exist on more than one of them — which is usb-first, the
opposite order. Any Arcade ROM-availability check has to use the second order, not the first —
see [Requirements](#requirements).

### RBF version selection

Also from `get_rbf` (`mra_loader.cpp`): when more than one `Arcade-<name>*.rbf` matches (a dated
one and an undated one, or two different dates), MiSTer keeps whichever filename sorts last
(`strcmp(lastfound, entry->d_name) < 0`) — an ordinary string compare, which is why a release
dated with an 8-digit suffix (`Arcade-KillerInstinct_20260924.rbf`) reliably beats an undated
copy of the same core. Confirmed directly this session: both files were present and byte-identical
on the reference device, so this did not end up mattering there, but it is exactly the same
"dated suffix wins" convention `CoreIndex::coreNameOf` in this codebase already strips for
matching purposes (`src/CoreIndex.cpp`) — worth keeping in mind if an Arcade RBF-availability
check is ever added, since it should not assume the file with the plain name is the one that
actually gets loaded.

### HDD/CHD-based games: no automated mount found

Killer Instinct's core README states plainly: "On first launch the HDD image must be selected,
this is not necessary for later launches." Looking for how that selection could be automated
turned up nothing to automate it with:

- Nothing in `support/arcade/mra_loader.cpp` references `hdd`, `hardfile`, or an IDE/disk-mount
  tag at all — the `.mra` format's own `<rom>` mechanism has no concept of a mountable disk
  image, only ROM parts.
- The generic `.mgl` disk-mount mechanism this project's own `Launcher.cpp` already uses for
  every CD-based console (`fileType = 's'`) cannot be combined with an `.mra` load — `.mra` and
  `.mgl` are two separate code paths in MiSTer's own loader (`xml_load` branches on the `.mra`
  extension specifically), and an `.mra`'s `<rbf>` cannot be wrapped inside an `.mgl` to get its
  file-mount tag for free.
- The command FIFO itself has no mount command to send after the fact either — see below.

Read together, this says the manual OSD step this session actually had to do by hand is, as far
as this investigation found, **not something this GUI can currently replicate from outside the
running core** — not a gap in this project's own code, a gap in what MiSTer exposes. See
[Non-goals](#non-goals-for-v1).

### The command FIFO's vocabulary

`Launcher.cpp` already talks to MiSTer through `/dev/MiSTer_cmd` (`kCommandFifo`). Reading the
dispatcher on the other end (`input.cpp`, around its `MiSTer_cmd` read loop) confirms the full
list of commands it understands: `fb_cmd`, `video_mode`, `load_core`, `screenshot`, `volume`.
**Nothing resembling a disk-mount or "select image" command exists there** — reinforcing the
point above. `load_core` itself takes the rest of the line verbatim as a path, spaces and
parentheses included (confirmed directly and repeatedly this session against a real filename,
`Killer Instinct (v1.5d).mra`, with no escaping needed) — useful to know, since arcade filenames
are routinely exactly this shape.

### DIP switches (not explored in depth)

`arcade_sw_load`/`arcade_sw_save` in `mra_loader.cpp` persist a game's current DIP switch state
to a file under `dips/<name>` relative to the arcade root. Confirmed to exist; not investigated
further this session — a DIP-switch editing screen is real, wanted eventually, and explicitly a
[non-goal for v1](#non-goals-for-v1).

## What already exists in this codebase

*This section describes the codebase before any of the Arcade work below was done, and is kept
as the starting point. What exists now is under the sections headed "Implemented".*

Worth being precise about, so this document does not read as if Arcade were untouched: it was
*named* in a few places, but nothing made it work.

- `CoreIndex.cpp` and `Library.cpp` both list `_Arcade` among the directories scanned for cores
  (`kCoreDirs`), and `_Arcade` → `"Arcade"` among the group-label mappings — this is what makes
  Arcade RBFs count as "installed cores" and get grouped under an "Arcade" label *for whatever
  else happens to ask*, nothing specific to Arcade games.
- `Library.cpp`'s `kGroups` table names `Arcade.ini` as the section file it would read from
  Console Mode's `themeconfig/section_groups/` to learn Arcade's `ROMDIRS`/`ROMEXTS`, exactly
  the way it does for `Console.ini`, `Handheld.ini`, and so on.
- **This does not currently produce a usable Arcade system.** Console Mode's section-file format
  describes "one directory, one extension list, one core" systems, the exact model
  [above](#why-arcade-doesnt-fit-the-existing-model) explains does not fit Arcade at all — whether
  Console Mode's own distribution even ships an `Arcade.ini` in that shape was not confirmed this
  session (the device went offline mid-session, mid-verification — genuinely unresolved, not
  skipped; see [Open questions](#open-questions)). Even where it is present, one core-directory
  match against thousands of differently-cored `.mra` files is not the right lookup regardless.

In short: the group label "Arcade" already existed in this codebase's vocabulary. An actual
Arcade *library* — games, launching, artwork — did not. It does now. The `Arcade.ini` route
through Console Mode's section files is still there as the fallback for an installation with no
game database of its own, and still does not produce a usable Arcade system; with a database,
it is never consulted.

## Requirements

What a real implementation needs to get right, drawn directly from the case study and the source
reading above:

1. *(Met.)* **List every `.mra` under `_Arcade` on every mounted root** — not just the SD card; a library
   can legitimately span drives, and the case study's own USB drive shows this is not
   hypothetical.
2. *(Met.)* **Launch by handing the `.mra` path straight to `load_core`** — no `.mgl` involved, matching
   how MiSTer's own loader actually branches on the extension.
3. *(Met — and the library now goes one step further, listing only what passes.)* **Report ROM availability using MiSTer's own search order, not the `.mra`'s own folder** — the
   case study's whole first bug was exactly this gap: the naive assumption ("the zip next to this
   `.mra`'s folder is the one that gets used") is wrong whenever a USB drive with a `games/mame`
   directory is mounted, which this GUI already has to support (`GameDatabase`'s own multi-drive
   design already treats this as normal). Getting this right means replicating
   `findPrefixDir`'s exact order — `/media/usb0/games` … `/media/usb5/games`, then
   `/media/fat/games` — not this codebase's own root-iteration order, which serves a different
   purpose (see the flag above).
4. *(Open.)* **Surface the HDD/CHD case rather than silently failing at it** — a game whose `.mra` has no
   ROM-only path (a `<rom>` block with no matching zip content at all, or a core known to need a
   disk image) should say so plainly rather than flash-and-return the way stock MiSTer does,
   since [nothing found this session can automate the mount itself](#hddchd-based-games-no-automated-mount-found).
5. *(Met.)* **Fit the existing tab/grid/favourites/history machinery** rather than duplicate it — a
   library of a few thousand `.mra` files is exactly the scale this GUI's letter-jump, grid
   views and progressive artwork loading already exist for.

## Implemented: the diagnostic table

Before any launching, artwork or DIP-switch work, the concrete next step agreed on was narrower
and more answerable: a read-only table over every `.mra` this GUI can find, showing exactly the
facts the Killer Instinct case study needed an SSH session to work out by hand. Reached from
**Settings -> Manage Arcade -> Arcade Games**. Built without hardware access, on the strength of
this document's own source citations and a real cross-compile plus the host test suite (see
below) — not yet verified on a device.

**Launching:** confirming a row sends `load_core <.mra path>` via `Launcher::launchArcade()` —
exactly what [Phase 1](#phase-1--list-launch-done) describes, unescaped, straight to the command
FIFO. Verified on a real device.

**Columns**, one row per `.mra`: might-work verdict, name, year, manufacturer, category, whether
the core is present and its resolved filename, whether the ROM zip is present, whether every
part's CRC matches, and the ROM zip name(s) the `.mra` itself asks for.

**New files:**

- `src/MraFile.h/.cpp` — parses one `.mra` (a small hand-rolled tag scanner, not a general XML
  library — deliberately, since the format's own shape is simple and fixed) into an `ArcadeEntry`
  plus its `ArcadeRomBlock`/`ArcadeRomPart` list, then resolves the two lookups from [Two
  independent root lookups](#two-independent-root-lookups--the-case-studys-actual-bug):
  `resolveCore()` (same volume as the `.mra`, `<volume>/_Arcade/cores`, `Arcade-<rbf>*.rbf`,
  lexicographically-last match) and `resolveRom()` (MiSTer's own global, USB-before-SD search
  order, via the new `misterGameSearchOrder()`). CRC verification reads a zip's central
  directory only — no decompression — via the extension to `Archive` below.
- `src/Archive.h/.cpp` — extended (not replaced) with `Archive::list()`, returning each entry's
  CRC32 alongside its name; `entries()` is now built on top of it, unchanged for every existing
  caller.
- `src/ArcadeScan.h/.cpp` — walks every mounted volume's `_Arcade` directory recursively (a real
  library is routinely organised into manufacturer subfolders) and resolves each `.mra` found, a
  small batch at a time per `step()` call rather than all at once — the same "do not hammer a
  marginal drive flat out" discipline `LibraryScan` already applies elsewhere in this project,
  since a full arcade set is a thousand-plus files and each resolved one can mean opening a zip.
- `src/ArcadeGamesScreen.h/.cpp` — the table itself: a loading state while `ArcadeScan` is still
  working, then a scrollable, column-based list once it is done.
- `src/ArcadeSettingsScreen.h/.cpp` — the "Manage Arcade" menu this is reached through, with
  exactly one entry today and room to grow into the rest of this document's roadmap (DIP
  switches, a rescan action) without crowding the main Settings list.
- `SettingsScreen`, `App.h/.cpp` — one new row ("Manage Arcade"), wired the same way "Manage
  systems" and "Controllers" already are: an owned screen, an `Active_` flag, open/close
  functions, and the same modal precedence in `dispatch()`/`activeScreen()`.

**Deliberately not done here** — kept separate from Phase 1/2 below, which still stand as written:

- **No persistence.** Every visit re-walks the drives and re-resolves every entry from scratch —
  simplest thing that works for a diagnostic screen someone opens occasionally, and correctness
  mattered more than speed for a first version built without a device to measure on. Caching
  this the way `GameDatabase` caches everything else (see [Where does Arcade metadata
  live](#open-questions), question 3) is exactly the kind of thing worth deferring until it is
  known to actually be slow on a real library, not assumed.
- ~~No launching.~~ Since built: confirming a row launches it, see above.
- **No distinction for the HDD/CHD case.** A Killer Instinct-style game that needs the one-time
  manual mount (see [HDD/CHD-based games](#hddchd-based-games-no-automated-mount-found)) shows up
  as an ordinary "might work: Y" if its ROM zip and core both resolve correctly — this table
  cannot yet tell that class of game apart from one that will actually boot unattended. Surfacing
  that distinction (Requirement 4) is still open.
- **Was not verified on real hardware when first written; it has been since** — the table was
  opened on a real device against a real arcade library and launches games. What follows is the
  original note, kept because it still describes what the automated tests cover. Everything
  above compiles cleanly (ARM cross-build) and
  passes a new host-side test suite (`tests/archive_test.cpp`, `tests/mra_file_test.cpp` —
  `make -C tests`) that exercises `MraFile`'s parsing and both resolve steps against hand-built
  fixtures, plus the real, unmodified Killer Instinct `.mra` text from this document's own case
  study. None of that substitutes for opening the screen on a real MiSTer with a real, messy
  arcade library — that verification is still owed and specifically flagged, not implied by "it
  built and the tests passed."

## Implemented: Arcade as a real system

Phase 1 below, built. `Systems -> Arcade` now exists and behaves like any other system:
`GamesScreen` with its presentations, favourites, history, and the Games tab, with no
Arcade-specific code in any of those screens.

- **`LibraryScan`** gained an Arcade step after the ordinary systems, and it is the plan from
  [Decision: extending "Build game database"](#decision-extending-build-game-database-for-arcade)
  with one deliberate change: **only games that can actually start are recorded.** The step runs
  the same `ArcadeScan` the diagnostic table uses — paced discovery of every volume's `_Arcade`
  at any depth, then parse and resolve core, ROM zip and CRCs for each `.mra` — and keeps an
  entry only when `mightWork()` says yes. The original plan listed every `.mra` and left the
  judging to the table; in practice that gave a list of all 6,416 files across two drives,
  including duplicates, of which only a fraction start. The table stays what explains *why* a
  game is missing.
  - **Each game once.** A full copy on the card and another on a drive is normal, so entries are
    de-duplicated by their path below `_Arcade/`. `cores/`, `media/` and `_Organized/` are not
    walked (the last is the Arcade Organizer's tree of links to the same files).
  - **What gets written**, all flat `.tsv` files in `gamesdb/`, all in the existing
    `<root id> \t <relative path>` row format: `Arcade.tsv` (plus its catalogue line),
    `arcade-manufacturers.tsv` and `arcade-categories.tsv` (`<name> \t <count> \t <key>`), and
    one `Arcade-Manufacturer-<name>.tsv` / `Arcade-Category-<name>.tsv` per group. A game with no
    manufacturer or category goes to `(Unknown)` / `(Uncategorized)`. Group keys are made
    filename-safe, and colliding ones — names differing only in a replaced character or in case,
    which FAT does not tell apart — get a number rather than silently sharing a list.
  - **Paced**, like the rest of the scan: the lists are written one file per step, the two
    catalogues last, so a scan that stops part-way never leaves a catalogue naming a missing list.
  - **`GameDatabase`** gained `writeGroupList()`, `writeGroupCatalog()` and `groupsFor()`; the
    header is now `gamesdb 4`, so an older database is rebuilt, not misread.
  - Read by [the Arcade tab](#implemented-the-arcade-tab): `GameDatabase::groupsFor()` for the
    two catalogues, the ordinary `pathsFor()` for a group's games.
  - **Names that differ only in case or spacing are one group** ("Beat 'Em Up" / "Beat 'em Up"),
    shown under whichever spelling the scan met first.
  - **The ROM search order is MiSTer's own** (`MraFile::misterGameSearchOrder`, USB first, SD
    last) whatever order the scan's volumes happen to be in. An early version of this step passed
    its volumes through in SD-first order — the very bug of the case study, reintroduced — and
    was caught in review; `ArcadeScan::start()` now takes the order only as an explicit,
    test-only override.
  - **A machine with only arcade games can be scanned**; the old "no game directories found next
    to an installed core" stop no longer applies when an `_Arcade` folder exists.
- **`GameSystem::isArcade`**, set in `Library::loadFromDatabase()` for the catalogue key
  `Arcade`. `core` stays empty; `launchable()` is true for Arcade regardless. Every mounted
  volume's `_Arcade` is registered as one of the system's `romDirs`, so `systemForPath()` maps a
  favourite or history entry on the second drive back to Arcade.
- **`Launcher::launchGame()`** hands an Arcade game to `launchArcade()`; `launchCore()` refuses
  Arcade, since there is no core to start without a game.
- **Test:** `tests/arcade_scan_test.cpp` — two scratch volumes with real zip fixtures: working
  games, a missing ROM, a wrong CRC, a missing core, a game on both volumes, the three
  directories that must not be listed, and the manufacturer/category lists.

### Implemented: the Arcade tab

[Decision: a dedicated Arcade tab](#decision-a-dedicated-arcade-tab), built as specified, with the
open question settled the pragmatic way:

- `ArcadeScreen` (the tab), `ArcadeGroupsScreen` (the full Manufacturers / Categories list), and
  `GamesScreen` unmodified behind both — reached with a copy of the Arcade system whose database
  key is the group's list. It stays called `Arcade`, so a favourite or history entry made from
  inside a group still files under Arcade; only the header says "Arcade > Manufacturers > Capcom"
  (plain `>`, because the interface font is not known to carry a typographic chevron).
- Each row's title is a stop of its own and the cursor starts there; Right steps onto the tiles;
  Back from a tile returns to the title before it leaves the tab (`App` asks
  `ArcadeScreen::wantsBack()`). Letter jump works in the Games row.
- **Groups are plain text entries, not game tiles** — each shows the group name and game count;
  the focused entry uses the same blue underline as the tabs. Manufacturer and category entries
  are alphabetical in both the preview rows and full lists.
- Settings -> Show Arcade menu item, on by default; the tab only appears when the library holds
  Arcade games at all (`Library::arcadeSystem()`).
- Rendered on the host with the real screens and a scratch library to check layout. The Arcade
  tab has since been run on a real MiSTer.

### Implemented: box art

Built as [Scraper and box art](#scraper-and-box-art) specified it. Checked on the host against
the server's real listing and the real library's names; the scrape itself runs on the device
(Settings → Prepare box art):

- `LibretroIndex` maps `Arcade` to the `MAME` platform and gained `matchExact()`, tried before the
  old bracket-stripping `match()`. The exact key is the whole title, lower-cased, spaces collapsed,
  with the characters the server replaces in filenames (`& * / : \` < > ? \ |`) mapped to `_` on
  both sides — so `Bubble Bobble : Part 2` and `10 Yard Fight (…, 11/05/84)` still line up with
  the server's underscores. A host test pins the wrong-cover bug: against the real listing the
  loose match picks `Killer Instinct (proto v4.7)` for `(v1.5d)`; the exact one does not.
- `MediaScraper` gives each Arcade job the `_Arcade/media` folder of *its own* volume, taken from
  the game's path, instead of the one system-wide folder — `Library::resolveArtwork` already
  looks exactly there.
- **Box art only.** Backgrounds (`Named_Snaps`) are skipped for Arcade until someone has looked at
  whether gameplay screenshots make a good background.
- **Measured hit rate**, real `MAME` listing (5,824 titles) against the 849 games this library
  lists: **418 match exactly**, 230 more only by the loose match (a cover for the right game, not
  necessarily the right revision), **201 find nothing**. Many misses are title differences
  between the .mra and MAME's name, not missing art. Left as is on purpose — the existing
  `scrape-misses.txt` will list them, and guessing tolerances ahead of real data is what this
  project's own precedent says not to do.
- Run it like any other system: Settings → Prepare box art. Existing covers can also be
  converted to view-sized BMPs; the current format is described in [BOXART.md](BOXART.md).

**Measured:** a full rebuild of the game database on the reference device — 58 systems, 11,352
games of which 849 Arcade at the time (before the ROM search order fix below), and a ROM check
of 6,416 `.mra` files across two drives — about 3,200 titles represented on both volumes —
took 2 min 29 s. That was not split by phase, so what the Arcade step alone costs is not known; it is
paced (a few files per frame) for the same reason everything else in the scan is.

**Known limits:** nothing tells an HDD/CHD-based game from one that starts unattended — Killer
Instinct lists as an ordinary game and still needs its one-time manual mount. Backgrounds are not
fetched for Arcade. A game whose `.mra` has no zip-backed ROM block at all is never listed,
because the check that decides has nothing to confirm; none was seen in practice.

## Proposed architecture

**Recommended: two phases, not one big feature.** The full shape below — metadata parsing, DIP
switches, ROM-availability checking — is real work and does not all need to land before Arcade is
usable at all. A lean first slice reuses almost everything that already exists.

### Phase 1 — list, launch, done

**Built — see [Implemented: Arcade as a real system](#implemented-arcade-as-a-real-system).**

Treat Arcade as a single `GameSystem` (name `"Arcade"`, group `"Arcade"`) whose games are every
`.mra` file under each mounted root's `_Arcade` directory — structurally identical to how any
other system's `romDirs`/`romExts` already work (`Library.cpp` already treats "any file in this
directory with this extension" as a game; `.mra` needs nothing new there). The display name falls
out of the filename the same way it already does for everything else
(`Library::nameFor` strips the extension) — an `.mra` is conventionally already named
`Game Title (revision).mra`, so this alone gives a reasonable list with zero XML parsing.

Two real deltas from every other system, both small and localised:

- **`GameSystem` needs to say "this is arcade," not "this is core X."** Add a bool
  (`isArcade`, or reuse `discBased`'s pattern of a dedicated flag) rather than overload `core`
  with a sentinel value — `core` for Arcade names nothing meaningful the way it does for every
  other system, and a dedicated flag keeps `Launcher` and anything else that branches on it
  honest about why.
- **`Launcher` needs a second launch path.** `launchGame` today always calls `writeMgl` then
  sends `load_core <mgl path>`. For an arcade game it should skip `writeMgl` entirely and send
  `load_core <absolute .mra path>` directly — confirmed safe with spaces and parentheses in the
  path, unescaped, against real hardware this session (see [the command FIFO's
  vocabulary](#the-command-fifos-vocabulary)).

Favourites and history need no changes at all: both already key on `(system, path)`
(`FavoriteEntry` in `src/Favorites.h`) — an `.mra` path is just another path.

This phase alone gets Arcade browsable, favouritable, and launchable, with box art absent (no
name-normalisation to match a thumbnail source yet) and no indication of whether a game's ROMs
are actually present — good enough to ship and use, not good enough to explain a failure the way
this document's own case study needed explaining.

### Phase 2 — metadata: names, artwork, ROM availability, DIP switches

**Status.** Built: manufacturer and category, cached in the game database; ROM availability —
not as a flag per game but as a filter, since only what passes is listed at all; artwork. Not
built: year (parsed, not stored or shown), the HDD/CHD note, DIP switches. The cache question
this section ends on was settled the way it leans — see [Decision: extending "Build game
database"](#decision-extending-build-game-database-for-arcade).

Once Phase 1 is real and in use, add a lightweight `ArcadeIndex` (parallel to `CoreIndex` /
`GameIndex`, not a replacement for either) that parses each `.mra` far enough to extract what
Phase 1 cannot get from the filename alone:

- `<name>`, `<year>`, `<manufacturer>`, `<category>` — better display names and, eventually,
  filtering (category makes "show only fighting games" nearly free, since it is already per-game
  data, not a feature that needs its own taxonomy built).
- The `<rom>` blocks' `zip` attributes and part list — for ROM-availability checking done MiSTer's
  own way (**Requirement 3** above: `/media/usb0/games/mame` … `/media/usb5`, then
  `/media/fat/games/mame`, exactly matching `findPrefixDir`), and, if worth the cost, actual CRC
  verification by looking inside the zip — `Archive.cpp` already opens zips for console ROMs, so
  the primitive exists; full CRC verification across a large arcade set is meaningfully more I/O
  than existence-checking and should be judged against real load time before committing to it,
  not assumed cheap.
- Presence of a plausible HDD/CHD case (a `<rom>` with no zip-resolvable content, or simply
  cross-referencing a short list of known HDD-based cores) — enough to show a "needs one-time
  setup" note rather than nothing (**Requirement 4**).
- `<switches>`, eventually, for a DIP-switch settings screen — deliberately out of Phase 2's own
  minimum scope; see [Non-goals](#non-goals-for-v1).

**A parsing cost worth stating plainly rather than assuming away:** a full MiSTer arcade
distribution is on the order of a thousand-plus `.mra` files. Parsing all of them for metadata on
every app start is not obviously free the way reading a filename is — this needs the same
"cache what a scan already found" treatment `GameDatabase` gives every other system
(`gamesdb/<key>.tsv`), not a live parse on every launch of the GUI. Whether that cache belongs in
the existing per-system `.tsv` format as-is, or needs its own sibling file for the extra fields
(year/manufacturer/rbf/rom-availability) a plain path list has no room for, is an open decision —
see [Open questions](#open-questions) — but "parse a thousand XML files on every cold start" is
not the answer either way.

### Artwork

Superseded by [Scraper and box art](#scraper-and-box-art) below, written once the open question
here ("does the server even carry an arcade set, and matched by which name") had a real,
checked-live answer instead of a guess.

## Scraper and box art

[Open question 5](#open-questions) asked whether the libretro thumbnail server carries an arcade
set at all, and what name it expects. Checked directly against the live server this session
(`thumbnails.libretro.com`) rather than guessed — the answer changes how this has to be built,
not just whether it is possible.

### What's actually there

Two arcade-relevant platforms exist on the server, confirmed by fetching their listings:

| Platform (server folder) | Titles | Naming |
| --- | ---: | --- |
| `MAME` | 5,824 | Matches MRA convention closely — see below |
| `FBNeo - Arcade Games` | 6,456 | Diverges — see below |

Both have `Named_Boxarts/` and `Named_Snaps/` (MAME's `Named_Snaps/` confirmed to exist, HTTP
200 — its actual content was not inspected; see [Open questions
add-ons](#open-questions-this-section-adds)).

**`MAME`'s naming matches this project's own case study almost exactly.** The server has, among
others: `Killer Instinct.png`, `Killer Instinct (v1.0).png`, `Killer Instinct (v1.3).png`,
`Killer Instinct (v1.5d).png`, `Killer Instinct (proto v4.7).png` — the last of those four
**character-for-character identical** to the `<name>` field (and the filename) of the real
`.mra` this whole document is built around. `FBNeo - Arcade Games` has the same game under
completely different names — `Killer Instinct (ROM ver. 1.5d) [Works best in 64-bit build].png`
— which a fuzzy match can still land on, but not as reliably, and not by construction the way
`MAME`'s naming does. **`MAME` is the platform to use; `FBNeo - Arcade Games` is, at best, a
fallback for whatever `MAME` does not have** — see [Open questions add-ons](#open-questions-this-section-adds)
for whether that fallback is even worth building.

### A real problem this surfaces: today's matching would silently pick the wrong revision

This is not a hypothetical. `LibretroIndex::normalise()` (`src/LibretroIndex.cpp`) strips every
bracketed part of a title before matching — right for a console library, where a bracketed tag
is almost always a region or dump-quality marker irrelevant to which cover art is correct
(`"Super Mario Bros. 3 (USA) [!]"` and `"Super Mario Bros 3"` should both find the one cover that
exists). For arcade, a bracketed part is routinely the opposite: **the one piece of information
that tells two entries apart**, since MAME (and this server's `MAME` set) lists every ROM
revision of a game separately.

Traced end to end against the real data above: every one of `Killer Instinct.png`, `Killer
Instinct (v1.0).png`, `(v1.3).png`, `(v1.5d).png` and `(proto v4.7).png` normalises to the
identical key `"killerinstinct"` — none contain a recognised region tag, so
`LibretroIndex::regionRank()` ranks all of them equally (`4`), and `parse()`'s tie-break
(`regionRank(name) < regionRank(existing->second)`, strictly-less, never overwrites an existing
equal-ranked entry) keeps whichever one the server listed **first**. Alphabetically, that is
`Killer Instinct (proto v4.7).png` — a prototype build's box art — not the `v1.5d` release the
real `.mra` is actually for. Pointing today's `LibretroIndex` at `MAME` completely unmodified
would not fail to find a cover for Killer Instinct; it would confidently fetch the **wrong**
one, for every game with more than one listed revision, silently.

### The fix: match the exact title first, fall back to today's fuzzy match second

`LibretroIndex` needs a second, stricter lookup tried before its existing one:

1. **Exact-ish match** — case-insensitive, whitespace-trimmed, brackets kept intact — against the
   `.mra`'s own title. This is what correctly separates `Killer Instinct (v1.5d)` from every
   other revision, and works because of the naming match confirmed above; it is not a general
   solution being hoped into working, it is what the real, checked data actually supports.
2. **Today's existing bracket-stripped match**, unchanged, only as a fallback when step 1 misses
   — a locally renamed `.mra`, or a revision the server does not carry under an identical string.

Concretely: a second map, keyed by a lighter normalisation (case/whitespace only, brackets
untouched), built from the same directory listing `parse()` already downloads — no second
network fetch, no second cached file, just a second index kept alongside the existing one.

### Where the title to match comes from

The `.mra`'s filename without its extension — the same thing `Library::nameFor()` already
derives for every other system, so this needs no new code path, only for `LibretroIndex`'s new
exact-match step to be given it. This works because a `.mra`'s filename and its own internal
`<name>` field are the same string in every set checked this session (MiSTer's own official
Killer Instinct release among them) — a convention, not something enforced anywhere, so a
locally renamed `.mra` will simply miss step 1 above and fall through to step 2, same as any
other unusual case would.

### The other real problem: where scraped box art actually gets written

`MediaScraper::prepareNextSystem()` (`src/MediaScraper.cpp`) computes **one** `mediaDir` per
system — `system.dir + "/media"` — and reuses it for every game in that system, because every
system this project has handled so far genuinely has one game directory. Arcade does not: its
`.mra` files are spread across every mounted volume's `_Arcade` folder (and, within that,
however deep a library organises manufacturer or genre subfolders) — exactly the shape [Why
Arcade doesn't fit the existing model](#why-arcade-doesnt-fit-the-existing-model) already laid
out for launching, and it turns out to matter here too.

**The fix does not need a new concept — `MediaScraper::Job` already carries `mediaDir` per job,
not per system.** `prepareNextSystem()` simply never varies it within one system's loop today.
For Arcade specifically, each job's `mediaDir` needs to be derived from *that game's own*
resolved volume — `<volume>/_Arcade/media` — the same `"/_"`-truncation `MraFile::parse()`
already does to find `entry.root`, applied here to each path read back out of
`GameDatabase::pathsFor()` rather than re-parsed from a `.mra` a second time. A flat `media`
folder directly under each volume's `_Arcade`, not mirroring however deep the library's own
subfolders go — simplest thing that works, and consistent with `_Arcade/cores` already being a
single flat folder regardless of where a `.mra` sits.

### Proposed changes, concretely

*All three were made as written below; see [Implemented: box art](#implemented-box-art).*

- **`LibretroIndex`**: add `{"Arcade", "MAME"}` to `kPlatforms` (matched by system key exactly
  like every other entry — Arcade is a system now, see [Decision: a dedicated Arcade
  tab](#decision-a-dedicated-arcade-tab), so nothing about how `platformFor()` is called needs
  to change). Add the second, exact-ish index and lookup described above — a new method
  (`matchExact()`, say) tried by the scraper before falling back to the existing `match()`.
- **`MediaScraper`**: `prepareNextSystem()` needs an Arcade-specific branch that builds one
  `Job` per game with that game's own volume-derived `mediaDir`, instead of the single
  system-wide one every other system still gets unchanged.
- **Nothing about `GameDatabase`'s own format needs to change.** Per-system game paths are
  already stored as `<root id> \t <relative path>` — already multi-root, already exactly what
  this needs; only the scraper's *use* of a system's single `catalog.tsv` `dir` field was ever
  the limiting assumption, and only for Arcade.

### Open questions this section adds

- **Is `MAME/Named_Snaps/` actually suitable as the "-BG" background image?** Not inspected —
  MAME's own traditional "snap" folders are gameplay screenshots, which may read as repetitive
  or simply less attractive as a full-screen background than the fan-art-style images console
  systems' `Named_Snaps/` tend to hold. Worth actually looking at a handful before wiring
  backgrounds up for Arcade at all; boxart alone may be the better v1 scope regardless.
- **Is the `FBNeo - Arcade Games` fallback worth building at all?** Its naming diverges enough
  (see above) that even the fallback fuzzy match will not land reliably — it may cover few
  enough additional titles over `MAME` alone that shipping without it, and revisiting only if
  real misses justify it (`Settings → Prepare box art` writes exactly that list, see
  `MediaScraper::kMissesFile`), is the simpler, honest v1 scope.
- **Does the exact-ish match need to tolerate anything beyond case and whitespace** — a curly
  vs. straight apostrophe, an en dash vs. a hyphen? Not knowable without a real library's worth
  of misses to look at; guessing tolerances in ahead of that data is exactly the kind of thing
  this project's own `PERFORMANCE.md` precedent says to measure instead.

## Decision: a dedicated Arcade tab

**Built as decided** — see [Implemented: the Arcade tab](#implemented-the-arcade-tab). Two
differences from the text and mockups below: group tiles are plain text tiles in every case (the
open question below, answered), and the header's separator is a plain `>` rather than `›`.

Decided, on top of everything above: Arcade is **both** a system and a tab of its own, not
one or the other.

1. **Arcade is a real system.** Exactly [Phase 1](#phase-1--list-launch-done) as already
   written — `Systems -> Arcade` behaves like every other system (`GamesScreen`, five
   presentations, favourites, history), no special-casing there at all.
2. **A separate `Arcade` tab also exists**, alongside Home/Favorites/Systems/Games/Settings,
   toggled from Settings the same way the Games tab already is (`Preferences::showGamesTab`,
   `TopBar::visibleTabs`). Off by default is a reasonable default to revisit once this exists —
   not decided either way yet.

The tab is not a duplicate of `Systems -> Arcade`; it exists because "every arcade game in one
flat list" is only one of three useful ways into this library, and the other two —
manufacturer, category — have no equivalent anywhere else in this GUI today.

### Inside the tab: three rows, Home-style — plus one interaction Home does not have

Three horizontally-scrolling preview rows, reusing `HomeScreen`'s own row rendering almost
exactly (`Row`, `Metrics`, the focus-fade tile layout): **Games**, **Manufacturers**,
**Categories**. Up/Down switches the active row, Left/Right moves within it — identical to Home
so far.

**New, and worth stating plainly since nothing in this GUI does it today:** each row's own
title is itself a focusable stop, to the left of its first tile — landing on a row (via Up/Down)
starts there, and **confirming while the title is focused opens a full view of that whole
dimension**, not a game. Pressing Right steps off the title onto the first tile, where Confirm
goes back to behaving exactly like Home (launch a game, in the Games row) or, for
Manufacturers/Categories, jumps straight into that one group's filtered game list — a shortcut
past the intermediate full list for a group already visible in the preview.

This needs `Row::cursor`'s existing default (`0`, landing on the first tile) changed to a
sentinel (`-1`, landing on the title) for these three rows specifically — small, but a real
behavioural difference from Home's rows, not just a rendering one, and worth a test once this is
built: a first-time visitor should discover "open the full view" without having to already know
it is there.

- **Games row** — tiles are ordinary game tiles (box art, launches directly). Confirming the
  row title opens the exact same list `Systems -> Arcade` already shows — one underlying list,
  reached three ways (Systems, tab preview → full, and — once box art matching for arcade
  exists, see [Artwork](#artwork) — indistinguishable from any other system's tile grid).
- **Manufacturers / Categories rows** — tiles are *groups*, not games: a name and a count
  ("Capcom · 128"), no box art of their own (see the open question below). Confirming the row
  title opens the full list of every manufacturer/category; confirming a group tile directly
  (from the preview, skipping that full list) jumps straight into `GamesScreen` filtered to that
  one group.

### The three full views

- **Games** — `GamesScreen` over the Arcade system, unmodified. Nothing new to build here at
  all beyond what [Phase 1](#phase-1--list-launch-done) already covers.
- **Manufacturers / Categories** — a new screen this project does not have an equivalent of yet:
  a tile grid of *groups*, each showing a name and a count, confirming one enters `GamesScreen`
  pre-filtered to it (breadcrumb title, e.g. "Arcade › Manufacturers › Capcom"). Closer in shape
  to `SystemsScreen` (named tiles, no per-tile box art) than to `GamesScreen` — except
  `SystemsScreen` has one fixed grid presentation. This earlier proposal assumed switchable
  presentations for groups; the implemented group entries are plain text in every game view.
  Reusing `GamesScreen` outright for this would mean stretching its `Entry`/`Game` model
  over something that is not a game; a small, separate grouped-tile screen sharing `GridView`
  and `Tile` (the same low-level pieces both existing screens already build on) rather than
  sharing `GamesScreen` itself is the shape this points to, but is not fully worked out — see
  the open question right below.

### Open question this decision adds

**Resolved:** manufacturer and category entries are not game tiles and have no box art. They use
the same plain-text presentation regardless of the selected game view.

### This depends on Phase 2, not just Phase 1

*Resolved: the cached metadata exists, see the decision below.*

Worth being explicit about: everything in this section needs manufacturer/category metadata for
*every* arcade game available at ordinary browsing speed — the [diagnostic
screen](#implemented-the-diagnostic-table) built so far deliberately re-walks and re-parses the
whole `_Arcade` tree on every visit, which is fine for a screen opened occasionally from
Settings and not fine for a tab meant to open instantly like every other one does. The Arcade
tab as described here is not buildable on top of today's `ArcadeScan` as-is; it needs
[Phase 2](#phase-2--metadata-names-artwork-rom-availability-dip-switches)'s cached metadata
first (see also [Open questions](#open-questions), question 3). **Specified — see [Decision:
extending "Build game database" for
Arcade](#decision-extending-build-game-database-for-arcade).**

### Mockups

The Arcade tab, freshly opened — cursor on the Games row's own title, the new focusable stop:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Home   Favorites   Systems   Arcade   Games   Settings                      │
├──────────────────────────────────────────────────────────────────────────────┤
│  Arcade                                                                       │
│                                                                                │
│  ▸[ Games ]                                                    1,842         │
│    ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                    │
│    │ Killer │ │ Street  │ │  1942  │ │ Pac-Man│ │ Metal  │   ···              │
│    │Instinct│ │Fighter II│ │        │ │        │ │  Slug  │                  │
│    └────────┘ └────────┘ └────────┘ └────────┘ └────────┘                    │
│                                                                                │
│    Manufacturers                                                  214        │
│    ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                    │
│    │ Capcom │ │  SNK   │ │ Konami │ │ Midway │ │  Sega  │   ···              │
│    │   128  │ │   94   │ │   71   │ │   65   │ │   58   │                    │
│    └────────┘ └────────┘ └────────┘ └────────┘ └────────┘                    │
│                                                                                │
│    Categories                                                      18        │
│    ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                    │
│    │Fighter │ │Shooter │ │Platform│ │  Maze  │ │ Puzzle │   ···              │
│    │   243  │ │   401  │ │   188  │ │    52  │ │    97  │                    │
│    └────────┘ └────────┘ └────────┘ └────────┘ └────────┘                    │
│                                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│  A Open Games   ▶ Browse tiles   ▼ Next row   Hold X 2s Favorite   L2/R2     │
└──────────────────────────────────────────────────────────────────────────────┘
```

The same screen with the cursor moved right, off the title and onto a manufacturer tile — `A`
now means something different, and the bottom bar says so:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Arcade                                                                       │
│                                                                                │
│    Games                                                       1,842         │
│    ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                    │
│    │ Killer │ │ Street  │ │  1942  │ │ Pac-Man│ │ Metal  │   ···              │
│    │Instinct│ │Fighter II│ │        │ │        │ │  Slug  │                  │
│    └────────┘ └────────┘ └────────┘ └────────┘ └────────┘                    │
│                                                                                │
│  ▸ Manufacturers                                                   214       │
│    ┌────────┐ ▛▀▀▀▀▀▀▀▜ ┌────────┐ ┌────────┐ ┌────────┐                    │
│    │ Capcom │ ▌  SNK   ▐ │ Konami │ │ Midway │ │  Sega  │   ···              │
│    │   128  │ ▌   94   ▐ │   71   │ │   65   │ │   58   │                    │
│    └────────┘ ▙▄▄▄▄▄▄▄▟ └────────┘ └────────┘ └────────┘                    │
│                                                                                │
│    Categories                                                       18       │
│    ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                    │
│    │Fighter │ │Shooter │ │Platform│ │  Maze  │ │ Puzzle │   ···              │
│    │   243  │ │   401  │ │   188  │ │    52  │ │    97  │                    │
│    └────────┘ └────────┘ └────────┘ └────────┘ └────────┘                    │
│                                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│  A Open SNK's games   ◀▶ Browse   ▲▼ Row   B Back to title   L2/R2 Letter    │
└──────────────────────────────────────────────────────────────────────────────┘
```

Confirming the Manufacturers row's own title (not a tile) instead — the full list, same
switchable presentations `GamesScreen` already has elsewhere in this GUI:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Arcade › Manufacturers                                    214 manufacturers │
├──────────────────────────────────────────────────────────────────────────────┤
│   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│   │  Capcom  │  │   SNK    │  │  Konami  │  │  Midway  │  │   Sega   │       │
│   │    128   │  │    94    │  │    71    │  │    65    │  │    58    │       │
│   └──────────┘  └──────────┘  └──────────┘  └──────────┘  └──────────┘       │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│   │  Namco   │  │  Taito   │  │  Atari   │  │Data East │  │   Irem   │       │
│   │    52    │  │    47    │  │    41    │  │    38    │  │    30    │       │
│   └──────────┘  └──────────┘  └──────────┘  └──────────┘  └──────────┘       │
├──────────────────────────────────────────────────────────────────────────────┤
│  A Open   B Back   L2/R2 Letter                                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

...and one level further in, after confirming "Capcom" above (or the shortcut straight from the
tab preview) — an ordinary `GamesScreen`, filtered, with a breadcrumb saying so:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Arcade › Manufacturers › Capcom                                 128 games   │
├──────────────────────────────────────────────────────────────────────────────┤
│   ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐     │
│   │Street  │  │ Ghouls'│  │ 1942   │  │ Final  │  │Strider │  │ Bionic │     │
│   │Fighter │  │N Ghosts│  │        │  │ Fight  │  │        │  │Commando│     │
│   │  II    │  │        │  │        │  │        │  │        │  │        │     │
│   └────────┘  └────────┘  └────────┘  └────────┘  └────────┘  └────────┘     │
├──────────────────────────────────────────────────────────────────────────────┤
│  A Start   B Back   Hold X 2s Favorite   Y Cycle view   L2/R2 Letter        │
└──────────────────────────────────────────────────────────────────────────────┘
```

## Decision: extending "Build game database" for Arcade

**Built**, with these differences from the plan below:

- **Only working games are recorded**, not every `.mra`. The flow below says the parse step does
  "no core/ROM/CRC resolve, that stays the diagnostic screen's own job"; that was reversed after
  the first real rebuild's all-MRA list included 6,416 files across two drives, including
  duplicates, of which only a small fraction start.
- The step **reuses `ArcadeScan` whole** — its paced discovery and its parse-and-resolve — rather
  than a second walk of its own inside `LibraryScan`.
- Group spellings that differ in case or spacing are merged, and colliding file keys are
  numbered (the plan accepted that two names could collide).
- `GameSystem::isArcade` is set, as planned; the header bump is to 4, as planned.

See [Implemented: Arcade as a real system](#implemented-arcade-as-a-real-system).

Specified before any code, at the user's explicit request — this is the mechanism that would
actually satisfy [Decision: a dedicated Arcade tab](#decision-a-dedicated-arcade-tab)'s own
unmet dependency ("[This depends on Phase 2, not just Phase
1](#this-depends-on-phase-2-not-just-phase-1)", and [Open questions](#open-questions) question
3) — the tab needs manufacturer/category metadata for every game at ordinary browsing speed,
and today's `ArcadeScan` deliberately re-walks and re-parses the whole `_Arcade` tree on every
visit, which is fine for a diagnostic screen opened occasionally and not fine for a tab meant to
open instantly.

### The format question, settled by noticing a row is already generic enough

Nothing about `GameDatabase`'s existing per-system file format
(`<root id>\t<relative path>`, one line per game — see `GameDatabase.h`) says anything about
*why* a game belongs in that file. A console's `<key>.tsv` holds it because the game sits in
that console's own folder; **a grouping file can hold exactly the same row shape for a
completely different reason — because the game's manufacturer or category matches — and
`GameDatabase::pathsFor()` cannot tell, and does not need to, the difference.** This is the
single decision that makes everything below cheap: **Manufacturers and Categories are not a new
kind of data, they are more files in the same format**, each one just a different subset of the
same underlying game list.

Concretely, three flat-file shapes, matching this document's own framing (all games / per
manufacturer / per category), all using the row shape that already exists:

- **`Arcade.tsv`** — every arcade game, exactly like any other system's `<key>.tsv`. Answers
  `Systems -> Arcade` and the Arcade tab's own "Games" row/full view directly, no new read-side
  code at all.
- **`Arcade-Manufacturer-<safe name>.tsv`** — one per manufacturer, same row shape, just the
  subset of games with that manufacturer.
- **`Arcade-Category-<safe name>.tsv`** — one per category, same idea.

Reading any of these is `GameDatabase::pathsFor("Arcade-Manufacturer-Capcom")` — the exact same
call every system already makes. **The Manufacturers/Categories full-view screens do not need
their own data-loading code**: navigating into "Capcom" is building one ad hoc `GameSystem`
(`name = "Capcom"`, `dbKey = "Arcade-Manufacturer-Capcom"`) and handing it to the *existing*
`GamesScreen` — the same screen `Systems -> Arcade` already uses, unmodified. This is the
"similarly flexible as the Organizer" result asked for, but as database rows instead of an
actual reorganised folder tree on disk — cheaper to build, nothing to keep in sync with the
real `_Arcade` folder, and gone as cleanly as it arrived on the next rebuild.

**Two new, small catalog files** are still needed — not per game, once each — so the
Manufacturers/Categories full views can list *which* groups exist, with counts, the same way the
top-level `catalog.tsv` already lets the Systems tab list every system without opening any of
their game files first:

- **`arcade-manufacturers.tsv`** — `<manufacturer name>\t<count>\t<file key>` per row.
- **`arcade-categories.tsv`** — `<category name>\t<count>\t<file key>` per row.

**Filename safety, reusing an existing precedent rather than inventing one:** a manufacturer
string can contain `/` (seen directly in real data this session — e.g. "Rare / Nintendo") or
spaces, neither safe in a filename. `LibretroIndex::open()` already solves exactly this problem
for its own per-platform cache filename — `for (char c : platform) cacheName.push_back((c ==
'/' || c == ' ') ? '_' : c);` — and the same substitution is the right fit here, with the same
known, accepted limitation (two names differing only in a replaced character collide) rather
than a new, more careful scheme this data has not been shown to need.

**A category, and rarely a manufacturer, can be empty** — confirmed directly in real `.mra`
files this session (not every one carries a `<category>` tag). These games fall into a single
`(Uncategorized)` / `(Unknown)` bucket rather than being silently dropped from that dimension —
still findable by browsing, honestly labelled as unclassified rather than pretending every game
fits neatly.

### Flow

```
                              Settings -> Build game database
                                            |
                                            v
                              +----------------------------+
                              |  Discovering                 |   (existing, unchanged)
                              |  cores_.scan(roots)           |
                              |  SystemCatalog::discover(...) |  -> queue_: every NES/SNES/
                              +---------------+----------------+  PSX/... system found
                                              |
                                              v
                              +----------------------------+
                              |  Scanning                    |   (existing, unchanged)
                              |  one system per step          |
                              |  queue_[i] -> <key>.tsv        |
                              +---------------+----------------+
                                              | queue_ exhausted
                                              v
                    +---------------------------------------------------------+
                    |  ScanningArcade  (NEW)                                    |
                    |                                                            |
                    |   +--------------+        +---------------------------+   |
                    |   | Discovering   |------->| Parsing                    |   |
                    |   | paced walk of |        | paced, MraFile::parse()    |   |
                    |   | every root's  |        | per .mra found:            |   |
                    |   | _Arcade       |        | name/year/manufacturer/    |   |
                    |   | (reuses       |        | category/rbf ONLY -        |   |
                    |   | ArcadeScan's  |        | no core/ROM/CRC resolve,   |   |
                    |   | own paced     |        | that stays the diagnostic  |   |
                    |   | directory     |        | screen's own job           |   |
                    |   | walk)         |        +-------------+---------------+   |
                    |   +--------------+                      |                   |
                    |                                          v                   |
                    |                              in memory, growing as each      |
                    |                              .mra is parsed:                 |
                    |                                - allGames[]                  |
                    |                                - byManufacturer{name: [...]} |
                    |                                - byCategory{name: [...]}     |
                    +---------------------------+-------------------------------+
                                                | every .mra parsed and bucketed
                                                v
                    +---------------------------------------------------------+
                    |  Writing  (existing state, extended, same one-unit-of-     |
                    |            work-per-step pacing as today)                  |
                    |                                                            |
                    |   existing, unchanged:                                     |
                    |     - one catalog.tsv line per non-arcade system            |
                    |     - one <key>.tsv per non-arcade system                   |
                    |                                                            |
                    |   new:                                                     |
                    |     - one catalog.tsv line for "Arcade" itself              |
                    |     - Arcade.tsv                    (allGames)             |
                    |     - arcade-manufacturers.tsv       (group catalog)        |
                    |     - arcade-categories.tsv          (group catalog)        |
                    |     - Arcade-Manufacturer-<safe>.tsv  x  |byManufacturer|   |
                    |     - Arcade-Category-<safe>.tsv      x  |byCategory|       |
                    +---------------------------+-------------------------------+
                                                |
                                                v
                                              Done
```

One `.mra`, fanning out during Parsing:

```
   one .mra found under any root's _Arcade
                    |
                    v
        MraFile::parse(path, entry)
        -> entry.name, .manufacturer, .category, .root
                    |
       +------------+-------------------------+
       v            v                         v
  allGames[]   byManufacturer[             byCategory[
  .push(path)   entry.manufacturer          entry.category
                  or "(Unknown)" ]            or "(Uncategorized)" ]
                 .push(path)                 .push(path)
```

Read side, once this exists — why the format choice pays off immediately:

```
Arcade tab -> "Manufacturers" row -> confirm the row's own title (full view)
        |
        v
  read arcade-manufacturers.tsv        (one small file: names + counts)
        |  cursor lands on "Capcom", confirmed
        v
  synthesize GameSystem{ name="Capcom", dbKey="Arcade-Manufacturer-Capcom" }
        |
        v
  database_.pathsFor("Arcade-Manufacturer-Capcom")   <- the exact same call
        |                                                every system already uses
        v
  GamesScreen renders it — unmodified, no Arcade-specific code in that screen at all
```

### Why a new `LibraryScan` state, not folding this into the existing `Scanning` one

Each existing `Scanning` step costs one directory read for one system. A `.mra` needs an actual
file read and parse, one per game rather than one per system — thousands of them, the exact
shape [`ArcadeScan`'s own two-phase pacing](#implemented-the-diagnostic-table) already exists to
spread across frames rather than block on. Reusing that shape as `ScanningArcade`'s own internal
structure (paced discovery, then paced parsing) rather than inventing a second pacing scheme is
the same reasoning that produced `ArcadeScan` in the first place — this is not new risk, it is
reapplying an already-fixed one.

### Other things this touches

- **`GameDatabase`'s version header** (`kHeader`, currently `"#mister-pat gamesdb 3"`) needs
  bumping, matching its own documented precedent for a layout change ("2 added the probe
  column…", "3 stores a system's directory…") — "4 adds Arcade as a system, plus its own group
  catalogs and per-group game lists."
- **`catalog.tsv`'s `core` field, for the Arcade row specifically, stays empty** — Arcade has no
  single core the way every other system's catalog row names one. `Library::loadFromDatabase()`
  needs the small, already-anticipated addition from [Decision: a dedicated Arcade
  tab](#decision-a-dedicated-arcade-tab)'s own proposed architecture: a dedicated `bool
  GameSystem::isArcade` flag, set when the catalog key is `"Arcade"`, rather than overloading
  `core` with a meaning it does not have here.
- **No changes to `GameDatabase`'s prune/cleanup mechanics.** Every new file is a flat `.tsv`
  directly inside `gamesdb/`, indistinguishable in shape from `SNES.tsv` — deliberately not a
  `gamesdb/arcade/` subdirectory, since `removeStragglers()`'s own existing behaviour (confirmed
  by reading it this session) treats *any* non-`.tsv`, including a directory, found while
  clearing a stale generation as something to recursively remove. A subdirectory would likely
  still work, but flat naming needs zero changes to code that already works, so it is the
  simpler choice, not just the cautious one.

### Open questions this decision adds

- Real counts and timing for `ScanningArcade` against a library this size (10,500+ games,
  about 3,200 unique `.mra` titles (6,400+ files across two drives) — not measured;
  `ArcadeScan`'s own live-tested pacing is the only real
  data point so far, and that included the resolve step this scan deliberately skips, so it is
  not a direct estimate either way.
- Whether a few hundred small `Arcade-Manufacturer-*.tsv`/`Arcade-Category-*.tsv` files
  alongside the existing per-system ones is fine as-is, or worth a subdirectory after all once
  real counts are known — noted above as deliberately not done yet, not ruled out forever.
- Whether the Games tab (the merged, all-systems list) should include Arcade once this exists —
  a presentation decision, not a data one, since `Arcade.tsv` would already be a perfectly
  ordinary system's game list from that screen's point of view.

## Where cores and MRAs would come from, if this ever fetches them

The question asked: if the diagnostic table's "Core? N" / "ROM? N" columns ever grew an
"install it" action, where would the file actually come from? The MiSTer community already
solved this well; worth understanding before inventing a competing answer. **Now taken further
than research** — see [Decision: Manage Cores and Manage
MRAs](#decision-manage-cores-and-manage-mras) below for the actual plan.

**`Update_All_MiSTer` is a menu-driven wrapper, not the engine.** The actual download/verify
tool is [`MiSTer-devel/Downloader_MiSTer`](https://github.com/MiSTer-devel/Downloader_MiSTer)
(Python, run as `downloader.sh` from `/media/fat/Scripts`); `Update_All_MiSTer` mainly edits
`downloader.ini` (which databases are enabled) and calls it.

**One published manifest is the entire source of truth**, for cores and MRAs both — no separate
arcade-specific index needed. `MiSTer-devel/Distribution_MiSTer`'s
[`db.json.zip`](https://raw.githubusercontent.com/MiSTer-devel/Distribution_MiSTer/main/db.json.zip)
(fetched and parsed live this session: 272 KB uncompressed, 1,597 files, 326 folders) has a flat
`files` map — a `.rbf` and an `.mra` are entries in exactly the same dictionary, e.g.:

```json
"_Arcade/cores/ActFancer_20260708.rbf": {"hash":"cbf83337687f168e4a3dbdc3a251932c","size":3765792,"tags":[29,46,254,420],"tangle":["actfancer_core"]}
"_Arcade/280Z-ZZAP (US).mra": {"hash":"897f479a94a6712ada32b9f7e8ca3759","size":703469,"tags":[22,29,33,34,35,36]}
```

191 arcade cores and 681 `.mra` files in that one live snapshot. A file's URL is
`base_files_url` (a commit-pinned `raw.githubusercontent.com` prefix, also in the manifest) plus
its key, unless the entry carries its own `url` — mainly used by `archives` entries (zipped
asset packs: palettes, cheats). Verification is **MD5 (`hash`) plus `size`, from the manifest,
per file** — never a blind overwrite.

**The `tangle` field is what this project's own `MraFile::resolveCore` heuristic exists
without.** `db.json` lists exactly **one** current dated `.rbf` per core — verified live, `0` of
191 arcade cores had more than one dated entry — because `tangle` (e.g. `["actfancer_core"]`)
is Downloader's pointer for "this dated file supersedes whichever older one with the same
identity is already installed," and it deletes the stale one. This project's own
`resolveCore()` (see [RBF version selection](#rbf-version-selection)) picks the
lexicographically-last `Arcade-<rbf>*.rbf` it finds on disk specifically *because* nothing here
manages installation and stale dated files can and do accumulate — worth keeping if this project
never fetches anything itself, since a manifest-driven install is what would make that heuristic
unnecessary, not a reason to remove it pre-emptively.

**Directly fetchable by this project's own code, no Python involved.** `db.json.zip` is a
static file; this project already parses a zip's central directory by hand (`Archive.cpp`) and
already links `zlib` (for `libpng`/`freetype`), which is the one piece `Archive.cpp` does not
currently need — decompressing an entry — but would, to unzip this manifest. Downloader itself
works this way: fetch and cache the manifest, nothing about the format requires running its own
tooling to consume it.

**Licensing and etiquette, if this is ever built:**

- `Downloader_MiSTer` and `Update_All_MiSTer` are GPL-3.0. Fetching their published `db.json` as
  data is not itself a GPL-triggering act; vendoring code or logic from those repos would be.
- `Distribution_MiSTer` aggregates many per-core repos, each under its own licence — a GUI that
  ever shows "source" for a given core should attribute the original core's own repo, not this
  project.
- Community databases (jotego's `jtcores_mister` among them) follow the identical schema —
  jotego's non-public cores download freely but are gated behind a separate paid key file, so a
  successful download would not by itself mean a playable core; worth surfacing if this project
  ever lists jotego cores specifically.
- Fetches are plain unauthenticated `raw.githubusercontent.com` requests — GitHub's standard
  rate limit applies (60/hr per IP without a token). One manifest fetch plus per-file downloads
  pinned to a commit SHA is naturally light; caching the manifest locally using its own
  `timestamp` field (re-fetching only when stale) is what `Downloader_MiSTer` itself does and
  would be worth matching.

Not investigated: whether this belongs in this project at all, versus pointing someone at
`Update_All_MiSTer` itself (already installed on most MiSTers, already trusted, already
maintained) the way a missing core or ROM set is surfaced. An "install this" action duplicates
real, ongoing maintenance work (manifest curation, per-core packaging) this project does not do
today — a link or a clear instruction may be the more honest v1 than a competing downloader.

## Decision: Manage Cores and Manage MRAs

Two more rows under Settings → Manage Arcade, alongside Arcade Games: **Manage Cores** and
**Manage MRAs**. Both sync against the same manifest described above
([Distribution_MiSTer's `db.json`](#where-cores-and-mras-would-come-from-if-this-ever-fetches-them)),
through the same downloader this project already has (`Downloader`, currently used for box art
and the update check) plus two genuinely new pieces — a manifest parser and an MD5 check — both
scoped narrowly rather than pulled in as general libraries, matching how this document's earlier
decisions already favour a small, purpose-built parser (`MraFile`'s own tag scanner) over a
general one.

### The starting point is the manifest, not your ROM library

**The first version of this section had the direction backwards.** It proposed scanning
`games/mame/*.zip` first and working out which MRAs to fetch from that — reasonable-sounding,
but built on an unexamined assumption: that syncing the *whole* manifest would be wasteful. That
assumption turned out to only be true for one of the two file types, and checking it against the
real manifest (the same `db.json` already pulled and sitting in this session) settled it
outright:

| | Files | Total size | Average |
| --- | ---: | ---: | ---: |
| `_Arcade/cores/*.rbf` | 193 | 621.0 MB | 3.3 MB |
| `_Arcade/*.mra` | 681 | 40.5 MB | 61 KB |

**MRAs are flat text files and cost almost nothing in aggregate — 40 MB for every arcade game
MiSTer officially supports.** There is no real reason to be selective about which ones to have;
"Manage MRAs" can simply mean *sync all of them*, no ROM-ownership pre-filtering, no setname
matching, none of the gap the first draft flagged as unresolved. **Cores are the expensive
side** (621 MB for 193 of them) — that is where being selective actually matters, and that
selectivity is best driven by something this project already has, not a new ROM scan: once
every official MRA is on disk, [the diagnostic screen already
built](#implemented-the-diagnostic-table) already knows, per game, whether its ROM is present
and CRC-correct. A core is worth fetching exactly when that answer is yes and the core alone is
what is missing — the ROM-ownership question answers itself as a side effect of syncing MRAs
first, rather than needing its own separate scan and its own separate matching logic.

This also quietly deletes the open question the first draft could not resolve (matching a ROM's
setname back to a manifest `.mra` without downloading it first) — not answered, made
unnecessary. With every official MRA present locally, `ArcadeScan`/`MraFile` already read each
one's own `<rom zip="...">` directly, the same way they do for anything on disk today.

**A concrete limit this reversal surfaces, checked directly against the live manifest: Killer
Instinct — this document's own opening case study — is not in it.** Searching the same `db.json`
for `killerinstinct` or `killer instinct`, case-insensitively, in any field, returns nothing.
Confirms what the case study itself already said in passing (a core "still under active
development," first booting August 2026): the official manifest only covers MiSTer's
*cataloged* releases. A "Manage MRAs"/"Manage Cores" built on this manifest — however well it
works for the other 193 cores and 681 MRAs — **would never have found the exact game this whole
document started from.** For that, the only path is the one this session actually used: the
core's own GitHub repo, by hand, on this document's own case study's terms. Worth stating
plainly rather than letting the feature's own cover story imply it solves everything the case
study ran into.

### How `Update_All_MiSTer` itself actually proceeds

Worth checking directly rather than assumed, since it is the established, trusted precedent
this whole feature is deliberately following rather than competing with. Read straight from its
own README this session:

- **Run explicitly, not automatic.** Installed once into `/Scripts`; the person launches it by
  hand from MiSTer's own Scripts menu whenever they want to sync. It self-updates on each run,
  then runs `Downloader_MiSTer` under the hood against whatever `downloader.ini` currently
  enables.
- **Bulk sync of whatever is enabled — not selective by ROM ownership, at all.** Its Settings
  screen (reachable by pressing Up during the post-launch countdown) is a set of database
  on/off toggles, not a per-game picker. "Main Distribution" — the exact `db.json` this
  document's own numbers came from — **downloads cores and firmware wholesale, and is on by
  default.** There is no ROM-cross-referencing step anywhere in it: it does not know or care
  what ROMs exist before deciding what to fetch. **This is real, direct precedent against the
  "only fetch cores for ROMs you already verifiably own" selectivity this section originally
  proposed** — the tool actually trusted by the MiSTer community does not do that, for cores or
  for anything else.
- **Also on by default: sources beyond Distribution_MiSTer.** JTCORES (jotego's cores) and the
  Coin-Op Collection are *both* enabled out of the box alongside Main Distribution — this
  project's own scope (the one official manifest) is narrower than what `Update_All` itself
  ships by default, not wider. A "Disabled by default" tier below that covers patched/unofficial
  variants (e.g. "Arcade Offset") and LLAPI-specific forks — deliberately opt-in even there.
- **An "Arcade Organizer" tool exists** (disabled by default) that builds a browsable
  `_Arcade/_Organized` folder structure from installed MRAs — worth a mention alongside [Decision:
  a dedicated Arcade tab](#decision-a-dedicated-arcade-tab)'s own Manufacturers/Categories rows:
  independent prior art for "MRAs deserve to be browsed by more than one flat list," arrived at
  by a different tool for a different (file-system-level) reason.
- **First run: about 15 minutes; later runs are quick** — consistent with a hash-checked,
  skip-what-is-already-correct design, the same shape [the earlier section on
  verification](#where-cores-and-mras-would-come-from-if-this-ever-fetches-them) already
  described.

**What this changes here:** given the community's own trusted tool already treats "sync the
whole enabled database" as the normal, unremarkable default — for cores as much as for
anything else — inventing bespoke ROM-driven selectivity for *this* project's Manage Cores
screen would be solving a problem the ecosystem has not needed solved. The simpler, better-
precedented v1: **Manage Cores syncs the whole official arcade core set, exactly like Manage
MRAs, both behind one explicit action** (never automatic, matching `Update_All`'s own "you run
it when you want to" behaviour) — not gated behind a ROM scan at all. Filtering "show me only
cores I can actually use right now" stays valuable, but belongs to [the diagnostic
screen](#implemented-the-diagnostic-table) already built (it already has a `Filter` — see
[Requirement 5](#requirements) and the live-tested version of that screen), not to the
download step.

### Shared mechanism for both screens

- **`ArcadeDistribution`** (new): fetches `db.json.zip` via the existing `Downloader`
  (`raw.githubusercontent.com`, no new networking code — this is exactly the same mechanism
  already fetching box art), then **shells out to `unzip`** rather than implementing inflate —
  confirmed present on a real device this session (`/usr/bin/unzip`, `/usr/bin/gunzip`,
  alongside `curl`/`wget`), consistent with `Downloader`'s own existing philosophy of reusing
  what MiSTer's Linux already has rather than linking a new library for it.
- **A small, purpose-built manifest parser**, not a general JSON library — this document's
  own `MraFile` already set the precedent (a hand-rolled tag scanner over a full XML library,
  because the actual shape needed was narrow and fixed). `db.json`'s `files` map is a flat
  `"path": {"hash": "...", "size": N, "url": "...", ...}` structure; a v1 parser only needs to
  walk that one level and read three fields (`hash`, `size`, optional `url`) per entry, filtered
  by key prefix (`_Arcade/cores/` for cores, `_Arcade/` plus a `.mra` suffix, excluding
  `cores/`, for MRAs) — `tags`, `tangle`, `archives` and `tag_dictionary` can all be ignored for
  this scope, since path-prefix filtering already answers "is this an arcade core or MRA"
  without needing to interpret them.
- **MD5** (new): nothing in this codebase computes it today — `Archive.cpp`'s existing checksum
  is CRC32, a different algorithm, used for a different purpose (verifying a ROM part already
  believed to be the right file, not verifying a fresh download against a manifest). A small,
  self-contained implementation (the algorithm is public domain and short, on the order of what
  `Archive.cpp` itself already is) is the right scope — not a new linked dependency for one hash
  function.
- **Cache the manifest like `LibretroIndex` already caches its own index** — on the card, not
  the game drive, re-fetched only when stale (the manifest's own `timestamp` field is exactly
  what `Downloader_MiSTer` itself already uses this way).

### The two screens

Both the same shape, both syncing their whole side of the manifest unconditionally, behind one
explicit action — no ROM scan gating either one, per the precedent above:

- **Manage Cores**: every `_Arcade/cores/*.rbf` the manifest lists, against what is actually in
  that folder — Missing / Update available (manifest hash differs from the installed file's) /
  Up to date. Confirm to download, verify by MD5, install. Also — mirroring the manifest's own
  `tangle` semantics by hand, since this project would now be the thing doing the installing —
  remove a superseded dated duplicate of the same core rather than leaving it to accumulate,
  which is exactly the situation [`resolveCore()`'s own lexicographic-last
  heuristic](#rbf-version-selection) exists to paper over when nothing manages installation at
  all.
- **Manage MRAs**: the same shape, for every `.mra` the manifest lists directly under `_Arcade/`.
- **The diagnostic table stays the filter, not the download step.** Once both are up to date,
  [Arcade Games](#implemented-the-diagnostic-table) already answers "which of these can I
  actually play" — its ROM?/CRC? columns, driven by whatever is genuinely in `games/mame/`, are
  what turns "693 MRAs and 193 cores are now installed" into "here are the dozen that will
  actually boot." Two separate, simple jobs — sync everything official; then look at what that
  makes possible — rather than one job trying to be clever about both at once.

### The complete path, end to end — what the user actually asked to see sketched

1. **This GUI is installed.** (Already true today.)
2. **Settings → Manage Arcade → Manage Cores**, then **Manage MRAs** — one explicit action each,
   syncing the whole official set via `ArcadeDistribution`, same shape as `Update_All_MiSTer`'s
   own default "Main Distribution" sync, just scoped to `_Arcade/`.
3. **Settings → Manage Arcade → Arcade Games** (already built, live-tested this session — see
   [Implemented: the diagnostic table](#implemented-the-diagnostic-table)) — now has accurate
   Core?/ROM file? columns for the whole official catalog, not just whatever happened to be
   installed already. Its **ROM?/CRC? columns are where your own `games/mame/*.zip` collection
   finally enters the picture** — not as a pre-filter on what gets downloaded, but as the
   answer to what, out of everything now installed, is actually playable.
4. **A ROM this reports missing or CRC-mismatched** stays entirely the user's own responsibility
   to source — see the boundary below.
5. **Launching** — [Phase 1](#phase-1--list-launch-done), not yet built.

**Where this path deliberately, permanently stops:** ROM content itself. Cores and `.mra`
descriptors are MiSTer-devel's own open build artifacts — fetching them is exactly what
`Update_All_MiSTer` already does openly, and step 2 above is not meaningfully different from
that. The actual MAME ROM content is never part of `db.json`, was never going to be, and this
project will not add that separately either — consistent with this project's own README
disclaimer ("does not provide, host, link to, or in any way distribute copyrighted game ROMs").
Sourcing a ROM is, and stays, the user's own responsibility; this project's role stops at
*telling them precisely which file is missing*, which the diagnostic table already does. And,
per the Killer Instinct finding above, **this path's own reach stops at the official catalog
too** — a core still outside it needs the manual route this session actually used, same as
always.

### Mockup

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Manage Arcade › Manage Cores                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│  193 cores in the official distribution                                      │
│                                                                                │
│  ▸ Missing (4)                                                               │
│    Vanguard.rbf                                                      1.8 MB  │
│    ActFancer.rbf                                                     3.6 MB  │
│    …                                                                          │
│                                                                                │
│    Update available (3)                                                      │
│    ActFancer_20260601.rbf → _20260708.rbf                           3.6 MB   │
│    …                                                                          │
│                                                                                │
│    Up to date (186)                                                          │
│                                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│  A Download selected   X Select all missing/outdated   B Back               │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Open questions this decision adds

- Real download volume/time for a first full sync (621 MB of cores, 40.5 MB of MRAs, against
  the library this session tested against — 10,500+ games, about 3,200 unique Arcade `.mra`
  titles across two drives (6,400+ files) already
  installed) — not measured; `Update_All_MiSTer`'s own "~15 minutes the first time" is the one
  real data point so far, for its own, larger default selection.
- Whether `ArcadeDistribution` should generalise beyond Arcade later (the same manifest covers
  `_Console`/`_Computer`/etc. cores too) — explicitly out of scope for this document, flagged
  once, not designed further here.
- Whether the Arcade Organizer's `_Arcade/_Organized` convention (see above) is worth aligning
  with, if [Manufacturers/Categories browsing](#decision-a-dedicated-arcade-tab) ever needs an
  on-disk representation rather than a purely in-memory grouping — not needed for that decision
  as written, but a real prior art worth a glance before building it.

## Non-goals for v1

- **DIP-switch editing.** Real, wanted eventually, materially its own screen (a per-game settings
  UI nothing else in this GUI currently has an equivalent of) — not blocking a first usable
  Arcade tab.
- **Automating the HDD/CHD first-launch mount.** [Nothing found this session gives a way to do
  this from outside the running core](#hddchd-based-games-no-automated-mount-found) short of
  simulating OSD navigation and input — a materially bigger and riskier feature (the same
  category CONTROLLER.md flagged and deferred for a different reason: simulating input is not
  something this project currently does anywhere). Surfacing that a game needs the step
  (Requirement 4) is in scope; performing it is not.
- **Full ROM-set verification / building** (a `clrmamepro`-style "your set is a `1G1R` mismatch"
  checker). Existence-checking against MiSTer's own search order is the v1/Phase-2 bar; anything
  closer to what the `mra` tool itself does internally at load time is real scope, not assumed
  free.
- ~~**Category/manufacturer browsing or filtering.**~~ No longer a non-goal: the metadata exists
  and [the Arcade tab](#implemented-the-arcade-tab) browses it.

## Open questions

1. **Does Console Mode's distribution actually ship an `Arcade.ini` section file**, and if so, in
   what shape? Directly relevant to whether `Library.cpp`'s existing `Arcade.ini` wiring is worth
   keeping, repurposing, or removing now that real Arcade support exists. **Still open** — the
   device went offline in the session that wrote this before it could be checked, and nothing
   since has needed the answer: with a game database the file is never read. A plain "does this
   file exist, and what does it contain" check on a real device settles it.
2. **One `GameSystem("Arcade")`, or split by category/manufacturer at the data-model level rather
   than only in presentation?** **Decided and built:** one system plus a dedicated tab that
   browses it three ways — see [Decision: a dedicated Arcade
   tab](#decision-a-dedicated-arcade-tab). Manufacturer/category stay presentation concerns over
   the one underlying list, not a second data model.
3. **Where does Arcade metadata live?** **Decided and built** — see [Decision: extending "Build
   game database" for Arcade](#decision-extending-build-game-database-for-arcade): the existing
   row format, unchanged; manufacturer/category are more files in it.
4. **Is CRC verification (not just existence-checking) worth its I/O cost** for a library the
   size a real arcade set reaches? **Answered in practice:** it is done, for every `.mra`, in a
   full database rebuild that took 2 min 29 s on the reference device with 6,416 `.mra` files on
   two drives, and it is what makes the library trustworthy: without it the list holds every
   `.mra`, most of which have no ROMs behind them. Not split out from the rest of the rebuild, so the
   Arcade-only cost is unmeasured.
5. **What does the libretro thumbnail server actually offer for arcade**, and matched by which
   name? **Answered and built — see [Scraper and box art](#scraper-and-box-art).** `MAME`
   (5,824 titles), naming close enough to a `.mra`'s own name to expose a real bug in the
   existing matching along the way. Measured against the 849 games the reference library lists:
   418 exact matches, 230 more by the loose fallback, 201 with no match.
6. **What do the Boxart-large/Boxart-small presentations mean for a manufacturer or category
   tile**, which has no box art of its own? **Settled:** they do not — a group is a plain
   typographic tile, the same in every presentation. See [the tab](#implemented-the-arcade-tab).
7. **Open: are MAME's `Named_Snaps` a good background** for Arcade? Not looked at; backgrounds
   are skipped for Arcade until they have been.
8. **The original 201 misses were audited.** The audit found additional title matches in the
   Libretro index; see [BOXART.md](BOXART.md). A subsequent scrape was completed on the MiSTer,
   though its summary totals are not recorded in this repository. A later 0.4.0 preparation
   run over 11,502 entries also completed; its per-system Arcade totals were not recorded.
9. **Open: how the Arcade step's cost scales** on a slower card or a bigger library than the
   reference one — unmeasured beyond the single full-rebuild figure above.
