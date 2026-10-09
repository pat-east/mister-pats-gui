# vNext — 0.4.0 Optimisation

This records the 0.4.0 implementation. The release is about how the interface *feels*:
responsive navigation, smooth tab changes, quick artwork appearance and visible progress
for long operations. After testing on the MiSTer, the user considers the current menu
responsiveness good and has approved the version for commit.

## Implemented in the current development build

- **Artwork can be hidden.** Settings → Show box art disables cover and background requests
  across browsing screens. The user reported that navigation then felt very snappy.
- **System icons load during the splash.** The first second moves the bar from 0 to 25%; the
  remaining 75% advances as icons are loaded sequentially on the UI thread. The splash stays
  until each icon file has been processed. Native 163×163 opaque BMPs are preferred; older
  PNGs still work.
- **Artwork is prepared for each view.** Settings → Prepare box art makes Home, Arcade, Grid,
  Boxart small and List-detail BMP variants. JPEG/PNG covers remain fallbacks when a variant
  is missing. A completed run over 11,502 entries on the MiSTer felt better to the user so far.
- **Cover decoding uses one worker thread.** A queue of at most 32 requests prioritizes the
  focused tile; the cache is bounded to 120 entries and 32 MiB. The render thread owns cache
  entries and draws completed images. If worker startup fails, synchronous loading remains
  available. Repeated stress scenarios remain useful for later regression checks.
- **The scrape ETA uses whole-run progress.** Elapsed time since start is multiplied by
  `(total games - processed games) / processed games`, then shown as an approximate clock
  time in the MiSTer's time zone. The update was deployed after the completed run, so its
  accuracy on another full run has not yet been measured.
- **Fatal errors stop relaunching.** A crash screen asks for a manual reboot; an available
  crash record goes to `logs/crash.log`. Debug symbols are kept alongside local builds.
- **The renderer supports incremental redraw.** Earlier DE10-Nano idle measurements were
  7.8 ms/frame for Systems and 13.3 ms/frame for Games versus 38.1 and 39.6 ms with full
  redraw. These are historical idle measurements, not active-navigation results for 0.4.0.

## User review and follow-up observations

The user has marked **smoother, faster menu control everywhere** complete for 0.4.0. The
following scenarios remain useful when diagnosing future reports: cold artwork, repeated
A-to-Z letter jumps, rapid tab changes, long operation feedback and incomplete BMP files.
No new on-device frame-time measurement or exact 30 fps claim is implied by that approval.

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
| 12 | Choose Prepare box art | Time until visible feedback and first progress; whether the screen remains alive | |
| 13 | Open Manage systems or Manage Arcade | Time until the list/table appears and can be navigated | |
| 14 | Toggle Show box art off, browse, then turn it on again | Difference in tab switching, scrolling and screen entry; any layout change | |

### Observed on the MiSTer — 2026-10-08

- The tab underline animation is smooth on **Home**.
- It is also smooth on **Systems once its graphics have loaded**.
- A slowdown is noticeable on the remaining tabs: **Favorites, Arcade, Games and Settings**.

This is an observation, not yet a diagnosis. Recheck those tabs with box art hidden and shown,
and with their graphics cold and already loaded, to separate screen-render cost from first-load
cost.

### Observed on the MiSTer — 2026-10-09

- A Prepare box art run over 11,502 entries completed. The user reports that browsing feels
  better; after further use, the current version looks and feels very good overall. The user
  approved the 0.4.0 commit and push.
- The ETA shown during that run changed but did not give a useful prediction. The later
  whole-run formula was manually deployed only after the run had completed; its accuracy
  remains unverified.

For each observation, note the tab/screen, whether the library or artwork is being accessed for
the first time, and whether the pause happens before feedback, before the first progress
update, or during ongoing work. The library-build example specifically needs the interval from
confirming the warning to the first progress display captured; the total scan duration alone
does not describe that initial dead time.

## Implementation decisions

Earlier attempts to put the tab underline animation or splash icon loading in their own
threads caused GUI exits on the MiSTer. Both attempts were removed. The underline and the
framebuffer remain on the app thread; the splash loads icons sequentially. The current
image worker is a separate design for cover decoding only. It was introduced after crash
handling and the static pthread-link fix; repeated stress cases remain valuable regression
checks.

The worker is created lazily on the first cover request. The app thread queues paths and
adopts completed images at the start of a frame. The worker never draws or mutates the cache.
Queued requests from a previous frame are discarded after a tab switch or letter jump;
currently decoding work is allowed to finish safely. A queue limit and RAM limit prevent
10,000 entries from becoming 10,000 pending images. Failed reads are retried later. If the
worker cannot start, the older synchronous path is used.

The JPEG/PNG fallback keeps older libraries usable while BMP variants are generated. This
is important after an interrupted Prepare box art run. A file that exists but contains
invalid BMP data is a separate case to check during testing; file existence alone does not
prove it is usable.

### Further device checks

- Repeat fast tab switching, A-to-Z letter jumps, rapid directional input and immediate
  returns between Home, Favorites, Systems, Arcade, Games and Settings on a 10,000+ entry
  library. Watch for stale art, flicker, memory growth, long pauses and crashes.
- Repeat with Show box art off and on, with cold and warm image caches. Record which tabs
  fall below 30 fps and whether the underline remains smooth.
- Check a library with missing BMP variants, only legacy PNG/JPEG covers, and covers that
  arrive after a canceled preparation run. Check the incomplete/corrupt-file case separately.
- Check first visible feedback for Build game database, Reload library and Prepare box art.
  Record the time from confirmation to first progress, not just total duration.
- Check the new whole-run ETA on a subsequent long preparation run. The 11,502-entry run
  completed before this ETA version was deployed; no accuracy result is available yet.
- Record on-device active frame times, RAM use and storage behavior in PERFORMANCE.md.
  Preserve the ability to compare with `--full-redraw`.

The target is smooth 30 fps at native 1080p (33 ms/frame). Historical idle measurements do
not establish this for animation, scrolling, asynchronous image arrival or a cold cache.
The earlier first-visit Systems improvement remains a completed database change rather
than new 0.4.0 work.
