# Result

A container for [ResultItem](ResultItem.md) lines with an optional summary bar and a next-step
area. Use it to show an outcome or a running check: what was checked, installed, scanned or
found. Do not use it for choosing among options (see `List`).

Layer: container. Status: **proposal.** Values marked (M0) wait for the owner's decision.

Inside a wizard the outcome page is `WizardResult` / `ResultPage` (status icon and word, headline, what changed, optional items drawn as these
`ResultItem` rows, next step, small Details); see `Wizard.md`, section Result layout.

## Anatomy

```
 +-----------------------------------------------------------------------------------+
 | (v)  Item one                                              Installed       12.4 MB |  ^
 +-----------------------------------------------------------------------------------+  |
 | (v)  Item two                                              Installed        3.1 MB |  | items
 +-----------------------------------------------------------------------------------+  | (scrolls)
 | (x)  Item three                                            Failed                  |  |
 |      ~Details: the file could not be verified~                                     |  v
 +-----------------------------------------------------------------------------------+
 +-----------------------------------------------------------------------------------+  ^
 |  2 installed - 1 failed                                                           |  | summary
 |  ~Nothing else on the device was changed.~                                        |  | 88
 +-----------------------------------------------------------------------------------+  v
   next step: only in the bottom bar:   A Try again   B Close
```

- The items scroll as a `List` (position text and scroll bar when longer than the area).
- The summary bar and the next-step text are optional; the next action is always a hint.

## Variants

```
Outcome    finished work: items, summary ("2 installed - 1 failed"), hints "A Done" or
           "A Try again   B Close"
Live list  running work: newest line at the bottom emphasized, older lines Muted, no focus,
           no scroll bar until the box is full; sits in a surface box
Review     what will happen before the user confirms: one-line items in a surface box with
           a scroll bar and "a-b of n"; the summary bar says how much will change
```

Live list (a calm default: one bar above, a few lines, no counters on the lines themselves):

```
 Checking official files...
 =============================.........
 +------------------------------------------------------------------------------+
 |  ~Item one                                                       Up to date~  |
 |  ~Item two                                                       Up to date~  |
 |   Item three                                                     Checking...   |   <- newest, Primary
 +------------------------------------------------------------------------------+
```

## States

```
running        Live list; the owner shows a ProgressBar and one status line above it
done           Outcome with Done items and a summary in the accent
partly failed  Outcome with Error items; summary says what did and did not change
failed         EmptyState error variant (nothing to list) or Outcome with one Error item
empty          EmptyState: "Nothing to show here." plus why and what to do next
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Item frame | one line 46, two lines 62 (M0) | `ResultItem` |
| Live list box | `surface`, radius 8, inner padding 8 vertical, 14 horizontal | radius 9 is dropped |
| Review box | `surface`, radius 8, padding 6 | one-line rows 46, scroll bar inside |
| Summary bar | 88 high, radius 10, `surface`, inner padding 24 | title bold body, detail body `Muted` |
| Gap items to summary | 10 | |
| Scroll bar | as `List`: 4 px, `focus` thumb, 8 from edge | only when longer than the area |

The caller supplies: items, optional summary (headline, detail), mode. The container decides
layout, scrolling, emphasis of the newest line, empty and error states.

## Behavior

- Read-only by default: no cursor. Up/Down scroll the items when the box is longer than the area.
- Focusable only when the items open details: then the Result behaves as a `List` of `Nav`
  `ListItem`s drawn with the result look.
- The live list appends lines at the bottom and keeps the newest in view; it never jumps back.
- Text changes at most about once a second (calm); lines are elided, never wrapped.
- Hints contributed: `UP/DOWN Scroll` when longer than the area; the next step (`A Done`,
  `A Try again   B Close`, `B Cancel` while running if cancellable).
- Position text: `a-b of n` (read-only scroll).

## Accessibility

- State is icon + word + tone for every line; the summary also names what changed (rule 3.3).
- Success uses the accent in the summary (not muted text); failures say what was and was not
  changed and give a controller-operable next step.
- The newest line is distinguished by tone tier (Primary versus Muted), not by color alone: its
  status word is "Checking..." or a result word.
- Nothing important is time-limited: the summary stays until the user acts.
- Boxes keep their contrast: `Muted` text on `surface` meets 4.5 : 1 (contrast registry).
- Rows grow at Largest text; labels elide before status words.

## Largest text and high contrast

```
Largest: fewer lines fit, the box scrolls; the summary grows with its text

 | (v)  A long item name that is cut with an ell...             Installed |
 +-------------------------------------------------------------------------+
 |  2 installed - 1 failed                                                  |
 |  ~Nothing else on the device was changed.~                               |
```

High contrast: Done and Action take the highlight color; the surface boxes get the white 2 px
border (Roomy rule, M0); the scroll thumb follows `focus`.

## API

```cpp
struct ResultSummary { std::string_view headline, detail; Tone tone = Tone::Neutral; };
enum class ResultMode { Outcome, Live, Review };

struct ResultConfig {
    ResultMode mode; Rect area;
    const ResultItemSpec *items; int count;
    const ResultSummary *summary = nullptr;      // optional
};
void drawResult(Painter &, Theme &, const ResultConfig &, const ListView &);
```

State is a `ListView` (scroll only), owned by the screen or the `Modal`.

## Derived from / Used by

Derived from: the core updater's live checking list, review box and result page; Scan progress
and overview; Disc drive status table; selection summary bar. Survey: inventory C10, C10b.

Used by: *none yet.*
