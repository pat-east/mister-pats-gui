# GUI Design

Design document: layout, behavior and rendering rules. The technical groundwork is described
in [README.md](README.md), the measurements in [PERFORMANCE.md](PERFORMANCE.md).

**Implementation status.** The basic framework is built and running; screenshots taken on the
device are in `build/shots/`. Deviations from this design that came out of the implementation:

- **Tabs:** Favorites · Systems · Games · Settings (Favorites was added).
- **Games** does not show the games of one system, but everything across all systems, in three
  sections: Recently played, Favorites, All games. The game list of a *single* system is a
  separate detail view, opened from the Systems tab.
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
| CPU | 2× Cortex-A9 @ 800 MHz | One core renders, the other loads/decodes in the background |
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

### System catalog: Console Mode's section files

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

Naming scheme: the basename of the ROM file plus `.png`, or `-BG.png` for the background
variant. On top of that there is a global `/media/fat/media/` (~4900 files, mostly arcade)
with an `optimized/` subfolder.

**The `-BG.png` variant is a gift for the parallax background** — we do not have to invent
artificial backdrops, every game brings its own.

### Fonts and symbols

`/media/fat/ConsoleMode/themeconfig/resources/`:

- `Akrobat-Bold.ttf`, `Akrobat-SemiBold.ttf` — a modern geometric sans, a good fit for the target look
- `promptfont.ttf` — controller button glyphs, ideal for the button hints along the bottom
- `back22.png` — background texture

### Network and time

Straight from the running Linux system: the time via `localtime()`, interface and IP via
`getifaddrs()`, Wi-Fi detection from the interface name (`wlan*` vs. `eth*`).

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
│  (A) Select        (B) Back        (Y) Search        (Start) Menu                    │  ← 80 px
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- **Top left:** the time, large (32 px, SemiBold), with the date below it in a small size
  (18 px, 55 % opacity).
- **Top center:** the three tabs. The active tab is white and bold with a 3 px underline in the
  accent color; inactive ones sit at 45 % opacity. When you switch tabs, the underline glides
  to its new position (180 ms).
- **Top right:** connection type plus signal strength as bars, with the IP address below it in
  a small size. On a wired connection the bars are omitted. With no connection at all, a muted
  red "Offline" appears there instead.
- **Bottom:** button hints, context-dependent, with glyphs from `promptfont.ttf`. Restrained,
  at 40 % opacity.
- **Safe area:** 80 px left/right, 40 px top/bottom — televisions crop the edges.

## 5. Screens

### 5.1 Systems (the first PoC)

A grid of tiles, freely navigable in all four directions.

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  14:32                       SYSTEMS   Games   Settings                 Wi-Fi ▪▪▪▪   │
│  Sunday, 21 September                                               192.168.64.163   │
├──────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                      │
│   CONSOLES                                                              18 systems   │
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
│  (A) Open        (Y) Search        (Start) Menu                                      │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- **Grid:** 6 columns. Tiles of 266×200 px with a 32 px gap, which together with the 80 px
  margin adds up to exactly 1920. Three rows are visible, with a page indicator below them
  (`● ○ ○`).
- **Groups:** one heading per group ("CONSOLES", "HANDHELDS", "COMPUTERS", "ARCADE", "PORTS")
  taken from the section files, with the number of systems found on the right. Groups run on
  within the same grid, separated by their headings — no separate scrolling per group.
- **Tile content:** the system logo if one exists, otherwise the name in Akrobat-Bold, wrapped
  automatically and fitted to the tile width. Below the tile, the number of games, in a small
  size.
- **Focus:** the focused tile grows to 112 %, becomes brighter, and gains a stronger shadow and
  a 3 px border in the accent color. Its neighbors do not move out of the way — the tile grows
  over them, with its shadow on top.
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
│  14:32                       Systems   GAMES   Settings                 Wi-Fi ▪▪▪▪   │
│  Sunday, 21 September                                               192.168.64.163   │
├──────────────────────────────────────────────────────────────────────────────────────┤
│                                                     ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│  NINTENDO 64                 287 games              ░░░  -BG.png as background  ░░░  │
│                                                     ░░░  softly darkened        ░░░  │
│    1080 Snowboarding                                ░░░                         ░░░  │
│    AeroFighters Assault                             ░░░      ┌─────────────┐    ░░░  │
│  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓               ░░░      │             │    ░░░  │
│  ┃  Banjo-Kazooie                   ┃               ░░░      │   Boxart    │    ░░░  │
│  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛               ░░░      │   large     │    ░░░  │
│    Diddy Kong Racing                                ░░░      │             │    ░░░  │
│    Donkey Kong 64                                   ░░░      └─────────────┘    ░░░  │
│    F-Zero X                                         ░░░                         ░░░  │
│    GoldenEye 007                                    ░░░   BANJO-KAZOOIE         ░░░  │
│    Mario Kart 64                                    ░░░   Nintendo 64 · .z64    ░░░  │
│    Super Mario 64                                   ░░░                         ░░░  │
│                                                     ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
├──────────────────────────────────────────────────────────────────────────────────────┤
│  (A) Start   (B) Back   (X) Favorite   (Y) View   (L2/R2) Letter                     │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- The focused list entry gets a bar in the accent color and white text; the remaining entries
  sit at 60 %.
- On the right, the `-BG.png` of the focused game as a darkened full-area background, with the
  boxart and its shadow in front of it. Moving between entries cross-fades (200 ms) — this is
  the most conspicuous animation in the interface, and it justifies its budget.
- **Loading:** only the visible entries plus five above and five below the focus are prepared.
  Decoding runs on a background thread; until the image is there, a placeholder with the game
  title stands in its place. An LRU cache caps memory use.
- The screen can alternatively be shown as a grid of tiles (the same tile as for systems, with
  the boxart as its content). Y cycles through five steps: List, Boxart large, Grid (with
  titles), Boxart small, Compact. At 1080p, Compact fits thirteen tiles into a row — meant for
  libraries where you already know what you are looking for and only need to get there.
- **The following row is deliberately cut off.** A list that ends flush with the bottom edge
  looks complete; a tile cut in half says "there is more below". Simply showing as many whole
  rows as happen to fit is not enough for that — the last one then ends flush by accident.
  Instead the row height is derived from the available area in such a way that exactly one
  third of a row is left over. In doing so the tile is only compressed, never stretched, and
  the boxart inside it keeps its own proportions anyway.

### 5.3 Settings

A plain list: categories on the left, the options of the selected category on the right. The
contents are not settled yet; candidates are Display (resolution, frame rate, reduce
animations), Sources (which section groups and directories), Boxart (clear cache) and System
(network, version, restart).

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
| Text fallback | Akrobat-Bold, wrapped automatically, fitted to the width |
| Caption | below the tile, 18 px, single line, centered, ellipsized when too long |
| Secondary line | optional, 15 px, 55 % opacity (e.g. "412 games") |
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
| X | Toggle favorite |
| Y | Change view (search is still to come) |
| L / R | Switch tabs (Systems ↔ Games ↔ Settings) |
| L2 / R2 | Jump to the previous/next initial letter |
| Start | Context menu |

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
| Heading | Akrobat-Bold 34 px, slightly increased letter spacing, uppercase |
| Tab | Akrobat-SemiBold 24 px, uppercase |
| Tile caption | Akrobat-SemiBold 18 px |
| Secondary text | Akrobat-SemiBold 15 px, 55 % opacity |
| Time | Akrobat-SemiBold 32 px |
| Button glyphs | promptfont 20 px |

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
