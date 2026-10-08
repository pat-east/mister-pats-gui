# vNext — 0.4.0 Optimisation

This document turns the 0.4.0 roadmap into an implementation plan. The release is about the
*feel* of the interface: tab changes should look smooth, and moving through the library should
respond immediately. The first step is a Settings switch that hides box art and backgrounds,
so normal use can show whether browsing feels faster without artwork in the way. We will then
use the GUI and record where delays are noticeable before choosing the next optimisation.

The Systems tab's first-visit delay is already addressed by the game database catalogue and
is checked off in the roadmap; it is not new 0.4.0 work.

## What is already in place

- The renderer can cache the background, track damaged rectangles and copy only those areas
  to the framebuffer. On the DE10-Nano, idle Systems measured 7.8 ms/frame with dirty
  rectangles versus 38.1 ms with a full redraw; Games measured 13.3 ms versus 39.6 ms.
- Systems and Home redraw incrementally. Games still redraws its content area when needed.
- The image cache limits decode work per frame, and visible artwork paths are resolved in
  small slices. In practice the current 32 ms decode allowance can still consume almost an
  entire 30 fps frame: it is a time budget checked between synchronous decodes, not a hard
  limit on one decode.
- The scraper already writes a reduced `-sm.jpg` variant (up to 300 px) beside the full-size
  image, and small presentations prefer it. There is no medium tier, dwell-based quality
  progression, or cross-fade yet.
- Database metadata makes the Systems grid available immediately when its database or the
  Console Mode index is present.

Measurements above are idle measurements on one DE10-Nano setup, not proof that every screen
stays within budget while scrolling. Navigation latency and artwork decode spikes still need
to be measured on the device.

## 0.4.0 outcomes

1. **Tab switching feels smooth.** The current tab transition is visibly uneven when the GUI
   falls below 30 fps. Measure the transition and keep its animation within the 33 ms frame
   budget during normal use.
2. **Navigation feels immediate.** Focus changes and screen entry should not appear to hang
   while artwork, a database or a directory scan is loading. First gather the user's
   experience with the matrix below, with box art both on and off.
3. **Artwork remains optional and staged.** The new Settings switch hides game covers and
   background artwork (system icons remain). Later in 0.4.0, artwork should appear quickly and
   improve only for a selection that stays put: thumbnail, medium and full in aligned bounds,
   with a cross-fade if it stays within the frame budget.
4. **Idle rendering stays cheap.** Extend damage-based redraw where profiling shows it helps,
   and avoid redrawing unchanged chrome. Preserve the full-redraw switch as a diagnostic.

## Responsiveness check matrix

Use these situations during ordinary play. The “what to notice” column is a prompt, not a
predetermined diagnosis. Fill in the last column with the observed feel and whether box art was
on or off. Timing can be measured later if the experience points to a specific area.

| # | Action | What to notice | Your observation |
| --- | --- | --- | --- |
| 1 | Switch between each pair of tabs | Tab animation smoothness; delay until the new screen responds | |
| 2 | Open Systems the first time after launch | Delay before tiles appear; whether the screen is immediately usable | |
| 3 | Open a system with a small library | Time from confirm to first usable game screen | |
| 4 | Open a large system, then repeat | Time to first entries; whether the second visit differs | |
| 5 | Scroll through already-seen games | Focus movement and animation while artwork is cached | |
| 6 | Scroll into games whose artwork has not been seen this session | Stalls, placeholders, and delay until cover art appears | |
| 7 | Use L2/R2 for a distant letter jump | Delay before focus and the destination list settle | |
| 8 | Open Home with recent games and favorites | Initial tile appearance and response while rows populate | |
| 9 | Open Arcade games, then Manufacturers/Categories | Delay to first row and response while navigating | |
| 10 | Choose Build game database, confirm the warning, then wait for progress | Already reported: several seconds of apparent stillness. Recheck the gap to first visible progress and whether controls respond | |
| 11 | Choose Reload library and confirm | Time until visible feedback and first progress; any pause before it | |
| 12 | Choose Fetch box art | Time until visible feedback and first progress; whether the screen remains alive | |
| 13 | Open Manage systems or Manage Arcade | Time until the list/table appears and can be navigated | |
| 14 | Toggle Show box art off, browse, then turn it on again | Difference in tab switching, scrolling and screen entry; any layout change | |

For each observation, note the tab/screen, whether the library or artwork is being accessed for
the first time, and whether the pause happens before feedback, before the first progress
update, or during ongoing work. The library-build example specifically needs the interval from
confirming the warning to the first progress display captured; the total scan duration alone
does not describe that initial dead time.

## How to get there

### 1. Use the interface and classify the pauses

First use the Settings switch to compare normal browsing with artwork hidden. Fill in the
matrix from real use. For slow operations, distinguish “no feedback yet” from “progress is
moving slowly”: the first points to when work starts or when the screen is repainted; the
second points to the work itself.

Then reproduce the reported cases on the DE10-Nano with `--stats` and `--full-redraw` where
useful. Record idle frames, tab transitions, D-pad movement, letter jumps, screen entry, and
the gap from confirming a long operation to its first progress update. Compare artwork on and
off. This separates rendering/decode cost from database or filesystem delay. Keep the results
and hardware conditions in `PERFORMANCE.md` before selecting further work.

### 2. Keep image work off the input/render path

Replace synchronous cache misses in `ImageCache::get()` with requests to a bounded worker
queue. The render path should return the best already-decoded tier (or a placeholder) without
waiting for storage or a decoder. Decode results return to the UI thread through a small
completion queue; only the UI thread changes cache entries and draws them. Bound queued work,
cancel or deprioritise requests for tiles no longer visible, and favour the focused tile and
nearby tiles. Keep storage reads paced so making the UI responsive does not create bursts on
MiSTer's USB power-limited drives.

The frame time budget remains useful for work that can be split into small steps, but it cannot
bound one synchronous 20–50 ms decode. Treat worker count, queue size and drive pacing as
measured limits. Preserve retries for transient startup read failures and make stale worker
results harmless after a screen or selection changes.

### 3. Add staged artwork

Extend the scraper's current small/full pair to three on-disk levels:

| Tier | Use | Starting target |
| --- | --- | --- |
| Thumbnail | Grid and fast browsing | existing `-sm` image, up to 300 px |
| Medium | Focused tile/detail view | generate a new variant, around 480 px |
| Full | Large artwork after the selection settles | current full-size image |

Keep existing libraries valid: if a tier is absent, fall back to the best available image.
Generate the medium variant from the one downloaded source during scraping, and provide a
local backfill from the full image for already-scraped artwork. Do not make ordinary browsing
trigger a rescrape or network access.

When focus enters a tile, request its thumbnail first. After a short dwell, request medium;
only request full if focus remains. Begin with a roughly three-second dwell before each
upgrade, then tune it on hardware. Each upgrade uses the exact same destination rectangle and
aspect-fit calculation as the tier below it. Fade the new image in over the old one; if
measurements show two simultaneous image passes cause a visible frame-time spike, shorten or
remove the fade before compromising input response. Moving focus cancels pending upgrades for
the previous tile.

### 4. Extend incremental drawing where it pays

Use the measurements from step 1 to migrate Games and Arcade screens to tracked repaint areas,
including focus animation, scroll changes, late image arrivals and overlays. A newly decoded
image must invalidate its tile even when focus and scroll are unchanged. Do not skip a repaint
based only on input state. Cache or update top and bottom chrome only when their visible values
change (the clock, network status, tab and hints), while keeping the full-redraw path available
for comparison and recovery.

The correctness condition is that every old footprint is restored before drawing the new one:
focus growth/shadow, removed artwork, scroll movement, and a tier fade must leave no stale
pixels. If an area cannot be bounded reliably, repaint the enclosing content region.

## Completion checks for 0.4.0

- On-device measurements cover idle and active navigation for each major menu, with a cold
  artwork cache as well as a warm one; results and hardware conditions are recorded in
  `PERFORMANCE.md`.
- A cold artwork miss never blocks input handling for the duration of storage access or image
  decoding; focus remains visible through a placeholder or available lower tier.
- Thumbnail, medium and full variants resolve with safe fallback when any file is missing.
- Tier upgrades stay aligned during the fade, and leaving a tile prevents its later upgrades
  from wasting work or appearing on the wrong selection.
- Incremental and full redraw produce the same visible result during focus movement, scrolling,
  asynchronous image arrival and navigation back to a previously visited screen.
- The first Systems visit remains immediate with a current game database, preserving the
  already completed roadmap item.

The performance target remains smooth 30 fps at native 1080p (33 ms/frame). The measured idle
Systems and Games results already meet it; 0.4.0 must establish and improve the active-navigation
case rather than claiming success from idle averages alone.
