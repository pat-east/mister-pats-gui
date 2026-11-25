# ProgressPanel

Calm progress for work that takes a while: a title, one bar and one short line saying what is
happening. Use it for scans, installs and other multi-second jobs, standalone on a page or as the
body of a `Modal`. Do not use it for quick actions (use a `Toast` or inline status) or for failures
(use `Result` or an `Error` modal).

Layer: container. Status: **implemented in the wireframes.** Values marked (M0) wait for the owner's decision in vNext M0.

## Anatomy

Default: title, bar, one status line. Nothing else.

```
                    +--------------------------------------------------+
                    |                                                  |
                    |  Looking for items                               |  title: heading, bold,
                    |                                  <-20->          |  elided, never shrunk
                    |  (==============================..............)  |  bar: 12 (ProgressBar)
                    |                                  <-24->          |
                    |  Reading the folders                             |  status: body, textMuted,
                    |                                                  |  one closed phrase
                    +--------------------------------------------------+
                    <-- width 700 standalone, centered; modal body width in a Modal -->
```

## Variants

Opt-in extras, each justified by the owner of the job. All of them are off by default.

```
default       title + bar + status line

with detail   title + bar + status line
              12 items found                      <- detail: small, Faint, changes at most once a
                                                     second; a number or short noun phrase, no names

cancellable   same panel; the bottom bar shows  B Cancel   (never a button in the panel)
```

- **No ETA, no spinner, no counters per file, no file or item names.** The bar shows how far it is.
- The **status line** comes from a closed set of phrases, one per phase. The application defines its
  phase enum and its phrases; the component never formats strings per frame.
- Indeterminate phases show a static minimum-fill bar and the phrase (see `ProgressBar`).

## States

```
running           title, bar at the fraction, status phrase of the current phase
phase changes     the phrase changes (no fade); the bar continues, it never goes back
long-running      same panel; after 20 s without a phrase change one fixed detail line may appear:
                  "This can take a few minutes."
failed            the panel is replaced (see below)
done              the panel closes, or becomes a Result with the outcome
```

Failed work leaves the panel. Nothing is drawn inside the panel for an error:

```
  +--------------------------------------------------+       bottom bar:  A Try again   B Close
  |  Could not finish                                |
  |                                                  |       Modal(Error): plain words, what was
  |  The list was not saved. Nothing was changed.    |       or was not changed, a small Details
  |  Try again, or check that the card is writable.  |       line, one controller-operable next
  |  ~Details: write failed~                         |       step; hints live in the bar.
  +--------------------------------------------------+
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Width | 700, centered (standalone); modal body width in a `Modal` | (M0) |
| Title | heading, bold, `textPrimary`, elided | no shrink-to-fit |
| Gap title to bar | 20 | (M0) |
| Bar | 12, `ProgressBar` default | |
| Gap bar to status | 24 | (M0) |
| Status line | body, `textMuted`, one line, elided | |
| Detail line | small, Faint tier, 8 below the status | opt-in, 1 Hz |
| Height | content-driven | grows with Largest text, never fixed |

The caller supplies: title, fraction (or unknown), phrase of the phase, optional detail, whether it is
cancellable. The component decides: layout, fonts, tiers, elision, repaint rate.

## Behavior

- No input. Cancellation is a hint (`B Cancel`) handled by the owner; a non-cancellable job shows no
  such hint and swallows Back silently-with-no-hint only for an unstoppable commit step (the owner
  says so in the phrase, for example "Installing updates").
- The status line changes at most about once a second; the detail line is throttled to 1 Hz.
- Repaints only the bar, the status line and the detail line when they change; nothing animates
  between changes.
- It reserves its full height from the first frame, so the layout does not jump when a detail line
  appears.

## Accessibility

- Status and detail text meet 4.5 : 1 (`textMuted` and Faint on the panel surface, in every theme and
  mode); the bar meets 3 : 1.
- Nothing moves or flashes (no spinner, no sweep by default), nothing is announced by color alone.
- Text sizes scale; at Largest the title and lines elide and the panel grows.
- A long job tells the user it is normal and still working, in words, once.
- Calm default (premise): one bar, one phrase. Detail and counters are the exception.

## Largest text and high contrast

```
Largest (130 %):
  Looking for ite...                         title elided at the panel width
  (==================...................)
  Reading the fold...                        status elided

High contrast: black panel surface, bar fill in the highlight color, track outlined in white.
```

## API

```cpp
struct ProgressPanel {
    std::string_view title;
    float fraction = -1.0f;            // negative: unknown
    std::string_view phrase;           // from the application's closed phase set
    std::string_view detail;           // optional, throttled to 1 Hz by the owner's timer
    bool wide = false;                 // false: 700 standalone; true: use the container width
};
int drawProgressPanel(Painter&, Theme&, const Rect& area, const ProgressPanel&);  // returns height
```

## Derived from / Used by

Derived from: `LoadingIndicator` (title, bar, status, counts, extra line, spinner), the Scan
jump-start working page, the core-updater Checking and Installing pages (inventory C4e).

Notes for the New GUI: today these pages show counters ("System 3 of 20 ... 1234 games found"),
per-item names, a wall-clock ETA and a spinner; those go. Phrase sets proposed per job, written in the
application: library scan (Looking for your games / Reading your game folders / Saving your game
list), Arcade (Looking for Arcade games / Checking Arcade games), box art (Getting the box art list /
Getting box art), updater (Checking official files / Installing updates).

Used by: the wizard's `BusyPage` (a Busy page), the Data tab.

## Implementation

`drawProgressPanel` in src/ui-framework/kit/Data.cpp: title, bar, status line, optional detail. The cancel hint belongs to the caller's help. Shown in the Data tab.
`ProgressPanelSpec::showTitle = false` omits the title line: a dialog or wizard page already names itself in its title row (`BusyPage` in
src/ui-framework/kit/WizardParts.h draws it that way, in the text column, with the "This can take a few minutes." line after 20 s).
The panel has exactly one detail line; counters per file, bytes, an ETA and item names stay out (see Variants), so a Result page, not the
Busy page, carries the final numbers.

## Busy page in a wizard (rule changed on purpose: one phase line, one "what now" line, elapsed time, one wait note)

The earlier rule was "ProgressPanel: one detail line". A scan of 10 000 games runs for minutes with long stretches where nothing visible
changes; one fixed phrase and a counter made that look frozen. The owner accepted a deliberate extension for `BusyPage` (the plain
`ProgressPanel` is unchanged: title, bar, status, one detail line). Top to bottom, every line with its place reserved from the first frame:

```
              Reading your game folders                     phase phrase: Heading, bold, one per phase (two-line slot, bottom aligned)
   (=========================...........................)   bar: ONE for the whole job when the service can say how far it is
          System 12 of 46: Super Nintendo                    what now: Body, Primary, one line, elided at the end; changes at most once a second
         Running for 1:25  .  46 systems, 10485 games        elapsed time: Small, Muted, once a second; optional facts after it
   Big collections take a few minutes. Nothing is stuck.     wait note: Small, Muted, two lines reserved; appears after 10 s without a change
```

- **Phase** (`status`): a closed phrase per phase; it never flips between phrases of one phase (the cover job keeps one phrase for the
  whole job; the changing part is the "what now" line).
- **What now** (`detail`): text the service already provides is fine ("Super Nintendo", "Saving game lists: 3 of 47"); set it with
  `setDetail`, which shows a new value once a second at most (the first value and an emptied line at once). Longer than one line it wraps
  to a second line (translations, Largest text) instead of being cut.
- **Elapsed time** (`showElapsed`, default on): "Running for 0:05", "1:25", "61:00" (minutes are not wrapped at 60). Pages whose own
  detail already counts time (the Bluetooth countdown) or that end within a frame switch it off.
- **Wait note** (`waitNote`, `waitNoteAfter`, default 10 s): shown by the page itself once status, detail, facts and the bar did not change
  for that long; any change hides it and restarts the quiet period; the elapsed clock does not count as a change. Plain words, in the
  flow's catalogue (translatable), naming what takes the time and that nothing is stuck. Empty `waitNote`: the generic line
  `wizard.longWait`.
- Still out: per-file names or counters that change faster than once a second, an ETA unless a service gives a real one, a spinner,
  invented progress fractions. When the page joins several service phases into one bar (the scan), the weights are written next to the code and the
  bar only moves forward.
- Accessibility: text only, never colour alone; Small Muted text meets 4.5 : 1; at text 130 % and in the pseudo-language the lines wrap or
  elide at the end, nothing overlaps; nothing animates, the page changes at most once a second.
