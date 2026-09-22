# Performance

All numbers measured on real hardware (DE10-Nano, dual-core Cortex-A9 @ 800 MHz, no GPU,
framebuffer 1920×1080 × 32 bpp). Target: **smooth 30 fps at native 1080p**, i.e. ≤ 33 ms
per frame.

## Measurement Method

The application ships with its own instrumentation, so that nothing has to be guessed:

```sh
mister-gui --tab systems --frames 30 --no-grab --stats
```

It sums up four phases and reports the mean per frame:

| Phase | What happens in it |
| --- | --- |
| `background` | Full-surface gradient across the entire canvas |
| `chrome` | Header and footer (clock, tabs, network, key hints) |
| `screen` | The active screen, i.e. tiles, lists, box art |
| `present` | Transferring the canvas into the framebuffer |

Coarse measurement of the total time from the outside:

```sh
S=$(date +%s%N); mister-gui --tab systems --frames 30 --no-grab >/dev/null; E=$(date +%s%N)
echo $(( (E-S)/30000000 )) ms/Frame
```

Careful: the loop waits up to 33 ms per frame for input. The external measurement includes
that wait, the phase measurement does not.

## Starting Point

First working version, systems grid with 12 tiles:

| Phase | ms/frame |
| --- | --- |
| background | 12.2 |
| chrome | 1.7 |
| **screen** | **137.4** |
| present | 13.8 |
| **Total** | **≈ 165 ms (6 fps)** |

That is roughly 11 ms per tile. Unusable.

## What Made the Tiles So Expensive

The shadow was implemented as a stack of semi-transparent rounded rectangles. With a spread
of `px(14)` that meant 14 layers, each covering the **full** tile area, each going through
the alpha-blend path. With 12 tiles that added up to several million blend operations per
frame.

Three fixes, all following the principle "touch fewer pixels":

1. **Layer count capped at 4**, regardless of the spread. Every layer costs a full pass, and
   beyond four layers the visual result barely improves.
2. **Shadow only for the focused tile.** On a dark background the shadows of the remaining
   tiles are hardly perceptible anyway.
3. **Skip the occluded area.** The shadow was also painted *underneath* the tile that was
   subsequently drawn opaquely on top of it — pure waste. Now only the ring outside the tile
   body is touched.

Two more fixes addressing the same root cause:

4. **The border draws only the ring.** `strokeRoundedRect` previously tested the entire area
   pixel by pixel via a distance field, square root included, even though only a narrow ring
   can ever be covered.
5. **Opaque image pixels are written, not blended.** Box art is usually fully opaque; the
   blend path was redundant there.

## State After These Fixes

| Screen | background | chrome | screen | present | Total |
| --- | --- | --- | --- | --- | --- |
| Settings (7 rows) | 12.2 | 1.8 | ~1 | 13.7 | ≈ 29 ms |
| Systems (12 tiles) | 12.2 | 1.8 | 24.5 | 13.7 | ≈ 52 ms |
| Games, grid (box art) | 12.3 | 1.9 | 32.9 | 13.7 | ≈ 61 ms |

The screen portion has thus dropped by a factor of 5.6 (137 → 24.5 ms). But we are not at
the target yet.

## The Real Limit

`background` and `present` are content-independent and together account for
**≈ 26 ms per frame**:

- `background`: 1920 × 1080 pixels × 4 bytes = 8.3 MB written in 12.2 ms → **≈ 680 MB/s**
- `present`: the same 8.3 MB copied into the framebuffer in 13.7 ms → **≈ 605 MB/s**

That matches the documented DDR3 bandwidth of the scaler path (a good 1000 MB/s gross). So
we are **not compute-bound here, but bandwidth-bound**.

From this follows a hard conclusion: as long as every frame rebuilds the whole screen *and*
transfers it in full, the upper limit is about 38 fps — **with an empty screen**. Any content
comes off that budget. 30 fps is not reachable this way.

## Implemented: Dirty Rectangles

Built and measured on the device — the numbers are [further below](#measured-on-the-device).

Three building blocks:

1. **The background is prepared once.** `App` keeps a second canvas holding the finished
   gradient. Instead of redrawing it per frame (measured: 12.2 ms), it is only copied back
   where something new is about to be drawn.
2. **Every drawing operation reports its area.** `Canvas` maintains a short list of merged
   rectangles (at most twelve, adjacent ones get coalesced). The marking deliberately does
   **not** live in `blendPixel` — there it would cost more than the entire saving; instead
   the calling functions report their outlines once.
3. **Only what was reported gets transferred.** `Framebuffer::present(canvas, regions)`
   copies just the affected row segments.

On top of that, each screen decides for itself whether it draws incrementally
(`Screen::incremental()`). The systems screen already does: it remembers the focus values it
last painted along with the scroll position and redraws only those tiles whose focus actually
changed — including the neighbors that get covered by the growth and shadow of the moving
tile. If nothing changes, nothing is drawn at all.

The remaining screens report `false` and get their area reset as before. That way the
migration can be done one screen at a time without anything being broken in between.

**Fallback switch:** `--full-redraw` turns the whole thing off and paints every frame in full
again. Should rendering glitches show up tomorrow, this makes it possible to check
immediately whether they come from incremental drawing.

### Verified on the Development Machine

Incremental drawing fails on small details that are hard to spot on a TV: an area that is not
reported while drawing leaves stray old pixels behind. Because `Canvas` only depends on
`Color` and `Geometry`, it can be compiled and tested on the Mac — see [`tests/`](tests/), run
with `make -C tests`.

The load-bearing guarantee is: *draw, then copy the reported area back from the background,
and the result must be exactly the initial state.* That is precisely what the test checks,
pixel by pixel, together with the merging of regions and the upper bound on the list.

In doing so, the test **found two real bugs** that I would otherwise only have noticed
through odd leftover pixels:

1. **`dropShadow` did not report its area.** I had removed the reporting from `blendPixel`
   because it is too expensive in the hot path, and forgot to add a replacement report in the
   shadow.
2. **The safety margin around a tile was too small at the bottom.** The shadow is offset
   downward and extends one and a half times its spread beyond the tile — my estimated margin
   was five pixels short. Rather than bumping the number, `Canvas::shadowBounds()` now
   exposes the actual extent, and `Tile::footprint()` derives the area from it. Nothing is
   guessed anymore.

For the games screen I deliberately chose a coarser granularity based on the same insight: a
focused tile grows beyond its row, so restoring row by row would leave residue in the
neighboring row. With three rows, "redraw the content area on every change, draw nothing when
idle" is provably correct and barely slower.

### Measured on the Device

60 frames while idle, using `--stats` against `--full-redraw`:

| Screen | Mode | background | chrome | screen | present | Total |
| --- | --- | --- | --- | --- | --- | --- |
| Systems | Dirty Rectangles | 0.3 | 4.3 | 0.7 | 2.5 | **7.8 ms** |
| Systems | full redraw | 19.6 | 1.5 | 2.7 | 14.3 | 38.1 ms |
| Games | Dirty Rectangles | 0.3 | 4.1 | 5.5 | 3.4 | **13.3 ms** |
| Games | full redraw | 18.7 | 1.6 | 6.9 | 12.4 | 39.6 ms |

Roughly **five times faster**, and at 7.8 ms the systems screen sits far below the 33 ms that
30 frames per second require. The two content-independent items have all but vanished: the
background from 19.6 to 0.3 ms, the transfer from 14.3 to 2.5 ms.

Two caveats about this measurement:

- It shows the **idle state**. During navigation the redraw of the moving tiles is added on
  top; that is not quantified yet, because it requires input.
- `chrome` has **gone up** from 1.5 to 4.3 ms, because header and footer are now restored
  from the background and redrawn every frame. That could be saved by touching them only on
  an actual change — the clock changes once a minute. At a total of 7.8 ms, though, this is
  not urgent.

## Original Plan: Dirty Rectangles

Console Mode renders smoothly at native 1080p on the same device. That is the proof that the
resolution is not the problem — rather, that you must not rebuild every frame from scratch.

The next step is therefore not micro-tuning but a change of strategy:

1. **Render the background once** and keep it in a buffer. Instead of a gradient per frame,
   only copy back the areas that were overdrawn.
2. **Transfer only changed areas.** The canvas maintains a list of damaged rectangles;
   `present` transfers only those row segments.
3. **Animations bound themselves.** A focus change affects two tiles, not the grid. As soon
   as the animation runs out, the damaged area drops to zero and the frame rate is determined
   solely by the input loop.

Expectation: near 0 ms of transfer while idle, and on a focus change two tile areas
(~300 × 250 px each) instead of 2.07 million pixels — about one fortieth.

## Further Approaches, If That Is Not Enough

- **Blit font glyphs as a whole.** `Font::draw` calls `blendPixel` per pixel, clip check
  included. A glyph could be blended row by row with a precomputed offset.
- **Drop the gradient in the background.** A solid fill is just as expensive as a gradient,
  because the cost is in the writing — but a cached background makes both irrelevant.
- **NEON for the blend loops.** The blend computes three multiplications and divisions by 255
  per pixel. Four pixels in parallel would be realistic.
- **Lower the internal resolution** (`fb_cmd0 8888 0 2` halves it, the FPGA scaler scales it
  back up). Explicitly the *last* resort — native 1080p is the goal.

## One-Time Costs: Decoding Images and Building Glyphs

The phase measurement averages over all frames and thereby swallows one-time costs. They can
be factored out, however, by taking a short and a long measurement (games grid, one piece of
box art):

| Run | screen ms/frame | screen total |
| --- | --- | --- |
| `--frames 2` | 53.0 | 106 ms |
| `--frames 30` | 31.2 | 936 ms |

From `2S + D = 106` and `30S + D = 936` it follows that the recurring cost is
**S ≈ 29.6 ms** and the one-time cost **D ≈ 47 ms**.

**So decoding and scaling a single piece of box art costs about 47 ms** (source: 512 × 364 px,
275 KB PNG). That is more than an entire frame at 30 fps.

The same holds for text: `chrome` costs 10 ms in the first frame and 1.7 ms afterwards. So
the glyph cache has to be filled once per font size, at a cost of roughly **18 ms**.

### A Bug in the Current Version

`ImageCache::beginFrame(3)` permits three decodes per frame. At 47 ms each that works out to
**as much as 140 ms in a single frame** — a clearly visible stutter, exactly when scrolling
into tiles that have not been loaded yet. The budget has to be time-based (something like
"keep decoding as long as less than 8 ms of this frame has been used up"), or the decoding
belongs on the second core.

### The Key Insight

The decode time depends on the **resolution of the source file**, not on the target size.
Shrinking a piece of box art to tile size after decoding saves nothing — the expensive part
has already happened. Smaller images can only be had through **smaller files on disk**. That
is exactly why the three-tier store described below is the right approach and not a
micro-optimization.

## Constraint: Do Not Overrun the Storage Device

The MiSTer's power supply for USB devices is tight — a well-known issue on the platform. Load
spikes can cause a drive to drop off the bus.

**Observed on the test device, four failures in one evening**, each at a load spike:

| Event | Trigger |
| --- | --- |
| 1 | FPGA reconfiguration by our own bitstream loader |
| 2 | Core switch via `load_core` (FPGA reconfiguration) |
| 3 | During normal operation, shortly after a core switch |
| 4 | **Startup of our GUI: 7192 games from 26 directories read in one go** |

Symptoms in the kernel log: `device descriptor read/64, error -110`, followed by
`unable to enumerate USB device`; later `FAT-fs: Directory bread(block …) failed` and
`Volume was not properly unmounted`.

Case 4 is the uncomfortable one for us: **that was our design, not the hardware.** The current
startup behavior is about the worst possible:

- `Library::load()` checks all 92 systems, each with its own directory scan
- `HomeScreen::refresh()` then reads in all games of all systems
- both immediately after boot, while drive and FPGA are drawing power anyway

### Solved: Console Mode's Cache

The decisive hint came from the observation that Console Mode runs without trouble on the same
device — and that our GUI runs without trouble too when started **after** ConsoleMode. The
difference is not the amount of data but **where it comes from**: at startup, Console Mode does
not scan the games volume, it reads its own index from the SD card.

`/media/fat/ConsoleMode/caches/usb_<volume-id>.txt`, 834 KB, 8856 lines, tab-separated:

```
ATARI 2600	Adventure (NA).a26	games/Atari2600/Adventure (NA).a26	0
```

System name, file name, volume-relative path. The system names are the same as in the section
files we read anyway — so the file fits our data model without any translation. `GameIndex`
reads it, and `Library` prefers it over any directory scan.

**Measured, with the storage device unmounted:**

| | Directory scan | Index from the SD card |
| --- | --- | --- |
| Time per frame while building | 555 ms | 82 ms |
| Accesses to the games volume | thousands | **none** |
| Works without the storage device mounted | no | **yes, all 8856 games** |

The last point is the most important: the UI is fully usable while the storage device is not
even present. It is only needed when a game is actually launched.

**Rule derived from this:** The application **never** scans the games volume on its own
initiative. `Library::setScanningAllowed()` is off by default; it is enabled exclusively by
"Rescan library" in the settings, i.e. by a deliberate decision of the user. Existence checks
on directories are skipped as well — box art paths are constructed, and a missing file costs
no more than a failed open.

### Rules Derived From This

1. **Read nothing at startup.** The UI appears first; data follows.
2. **Only start after a quiet period** — a few seconds after boot, once drive and FPGA have
   settled down.
3. **Read throttled instead of in one go.** One directory per time slice, not 26 back to back.
4. **Only read what is shown.** "All games" needs the first entries when it is opened, not
   7192.
5. **Index once, then read from the database** (see below). That replaces repeated directory
   scans with a single file.
6. **Never read blocking.** If a filesystem hangs, otherwise the whole UI hangs — on the test
   device, `ls /media/usb0` could not be terminated for minutes. File access belongs in a
   background thread.

Point 6 is additionally a robustness bug in the current version, independent of the power
issue.

## Planned: Large Libraries

Two things determine responsiveness, and both scale with library size.

### What the Current Version Gets Wrong

- `Library::gamesOf()` rescans the directory and sorts the result on **every** opening of a
  system. Nothing is retained between calls or across program starts.
- `Library::hasGames()` iterates over **all 92** systems at startup in order to filter the
  grid — 92 `opendir` passes before the first frame is up.
- On exFAT mounted with `sync,dirsync`, every directory access is expensive.
- There is still **no measurement with a large library**. At the time of measurement the test
  device only had the contents of `/media/fat` (12 systems), because the USB storage device
  had dropped out. This measurement has to be repeated once the library is reachable again —
  only then will we know the real startup times.

### Intended: Our Own Game Database

Instead of interrogating the filesystem, the inventory is indexed once and read from
afterwards:

- **Built in the background**, not at startup. The UI is up immediately and fills in.
- **One file per system** instead of one big one. Opening SNES loads only SNES.
- **Chunked loading while scrolling.** The file is held in fixed-size blocks (about 200
  entries); only a window is ever visible, and adjacent blocks are prefetched. The rest stays
  on disk.
- **Fixed record format**, so that the n-th entry is reachable without parsing everything
  before it — a prerequisite for jumping into the middle of a 10,000-entry list.
- **Change detection** via directory timestamps, so that only new additions get read in.

Open question: whether the database can be shared with Console Mode's `caches/` files. Its
`usb_*.txt` is 834 KB and looks like exactly this kind of index — that needs checking before
we invent a format of our own.

## Planned: Three-Tier Image Quality

Goal: show something immediately, then sharpen it up unobtrusively — and never wait for an
image.

### Three Tiers on Disk

Alongside the original, two smaller versions are kept, generated while the database is being
built:

| Tier | Intended for | Rough target width |
| --- | --- | --- |
| Thumbnail | Grid, small box art, fast scrolling | ~160 px |
| Middle | Large box art, detail pane | ~480 px |
| Full | Original file, only when the image stays put | unchanged |

Since decode time depends on the source resolution, a thumbnail should be about an order of
magnitude cheaper than the measured 47 ms. That is to be verified, not believed.

### Timeline

Following the idea of letting quality grow in afterwards:

1. **0 to ~2 s:** Load and show the thumbnail. Until it is there, a placeholder stands in.
2. **From ~3 s** of dwell time: fetch Middle into memory in the background.
3. **~3 to ~5 s:** **Fade in** Middle over two seconds.
4. **From ~5 s:** Fetch Full and fade it in the same way.

If the focus leaves the image before that, the next tier is not loaded at all. Scrolling
through quickly therefore incurs thumbnail costs only.

### Overlay Instead of Swap

The core of the idea: the higher tier is drawn **on top of** the lower one and its opacity is
driven from 0 to 255, while the lower one stays in place. Because both show the same subject
in the same spot, you do not see a switch, only the image becoming sharper. A hard swap, by
contrast, would stand out as a flicker, especially with edges that differ because of the
scaling.

The prerequisite is that both tiers are placed **pixel-perfectly on top of each other** — i.e.
the same target rectangle from `fitAspect`, not one derived from the respective image size.
Otherwise the subject shifts during the fade, and that is exactly what you would notice.

To be tried out:

- Whether drawing two layers at once stays within budget — during the two seconds of
  cross-fade, every frame costs two image passes instead of one.
- Whether the cross-fade from thumbnail to Middle is even noticeable, or whether a jump is
  good enough. If so, we save ourselves the second layer.
- Whether one intermediate step is enough (thumbnail and Full only).

## Measurement Log

| Date | Change | Systems grid |
| --- | --- | --- |
| 2026-09-21 | First working version | 165 ms (6 fps) |
| 2026-09-21 | Shadow capped at 4 layers, focused tile only | 60 ms (17 fps) |
| 2026-09-21 | Shadow and border ring instead of full area, opaque image pixels written directly | 52 ms (19 fps) |
| 2026-09-22 | Dirty Rectangles, background cached | 7.8 ms while idle |
