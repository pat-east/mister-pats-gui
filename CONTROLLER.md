# Controller Management — Design

Design document for the "Controller management" item on the [README roadmap](README.md#controllers).
Originally written as analysis and a proposed design, before any code changes, so the shape of
the feature was settled first. **Now implemented and verified on real hardware** (Settings →
Controllers) — kept up to date as a record of the design *and* of what hardware testing turned
up along the way, most of it wrong assumptions this document's own earlier version made with
confidence before a real pad proved otherwise: MiSTer's A/B/X/Y slots follow SNES-style
positions rather than Xbox letters, its OSD-confirm slot is unrelated to its OSD-open combo, a
D-pad-only pad needs a synthetic code MiSTer's own source builds for a hat axis, and level-
triggered button capture stampedes through a whole wizard run if one button is held a moment
too long. All fixed; all below.

- [Premise](#premise)
- [Why this belongs in this GUI at all](#why-this-belongs-in-this-gui-at-all)
- [What "affects the MiSTer menu" means, precisely](#what-affects-the-mister-menu-means-precisely)
- [Facts this design relies on](#facts-this-design-relies-on) — identifying a controller, two
  identical pads, `deadzone=`, the `.map` file, writing `MiSTer.ini` safely, Bluetooth pairing
- [Non-goals for v1](#non-goals-for-v1)
- [Features](#features) — list, input test, deadzone, button mapping, Bluetooth pairing
- [Mockups](#mockups)
- [Data flow](#data-flow)
- [Open questions](#open-questions) — 12 of them; 10 already decided inline, 2 still genuinely
  open (a joint hardware test, and an unmeasured placeholder number)

## Premise

Controller-first, end to end. Every screen this feature adds must be fully usable with nothing
but a game pad — no keyboard required at any point, including for the one operation that is
normally hardest to do without one: capturing which physical button should mean what.

**Side effect:** no mouse support, for now. The stock MiSTer menu already needs a keyboard's
spacebar to skip its own mouse-detection step at boot if no mouse is attached; that requirement
is a Stock-MiSTer thing this project does not control and is not trying to solve here. It is
called out so it is not mistaken for an oversight.

## Why this belongs in this GUI at all

Stock MiSTer already has a way to do most of this — "Define joystick buttons" in its own OSD,
reached by holding the menu button on a pad that already has one usefully mapped, or via a
keyboard. Three problems with leaving it at that, all surfaced directly by real use this
session (see the session's own troubleshooting, now in [INSTALL.md](INSTALL.md)):

- It is not reachable at all until *something* on the pad already does something MiSTer
  recognises — genuinely new hardware (a receiver, an unfamiliar pad) can leave someone with
  no way in short of finding a keyboard.
- The one thing that is supposed to make it reachable regardless — a case's own hardware OSD
  button — is not guaranteed to work with this project's patched main binary at all (see the
  `menu_ConsoleMode.rbf` finding in this session's own history: some hardware buttons are wired
  through logic that only exists in a distribution-specific FPGA core).
- Even once reached, two of the things that matter most in practice — the `deadzone=` INI
  line's vendor/product ID, and which specific one of several identical pads a mapping applies
  to — depend on facts (documented below) that are not obvious and were only found by reading
  MiSTer's own source directly in this session.

None of that is a criticism of stock MiSTer's own tool. The point is narrower: this project
already puts its own controller-driven UI in front of the user before the stock menu is ever
seen, and that UI is in a good position to remove the one class of setup step that currently
still requires falling back to something else.

## What "affects the MiSTer menu" means, precisely

MiSTer keeps a *separate* button-mapping file per core, plus one default. The default is what
applies while browsing the menu itself, and it is what any core without its own mapping falls
back to. This feature manages **the default mapping only** — not a per-core override. Per-core
mapping stays reachable the existing way (stock OSD, from inside that core) and is out of scope
here; see [Non-goals](#non-goals-for-v1).

## Facts this design relies on

All of the following was confirmed by reading MiSTer's own source
(`third_party/Main_MiSTer/input.cpp`, `cfg.cpp`, `file_io.cpp`) and, where noted, by probing the
actual files on a running device — not assumed. Anything not confirmed this way is flagged as
such.

### Identifying a controller

MiSTer reads a device's vendor/product ID via the kernel's `EVIOCGID` ioctl on its evdev node —
the same thing `cat /proc/bus/input/devices` prints as `Vendor=`/`Product=`. **This can differ
from what `lsusb` reports for the same physical hardware.** Confirmed on this session's own
Xbox 360 Wireless Receiver: `lsusb` reports `045e:0719`; the actual input device MiSTer reads
reports `045e:02a1`. A deadzone line (or anything else) keyed by the `lsusb` ID silently never
matches. **Any UI this feature adds must read and display the ID the same way MiSTer does**
(the evdev one), never shell out to `lsusb`-equivalent USB descriptor info, or it will
reproduce this exact trap for the next person.

### Two identical pads

MiSTer's own internal id for a controller (`idstr`) is `<vid>_<pid>` by default — **two
identical pads (this session's own setup: two Xbox 360 controllers on one receiver) share one
id, and therefore one mapping file, unless `controller_unique_mapping` in `MiSTer.ini` is set**
to force per-connection uniqueness (MiSTer then suffixes the id with a hash of the connection's
own stable identity — the USB path, or the Bluetooth MAC — not the VID/PID, so it survives a
reboot as long as the pad is plugged into the same physical port). This is not a hypothetical
edge case — it is this session's own literal hardware.

**The controller list (below) must make this visible.** Two rows that look identical and share
one mapping is confusing enough to guarantee support questions; the list should show whichever
piece of `idstr` is actually true today (unique-mapping on or off) so it is clear when two pads
are, and are not, sharing a configuration.

### `deadzone=` (MiSTer.ini)

```
deadzone=<vid><pid as one 8-hex-digit value>, <0-100>
```

e.g. `deadzone=0x045e02a1, 25` (not `0x045e0719` — see above). Parsed by `cfg.cpp` into
`cfg.controller_deadzone[32]`, one line per configured pad, matched by exact VID:PID (or by a
`VID:`/`PID:`-prefixed partial match — confirmed in source, not exercised on hardware this
session). The number is compared directly against stick displacement on a scale that tops out
around 127 (not a literal percent of the input's own raw range) — see `joy_apply_deadzone` in
`input.cpp` for the exact curve if the UI ever needs to preview it numerically.

### The `.map` file

Path: `/media/fat/config/inputs/input_<idstr>_v3.map` (a `_m` suffix variant exists for a
"modifier held" alternate mapping — out of scope for v1, see below). Confirmed byte-for-byte
against a real file this session (`input_045e_02a1_v3.map`, an Xbox 360 pad): **32 little-endian
`uint32` values, 128 bytes total, fixed slots:**

| Index | Meaning | Encoding |
| ---: | --- | --- |
| 0–3 | D-pad: Right, Left, Down, Up | raw evdev key code (e.g. `BTN_DPAD_RIGHT` = `0x223`) — **or**, on a pad whose D-pad is a hat axis rather than digital keys (confirmed on hardware: a Retro-Bit Saturn-style pad, reported as `Switch Co. Ltd. Retro-bit Controller`), a synthetic code MiSTer's own `input.cpp` builds for exactly this case: `KEY_EMU + (hat_axis_code << 1) + (0 for the axis's minimum / 1 for its maximum)`, where `KEY_EMU` is `KEY_MAX + 1` (`0x300`). A `.map` writer has to produce this form for a hat-based pad — writing a literal `BTN_DPAD_*` code that pad never actually sends leaves the slot silently unmappable. |
| 4–7 | Face buttons: A, B, X, Y | raw evdev key code |
| 8–9 | Shoulders: L, R | raw evdev key code |
| 10–11 | Select, Start | raw evdev key code |
| 12–19 | Mouse emulation (right/left/down/up, buttons L/R/M, emu toggle) | raw evdev key code |
| 20 | `SYS_BTN_OSD_KTGL` — "open the OSD" key, **keyboard sessions only** | raw evdev key code; always `0` for a gamepad mapping (see below) |
| 21, 22 | `SYS_BTN_CNT_OK`/`SYS_BTN_CNT_ESC` — reused, for a gamepad, as the "open OSD from inside a running core" combo's two halves | raw evdev key code, each — the same code in both for a single-button combo |
| 23 | `SYS_BTN_MENU_FUNC` — "OK"/"Back" **while the OSD is already open** | low 16 bits = "OK" button's code, high 16 bits = "Back" button's — each packed in directly as captured, not read back from 21/22 |
| 24–27 | Stick 1 X/Y, Stick 2 X/Y | `0x0002_0000 \| abs_code` (`ABS_X`=0, `ABS_Y`=1, `ABS_RX`=3, `ABS_RY`=4) |
| 28–29 | "Active" stick X/Y (mirrors whichever of 24–27 is in use) | same encoding as 24–27 |
| 30–31 | Spinner / mouse-emulation axes | same encoding, usually unset (`0`) |

An unset slot is `0`. **Indices 20–23 confirmed on real hardware to mean something different
from an earlier assumption here — worth spelling out in full, since getting this wrong is
exactly the kind of silent, hard-to-notice breakage this whole document exists to avoid.**
Reproducing a PS4 controller's own mapping side by side with the file MiSTer's own stock
wizard wrote for the *same pad* (via `xxd`) showed index 20 at `0`, indices 21/22 holding the
*same* code as each other, and index 23 holding two *different* codes entirely unrelated to
21/22. Reading `Main_MiSTer/input.cpp`'s own mapping-capture state machine
(`SYS_BTN_OSD_KTGL`/`SYS_BTN_CNT_OK`/`SYS_BTN_CNT_ESC`/`SYS_BTN_MENU_FUNC` in `input.h`, and
`menu.cpp`'s three separate prompts for this area — "Menu", "Menu: OK", "Menu: Back") settled
it:

- **Slots 21/22 are the "open the OSD from inside a running core" combo** — entirely unrelated
  to confirm/cancel. `SYS_BTN_CNT_OK`/`SYS_BTN_CNT_ESC` (21/22) are literally
  `SYS_BTN_OSD_KTGL + 1`/`+ 2`, reused under different names depending on which feature is
  reading them — at runtime, both halves have to be *held simultaneously* for the OSD to pop
  up (`input[dev].osd_combo` becoming `3`, a two-bit mask); a single-button combo works simply
  because one physical press satisfies both halves at once. Left at `0`, there is no way to
  reach the OSD from inside a core at all with that pad, which is precisely the situation this
  whole feature is meant to prevent — see [Feature 4](#4-button-mapping) for why this wizard
  does not let this particular step be silently forgotten.
- **Slot 23 is what confirms/cancels once the OSD is already open**, and it is built
  incrementally, one half at a time, directly from two *separate* prompts ("Menu: OK", "Menu:
  Back") — there is no other slot these two raw codes are ever stored in first. Left at `0`,
  either half **falls back to this same pad's regular A/B mapping** (`input.cpp`: the low
  half's fallback is literally `ev->code == mmap[SYS_BTN_A]`) — so leaving this slot entirely
  untouched is a correct, working default, not a placeholder needing a follow-up fix.

The earlier assumption here — one captured button feeding *both* a single OSD-toggle slot
*and* directly becoming the packed "confirm" value — quietly wrote whatever button was
captured for "open the OSD" into slot 23's low 16 bits, which **overrides** the correct
A-button fallback with an unrelated, usually wrong button. That is exactly the "confirm with A
doesn't work any more, only the actual menu button does" symptom real hardware testing turned
up, and exactly why this document says to read the source rather than guess.

This is a **binary format with no version-tolerant fields** — a writer has to construct the
whole 128 bytes as one unit; there is no safe way to patch a single slot in place without
decoding the rest first. This project's own writer must therefore always read-modify-write the
full struct, matching MiSTer's own approach 1:1, not attempt a partial update.

**This means the button-mapping screen has to capture a raw evdev event per prompt**, not run
through this GUI's own `Action` enum (`Up`/`Down`/`Confirm`/... in `src/Input.h`) — that enum is
this GUI's *own*, fixed navigation scheme and is not what gets written to the `.map` file. The
two are related only in that both, today, read the same physical pad.

**Capturing the two stick-axis slots (24–27): resolved, deliberately not the way MiSTer's own
wizard does it.** MiSTer's own capture code (`tmp_axis[4]` in `input.cpp`) fills all four axis
slots from one combined calibration step, not four separate "press this" prompts — replicating
that exactly would need its own close read of that state machine, which this project's own
wizard does not need: the slot's *value* is just `0x0002_0000 | abs_code` (see the table
above), independent of any calibration procedure that decided to write it. This wizard instead
asks for each of the four slots separately ("Stick 1: move left/right", then "...up/down", and
the same for Stick 2), and for each one picks whichever of the pad's own reported stick axes
(`ABS_X`, `ABS_Y`, `ABS_RX`, `ABS_RY`) moved furthest from where it sat when the step began.

Confirmed on hardware to need a much stricter threshold than first assumed: a plain "moved at
all" or even "moved a third of the way" reading was too eager — a diagonal push, or noise on
an unrelated axis, could register before the intended one. The threshold is now **75% of the
axis's half-range** (centre to one edge — the most a resting stick can ever travel one way),
not a fraction of its whole span (a full deflection off centre is only ever ~50% of that). Once
an axis has answered one of the four stick prompts it is retired for the rest of the wizard, so
a stuck or absent second stick cannot silently answer a later prompt with the same axis the
first stick already used.

**Also not accounted for: analogue triggers.** This GUI's own `Input` class already treats
`ABS_Z`/`ABS_RZ` specially (trigger detection added for the letter-jump feature). The fixed
32-slot layout above has no dedicated trigger slot — `L`/`R` (indices 8–9) are the *digital*
shoulder buttons. Whether MiSTer represents an analogue trigger for a core as a very large
"button" threshold on one of the existing digital slots, as an axis slot reused for the purpose,
or not through this file at all, was not established this session and needs its own look before
a mapping wizard can offer to capture a trigger.

### Writing MiSTer.ini safely

`MiSTer.ini` is a large, actively-used file with a great deal else in it (confirmed: 300+ lines
on a real device, including four per-video-mode sibling files —
`MiSTer_RGsB.ini`/`_SVID.ini`/`_YPbP.ini` — that must independently carry `main=`/`gui=` for
this project's own boot patch to keep working, per this session's own INSTALL.md). A deadzone
writer must:

- Parse and preserve every line it does not own — comments, whitespace, everything.
- Replace an existing `deadzone=` line for the *same* VID:PID in place; add a new line if none
  matches; never touch a `deadzone=` line for a different pad.
- Write atomically (temp file + rename) so a crash or power loss mid-write cannot corrupt the
  file MiSTer itself depends on to boot into anything at all — this is the single highest-risk
  file this feature touches, precisely because everything else this project does to the boot
  path assumes it stays intact.
- Decide whether to also touch the three per-video-mode siblings. Leaving them out is
  *consistent* (deadzone was never mentioned as something those need duplicated, unlike
  `main=`/`gui=`) but should be a stated decision, not an oversight — see
  [Open questions](#open-questions).

**Decided: a backup before every single write, no exceptions.** `MiSTer.ini` must never be
allowed to end up broken — this is the file everything else this project's boot patch depends
on, so "atomic write" alone is treated as necessary but not sufficient. Before writing, copy the
current `MiSTer.ini` into a new directory, `/media/fat/mister-pat/mister-ini-backups/`
(inside this project's own directory, unlike the file being backed up — nothing about the
backups themselves needs to live where MiSTer looks for config), timestamped so backups sort
and are identifiable at a glance. Keep at most **30**; once the 31st is written, delete the
oldest. This is a plain, unconditional safety net — it runs on every write this feature makes,
not just ones a person explicitly asks to be careful about.

**The `.map` file needs no backup** — unlike `MiSTer.ini` it is not load-bearing for booting at
all (a missing or corrupt one just falls back to MiSTer's own built-in default mapping, per
`load_map`), and it is wholly owned by this feature rather than shared with a great deal else.
It still needs the **same atomic-write procedure** (temp file + rename) whenever it is written
or modified, for the same reason stated above under [The `.map` file](#the-map-file): a torn
write there produces a file that is well-formed-looking but wrong in a way nothing detects
until the button that got corrupted is pressed.

### Bluetooth pairing

Confirmed by reading `menu.cpp`'s own `MENU_BTPAIR`/`MENU_BTPAIR2` states (the stock OSD's
"Pair Bluetooth device" screen). Since then, confirmed end to end on real hardware too: a PS4
controller (DualShock 4) paired through this feature's own modal, and — separately worth
noting — the pairing survives a reboot on its own, with no re-pairing step needed (see
[README](README.md#tested-controllers)).

- **Not every MiSTer has Bluetooth**, and stock MiSTer already checks for that the right way:
  `hci_get_route(0) < 0` (from `<bluetooth.h>`, i.e. BlueZ) — an adapter either answers or it
  does not. This project's own GUI does not currently link `libbluetooth` (only Main_MiSTer's
  patched binary does, per its own `Makefile`); the cheaper option, and the one that adds no
  new dependency to an otherwise dependency-minimal static binary, is checking for
  `/sys/class/bluetooth/hci0` (or any `hciN`) directly rather than linking BlueZ — confirmed
  the sysfs path exists and is populated on the reference device (visible in this session's own
  `dmesg`: `Bluetooth: hci0: RTL: ...`). Not formally proven bit-for-bit equivalent to what
  `hci_get_route` checks, but a real pairing (see above) went through end to end on the same
  device this check runs on, which is as good a practical confirmation as this feature needs.
- **Pairing itself is not something to reimplement.** Stock MiSTer runs a separate helper,
  `/usr/sbin/btpair`, via `popen(..., "r")`, and simply streams whatever it prints to stdout
  into the OSD, line by line, until the process exits or is cancelled
  (`killall -SIGINT btpair btctl`). This project's own GUI can do the same — `popen()` the same
  `btpair` binary and show its output — rather than drive `bluetoothctl`'s scan/pair/trust
  sequence by hand. Lower risk, and consistent with this whole project's habit of reusing a
  data source or mechanism MiSTer already has rather than inventing a parallel one (see
  README's "not inventing formats of our own").
- `cfg.bt_reset_before_pair` (already a real `MiSTer.ini` key) resets the adapter
  (`hciconfig hci0 reset`) before a pairing attempt if set — this feature should honour it, not
  bypass it, same as stock does.
- **PIN entry: deliberately not handled.** Considered and dropped — in practice, controllers
  pairing over Bluetooth use BlueZ's "just works" Simple Secure Pairing, not a PIN prompt; a
  PIN is a keyboard/phone-pairing concern this feature does not need to solve for.
- **Detecting "a new controller just paired"**: rather than parse `btpair`'s own log text for
  a success message (coupling this feature to wording that is not this project's own and could
  change), this GUI already has a more reliable signal available: diff `Input`'s own device
  list (`Input::rescan`) from immediately before the pairing modal opened against immediately
  after it closes. Whatever `/dev/input` node is new is the newly paired pad; its vid/pid (via
  `EVIOCGID`, as everywhere else in this document) is what decides whether a `.map` file
  already exists for it.

## Non-goals for v1

- **Per-core mapping overrides.** Only the menu-wide default. A per-core override is a
  materially bigger feature (it needs a core picker, and has to work while a core is actually
  loaded, which is exactly when this GUI is not running) and nothing in this session's own use
  needed it. **Caveat worth surfacing in the UI, not just here:** if a core already has its own
  override (set previously via the stock OSD), changing the default here will visibly do
  nothing inside that specific game — a person who does not know per-core overrides exist has
  no way to guess why. Detecting that a per-core file exists for a given controller+core pair
  and saying so is cheap (it is just `FileExists` on a differently-named path) even though
  managing it is out of scope; worth doing for that reason alone.
- **The `_m` (modifier-held) alternate mapping slot**, or the separate joystick/keyboard
  "jump-kick" map (`_jk.map`) some cores use. Neither came up in this session's actual use.
- **Mouse support.** Explicitly deferred, see [Premise](#premise).
- **Advanced per-button-combo mapping** (the `advanced_input_..._v1.map` format, macros/combos).
  A materially different, more complex file format; nothing here needed it.
- **Rumble/force-feedback configuration.** Not touched by anything this session ran into.
- **A "treat identical controllers separately" toggle** (`controller_unique_mapping`). Two
  same-model pads sharing one mapping is the common case, not the exception this feature needs
  to solve for on day one — accessibility over complexity while this is still an early build.
  The controller list still *shows* when two pads share an `idstr` (see Feature 1); it does not
  yet offer to change it. **On the roadmap for later, not dropped.**

## Features

### 1. List connected controllers

Enumerates the same devices this GUI's own `Input` class already opens (`src/Input.cpp`,
`Input::rescan`), plus the metadata that class does not currently surface anywhere: name,
evdev-reported vendor/product ID (**not** a USB-descriptor ID — see above), whether it has a
hat (already probed today, for the D-pad double-count fix), and whichever `idstr` MiSTer itself
would use for it right now. Two pads sharing an `idstr` are shown as sharing one, visibly.

This is the entry point to everything else — input test, deadzone, and button mapping are all
reached by picking a controller from this list first.

**Two identical pads sharing one mapping is shown, not solved.** Giving two same-model
controllers different mappings (`controller_unique_mapping`) is real complexity — a new INI
key, a toggle, a "which one is this" question for the person configuring it — for a case that
is not the common one. Decided: **stay passive for v1** (see
[open question 4](#open-questions)); accessibility over completeness while this is still an
early build. The list still shows plainly when two pads share an `idstr` (that part is nearly
free — it is just what the list already has to display), it simply does not offer to change
it yet. The toggle itself moves to [Non-goals](#non-goals-for-v1), for a later roadmap pass.

A controller paired through [Bluetooth pairing](#5-connect-a-new-controller-via-bluetooth) is
not a separate category — once connected it is just another evdev device, and shows up in this
same list exactly like a freshly plugged-in USB pad. The Bluetooth flow only exists because
*getting* a Bluetooth pad connected in the first place needs its own step; nothing about it is
different from here on.

### 2. Input test

A live view of one selected controller's raw state: every digital button lights up while held,
both analogue sticks are shown as a dot inside a circle that moves live, both analogue triggers
(if present) as a fill bar. Read-only — nothing here writes anything, and (**decided after
hardware testing**) no button here does anything *but* light up either — see below for how you
actually leave.

**Default layout: the now-standard PS/Xbox-style pad** — one D-pad, two analogue sticks (each
with a click button), four face buttons, four shoulder buttons (L, R, L2, R2 — L2/R2 shown as
the analogue trigger bars above if the pad reports them that way, as a digital light
otherwise), Start, Select, and a Home/Guide button. This covers the large majority of pads
someone is likely to plug in without any setup at all. The face buttons are drawn as a real
diamond at the *physical* position each one actually reports (`BTN_NORTH`=top, `BTN_WEST`=left,
`BTN_EAST`=right, `BTN_SOUTH`=bottom), labelled with the Xbox letter that belongs at that
position (Y top, X left, B right, A bottom) — not the kernel's own `BTN_X`/`BTN_Y` aliases,
which do not match a real Xbox pad's own silkscreen for the left/top pair. Getting this
backwards is exactly what silently swapped X and Y for a real PS4 pad during a mapping run —
see [The .map file](#the-map-file), and [Button mapping](#4-button-mapping) below for where the
same distinction matters again and matters more.

**A "Sega layout" toggle for six-button pads (X/Y/Z top row, A/B/C bottom) was designed but is
disabled for now**, on request, rather than left in half-finished — dropped cleanly instead of
kept as dead, unreachable code. Revisiting it is still on the table; see
[Non-goals for v1](#non-goals-for-v1) for the same "accessibility over completeness" reasoning
this project applies elsewhere.

**Leaving this screen: decided, and revised twice after hardware testing.** Holding Start for
5 seconds leaves, exactly like the button-mapping wizard's own cancel gesture (see
[Button mapping](#4-button-mapping)) — chosen for the same reason: it cannot be mistaken for
this screen's own job of showing every other press. On top of that, **this screen also leaves
on its own after 10 seconds with no input at all**: some pads register extra sub-devices
alongside the real controller (a DualShock's own "Motion Sensors" or "Touchpad" nodes), and
pointing this screen at one of those can mean there is no Start to hold at all — without this,
that silently strands someone here for good. Getting "no input at all" right took two real
bugs to find: an axis's *rest* value is not reliably its range's mathematical centre (a
trigger sits at one end, not the middle), and it is not reliably *constant* either — a live
accelerometer/gyro keeps drifting a little even lying flat, so a one-time rest snapshot
eventually gets outrun by that drift over a full 10-second window. The working version tracks
each axis's own rest reading continuously, nudging it towards wherever the axis currently sits
*only while it reads as still resting* — real, sustained activity freezes it in place instead
of being allowed to quietly "catch up" and go stale.

### 3. Deadzone

Pick a controller (from the list), see its current deadzone if `MiSTer.ini` already has one for
it, adjust it (a simple left/right-adjusted number, 0–100), confirm to write. The live stick
view from the input test screen should stay visible while adjusting, drawn as a ring on top of
the stick position, so the effect on *this GUI's own reading* of the stick is visible
immediately. That is not quite the same claim as "this is what the deadzone will feel like in
a game" — this GUI never runs while a core is loaded, so it cannot preview the in-game feel
directly, only the raw displacement the setting is compared against. Whether the value then
applies live the next time a core loads, or needs a reboot, is an open question — see below.
Not every controller has an analogue stick at all (a Saturn-style pad, D-pad only); this screen
should not offer itself for one that does not.

### 4. Button mapping

Pick a controller, then step through the same fixed list of logical buttons MiSTer's own wizard
uses — "Press the button for **Right**", capturing the first raw event and advancing, exactly
the way the stock OSD wizard already does it (so this reproduces a flow players already know
from other consoles' setup, rather than inventing a new one). Also asks, separately, for the
combo that opens the OSD from inside a running game, and the buttons that confirm/cancel once
it is open — see [The .map file](#the-map-file) for exactly what each of those three prompts
writes and why they are not the same thing, after an earlier wrong assumption here broke OSD
confirm on real hardware. Of the three, only the OSD-open combo is worth a warning against
skipping: unlike "Menu: OK"/"Menu: Back" (which correctly fall back to this pad's own A/B
button if left unset), skipping it is a silent, easy-to-miss way to end up with the *original*
problem this whole feature is meant to prevent — no way back into the OSD from inside a game.

**Confirmed on hardware to need one more rule: once a physical control has answered a prompt,
it is retired for the rest of the wizard.** Without this, a button held a touch too long, or a
stick that has not quite recentred, silently answers the *next* prompt too — not a skip, a
button or axis quietly claiming a second slot it was never meant for. Every capture (a button,
a hat-derived D-pad direction, or a stick axis — see [The .map file](#the-map-file)) is checked
against everything already assigned earlier in the same run and ignored if it repeats.

**Confirmed on hardware to need a second, related rule: Start's own trailing release must not
answer the next step.** Whichever step just finished — Start's own capture, or a later step
Start was tapped to skip — the physical release of that same press can land a frame or two
into the *new* step. Without accounting for that, the new step's own hold/skip machinery sees
"Start down, then released before 5 seconds," which is exactly what a deliberate tap-to-skip
looks like — so it silently skips a step nobody meant to skip, from the tail end of a press
that was answering something else entirely. Each step now waits until Start has actually been
seen released at least once before its own hold/skip logic does anything at all; if Start was
not the control just used, this adds no delay (it is already up on the very first check).

**The very first button captured is always Start**, ahead of the D-pad and everything else —
not MiSTer's own ordering, a deliberate change. Before capturing anything, the wizard states
plainly what is about to happen and why it matters:

> First button to configure: **Start**. This button must be assigned correctly, because it is
> also how you can cancel this wizard once it is running: hold Start for 5 seconds at any point
> to abandon it, discarding everything captured so far. Choose wisely.

From that point on, **every remaining step follows one consistent rule built entirely around
the now-known Start button**:

- **Hold Start for 5 seconds** — abort the whole wizard, nothing saved, the previous mapping
  (if any) is untouched.
- **Press Start once** — skip the current button and move to the next, without assigning
  anything to it. This is also the recovery for a plain mis-press: fumble the button meant for
  "A" and press Start instead of trying again — the wizard moves on rather than locking in
  whatever was just pressed.
- **One minute with no input at all** — skip automatically, exactly as if Start had been
  pressed. This is what actually resolves "the wizard is asking for a button this pad does not
  have" (L2/R2 on a pad with no analogue triggers, for instance): nobody has to know in advance
  which buttons are missing, the wizard simply moves itself along. The wait has to be **shown
  counting down** (a small progress bar, not a silent pause) — a full minute of apparent
  nothing reads as a freeze unless it visibly is not one.
- **Unplugging the controller entirely** works too, as an alternative to the 5-second hold, not
  a replacement for it — handled the same way a disconnect anywhere else in this feature is
  (fall back to the controller list). Someone whose Start button is itself the dead one still
  has a way out.

The bottom-bar hint has to carry the hold-to-cancel reminder on every single screen of the
wizard from here on, not only the opening one. This whole scheme is the answer to "what if the
pad turns out to be unusable mid-wizard": Start is guaranteed to exist on any controller worth
mapping, is captured first (so it is always known from that point on, independent of whatever
comes later in the sequence), and every one of the three exits above only needs Start to already
work.

**Added after hardware testing: a schematic pad diagram, not just a word.** "Press the button
for X" is ambiguous on its own — X means a different physical button depending on which pad
convention the person grew up with, which is exactly what caused the swap described in
[The .map file](#the-map-file). Each step now draws a small schematic controller (D-pad,
face-button diamond, shoulders, Home, Select/Start, both sticks) with the one relevant part lit
up — a directional square, a face position, a stick with a bar through it pointing along
whichever axis ("left/right" vs "up/down") that step is asking for. The three OSD-related steps
have no diagram at all: "Confirm inside the menu" is not tied to one physical spot, and "open
the menu" is highlighted at the Home/Guide position not because the `.map` format requires it
but because that is, in practice, by far the most common real answer on a modern pad.

### 5. Connect a new controller via Bluetooth

Only offered at all if a Bluetooth adapter is present (see above) — hidden, not shown disabled,
if not, since "not every MiSTer has Bluetooth" per the premise for this feature. Reached from
the controller list (`Settings → Controllers → Connect new controller via Bluetooth`), it opens
as a modal rather than a full screen, since it does not need the controller list's own state and
should be trivially dismissible:

1. **Idle** — "Press A to start scanning" / "B to close." Nothing running yet.
2. **Scanning** — `btpair` running, its output shown live (translated into this GUI's own
   visual language rather than a raw scrolling log, where that is easy — see mockup below);
   cancellable at any point, which signals the child process directly (this GUI holds its own
   `popen` handle, so a targeted signal rather than stock's own `killall` by name is both
   possible and safer — nothing else on the device is named `btpair`, but killing by name is
   still killing something this process did not necessarily start).
3. **New device found** (the device-list diff described above fires) — compute its idstr, check
   `input_<idstr>_v3.map` for existence.
   - **Exists already** (this exact pad, or an identical one, was configured before): a plain
     success message, done.
   - **Does not exist**: offer the button-mapping wizard (Feature 4) right away, pre-selected
     to this controller, with a skip — pairing succeeded either way; mapping is a separate
     offer, not a requirement blocking "done."
4. **Timeout / no device found**: say so plainly rather than sit scanning forever — a fixed,
   generous timeout (stock's own screen has no timeout at all, relying on a manual cancel; this
   GUI can do better without breaking anything by adding one, e.g. two minutes) with a clear
   "did not find a new device — try again?" rather than an unbounded spinner.

## Mockups

All mockups below use this project's existing bottom-bar hint convention (see any current
screen) and reuse the tile-grid list style already established for
[SystemVisibilityScreen](src/SystemVisibilityScreen.h) ("Manage systems") — a full-height list
on the left, detail/preview on the right, since a controller list is structurally the same kind
of screen.

### Controller list (entry point)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Settings › Controllers                                                       │
├───────────────────────────────────┬──────────────────────────────────────────┤
│                                    │                                          │
│  ▸ Xbox 360 Controller       ●     │   Xbox 360 Controller                   │
│    Xbox 360 Controller       ●     │                                          │
│    Xbox Series X Pad              │   045e:02a1  ·  hat: yes                 │
│    Retro-Bit Saturn Pad            │   via Xbox 360 Wireless Receiver         │
│                                    │                                          │
│    ⎔ Connect new controller        │   ● Shares its mapping with one other    │
│      via Bluetooth                 │     identical controller below           │
│                                    │                                          │
│                                    │   Deadzone:  25   (set)                 │
│                                    │   Button mapping:  set, 12/12 buttons    │
│                                    │                                          │
│                                    │                                          │
├───────────────────────────────────┴──────────────────────────────────────────┤
│  A Open   B Back   Y Input test   X Deadzone   Menu Button mapping           │
└──────────────────────────────────────────────────────────────────────────────┘
```

The Bluetooth row above is only present if a Bluetooth adapter was detected at all (see
[Bluetooth pairing](#bluetooth-pairing)) — a device without one simply does not have this row,
rather than showing it disabled with no way to explain why.

### Input test

Default, PS/Xbox-style layout:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Controllers › Xbox 360 Controller › Input test                              │
├──────────────────────────────────────────────────────────────────────────────┤
│  L ( )  L2 ▓▓▓▓▓░░░░░░░  0%                          0%  ░░░░░░░▓▓▓▓▓  R2  R ( ) │
│                                                                                │
│                    ┌───┐                                    ( Y )            │
│                    │ ▲ │                                                     │
│              ┌───┐ └───┘ ┌───┐                        ( X )       ( B )      │
│              │ ◀ │       │ ▶ │              ( home )                        │
│              └───┘ ┌───┐ └───┘                                    ( A )      │
│                    │ ▼ │                                                     │
│                    └───┘                                                     │
│                                                                                │
│         ⬤ ← stick 1                              ⬤ ← stick 2                 │
│        (·)                                       (·)                        │
│         dead zone: 25                             dead zone: —              │
│                                                                                │
│                    Select ( )        Start ( )                              │
│                                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│  X Sega layout (adds Z, C)                                        B Back    │
└──────────────────────────────────────────────────────────────────────────────┘
```

With the Sega-layout toggle on, the face-button cluster grows a second row (Z/C added, X/Y/A/B
relabelled to the two-row arcade convention) — otherwise identical:

```
│                          ( X )     ( Y )     ( Z )                          │
│                          ( A )     ( B )     ( C )                          │
```

Buttons/D-pad fill solid while held; the stick dot moves live inside its circle; the dead-zone
ring is drawn on the same circle so its effect is visible without leaving the screen. The
Sega-layout toggle is remembered per this GUI's own preferences (not written anywhere MiSTer
itself reads) — it only changes what this one screen draws.

### Deadzone

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Controllers › Xbox 360 Controller › Deadzone                                │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│   Stick 1                                                                     │
│                                                                                │
│              ⬤   ◀────────────────█████████──────────────▶                   │
│             (·)                                                              │
│                        0                 25                 100              │
│                                                                                │
│   Move the stick around — the ring shows where input starts counting.        │
│                                                                                │
│   Writes to MiSTer.ini:  deadzone=0x045e02a1, 25                            │
│                                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│  ◀▶ Adjust   A Save   B Cancel                                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Button mapping (capture wizard)

Opening screen — the warning, before anything is captured:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Controllers › Xbox 360 Controller › Button mapping                          │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│         First button to configure:  Start                                    │
│                                                                                │
│         This button must be assigned correctly — it is also how you          │
│         cancel this wizard once it starts. Hold Start for 5 seconds           │
│         at any point to abandon it, discarding everything captured           │
│         so far.                                                               │
│                                                                                │
│         Choose wisely.                                                       │
│                                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│  A Begin   B Cancel, nothing captured yet                                    │
└──────────────────────────────────────────────────────────────────────────────┘
```

A later step, mid-capture — the hold-to-cancel hint is on every single screen from here on, not
just the first:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Controllers › Xbox 360 Controller › Button mapping                          │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│                                                                                │
│                         Press the button for  L2                             │
│                                                                                │
│                                    ┌───┐                                      │
│                                    │L2 │                                      │
│                                    └───┘                                      │
│                                                                                │
│              waiting… ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░  skips in 0:37    │
│                                                                                │
│              ●●●●○○○○○○○○○                                                   │
│              Start  Right  Left  Down  Up  A  B  X  Y  L  R  Select          │
│                                                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│  Start: press = skip · hold 5s = cancel wizard, nothing saved               │
└──────────────────────────────────────────────────────────────────────────────┘
```

No button pressed for a whole minute (this pad has no L2) auto-skips exactly as if Start had
been pressed — the countdown bar above is what makes that a visible, unsurprising thing to
watch happen rather than something that looks like the screen froze.

The wizard captures one raw evdev event per step, not through this GUI's own D-pad/`Action`
handling — which is exactly why the cancel gesture cannot be an ordinary button press on this
same screen (any single press is a candidate answer for the prompt on screen, never a command
*about* the wizard). A 5-second hold on a button whose identity the wizard already captured
first removes that ambiguity: nothing this early in the sequence can be mistaken for "that was
the answer," because holding is not the shape a normal answer takes.

### Connect a new controller via Bluetooth (modal, three states)

```
┌─ Connect new controller ──────────────────────┐  ┌─ Connect new controller ──────────────────────┐
│                                                │  │                                                │
│         Put your controller into              │  │              Scanning…                        │
│         pairing mode, then press A             │  │                                                │
│                                                │  │        (waiting for a device to               │
│                                                │  │             answer, up to 2:00)                │
│                                                │  │                                                │
│                                                │  │                                                │
├────────────────────────────────────────────────┤  ├────────────────────────────────────────────────┤
│  A Start scanning   B Close                    │  │  B Cancel                                       │
└────────────────────────────────────────────────┘  └────────────────────────────────────────────────┘

┌─ Connect new controller ──────────────────────┐
│                                                │
│         Found: Xbox 360 Controller             │
│         045e:02a1                              │
│                                                │
│         This controller has no button          │
│         mapping yet.                           │
│                                                │
│         Set it up now?                         │
│                                                │
├────────────────────────────────────────────────┤
│  A Set up now   B Skip for now                 │
└────────────────────────────────────────────────┘
```

## Data flow

```
  ┌────────────────────────┐   open modal   ┌─────────────────────────────┐
  │   Controller list       │ ─────────────▶ │  Bluetooth pairing modal      │
  │   (new Screen)          │ ◀───────────── │  popen("btpair"); diffs       │
  └───────────┬─────────────┘   closes,      │  Input::rescan() for a new    │
              │                 new pad      │  device while scanning        │
              │ select a        now in the   └──────────────┬────────────────┘
              │ controller      list                         │ new pad has no
              │                                               │ mapping yet
   ┌──────────┼──────────┐                                    │
   ▼          ▼          ▼                                    ▼
┌────────┐ ┌────────┐ ┌────────────────┐          offers, pre-selected:
│ Input  │ │Deadzone│ │ Button mapping  │ ◀────────────────────┘
│ test   │ │        │ │                 │
│(read-  │ │reads/  │ │ reads/writes    │
│ only)  │ │writes  │ │ config/inputs/  │
│        │ │MiSTer  │ │ input_*_v3.map  │
│        │ │.ini    │ │ (new writer)    │
│        │ │(new    │ │                 │
│        │ │writer) │ │                 │
└────────┘ └────────┘ └────────────────┘
```

Both file writers (deadzone, button mapping) are new, small, single-purpose classes (parallel
to `Preferences`, but writing *outside* `mister-pat/` — the first thing in this project to do so
from inside the running GUI, rather than from a one-time install/patch step). That distinction
is worth its own review; see below. The deadzone writer also writes *inside* `mister-pat/` —
its mandatory backup, into `mister-ini-backups/` (see
[Writing MiSTer.ini safely](#writing-misterini-safely)), before it ever touches `MiSTer.ini`
itself. The Bluetooth modal writes nothing itself — it only starts `/usr/sbin/btpair` as a
child process and reads its output, the same as stock MiSTer's own OSD screen does; a newly
paired pad becomes visible to the rest of this diagram simply by being a new entry in the
controller list, same as any other pad.

## Open questions

Things this design surfaces without a clear existing answer — for a person to decide, not
something to guess into the spec:

1. **Does a deadzone change need a reboot to take effect**, or does MiSTer's own core re-init
   pick up an `MiSTer.ini` edit live the next time `is_menu()` re-runs `cfg_parse()`? **Not
   answered from source — to be tested jointly, on hardware, before this screen is built**: has
   its own live-preview claim in [Feature 3](#3-deadzone) that depends on the answer, and
   guessing wrong here means shipping a screen that visibly lies about when "Save" takes
   effect.
2. **Do the three per-video-mode INI siblings need `deadzone=` too?** **Decided: no.** This
   feature reads and writes `MiSTer.ini` only, deliberately, even though the install step's own
   `main=`/`gui=` duplication touches all four files — deadzone is not part of that
   requirement, and keeping this feature scoped to one file is the simpler, correct choice, not
   an oversight.
3. **What happens if the button-mapping wizard is started with no keyboard and the pad itself
   turns out to be unusable mid-wizard** (a dead button, a disconnect)? **Decided.** The
   wizard's first captured button is always **Start** — every controller has one — and the
   wizard opens by saying so plainly before capturing anything:

   > First button to configure: **Start**. This button must be assigned correctly, because it
   > is also how you can cancel this wizard once it is running: hold Start for 5 seconds at any
   > point to abandon the whole wizard, discarding everything captured so far. Choose wisely.

   The 5-second hold-to-cancel is then live for the *entire* wizard, not just that first step,
   and the bottom-bar hint has to say so on every single screen of the wizard, not only the
   first — someone two steps in who forgot the warning still needs to see "hold Start 5s:
   cancel, nothing saved" without scrolling back. Cancelling this way must not write anything;
   the previous mapping (if any) stays exactly as it was. See the updated
   [Button mapping](#4-button-mapping) mockup below.
4. **Two identical pads, one plugged in later, silently inheriting the first one's mapping
   and deadzone**: **decided — stay passive.** No proactive notice when a second matching pad
   appears, no nag, no toggle offered yet at all; the list already shows the sharing plainly
   (Feature 1), and that is as far as v1 goes. Accessibility over complexity for an early
   build — revisit once `controller_unique_mapping` itself is on the roadmap (see
   [Non-goals](#non-goals-for-v1)).
5. **Concurrent write safety.** **Decided, and hardened further than "atomic write" alone:**
   every `MiSTer.ini` write is preceded by a timestamped backup into
   `/media/fat/mister-pat/mister-ini-backups/` (cap 30, oldest deleted first) — see
   [Writing MiSTer.ini safely](#writing-misterini-safely) above. "This file must never be
   allowed to break" was the explicit instruction; a backup is the recovery path if atomic
   write ever turns out not to be enough on some edge case this design did not anticipate. The
   `.map` file gets the same atomic-write discipline, deliberately without a backup — it is
   this feature's own file, not load-bearing for booting, and a missing one already has a sane
   built-in fallback (see `load_map`).
6. **Should the input test screen also show the two mouse-emulation axes and the OSD-toggle
   slot?** **Decided: no**, for the mouse-emulation axes — consistent with mouse support being
   out of scope for this whole feature (see [Premise](#premise)); showing controls for a mode
   this feature does not support would be confusing, not helpful. The OSD-toggle slot is a
   separate concern: it is not a mouse thing, and the button-mapping wizard still captures it
   (unchanged from the original design) — this input test screen still does not need to show
   it, since verifying "does this button press reach the MiSTer" for OSD-toggle specifically
   is what the wizard's own capture step already demonstrates live.
7. **What happens if the selected controller disconnects mid-screen?** **Decided: treat it
   exactly like an abort** — the input test, deadzone screen, or button-mapping wizard all fall
   back to the controller list, the same place the 5-second Start hold goes. Worth stating
   plainly in the UI as a *second*, equally valid way out of the button-mapping wizard
   specifically: **unplugging the controller works as an alternative to holding Start for 5
   seconds** — not instead of it, alongside it. Someone whose Start button is itself the
   problem still has a way out.
8. **Should there be a way back to "known good" if a mapping goes wrong?** **Decided.** Once
   Start itself is captured (always the first step), it doubles as this wizard's "confirm/
   recover" control for every step after: press it once mid-wizard and the *current* step is
   skipped without being (mis)assigned — so mis-pressing the button being asked for during, say,
   the A-button step does not lock in a wrong value; Start moves past it instead, leaving that
   slot as it was. See question 10 below for how this combines with a timeout into one
   consistent rule for every step after the first.
9. **Should the deadzone/mapping writers be covered by this project's existing host-side test
   suite** (`tests/`, `make -C tests`)? **Decided: yes, unconditionally** — "`MiSTer.ini` must
   never be allowed to break" was the explicit reason given, and that is exactly the kind of
   claim a test suite exists to keep true across future changes, not just something to believe
   once and hope holds.
10. **Per-step timeout during button capture** (distinct from the Bluetooth scan timeout
    below): what happens if the wizard asks for a button the pad does not have at all — L2/R2
    on a pad without analogue triggers, or any slot on a pad that is otherwise missing a
    button? **Decided**, and folded into the same Start-button mechanism as question 8, into
    one consistent rule that holds for every step after Start itself is captured:
    - **Hold Start 5 seconds**: abort the whole wizard, nothing saved.
    - **Press Start once**: skip this button, move to the next.
    - **1 minute with no input at all on this step**: skip this button automatically, same as
      pressing Start once — this is what actually solves "the pad has no L2," since nobody has
      to know in advance that a button does not exist; the wizard just moves on by itself.
    - The countdown for that third case needs to be **visible** — a small progress bar or
      similar, not a silent wait — so a minute of nothing happening reads as "this is
      counting down," not as "did this freeze?" See the updated
      [Button mapping](#4-button-mapping) mockup below.

    (The Bluetooth scan timeout is a separate, unrelated number — still an open placeholder at
    two minutes, see below — this one belongs to the capture wizard specifically.)
11. **Is the 2-minute Bluetooth scan timeout the right number?** Still open — placeholder,
    unmeasured against real hardware this session (the device was unreachable when this section
    was written). Not to be confused with the button-capture wizard's own 1-minute per-step
    timeout above, which is a separate, decided number for a different screen.
12. **What should "Delete all pairings" (stock's `Backspace` → `bluetoothd renew`) become
    here?** **Decided: yes, do it** — a "forget this controller" scoped to one paired device,
    not stock's all-or-nothing reset. Whether `bluetoothctl`/`bluetoothd` actually supports
    forgetting one device by address specifically (rather than only "forget everything") was
    not confirmed this session and needs its own look before this is built, but the *intent* is
    settled: per-controller, not global.
