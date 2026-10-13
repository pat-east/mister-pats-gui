# GUI Design

Shared visual and interaction rules for new screens live in [DESIGN.md](DESIGN.md). This document
describes the GUI layout, tiles, navigation, and rendering rules. Some sketches and proposals below
record earlier design choices; current release changes are summarized in
[CHANGELOG.md](../CHANGELOG.md), with measurements in [PERFORMANCE.md](PERFORMANCE.md).

**Implementation status.** The basic framework is built and running; screenshots taken on the
device are in `build/shots/`. Deviations from this design that came out of the implementation:

- **Tabs:** Home · Favorites · Consoles · Arcade · Games · Settings (the Arcade tab appears
  only when the library has Arcade games — see [ARCADE.md](ARCADE.md#implemented-the-arcade-tab)).
- **Home** contains Recently played and Favorites rows. **Games** is a flat, alphabetized list
  across all systems. A single system's game list opens from Consoles.
- **System logos** are solved: the set from
  [KyleBing/retro-game-console-icons](https://github.com/KyleBing/retro-game-console-icons)
  (GPL-3.0) lives in `assets/icons/`, and `Icons` does the mapping. Systems without an icon
  fall back to typography — which is why the typographic design described below still applies.
- **Group headings** in the systems grid are still missing; instead the header line shows the
  group of the currently focused tile.
- **Parallax** is not implemented yet. The detail pane does already use a game's `-BG.png` as
  a darkened background, though.
- **Shadows** now exist only on the focused tile — for performance reasons, see
  PERFORMANCE.md. Against a dark background the difference is not noticeable.
- **Games views:** List, Grid and Boxart small remain. Boxart large and Compact were removed.
  Current covers use view-sized BMPs when available, and a bounded worker decodes them.
- **Splash and failure handling:** the first second animates to 25%, then icons are loaded
  sequentially; a fatal GUI exit shows a crash screen instead of an automatic boot loop.

## 1. Target Look

An Xbox-like dashboard: calm, generous, high resolution. Few large elements instead of dense
lists. Status information small and unobtrusive at the edges, content in the middle. Motion
used sparingly, but always smooth — every change of focus is animated, nothing jumps.

Guiding principles:

- **One focus, always visible.** At any moment exactly one element is active, clearly set
  apart by scale, brightness and shadow.
- **Content before chrome.** Boxart is the main subject; the interface recedes.
- **Everything reachable with the gamepad.** No element requires a keyboard or mouse.
- **A single kind of tile.** Systems and games use the same basic element, only with different
  content. See [section 6](#6-the-tile--the-central-gui-element).

## 2. Hard Constraints

The design has to respect these limits, or it will end up unusably slow.

| Factor | Value | Consequence for the design |
| --- | --- | --- |
| GPU | **none** | Everything is rendered in software on the CPU |
| CPU | 2× Cortex-A9 @ 800 MHz | Rendering stays on the app thread; a bounded worker decodes covers |
| Framebuffer | 1920×1080 × 32 bpp = 8.3 MB/frame | Redrawing the full screen at 60 fps is unrealistic |
| DDR3 bandwidth | ~1000 MB/s (per the MiSTer docs, for the scaler) | 30 fps full screen ≈ 250 MB/s just for blitting |
| Amount of boxart | NES alone has ~2750 games (5507 files) | Never load eagerly, only what is visible plus a look-ahead |

**Target frame rate: 30 fps**, using dirty-rectangle updates instead of full-screen redraws
wherever possible.

Consequences that shape the visual style:

- **Shadows are pre-baked, not computed.** No real-time Gaussian blur. Instead there is a
  single soft shadow sprite (9-slice, generated once) that is drawn underneath tiles and only
  varied in opacity.
- **Parallax with few layers.** Three layers are enough (background, content, foreground). The
  background moves with a factor of ~0.15, and it is displaced by an offset within an already
  scaled buffer — nothing is recomputed per frame.
- **Scaling is cached.** Boxart is brought to tile size once and kept at that size; it is never
  rescaled per frame.
- **Animations only affect small areas.** A change of focus animates the two tiles involved,
  not the entire grid.

If 1080p turns out to be too tight, one way out is to render internally at 1280×720 and let
the MiSTer scaler upscale (`fb_cmd1` takes an explicit width/height and scales by an integer
factor, centered). That would be the first lever to pull if the frame rate does not hold.

## 3. Data Sources on the Device

All of this has been verified to exist on the device — we are not inventing formats of our own.

### Historical source: Console Mode's section files

The current GUI builds its own game database; it can use older Console Mode data as a
fallback. This section records the source format that informed the initial design.

`/media/fat/ConsoleMode/themeconfig/section_groups/` contains `Console.ini`, `Computer.ini`,
`Handheld.ini`, `Arcade.ini`, `Ports.ini`. The structure, using `Console.ini` as an example:

```ini
[CONSOLES]
consoleList = 3DO,ADVENTURE VISION,ARCADIA 2001,ATARI 2600,…,SNES,SATURN,…

[ATARI 2600]
execs = none
romExts = .zip,.a26,.bin
romDirs = /media/fat/games/Atari2600/
```

That gives us, without any work of our own: display name, ordering, ROM directories and valid
file extensions per system.

**Rule "only systems that have games":** a system is only shown if at least one of its
`romDirs` contains at least one file with a matching extension from `romExts`. The check is
cheap — the first hit is enough, nothing has to be counted.

### Boxart

One `media/` subfolder per game directory:

```
/media/usb0/games/NES/media/10-Yard Fight (UE) [!].png       ← boxart
/media/usb0/games/NES/media/10-Yard Fight (UE) [!]-BG.png    ← background image
```

The GUI also recognizes legacy JPEG covers and the five prepared BMP variants described in
[BOXART.md](BOXART.md). Backgrounds remain JPEG/PNG. The global
`/media/fat/media/` collection (~4900 files, mostly arcade) was part of the earlier
device inventory and is not a requirement for this GUI.

**The `-BG.png` variant is a gift for the parallax background** — we do not have to invent
artificial backdrops, every game brings its own.

### Fonts and symbols

`/media/fat/mister-pat/fonts/`:

- `SpaceGrotesk-Bold.ttf` and `SpaceGrotesk-Medium.ttf` — the interface typeface, installed by
  the setup/update script
- `back22.png` — background texture from Console Mode, when present

Space Grotesk replaces Akrobat as the interface typeface. At startup the GUI uses Space Grotesk
when enabled and installed; otherwise it uses its built-in bitmap face. Missing or unreadable
font files do not prevent startup. Akrobat is no longer loaded by the GUI.

### Network and time

Straight from the running Linux system: the time via `localtime()`, interface and IP via
`getifaddrs()`, Wi-Fi detection from the interface name (`wlan*` vs. `eth*`), and the connected
SSID via `wpa_cli status` when the supplicant control interface is available. Settings → System
shows the SSID and Wi-Fi IP address; without a wireless interface it reports “No Wi-Fi adapter”.
When a Wi-Fi interface is present, **Connect to Wi-Fi…** scans and tests a candidate network
before saving it to MiSTer's Wi-Fi configuration.

## 4. Global Layout

Applies to every screen. Dimensions are for 1920×1080.

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  14:32                       SYSTEMS   Games   Settings                 Wi-Fi ▪▪▪▪   │  ← 88 px
│  Sunday, 21 September                                               192.168.64.163   │
├──────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                      │
│                                                                                      │
│                               C O N T E N T   A R E A                                │  ← 912 px
│                                                                                      │
│                                                                                      │
├──────────────────────────────────────────────────────────────────────────────────────┤
│  (A) Select        (B) Back        (Up) Search        (Start) Menu                    │  ← 80 px
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- **Top left:** the time, large (32 px, SemiBold), with the date below it in a small size
  (18 px, 55 % opacity).
- **Top center:** the tabs. The active tab is white and bold with a 3 px underline in the
  accent color; inactive ones sit at 45 % opacity. In a modal submenu or error dialog, the
  underline turns gray to show that L1/R1 cannot switch tabs. When you switch tabs, the
  underline fades out, moves to the new tab, and fades back in (45 ms per fade).
- **Top right:** connection type plus signal strength as bars, with the IP address below it in
  a small size. On a wired connection the bars are omitted. With no connection at all, a muted
  red "Offline" appears there instead.
- **Bottom:** context-dependent button hints, rendered as compact text. Restrained, at 40 %
  opacity.
- **Safe area:** 80 px left/right, 40 px top/bottom — televisions crop the edges.

### Modal dialogs and confirmation actions

- Dim the current screen and show a centered, rounded panel so the user keeps context. Keep modal
  contents within the safe area and leave enough space for the global footer.
- Keep controller hints in the global bottom-left footer. They are the canonical instruction for
  which buttons act and must remain visible while a modal is open.
- Dialogs show no buttons for now. The next action is in the footer hint (see `DESIGN.md`,
  "Buttons").
- Use **B Back** (or a context-specific label such as **B Close**) to leave a dialog.
- Confirmation dialogs summarize what will change before the primary action. Keep the action
  explicit (for example, **Confirm and install**) and state relevant exclusions or restart behavior
  in the dialog body.
- Longer lists and progress views may use a larger modal, but retain the same dimmed background,
  rounded surface, focus treatment, and footer control pattern.

## 5. Screens

### 5.1 Consoles (the first PoC)

A grid of tiles, freely navigable in all four directions.

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  14:32                       SYSTEMS   Games   Settings                 Wi-Fi ▪▪▪▪   │
│  Sunday, 21 September                                               192.168.64.163   │
├──────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                      │
│   CONSOLES                                                              18 consoles  │
│                                                                                      │
│   ┌────────┐  ┌────────┐  ┏━━━━━━━━━━┓  ┌────────┐  ┌────────┐  ┌────────┐           │
│   │        │  │        │  ┃          ┃  │        │  │        │  │        │           │
│   │  NES   │  │  SNES  │  ┃   MEGA   ┃  │  N64   │  │  PSX   │  │ SATURN │           │
│   │        │  │        │  ┃  DRIVE   ┃  │        │  │        │  │        │           │
│   └────────┘  └────────┘  ┃          ┃  └────────┘  └────────┘  └────────┘           │
│    412 games   784 games  ┗━━━━━━━━━━┛   287 games  1204 games   96 games            │
│                            1123 games                                                │
│                            ▲ focused                                                 │
│   ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐             │
│   │ MASTER │  │  GAME  │  │ TURBO  │  │ COLECO │  │ ATARI  │  │  NEO   │             │
│   │ SYSTEM │  │  GEAR  │  │GRAFX-16│  │ VISION │  │  2600  │  │  GEO   │             │
│   └────────┘  └────────┘  └────────┘  └────────┘  └────────┘  └────────┘             │
│    203 games   158 games   312 games   94 games    487 games   218 games             │
│                                                                                      │
│   ● ○ ○                                                                              │
├──────────────────────────────────────────────────────────────────────────────────────┤
│  (A) Open        (Up) Search        (Start) Menu                                      │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- **Grid:** 6 columns. Tiles of 266×200 px with a 32 px gap, which together with the 80 px
  margin adds up to exactly 1920. Three rows are visible, with a page indicator below them
  (`● ○ ○`).
- **Groups:** one heading per group ("CONSOLES", "HANDHELDS", "COMPUTERS", "ARCADE", "PORTS")
  taken from the section files, with the number of systems found on the right. Groups run on
  within the same grid, separated by their headings — no separate scrolling per group.
- **Tile content:** the system logo if one exists, otherwise the name in Space Grotesk, wrapped
  automatically and fitted to the tile width. Below the tile, the number of games, in a small
  size.
- **Focus:** a focused cover tile keeps its size and gets a 3 px border in the accent color; the
  artwork is never scaled up and has no shadow (box art is prepared at its display size, see
  `DESIGN.md`). Tiles without cover art, such as system icons, still grow slightly and gain a
  shadow. Neighbors do not move out of the way.
- **Background:** very dark, with a soft radial gradient behind the focused tile that follows
  its position with a slight lag (parallax layer 1, factor 0.15).
- **Scrolling:** if the focus moves below the last visible row, the grid scrolls by exactly one
  row (220 ms, ease-out). No free pixel scrolling.

**For the PoC this is enough:** fill the grid from the section files, move the focus, animate
the focus change, draw the tile with its text fallback. Logos, group headings and parallax can
follow later.

### 5.2 Games

Two columns: the list on the left, a large presentation of the focused game on the right.

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  14:32                      Consoles   GAMES   Settings                 Wi-Fi ▪▪▪▪   │
│  Sunday, 21 September                                               192.168.64.163   │
├──────────────────────────────────────────────────────────────────────────────────────┤
│                                                     ┌─────────────────────────────┐  │
│  NINTENDO 64                 287 games              │                             │  │
│                                                     │                             │  │
│    1080 Snowboarding                                │       Centered boxart       │  │
│    AeroFighters Assault                             │                             │  │
│  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓               │                             │  │
│  ┃  Banjo-Kazooie                   ┃               └─────────────────────────────┘  │
│  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛                                             │
│    Diddy Kong Racing                                                               │
│    Donkey Kong 64                                                                  │
│    F-Zero X                                                                        │
│    GoldenEye 007                                                                   │
│    Mario Kart 64                                                                   │
│    Super Mario 64                                                                  │
├──────────────────────────────────────────────────────────────────────────────────────┤
│  (A) Start   (B) Back   (X) Favorite   (Y) View   (L2/R2) Letter                     │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- The focused list entry gets a bar in the accent color and white text; the remaining entries
  sit at 60 %.
- The selected cover is centered in the right panel. This view does not load or draw a
  background image. If the cover is missing, the game title appears large and centered instead.
- In Favorites, each left-hand row shows the system name below the game title in smaller,
  muted text. The detail panel also identifies the system beneath the title.
- **Loading:** visible covers and nearby entries are requested from one bounded worker.
  While the selected cover loads, the panel says “Loading cover art...”. The decoded cache is bounded by entry
  count and RAM use; stale queued requests are discarded after navigation changes.
- **Redrawing:** focused rows are repainted as their focus changes; the selected-game panel is
  repainted when its content changes. This avoids rebuilding static list text every frame.
- The screen can alternatively be shown as a grid of tiles (the same tile as for systems, with
  the boxart as its content). Y cycles through List, Grid (with titles) and Boxart small. In
  **Settings → Interface → Game view**, choose one view for every area or **Individual** to save
  a separate view for Home search results, Favorites, console game lists and Games. In Individual
  mode, Y changes and saves the view for the current area.
- **Boxart fade-in:** newly loaded, unfocused covers in Home and the Games grid appear through
  three fixed pixel-reveal phases: 50%, then 25%, then the final 25%, with 20 ms between phases.
  The focused cover appears immediately. See [BOXART.md](BOXART.md#boxart-fade-in-and-cache).
- **The following row is deliberately cut off.** A list that ends flush with the bottom edge
  looks complete; a tile cut in half says "there is more below". Simply showing as many whole
  rows as happen to fit is not enough for that — the last one then ends flush by accident.
  Instead the row height is derived from the available area in such a way that exactly one
  third of a row is left over. In doing so the tile is only compressed, never stretched, and
  the boxart inside it keeps its own proportions anyway.

### 5.3 Contextual search

Home, Games, Favorites, a system's game list and Arcade each show the same search field below
their screen title. The field is not focused on entry; **Up** from the first game row focuses it,
then **A** opens the reusable on-screen keyboard. The compact keyboard includes a separate
numeric keypad on the right. **B** cancels the draft, **Y** inserts a space, **X** deletes the
last character, and **Start** submits. Select **Symbols** with **A** to switch layouts; **Shift**
toggles letter case. Submitting an empty query restores the complete collection for that area.

Search is scoped to the current area: Home and Games use all games; Favorites uses only favorites;
a system list uses only that system; Arcade searches the Arcade game collection regardless of its
manufacturer or category grouping. Multiple terms are matched in any order, and every term must
match the beginning of a word in a game title. Home, Games and Favorites also search system names;
system lists and Arcade search titles only. Matching ignores case, so `cap com` finds “Captain
Commando”. Results use that area's normal game actions, letter jump and available views. Search
queries remain independent between areas. There is no Search tab, and Settings and the Systems
overview have no search field.

### 5.4 Settings

Settings uses three columns: categories on the left, the selected category's actions and
options in the middle, and a larger explanation of the selected option on the right. Use
up/down to move within the focused column and left/right to switch between categories and
options. Choose **Settings → System → Info** to open system facts in a modal. The categories are
Library, Controllers, Interface, Accessibility, Updates, and System. Interface settings include
**Game view**—Grid, Boxart small, List or Individual—tab display as names, icons or both, and
interface sound controls.
Accessibility settings include the Space Grotesk font switch or download action, three font sizes
(Standard, Larger and Largest), three sizes for the controller hints at the bottom left, and a
high-contrast switch with a choice of highlight color (White, Pink, Yellow, Cyan, Lime Green,
Electric Blue, Orange, Lavender). In high contrast the highlight color carries the focus outline,
progress bars, hints and selected values, and the search bar and Settings rows get a white border
(except with White). Clear focus indicators remain visible throughout the interface. Time format and time zone are in System, directly below **Info**. The six selected
Bootstrap Icons are recorded in `assets/tab-icons/manifest.tsv`; their upstream SVGs, license and
BMP conversion script are kept alongside them for future additions. The script produces 26×26
24-bit BMPs with a solid black background, matching the tab icon's 1080p display size and
avoiding runtime resizing there. Separate **Start sound** and **Menu sounds** switches are also
available. Both sound options
default to on. The start sound plays after the splash screen; menu sounds cover navigation,
tab changes, and successful game launches. The menu-sounds switch controls those three cues together. **Show Arcade in Consoles** is off by
default. Toggle settings show a round checkbox
beside **On** or **Off** in a fixed-width value slot, keeping checkbox columns aligned. The
dark-blue outline stays empty when off and contains an inset light-gray circle when on. The
time-zone picker lists the
continent/region folders and city files in `/usr/share/zoneinfo/posix`; choosing one copies its
zoneinfo file to `/media/fat/linux/timezone`, following MiSTer's `Scripts/timezone.sh` method.
**Info** opens hardware, storage, network and software details in a modal. System also includes
the full **Changelog**, loaded from the project on GitHub with loading and retry
states, plus scroll arrows when the text extends beyond the view. It also includes **Download
Assets**, which checks for missing console and tab icon BMPs and fetches only those assets. GUI updates also fetch missing tab icons from the installed release's
manifest. **Info** shows Ethernet and Wi-Fi details under **Networking**, with the Wi-Fi IP and
network name adjacent. System also includes **Connect to Wi-Fi…**, **Start MiSTer Core**, and **Reboot MiSTer**.
Reboot requires a separate confirmation and syncs pending writes before requesting the Linux
system action. Wi-Fi setup scans when an adapter is present; it supports open and WPA-PSK
networks, hidden SSIDs, a password keyboard with Show/Hide and Start-to-confirm, a temporary
connection test, rollback on failure or cancellation, and a final save to MiSTer's
`wpa_supplicant.conf`. Without a Wi-Fi interface, setup is disabled. Restart MiSTer to load saved
settings automatically.
See [README.md](../README.md) for the
user-facing feature list.

Updates keeps the automatic-check switch and a manual **Check now** action. A verified newer
release adds **Update now**. Its confirmation shows the version, release notes, restart notice
and any game-database warning. Download and verification progress stays visible after closing
the confirmation; B can cancel before the installation commit. Updates also shows the running GUI version and the version selected for
the next start. A valid `mister-pats-gui.so` symlink appears as a version pin; without it the
loader selects the newest installed library. The release comparison uses the pinned version
when there is one; an update adds only the new library and preserves any pin.
After installation, a success dialog offers **A** to restart MiSTer with the current startup
selection, **B** to continue without restarting, and, when pinned, **X** to remove the pin and
restart. It shows the version each restart choice will load. After B, **Restart options**
opens the dialog again.
Whenever a version pin exists, **Remove pinned version** appears as its own Updates action.
It removes the symlink, selects the newest installed library for the next start, and offers
**A** to restart or **B** to continue with the running GUI.
At startup, an enabled automatic check shows **Waiting for Internet** while it tries GitHub
HTTPS for about 60 seconds. If it cannot connect, Settings shows an error and **Check now**
allows a manual retry. **Checking GitHub** appears only after a successful HTTPS response.

## 6. The Tile — the Central GUI Element

One element, the same everywhere. Only the content and the caption change.

```
        ┌──────────────────────────┐        NORMAL state
        │                          │        · scale 100 %
        │                          │        · content at 85 % brightness
        │         Content          │        · soft shadow, 35 % opacity
        │                          │        · no border
        │                          │
        └──────────────────────────┘
                  Caption


      ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓      FOCUSED state
      ┃                              ┃      · scale 112 %, centered
      ┃                              ┃      · content at 100 % brightness
      ┃           Content            ┃      · larger shadow, 60 % opacity
      ┃                              ┃      · 3 px border in the accent color
      ┃                              ┃      · caption white, bold
      ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                  Caption
```

Specification:

| Property | Definition |
| --- | --- |
| Base size | 266×200 px (systems), 200×280 px (games, portrait boxart) |
| Corners | 8 px radius |
| Content | image, fitted preserving its aspect ratio (`contain`), or text fallback |
| Text fallback | Space Grotesk, wrapped automatically, fitted to the width |
| Caption | below the tile, 18 px, single line, centered, ellipsized when too long |
| Secondary line | optional, 17 px, 55 % opacity (e.g. "412 games") |
| Shadow | pre-baked 9-slice sprite, only opacity and size vary |
| States | `NORMAL`, `FOCUSED`, `PRESSED` (96 % for 80 ms), `LOADING` (pulse in the placeholder) |

Transition `NORMAL` ↔ `FOCUSED`: 160 ms, ease-out. Scale, brightness and shadow opacity are
interpolated together.

## 7. Navigation

| Input | Effect |
| --- | --- |
| D-pad / stick | Move the focus within the grid or the list, freely in four directions |
| A | Select (open a system, start a game) |
| B | Back; no effect at the topmost level |
| Select | Open the selected game's actions: play it or find/prepare its cover art |
| Hold X for 2 seconds | Add or remove the selected game from favorites; a progress bar fills while held |
| Y | Change view |
| L1 / R1 | Switch tabs when the underline is blue |
| L2 / R2 | Jump to the previous/next initial letter |

Behavior:

- **No wrap-around** at the edges. At the right-hand edge the focus stays where it is instead
  of jumping to the next row — on a television, wrapping is disorienting.
- **Key repeat:** 400 ms delay, then every 90 ms. While a direction is held, focus animations
  are shortened (60 ms) so that running quickly through a list does not feel sluggish.
- **Keyboard in parallel:** the arrow keys, Enter and Escape act like the D-pad, A and B.
- **Letter jumping as a layer of its own.** A single console can reach well over a thousand
  titles — the PlayStation on the test device has 1730. With the D-pad alone that stops being
  navigation, which is why the L2/R2 shoulder buttons skip over whole initial letters. Going
  backwards, the first press moves to the start of the current letter and only the second one
  to the previous letter — when you are in the middle of a large group, what you usually want
  is the start of that group.
- **The row jumped to moves up**, instead of leaving the focus stuck at the bottom edge.
  Otherwise a jump looks just like a single step.
- **Analog triggers are only read as triggers** if the axis reports a minimum of zero when the
  device is opened. On some pads L2/R2 sit on `ABS_Z`/`ABS_RZ`, on others that is where the
  right stick lives; a trigger rests at its minimum, a stick rests in the middle. That tells
  them apart reliably, without having to know devices by name.

## 8. Typography and Color

| Role | Definition |
| --- | --- |
| Heading | Space Grotesk Bold 36 px, slightly increased letter spacing, uppercase |
| Tab | Space Grotesk Medium 24 px, uppercase |
| Tile caption | Space Grotesk Medium 18 px |
| Secondary text | Space Grotesk Medium 17 px, 55 % opacity |
| Time | Space Grotesk Medium 32 px |

Colors (placeholders, still to be agreed on):

| Role | Value |
| --- | --- |
| Background | `#0B0D14` |
| Surfaces (tile, normal) | `#161A26` |
| Accent (focus) | `#4C8DFF` |
| Text, primary | `#FFFFFF` |
| Text, secondary | `#9AA4C0` |
| Warning / offline | `#E06A5A` |

A dark scheme, because the interface usually runs on a television in a dimly lit room, and
boxart looks better against a dark background.

## 9. Open Questions

1. **System logos.** Console Mode only has group images (`console1.png`, `arcade1.png`, …), no
   per-system logos. Attractive tiles would need one image per system. Create them ourselves,
   obtain them from another source, or stay with well-set typography?
2. **The Games tab with no system selected.** What does "Games" in the top menu show when no
   system has been chosen yet — all games across all systems, recently played, or favorites?
3. **Matching boxart.** The naming scheme requires an exact match with the ROM basename. Where
   names diverge (region codes, `[!]` markers), some normalization is needed — Console Mode's
   `alias.txt` could help, but it still has to be checked.
4. **Resolution.** Stay at 1080p, or render internally at 720p and let the scaler upscale? This
   will be decided by the measured frame rate.
5. **Favorites and history.** Our own file, or can we piggyback on Console Mode's `.state`?

## 10. Possible extension: game details

The existing **SELECT** action menu could offer a game details view for a selected ROM. It
could reuse the Disc tab's layout for title metadata, box art and available in-game screenshots.
Load data asynchronously and cache successful results locally. Keep the view useful without a
network key by showing available Libretro artwork; TheGamesDB can supply additional metadata
and images when configured. Keep the existing start, version-choice and artwork actions
available. See [GAME-VERSIONS.md](GAME-VERSIONS.md) for version choices and
[THEGAMEDB.md](THEGAMEDB.md) for scraper behavior and matching constraints.
