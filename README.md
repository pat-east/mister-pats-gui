# MiSTer Pat's GUI

A modern game launcher for the [MiSTer FPGA](https://github.com/MiSTer-devel/Main_MiSTer),
written in plain C++ for the device's own Linux side. It boots straight into a 1080p tiled
interface with box art, favourites and history — and hands control back to the MiSTer as soon
as you start a game.

It is not a core. Nothing about the FPGA changes. This is a frontend that picks a game and
asks the MiSTer to load it, the same way the stock menu does.

<!-- screenshots:start -->

| Home | Favorites |
| --- | --- |
| ![Home](screenshots/home.png) | ![Favorites](screenshots/favorites.png) |

| Systems | Arcade |
| --- | --- |
| ![Systems](screenshots/systems.png) | ![Arcade](screenshots/arcade.png) |

Every system's games in the presentation you prefer — Y cycles through them. Here, SNES in Grid, Boxart small and List:

| Grid | Boxart small |
| --- | --- |
| ![SNES, grid view](screenshots/snes-grid.png) | ![SNES, small box art view](screenshots/snes-small.png) |

| List | Games |
| --- | --- |
| ![SNES, list view](screenshots/snes-list.png) | ![Games](screenshots/games.png) |

Settings, including Interface, Accessibility, System, the Changelog, the update page and controller input test:

| Settings | Interface settings |
| --- | --- |
| ![Settings](screenshots/settings.png) | ![Interface settings](screenshots/settings-interface.png) |

| Accessibility settings | System settings |
| --- | --- |
| ![Accessibility settings](screenshots/settings-accessibility.png) | ![System settings](screenshots/settings-system.png) |

| Updates | Controller input test |
| --- | --- |
| ![Updates](screenshots/settings-updates.png) | ![Controller input test](screenshots/controller-test.png) |

<!-- screenshots:end -->

## Why

The MiSTer's stock menu is a file browser. It is fast, precise and does exactly what it was
built to do — but it looks like a file browser, and on a television across a room it reads
like one too.

This project takes the other approach: box art first, a tile grid you can cross with a
D-pad, and the things you actually reach for — recently played, favourites — on the first
screen. It aims to look like a console's own dashboard rather than a directory listing.

The constraint that shapes everything: the DE10-Nano has two Cortex-A9 cores at 800 MHz and
**no GPU**. Every pixel is drawn by the CPU into a framebuffer. That rules out the usual
toolkits and rules in careful, measured software rendering — see
[PERFORMANCE.md](docs/PERFORMANCE.md) for what that costs and where the budget went.

## The premise: easy to use, easy to understand

Every decision in this project is measured against one premise: **easy to use, easy to
understand.** It is drawn on throughout development, and a feature that cannot meet it is
changed until it does, or left out.

In practice that means:

- **No explanation needed.** Someone who picks up the pad for the first time should know what
  a screen does and what the buttons do, without a manual.
- **Order and behaviour can be worked out.** Either an arrangement is logical on its face —
  alphabetical order is the standard example — or it is made visible on screen, for instance by
  showing a count next to each entry. A clever ordering that the user has to guess at fails
  both tests.
- **Reduction first.** When both are possible, prefer the simpler one that needs no extra
  visual explanation. Less on screen, fewer rules to learn.


- **Tile grid** with box art in three presentations per system: list with a large preview,
  grid with titles, and small box art.
- **Home** with recently played games and favourites; **Games** is the flat, alphabetized list
  across every system.
- **Boxart fade-in animation** — newly loaded, unfocused covers appear in the same three fixed
  pixel-reveal phases across Home and the Games grid. Focused covers appear immediately.
- **Contextual game search** in Home, Games, Favorites, system game lists and Arcade. Each search
  is limited to that area; the reusable controller-operated keyboard has a separate numeric
  keypad. Settings and the Consoles overview have no search field.
- **Letter jump** on the shoulder triggers. A single console can hold a few thousand titles;
  L2/R2 skip whole initial letters.
- **Favourites and history**, stored as plain text files you can read and edit.
- **Arcade.** MiSTer's arcade library does not fit "one folder, one core", so it has its own
  game validation: core installed, ROM zip found, every CRC correct, checked the way MiSTer
  searches for them. The Arcade tab opens the games directly in the configured game view;
  box art matches the exact ROM revision. A diagnostic table explains why a game is missing.
  See [ARCADE.md](docs/ARCADE.md).
- **Its own game database.** A scan works out which systems are installed and what is in
  them, and writes one index file per system. Opening a console reads that one file and
  nothing else.
- **Its own box art preparation**, from Settings — converts existing covers into view-sized
  BMPs and fetches missing art from the libretro thumbnail server. A bounded image worker
  loads covers while the menus remain responsive.
- **Opens instantly, even at 10,000+ games.** A system's list — or the whole library, merged
  and sorted, in the Games tab — fills in progressively rather than blocking on every game's
  artwork before showing anything.
- **Console icons** for systems without art, with PNG fallback for older installations.
- **Tab icons**, with names, icons or both. Settings can download only missing BMPs; GUI updates
  also fetch missing tab assets from that release.
- **Download Assets**, in Settings → System — checks and downloads missing console and tab icons.
- **Accessibility**, in Settings — choose a larger interface font and high contrast. Focused
  controls have visible indicators throughout the interface.
- **Full Changelog**, in Settings → System — load and read the project's complete changelog from
  GitHub, with retry and scroll indicators.
- **Consoles** overview — the Systems tab, renamed, lists consoles with games. Arcade is
  excluded by default; **Show Arcade in Consoles** in Settings → Interface adds it to the grid.
  Manage systems can hide entries, and the Games tab can be switched off for a library large
  enough that browsing it flat stops being useful.
- **Game view**, in Settings → Interface — choose Grid, Boxart small or List for every game area, or
  choose Individual to remember a separate view for Home search results, Favorites, console
  game lists in Consoles, and Games. Press Y while browsing to change the current area's view.
- **Clock settings**, in Settings → System — choose a 12-hour AM/PM or 24-hour clock and set
  Linux's time zone using the MiSTer zoneinfo file.
- **Interface sounds**, in Settings → Interface — control the splash startup sound separately
  from menu sounds for navigation, tab changes, and successful game launches.
- **Wi-Fi setup**, in Settings → System — scan for networks, connect to open or WPA-PSK
  networks (including hidden SSIDs), test the connection, and save the profile for future boots.
  Passwords start masked and can be shown temporarily. Setup is disabled without a Wi-Fi
  interface; the complete flow was tested on MiSTer hardware.
- **Reboot MiSTer**, in Settings → System — confirm before restarting the system.
- **A box art switch**, in Settings — hide cover and background artwork while browsing, useful
  for comparing menu responsiveness with image loading out of the way.
- **Multiple game drives**, resolved live rather than assumed — a drive can come back at a
  different `/media/usbN` after a reboot, or a second volume can hold more games, and box art
  and launching both keep working either way.
- **Updates in Settings** — *Check now* works at any time; *Update now* confirms release notes,
  verifies and adds the new GUI library, then offers restart, later, and, when pinned, remove
  pin and restart. **Remove pinned version** is also available directly in Updates whenever
  a pin exists; it removes the symlink and offers a restart. Without a version pin,
  the loader starts the newest installed library. The optional automatic check is off by default;
  when enabled it waits for Internet access before showing *Checking GitHub*.
- **Starts games through the MiSTer's own loader** — core loading, ROM mounting and the
  in-game OSD all stay in the code that already does them well.
- **Boots straight into the GUI** via a small patch to the MiSTer main binary, which
  also teaches it to recognise a DVI display even when the video mode is pinned in the INI.
- **Designed for 1080p.** The interface scales to the framebuffer dimensions, but other output
  resolutions have not yet been verified on hardware.

## Requirements

- A MiSTer (Terasic DE10-Nano) with a working SD card setup
- SSH access to the device
- Your game library on the SD card or one or more USB volumes, wherever the MiSTer finds it
- Console Mode is not required for anything — see the next section
- To build it yourself: macOS or Linux with an `arm-unknown-linux-gnueabihf` cross-toolchain

## Console Mode is not required

This GUI finds your systems and games itself, and fetches its own box art and background
images from the libretro thumbnail server — from Settings, once the database is built.
[Console Mode](https://github.com/Retro-Remake/ConsoleMode_Distribution) by Retro Remake is not
a dependency for any of that:

| What | Where it comes from |
| --- | --- |
| The catalogue of systems | **built by this GUI** |
| The index of games | **built by this GUI** |
| Box art and background images | **prepared and fetched by this GUI** — Settings → *Prepare box art* |
| Fonts | Space Grotesk and its license are downloaded by the install/update script or from Settings → Accessibility; the built-in fallback remains available |

The setup script downloads Space Grotesk from its upstream project under the SIL Open Font
License, along with the license text. If those files are missing, Settings → Accessibility can fetch
them directly. Space Grotesk is enabled by default when available; the same setting can disable
it to check the fallback typeface. Routine GUI updates only add a new `.so`.

Once installed, this GUI takes over the boot path and Console Mode no longer starts. Only its
font files are still read, if present.

## Installation

See **[INSTALL.md](docs/INSTALL.md)** for the full procedure, including how to undo it.

In short: everything the GUI owns lives in one directory on the SD card,
`/media/fat/mister-pat`. Two lines in `MiSTer.ini` point the boot path at it. Nothing else on
the system is modified, and uninstalling means deleting that directory and those two lines.

## Building from source

```sh
# 1. Cross-toolchain (macOS example)
brew tap messense/macos-cross-toolchains
brew install arm-unknown-linux-gnueabihf

# 2. PIC static dependencies: zlib, libpng, freetype, libjpeg-turbo
third_party/build.sh

# 3. The GUI itself
make                    # produces build/mister-gui and build/mister-pats-gui-X.Y.Z.so

# 4. The patched MiSTer main binary that boots into it
git clone https://github.com/MiSTer-devel/Main_MiSTer third_party/Main_MiSTer
cd third_party/Main_MiSTer
git apply ../../patches/0001-autostart-gui.patch
make BASE=arm-unknown-linux-gnueabihf -j4
```

The GUI is a versioned shared library with `libstdc++` linked statically; the small loader
stays at the configured `gui=` path. Builds keep DWARF debug information and frame pointers
even at `-O2`. Keep both matching `.debug` files and `logs/module-map.log` from the crash run:

```sh
tools/symbolize-crash.sh crash.log build
```

Deploy the current build and patched MiSTer launcher:

```sh
make deploy                              # build, deploy the current version, reboot
tools/screenshot.sh                      # fetch what is on screen as a PNG
make -C tests                            # host-side checks, no device needed
```

## Controls

| Input | Action |
| --- | --- |
| D-pad / stick / arrow keys | Move |
| A / Enter | Open, or start the game |
| B / Escape | Back |
| Select | Open game actions: Start Game or Load box art for the selected game |
| X / F | Toggle favourite |
| Y / V | Cycle view |
| LB / RB, Page up/down, Tab | Switch tab |
| LT / RT (L2/R2), `,` / `.` | Jump to the previous/next initial letter |
| Menu button, long press | Leave a running game and return to the GUI |
| Q | Quit |

In Home, Games, Favorites, a system's game list or Arcade, press **Up** from the first game row to
focus its search field, then **A** to open the on-screen keyboard. While it is open, **B** cancels
the draft, **Y** inserts a space, **X** deletes the last character, and **Start** submits the
search. Select the on-screen Symbols key with **A** to switch layouts; Shift toggles letter case.
Search terms match the beginnings of words in game titles; Home, Games and Favorites also match
system names. All terms must match, in any order. Clear the field and submit to restore the full
current collection.

While the GUI is running, it grabs controller devices exclusively so button presses do not
also reach the terminal or MiSTer OSD. It releases them when the GUI exits or hands control to
a core, so core input works normally.

## Tested controllers

| Controller | Connection | Notes |
| --- | --- | --- |
| Xbox Series X pad | 8BitDo Adapter 2 | The daily driver used for most of this project's own development |
| Retro-Bit Sega Saturn pad | 2.4 GHz wireless adapter | D-pad only, no analogue stick. Works great once switched to X-Input mode — see the [pad's manual](https://retro-bit.com/sitepad-data/uploads/2025/07/NA-Saturn-Pro-2.4-GHz-Wireless-Controller_11-16-23.pdf) for the mode switch |
| Xbox 360 pad ×2 | Xbox 360 Wireless Receiver for Windows | Needs a `deadzone=` set to behave — see [CONTROLLER.md](docs/CONTROLLER.md#3-deadzone) — and [INSTALL.md](docs/INSTALL.md) for the vendor/product gotcha that surfaced along the way |
| PS4 controller (DualShock 4) | Wired (USB cable), and Bluetooth | Bluetooth pairing survives a reboot — the pad reconnects on its own, no re-pairing needed |
| 8BitDo Arcade Stick (Xbox version) | 2.4 GHz wireless dongle, and wired (USB cable) | Works flawlessly over both, no extra setup needed |

Still to try: PS5 (DualSense) and the Saturn pad wired. See [CONTROLLER.md](docs/CONTROLLER.md) for
the controller-management feature this is building towards.

## Configuration

| File | Purpose |
| --- | --- |
| `/media/fat/mister-pat/favorites.txt` | Favourites, one per line |
| `/media/fat/mister-pat/history.txt` | Recently played |
| `/media/fat/mister-pat/hidden-systems.txt` | Consoles hidden from the Consoles tab, one per line — see *Manage systems* in Settings |
| `/media/fat/mister-pat/preferences.txt` | Interface options, including sound, tab visibility, time settings and saved game views |
| `/media/fat/mister-pat/systems.conf` | Per-system loader slot overrides, see [the example](assets/systems.conf.example) |
| `/media/fat/mister-pat/icons/` | Console icons |
| `/media/fat/mister-pat/gamesdb/` | The game database: `catalog.tsv`, `roots.tsv` and one `<System>.tsv` per system |
| `/media/fat/mister-pat/logs/crash.log` | Fatal error details, when a crash log could be written |

The GUI also takes command-line options, which is how every screen can be reached and
captured without a controller:

```
--tab home|favorites|systems|arcade|games|settings  screen to open
--view list|grid|small                  game presentation
--system NAME                            preselect a system
--frames N                               render N frames, then exit
--dump PATH                              write the finished canvas to PATH
--press LIST                             feed button presses first, e.g. down,select,confirm
--launch-now                             start the preselected game at once
--dry-run                                print the MGL instead of loading it
--no-wizard                              never open the database wizard
--scan                                   build the game database and exit
--no-input                               do not open the input devices
--exclusive                              grab inputs exclusively (default)
--stats                                  report where frame time is spent
--full-redraw                            repaint everything every frame
--no-splash                              skip the startup splash
--splash-ms N                            set its initial wait in ms (default 1000)
```

## Repository layout

```
src/              the GUI, one class per file
tests/            host-side checks (damage tracking, letter jump, game database)
tools/            deploy, screenshot and run helpers
docs/             installation, design and performance documentation
assets/           console icons, example configuration
patches/          the patch that makes the MiSTer main binary start this GUI
poc/              the standalone experiments the design was proven with
third_party/      dependency sources; Main_MiSTer is cloned here when building
```

## Documentation

| Document | What it covers |
| --- | --- |
| [INSTALL.md](docs/INSTALL.md) | Installing, verifying and uninstalling |
| [GUI.md](docs/GUI.md) | The interface design: layout, tiles, navigation, typography |
| [PERFORMANCE.md](docs/PERFORMANCE.md) | What was measured, what it cost, and what made it fast |
| [BOXART.md](docs/BOXART.md) | Artwork preparation, formats and scraper matching audit |
| [WIFI.md](docs/WIFI.md) | Wi-Fi setup behavior and MiSTer configuration notes |
| [POC.md](docs/POC.md) | How the MiSTer boots, where a frontend hooks in, and what was proven on hardware |
| [CONTROLLER.md](docs/CONTROLLER.md) | Controller-first setup — listing pads, an input test, deadzone, button mapping and Bluetooth pairing: the design, and what real hardware testing found along the way |
| [ARCADE.md](docs/ARCADE.md) | Arcade support — the analysis, the design and what was built from it. The MRA format, a ROM-storage-priority bug found on real hardware, how the library, the tab and the box art work, and what is still open |

## Status

Working: booting into the GUI, browsing every system, box art, favourites, history, letter
navigation, launching games, returning from a game with a long press on the menu button, and
hiding systems or the Games tab from Settings for a large library. The 0.6 series adds controller-
operated search, clock and time-zone settings, Wi-Fi setup, interface sound controls, per-game
artwork actions, a confirmed system reboot action, and selectable face-button labels. Version
0.6.3 extended the existing three-phase boxart reveal to Home's recent and favorite covers and
fixed repeated fallback reloads. Version 0.6.4 adds a fading tab underline, selectable tab icons,
a unified asset download action, clearer toggle indicators, and the Consoles tab changes. Version
0.6.5 adds accessibility controls, a GitHub changelog viewer, explanations for settings and
clearer library and cover-art progress messages, the revised Settings layout and a Wi-Fi
activation fix. Version 0.7.0 adds a shared or individually remembered game view for Home,
Favorites, console game lists and Games. It also simplifies the List view's artwork panel,
shows each favorite's system, and avoids repainting unchanged list content. See
[CHANGELOG.md](CHANGELOG.md),
[GUI.md](docs/GUI.md), and [BOXART.md](docs/BOXART.md).

## Roadmap

**This roadmap is not set in stone.** It is a statement of intent, not a promise: what goes
into which release, and in what order, will move as testing on real hardware shows what matters
most, and a version's scope can shrink, grow or change places with another. Nothing here is
dated. Only the next release is planned with any precision, and even that gets sharper once its
testing has been done — the later ones are direction.

### Release plan

Releases are themed rather than feature-by-feature, so that each one has a single thing it is
for.

#### 0.3.0 — Tests and bug fixes

- [x] **Test the Arcade library, tab, and updated scraper on real hardware.**
- [x] **Improve box art matching** using the real scrape-miss list and an audit of Libretro's
      index. See [BOXART.md](docs/BOXART.md) for results and the limits of the available counts.
- [x] **Font fallback and interface fixes** — test without external TrueType fonts, tune fallback sizing,
      improve spacing, and show Ethernet and Wi-Fi addresses in Settings.
- [x] **Navigation and input fixes** — alphabetize Arcade group previews and require a
      two-second hold to change a favorite.

See [CHANGELOG.md](CHANGELOG.md) for the complete 0.3.0 change list.

#### 0.4.0 — Optimisation

- [x] **Smoother, faster menu control**, everywhere. Confirmed by user testing on the MiSTer.
- [x] **View-sized artwork files and bounded background decoding.** Prepare box art creates
      five BMP sizes with JPEG/PNG fallback; cover decoding runs in a bounded worker queue.
      See [CHANGELOG.md](CHANGELOG.md) for the release details.
- [x] **Optional box art and faster icon startup.** Settings can hide artwork; the one-second
      splash then loads system icons sequentially, preferring native-size BMPs.
- [x] **A much shorter first visit to Systems.** The game database already records which
      systems have games, so the tab can show its grid without building that answer on entry.

#### 0.5.0 — Install and update flow

- [x] **A dependable first-install flow.** The setup script installs the loader, the GUI
      library, fonts and system icons, and prints the needed `MiSTer.ini` changes.
- [x] **An in-GUI update flow.** The opt-in GitHub check stays; *Check now* and *Update now*
      install a verified GUI library, followed by a MiSTer restart. An optional symlink can
      select an older installed version.
- [x] **A stable, minimal GUI loader.** The existing `gui=` path stays and loads the active,
      versioned GUI library; older versions are retained for recovery.

See [CHANGELOG.md](CHANGELOG.md) for the complete 0.5.0 change list.

#### 0.6.0 — Wi-Fi, sound and settings

- [x] **Wi-Fi setup from Settings**, including network selection and credentials. Settings →
      System groups the connected network name beside its Wi-Fi address. The complete flow was
      tested on MiSTer hardware; setup notes are in [WIFI.md](docs/WIFI.md).
- [x] **Search** — search games with the shared on-screen keyboard, then browse, start, and
      favorite matches in the regular grid.
- [x] **Settings** — clock format and time zone, Space Grotesk control, per-game artwork actions,
      and a confirmed system reboot.
- [x] **Interface sounds** after the splash screen, while moving through the GUI, when
      switching tabs and when starting a game. Settings → Interface can disable the startup
      sound and menu sounds independently; menu sounds include navigation, tab changes, and game
      launches.

#### 0.7.0 — Usability

- [x] **Simplify the Arcade tab:** remove the Manufacturers and Categories entry points and show
      Arcade games directly in the configured standard Games view.
- [x] **Improve help and status text** with clearer explanations for settings and updated hints
      in views, including library loading and box-art scraping.
- [x] **Show a checkbox alongside “On” or “Off”** for toggle settings, with fixed-width values.
- [x] **Face button style** setting with XBOX, PlayStation and Nintendo labels. Confirm stays on
      the bottom face button and Back on the right; swapping them remains a separate backlog item.
- [x] **Add tab icons** with options to show tab names, tab icons or both; download missing BMPs
      through Settings → System → Download Assets or during a GUI update.
- [x] **Rename the Systems tab to Consoles** and add a “Show Arcade in Consoles” option,
      off by default.
- [x] **Add a search bar to relevant tabs** and search within the current context: all games in
      Home and Games, favorites in Favorites, the selected system in its game list, and Arcade
      games in Arcade. Remove the Search tab; Settings has no search.
- [x] **A separately remembered view per tab** — Home, Favorites, Consoles and Games — instead
      of the one default view for all of them.

#### 0.8.0 — Game variants and SuperStation Dock

- [ ] **Group language and revision variants of one game.** Show one entry for titles such as
      Pokémon Sapphire even when the library has separate Europe, Japan and USA ROMs or
      multiple revisions; let the player choose a specific version when needed. Keep the ROMs
      and their language/revision details intact.
- [ ] **Disc-drive support for the SuperStation Dock.** Discover and launch physical PlayStation
      and Saturn discs through their physical-disc MGLs. Start with manual launch; automatic disc
      detection and other CD systems can follow after the Dock's drive and MGL compatibility are
      confirmed on hardware. Keep physical-disc Main selection scoped to those MGLs, not the
      global MiSTer Main setting.

#### Not scheduled yet

- [ ] **Option to swap Confirm and Back buttons**, with the current European mapping as the
      default.
- [ ] **Fade in tab content when switching tabs** instead of sliding it into view.
- [ ] **Choose the order of consoles** in the Consoles tab. Hiding unused consoles already works.
- [ ] **Boxart scraping using TheGamesDB**, with a list of titles that have no match and the
      option to retry artwork lookup for one title or all unmatched titles.
- [ ] **Use `update_all.sh` from the GUI**, showing the installed version, what will be updated
      and whether a restart is needed.
- [ ] **Back up and restore settings.** Export favorites, hidden systems, per-tab views and GUI
      settings, and restore them after a reinstall.
- [ ] **Screensaver and screen dimming after inactivity.** Dim the display after a period
      without input and activate a screensaver after a longer idle period.
- [ ] **Localisation.** Everything is English today, with the strings still inline; extracting
      them is the prerequisite.
- [ ] **Loader slots for the remaining CD-based cores** (CD-i, Jaguar CD).
- [ ] **Telling two identical controllers apart**, with separate mappings for each (MiSTer's own
      `controller_unique_mapping`). Left out of the first pass of controller management on
      purpose — two same-model pads sharing one mapping is the common case.
- [ ] **Per-game DIP-switch editing** for Arcade.
- [ ] **Manage Cores / Manage MRAs** for Arcade, syncing against the official distribution
      manifest. Design in [ARCADE.md](docs/ARCADE.md#decision-manage-cores-and-manage-mras).
- [ ] **Video modes.** `video_mode=` in `MiSTer.ini`, including a clean fallback when a
      configured mode fails or is not available. Also covers running, and verifying, this GUI
      at resolutions other than 1080p — it only scales its own design pixels to whatever the
      framebuffer reports, which is untested outside 1080p despite "scales from 480p to 4K"
      being listed as a feature.
- [ ] **Testing more controllers.** See [Tested controllers](#tested-controllers) for what has
      been confirmed so far and what is still to try.
- [ ] **Telling HDD/CHD-based Arcade games apart** — boards such as Killer Instinct — from those
      that start unattended. Nothing found can automate the one-time manual step itself; see
      [ARCADE.md](docs/ARCADE.md#hddchd-based-games-no-automated-mount-found).

### Ideas, not committed

- [ ] **A web interface for managing the library** from a computer or phone, rather than with
      a game pad: everything under *Library management* above, plus a better search, uploading
      new games to the MiSTer, deleting and renaming them, managing box art, and editing
      favourites.

## Acknowledgements

- [Main_MiSTer](https://github.com/MiSTer-devel/Main_MiSTer) — the MiSTer main binary, which
  this project forks for its boot path and relies on for core loading
- [Console Mode](https://github.com/Retro-Remake/ConsoleMode_Distribution) by Retro Remake —
  whose library cache the GUI currently reads, and whose approach to being gentle with the
  drive it learned from
- [retro-game-console-icons](https://github.com/KyleBing/retro-game-console-icons) — the
  console icon set

## A note on how this was built

This project was built with the help of AI.

I want to be honest about that: without it, this project would not have grown from a proof
of concept into something that actually works — a project genuinely capable of replacing the
official Console Mode. Between the lack of documentation and the fact that Console Mode's own
source is no longer publicly available, reaching this point without that help would have
taken several months, if not years.

At the same time, I want to be just as clear that AI alone would not have gotten here either.
A lot of hard, careful work went into this project — done by a person. Me.

## Disclaimer

This is an independent, unofficial hobby project, provided "as is" and without warranty of
any kind, express or implied — including, without limitation, warranties of merchantability,
fitness for a particular purpose, and non-infringement. Use it entirely at your own risk. To
the extent permitted by law, the author accepts no liability for any damage or loss arising
from its use, including but not limited to a corrupted SD card, a bricked device, lost save
data, or any other harm to your hardware or data.

The installer modifies boot-time files on your MiSTer's SD card and replaces the main binary.
Care has been taken to make that reversible — see [INSTALL.md](docs/INSTALL.md) — but as with any
change to a device's boot path, something going wrong is always possible.

This project does not provide, host, link to, or in any way distribute copyrighted game ROMs,
BIOS files, or other game data. It only launches files already present on your own storage,
exactly as the MiSTer's own menu does. You are solely responsible for ensuring you are legally
entitled to any game files you use with it.

This project is not affiliated with, endorsed by, or sponsored by the MiSTer FPGA project,
Terasic, Retro Remake, or any console manufacturer. "MiSTer" and any console, system or
company names referenced or depicted in this project — including in its bundled console icon
set — are the trademarks of their respective owners and are used here only to describe
compatibility, not to claim any association with them.

## License

GPL-3.0. The project builds on Main_MiSTer and bundles the console icon set, both GPL-3.0,
so it is GPL-3.0 throughout. See [LICENSE](LICENSE) for the terms and [NOTICE](NOTICE) for
what came from where.
