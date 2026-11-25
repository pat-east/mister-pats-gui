# ResultItem

One line of an outcome or status list: an icon, a label, a status word and, if needed, one detail
line. Use it for what happened or how far something is (checks, installs, scans, verification). Do
not use it for choices the user makes (see [ListItem](ListItem.md)).

Layer: item. Status: **implemented in the wireframes.** Words and tones follow the status vocabulary; values marked (M0)
wait for the owner's decision.

## Anatomy

```
 +-----------------------------------------------------------------------------------+
 | (v)  Item name                                          Up to date        12.4 MB |
 |      ~second line, small, Muted (optional)~                                       |
 +-----------------------------------------------------------------------------------+
   ^    ^ label (body, Primary)                              ^ status word     ^ size / count
   icon 20, tone color                                        bold body, tone color     (optional)
```

- The icon and the status word are both always present; the tone only colors them.
- The label is elided; the status word is never cut.
- A **Details** secondary line (small, `Faint`) appears under an Error or Warning item only: the
  technical reason, one line, never the headline.

## Variants

By **tone**. One closed vocabulary; the word is always shown (never color alone).

```
(v)  Done      accent    Up to date, Installed, Ready, Saved, Connected, Verified
(..) Working   muted     Checking..., Downloading..., Installing..., Scanning..., Waiting...
(^)  Action    accent    Update available, Not installed, Changed, Restart MiSTer to finish
(!)  Warning   orange    Unverified, Incompatible, Missing, Low space
(x)  Error     red       Failed, Cannot be checked, Could not be read
(-)  Neutral   muted     Not set, Not checked, Offline, Not available, Skipped
```

`(..)` Working has no spinner and no motion: the word ends in "..." and the item may sit above a
`ProgressBar`. Success is the accent, never green. `Skipped` is Neutral, drawn in `Muted`.

By **extras** (all optional): size or count right-aligned, a second line, a **Retry** hint.

```
 | (x)  Item name                                          Failed                     |
 |      ~Details: the server answered with an error~                                  |
 +------------------------------------------------------------------------------------+
   hint bar:  A Try again   B Close            <- the retry action is a hint, not a button
```

## States

```
normal        icon + label + status word (+ size, + second line)
focused       only when the item is focusable (opens details): marker 4 + outline 2
newest        the latest line of a live list: label Primary, older lines Muted
disabled      not used (a ResultItem reports state; it is not an action)
```

```
 normal                                     focused (focusable result)
 | (!)  Item name         Unverified |      |#| (!)  Item name          Unverified |
                                            ########################################
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Frame | one line 46 (Dense), two lines 62 (M0) | grows with Largest text |
| Radius | `Radius.row` 8 (box 9 in the updater is dropped) | |
| Icon | 20, at left inset 18 | shape per tone; stroke at least 2 |
| Label | body, `Primary` (`Muted` when not newest in a live list) | elided |
| Status word | bold body, tone color, right inset 22 | fixed vocabulary |
| Second line / Details | small, `Faint` (`Muted` floor) | secondary only |
| Size / count | body, `Muted`, right of the status word | formatted by the shared formatter |

The caller supplies: tone, label, status word (from the vocabulary), optional size, detail, retry.
The component decides geometry, colors, icon, elision.

## Behavior

- A ResultItem is read-only by default: no marker, no outline, not part of the cursor path.
- If an item can open details it is focusable and behaves as a `ListItem` of kind `Nav`
  (Confirm opens); it then shows marker and outline.
- Retry is never a button in the item: the owning `Result` or `Modal` contributes `A Try again`
  to the bottom bar.
- Status words come from the closed vocabulary; a feature cannot invent a new word.

## Accessibility

- Every state has an icon, a word and a color; removing any one still leaves the state readable
  (rule 1.2). Done and Action share the accent, so the icon (check versus up arrow) and the word
  separate them.
- Warning (orange) and Error (red) differ in icon shape (`!` versus cross) and word.
- Text and icon meet 4.5 : 1 and 3 : 1 against the row surface in every theme and mode (contrast
  registry). Details uses `Faint`, never lower.
- Rows grow at Largest text; the label elides, the status word stays whole.
- Icons are in the self-explanatory glossary (check, cross, up arrow, exclamation mark); the word
  next to them is still always shown.

## Largest text and high contrast

```
Largest: second line moves down, frame grows, label elides before the status word

 | (v)  A long item name that is cut with an ell...            Up to date      12.4 MB |
```

High contrast: Done and Action use the highlight color, Warning and Error keep distinct hues but
are told apart by icon and word; the row gets the white border only if it is focusable.

## API

```cpp
enum class Tone { Neutral, Done, Action, Warning, Error };

struct ResultItemSpec {
    Tone tone;
    std::string_view label, status;          // status from the closed vocabulary
    std::string_view size, detail;           // optional
    bool newest = true;                      // live lists: older lines draw Muted
    bool focusable = false;
};
int  resultItemHeight(Theme &, const ResultItemSpec &);
void drawResultItem(Painter &, Theme &, const Rect &, const ResultItemSpec &, bool cursor);
```

No state: items are drawn from the caller's data.

## Derived from / Used by

Derived from: the core updater's status lists (checking live list, list rows, review lines),
Scan progress rows and "done" text, Disc drive status lines, Wi-Fi test results, update phase
rows (survey: inventory C10, C13; progress/status report section 4).

Used by: *none yet.*

## Implementation

`drawResultItem` in src/ui-framework/kit/Data.cpp: label, status word with icon and tone, optional count, Details line for Error and Warning. Shown in the Data tab.
