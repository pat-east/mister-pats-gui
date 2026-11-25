# Platforms and portability

Owner's goal (2026-11-22): the GUI may later be offered for **other Linux-based systems** with
framebuffer-style output, not necessarily a MiSTer: for example a Raspberry Pi, which has a GPU.

This is **not a 0.9.0 deliverable.** It is a design constraint: 0.9.0 must not make a second platform
harder, and the cheap preparations are done while the structure is being built anyway. The target device
stays the MiSTer DE10-Nano (two Cortex-A9 cores, no GPU, CPU rendering).

## The outlook in one picture (owner, 2026-11-22)

An **outlook only**: the work stays focused on MiSTer Pat's GUI. The picture shows why the seams are
cut where they are.

```
  Linux framebuffer / DRM-KMS / window          what puts pixels on the screen, and brings input in
            |
  Pat's UI Framework            src/ui-framework/   components, renderers (CPU, later GPU), presenters
            |
  Pat's Gaming GUI              src/app/ + core/    screens, library, favorites, history, scrapers
            |
  MiSTer (Main_MiSTer)   |   RetroArch (libretro cores)   |   other backends later
                                                what actually runs the games   (backend/ adapters)
```

The Gaming GUI is `src/app/` plus `core/` in the layers below; the platform adapter (paths, system
services, audio) sits beside the stack and serves the Gaming GUI. Only the bottom tier (display and
input) and the top tier (the backend) change from one system to the next; the two middle tiers are the
product. For 0.9.0 only the MiSTer column exists.

## Layers

```
 Application GUI    src/app/            screens, strings, palette, the product's look and flow
 Frontend core      core/               library, favorites, history, scanning, scrapers, preferences, artwork index
 Backend adapter    backend/            what runs games: systems, content, launch, hand-off      (MiSTer cores, RetroArch, ...)
 Platform adapter   platform/           paths, system services, audio, updates, display handover (one per platform)
 UI framework       src/ui-framework/   components, renderers, presenters, input sources                (generic)
```

Dependency direction: the application uses the core, the adapter and the framework; the core and the GUI
see the platform only through the adapter's interfaces; the framework knows nothing of the others
(see UI-FRAMEWORK.md, "Autonomy"). A new platform is a new adapter plus, where needed, a presenter or
renderer; screens and components do not change.

## Where the code stands today

A survey of `src/legacy/` (about 110 files):

| Dependency | Files | Notes |
| --- | ---: | --- |
| `/media/fat` paths | 27 | scattered; a path provider is missing |
| MiSTer names (`MiSTer_gui`, `Main_MiSTer`, `mister-pat`) | 25 | loader, updater, installers, paths |
| MiSTer concepts (`.rbf`, `.mra`, `_Console`, `_Arcade`) | 22 | system catalog, core updater, Arcade |
| `/media/usb*` | 9 | library roots |
| `reboot`, `system(`, `popen(` | 11 | process and shell calls |
| `/proc`, `/sys/class` | 7 | system information |
| audio output (`PcmOutput`, ALSA) | 6 | UI sounds |
| Wi-Fi tools / Bluetooth tools | 2 / 3 | `wpa` handling, `btpair` |
| `/dev/fb0` (framebuffer) | 2 | already isolated |
| `/dev/input` (evdev) | 2 | already portable on any Linux |
| MiSTer command FIFO (`/dev/MiSTer_cmd`) | 1 | the `Launcher`, already isolated |
| video (libav) | 1 | previews |

The launcher, the framebuffer and evdev are already behind narrow seams. The rest is scattered and needs
one.

## What is platform-specific

| Concern | MiSTer | A generic Linux system (for example Raspberry Pi) | Seam |
| --- | --- | --- | --- |
| Starting a game | write an MGL, send it to the command FIFO | run an emulator or RetroArch from a command template per system | `Launcher` interface |
| Systems and cores | MiSTer cores, `_Console`, MRA Arcade | emulator or core per system, from configuration | system catalog split: portable system data, platform mapping |
| Paths | `/media/fat`, `/media/usbN`, `/tmp` | XDG directories, configurable roots | `PathProvider` |
| Core and MRA updates | official MiSTer manifest | not applicable | optional feature |
| Disc drive (PlayStation, Saturn cores) | MiSTer cores | not applicable or other tools | optional feature |
| Wi-Fi, Bluetooth, time zone, restart | MiSTer tools and scripts | NetworkManager, BlueZ, `timedatectl`, `systemctl` | `SystemServices` interface |
| UI sounds | MiSTer audio path | ALSA or PipeWire | `AudioSink` interface |
| Display | framebuffer | framebuffer, DRM/KMS, a window | presenter in `src/ui-framework/` |
| Input | evdev | evdev (the same) | input source in `src/ui-framework/` |
| Video previews | software decoding | software now, hardware decoding possible | `VideoPlayer` backend |
| GUI self-update | GitHub release, MiSTer layout | GitHub release, an asset per architecture, a different layout | updater install layout in the adapter |
| Start and autostart | loader and the patched `Main_MiSTer` | a systemd service or a launcher script | adapter |
| Build | ARM hard-float cross build, static, MiSTer sysroot | aarch64 or armhf, or x86_64; dynamic or static | build matrix |

## Emulation backends (the RetroArch scenario)

Owner's scenario: a small Linux mini PC runs RetroArch, USB controllers attached. The GUI would be
RetroArch's frontend: it renders the dashboard and starts games; RetroArch and its libretro cores stay the
player. RetroArch is itself a frontend for emulators; this GUI would sit in front of it ("RetroArch Pat's
GUI" as a working name).

This separates two things that coincide on the MiSTer:

- the **platform adapter**: the operating system and hardware (paths, display handover, audio, network,
  power, updates);
- the **backend adapter**: what actually runs games. On the MiSTer: cores through `Main_MiSTer`. Here:
  RetroArch with libretro cores, later possibly standalone emulators through command templates.

On a MiSTer the platform implies the backend. On the mini PC the platform is "Linux PC" and the backend
"RetroArch". The frontend core (library, favorites, history, scrapers) talks to a backend through one
interface (`systems()`, `contentSources()`, `launch()`, `capabilities()`, the hand-off), and the existing
model (systems, games, versions) stays product-neutral: "core" becomes "emulator id", a backend-specific
field.

### Integration levels

| Level | What it is | Verdict |
| --- | --- | --- |
| **L1** launcher and hand-off | The GUI starts RetroArch with a core and a content file and waits until it exits; library, artwork, favorites, history live in the GUI. | The same model as the MiSTer (the GUI stands down while the game runs). Recommended first. |
| **L2** plus a control channel | RetroArch's network commands (a UDP socket, opt-in) for status, save and load state, close content, quit. | Selectively, later, where it improves the return path or an overlay. |
| **L3** own libretro host | The GUI loads libretro cores itself and does video, audio, input, saves, shaders. | No: it rebuilds RetroArch and removes the reason to use it. |

### What RetroArch offers a frontend (to verify against the installed version)

- **Command line:** `retroarch -L <core> <content>`, `--config`, `--appendconfig` (overrides for one
  launch, so the user's config is never rewritten), `--fullscreen`, `--menu`. It ends when the game
  quits.
- **Playlists:** JSON `.lpl` files (path, label, core, checksum, database name), also for favorites and
  history: a library source.
- **Core info files:** core name, supported extensions, system id: the map between systems and cores,
  and which cores are installed.
- **Thumbnails:** the libretro thumbnail layout (`Named_Boxarts`, `Named_Snaps`, `Named_Titles`).
  **The project already uses it**: `LibretroIndex` and the box-art scraper match against that server's
  names, so artwork for a RetroArch backend largely exists.
- **Configuration:** `retroarch.cfg` as key = value. The GUI reads it and changes behavior only through
  `--appendconfig`.
- **Network commands** (UDP, opt-in, default port 55355): runtime control for L2. Command set is version
  dependent and must be checked.
- **Its own menu** (Quick Menu: save states, shaders, core options) stays the in-game menu; the GUI does
  not replace it.

### Hand-off

The GUI **stands down** (the concept exists today as `Context::standDown`): stop drawing and sounds,
release the input devices (evdev ungrab), release the display, start the player as a child, wait for it,
take display and input back, repaint, refresh history. With DRM/KMS only one process can own the display,
so the presenter needs a `release()` / `acquire()` contract (a window system needs hide and show instead).
If the player crashes, the GUI regains control and shows an error modal in the framework's structure
(what happened, what changed, small Details line, a next step).

### What maps onto the existing GUI

| Existing feature | On the RetroArch backend |
| --- | --- |
| Library, versions, favorites, history | from playlists or a folder scan; favorites and history in the GUI's own store |
| Box art | libretro thumbnails (same pipeline) |
| Controller mapping wizard | could write RetroArch autoconfig profiles; or read them (opportunity, not required) |
| Core and MRA updates, Disc drive, MiSTer loader | not applicable (capabilities absent) |
| Wi-Fi, Bluetooth, time zone, restart | `SystemServices` of the Linux platform |
| Core downloads | RetroArch's own online updater; not ours |

## Delivery forms and the Linux image

Owner's extension: deliver it as a **Linux image**: the GUI starts at boot and uses software emulators
instead of MiSTer cores. That is a different kind of product from today's add-on.

| Form | What it is | Effort |
| --- | --- | --- |
| **MiSTer add-on** (today) | library and loader installed on the MiSTer SD card | exists |
| **App package** | the GUI as a package or archive for an existing Linux system with RetroArch (the mini PC as it is), started by a service or a script | small to medium |
| **Bootable image** | a complete operating system image with the GUI as the session shell, emulators included | large |

An image adds everything a distribution has: a build system, kernel and drivers, GPU drivers, audio,
network and Bluetooth stacks, boot to the GUI without a desktop, first-boot setup (language, network,
storage, controllers: a `Wizard` flow), mounting USB storage for games, updates of the system and the GUI
(for example two system partitions and a recovery path), logging and diagnostics, a hardware support
matrix, and the licensing of every bundled emulator and core. It is its own product with its own
maintenance, not an adapter. Prior art with this shape: Batocera (EmulationStation as the shell, RetroArch
and standalone emulators underneath), Lakka (RetroArch as the shell). The differentiators here would be
what this project already stands for: a controller-first dashboard, accessibility, i18n.

**Display architecture of an image** (decided when the image starts):

- *Direct DRM/KMS:* lean, no window system; the GUI owns the display and hands it over to the player
  (the `release()` / `acquire()` contract). Lakka-style.
- *A kiosk compositor* (a minimal Wayland compositor): the GUI and the emulators are fullscreen windows;
  simpler for standalone emulators that need a window system; a window presenter and a GPU renderer fit
  naturally.

Both are covered by the framework's presenter abstraction.

**Suggested order.** (1) The seams in the 0.9.0 work (`Backend`, `PlatformServices`, `core/`).
(2) The host development platform. (3) An app package for the mini PC with the RetroArch backend (L1):
this proves the backend model with the machine the owner already has, without an OS to maintain.
(4) The image, only if there is a reason the app package cannot serve. All after 0.9.0.

## Capabilities

An adapter declares what it can do (`launch.mgl`, `launch.command`, `network.wifi`, `bluetooth`,
`power.restart`, `updater.cores`, `disc.mister`, `audio`, ...). The GUI shows a setting or a screen only
when the capability exists. Components do not change; the application decides what to offer. An absent
capability is never a greyed-out entry with an apology: it is simply not there.

## Display paths on a Raspberry Pi

All three sit behind the framework's presenter interface (UI-FRAMEWORK.md, "Rendering is exchangeable").

| Path | Renderer | Notes |
| --- | --- | --- |
| Framebuffer (`/dev/fb0`) | CPU | what the MiSTer uses; works where the kernel provides fbdev emulation; simplest |
| DRM/KMS dumb buffer | CPU | page flip and vertical sync, no GPU libraries; recommended first step on other Linux systems |
| DRM/KMS + GBM + EGL/GLES | GPU | needs Mesa and the GPU driver; the optional GPU renderer |

"Framebuffer-rendered" on a Raspberry Pi can mean the first two. A GPU renderer is only worth building
when the CPU renderer is not enough there; a modern Pi is fast enough for the CPU renderer at 1080p, so
this is a later, evidence-driven decision. The same look on every renderer applies (UI-FRAMEWORK.md,
D29).

## Step by step

1. **Now, while the structure is built (small cost, in the 0.9.0 work).**
   - The framework contains no platform knowledge (already required).
   - In `src/app/`, everything platform-specific goes behind `platform/` interfaces from the start:
     `Backend` (it replaces the plain `Launcher`), `PathProvider`, `SystemServices`, `AudioSink`, the updater's install layout. No path
     such as `/media/fat` and no tool name is written outside `platform/mister/`.
   - Frontend logic that does not need a platform (library, favorites, history, scanning, scrapers,
     preferences) is written as UI-free code in `core/`.
2. **A development platform on the host.** A "host" adapter (fake launcher that prints what it would
   start, paths under a temporary folder, no Wi-Fi) plus a window presenter runs the **whole** GUI on the
   Mac or a Linux PC without a MiSTer. Cheap, and valuable on its own: development, screenshots, a demo.
3. **A second presenter:** DRM/KMS dumb buffer, then a Raspberry Pi test.
4. **A generic Linux adapter:** launcher with command templates, XDG paths, NetworkManager and BlueZ
   behind `SystemServices`, an `aarch64` build and release asset.
5. **A GPU renderer,** only if a platform needs it.

Steps 2 to 5 follow 0.9.0 unless the owner moves them.

## What it does not mean

- The MiSTer release does not get slower or bigger for it: adapters and renderers that a platform does
  not use are not linked into its build.
- It does not turn the GUI into a generic launcher framework. The product stays a controller-first
  dashboard; a platform adapter only changes how things are started and where files live.
- Core and MRA updates, the Disc drive and the MiSTer loader stay MiSTer features.

## Decisions needed (M0)

| # | Decision | Proposal |
| --- | --- | --- |
| D30 | Platform seam in the 0.9.0 work | `platform/` interfaces (`Launcher`, `PathProvider`, `SystemServices`, `AudioSink`, install layout) with `platform/mister/` as the first adapter; `core/` for UI-free logic |
| D31 | First other target | Raspberry Pi (aarch64 first), Linux desktop as the development platform |
| D32 | First non-MiSTer presenter | DRM/KMS dumb buffer with the CPU renderer |
| D33 | Host development platform | Yes: fake adapter plus window presenter, early (step 2) |
| D34 | Capability-driven settings | Yes: absent capability means the entry is not shown |
| D35 | Name and packaging for other platforms | Later; `src/ui-framework/` and `core/` stay product-neutral from now on |
| D36 | Backend adapter separate from the platform adapter | Yes: what runs games is not the operating system |
| D37 | RetroArch integration level | L1 (launcher and hand-off) first, L2 (control channel) selectively, no L3 (own libretro host) |
| D38 | Library source for the RetroArch backend | Read RetroArch playlists and core info as the truth of what can run; optional folder scan |
| D39 | Favorites and history on the RetroArch backend | Own store; optional import of RetroArch's favorites and history playlists |
| D40 | Display hand-over | Presenter contract `release()` / `acquire()`; the display architecture of an image (direct DRM/KMS or a kiosk compositor) is decided later |
| D41 | Order of delivery forms | App package for an existing Linux system first, bootable image later |
| D42 | Image build system and hardware | Buildroot as the working proposal; x86_64 generic PC and Raspberry Pi as the candidates; decided when the image starts |
| D43 | What an image may bundle | Only freely redistributable emulators and cores; no ROMs, no BIOS, no cores whose licence forbids redistribution (the existing project rule) |

## Open questions

- How is RetroArch installed on the mini PC (distribution package, Flatpak, AppImage, Lakka)? A Flatpak
  sandbox restricts spawning processes and reading its files.
- Does the mini PC run a bare console (KMS) or a desktop session? That decides the display hand-over.
- How does the user get back to the GUI after a game (RetroArch's quit hotkey, a service that restarts
  the GUI)?
- Name: "RetroArch" is a project name; check its naming policy before it appears in a product name.
- Which emulators and cores would an image ship, and under which licences (D43)?

- Should `core/` be a separate module with its own tests now, or grow inside `src/app/` first? Proposal:
  separate directory from the start, extracted from the services that already exist in `src/legacy/`.
- Is the system catalog's portable part (system names, extensions, art folders) separable from the MiSTer
  core mapping without touching Classic? It is data in `SystemCatalog`; to be checked.
- Which kernel and fbdev behavior is expected on the Raspberry Pi targets (fbdev emulation vs KMS only)?
