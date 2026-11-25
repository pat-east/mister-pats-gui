# Hints

The legend in the bottom bar: which buttons do what right now. Use it as the only place where actions
are named. Dialogs never draw a footer of their own; a modal that is on top replaces the screen's
hints. Do not write hint strings in render code or call the label formatter yourself.

Layer: primitive. Status: **proposal.** Values marked (M0) wait for the owner's decision in vNext M0.

## Anatomy

A screen or modal declares hints as data; the framework orders, formats and fits them.

```
 logical (what the code declares, in any order):
   Hint{Y,"Change view"}  Hint{B,"Back"}  Hint{Dpad,"Move"}  Hint{A,"Choose"}  Hint{Start,"Search"}

 ordered by key rank (movement first, then change, then confirm, then the way out):
   (+) Move   Y Change view   A Choose   Start Search   B Back
```

```
+----------------------------------------------------------------------------------------------+
| (+) Move   Y Change view   A Choose   Start Search   B Back                       v1.4.2     |
+----------------------------------------------------------------------------------------------+
  ^ hint line, left at the margin                                                  ^ version, right
```

`(+)` stands for the D-pad icon. Key rank: Dpad, UpDown, LeftRight, Shoulders, Triggers, X, Y, A,
Start, B.

## Variants

The same declared hints, rendered in the user's selected face-button layout (confirm is always the
bottom face button; ASCII stand-ins: `[x]` cross, `[o]` circle, `[sq]` square, `[tri]` triangle).

```
logical     {DPAD} Move   Y Change view   A Choose   Start Search   B Back
Xbox        (+) Move   Y Change view   A Choose   Start Search   B Back
PlayStation (+) Move   [tri] Change view   [x] Choose   Start Search   [o] Back
Nintendo    (+) Move   X Change view   B Choose   Start Search   A Back
```

Shoulder and trigger names follow the layout too (`ui::buttonName`, `src/ui-framework/kit/Buttons.h`): Xbox `LB/RB`, `LT/RT`;
PlayStation `L1/R1`, `L2/R2`; Nintendo `L/R`, `ZL/ZR`. Start is `Start` in every layout. The exact names:

| Button (by position) | Xbox | PlayStation | Nintendo |
| --- | --- | --- | --- |
| A, bottom (confirm) | `A` | cross (U+2715) | `B` |
| B, right (back) | `B` | circle (U+25CB) | `A` |
| X, left | `X` | square (U+25A1) | `Y` |
| Y, top | `Y` | triangle (U+25B3) | `X` |

**Layout input (implemented).** The layout is an input of the theme, not of a component: `ui::Theme::setButtonLayout(ButtonLayout)`
(`Xbox`, `PlayStation`, `Nintendo`) and `buttonLayout()`. The application sets it from the user's choice (no
behavior changes: the bottom button confirms in every layout); `Hints` and the A-glyph of `ListItem` Nav and
Action rows read it when they draw, so a change shows on the next frame. A text that has to name a button uses
the placeholders `{A} {B} {X} {Y} {START} {SELECT} {LB} {RB} {LT} {RT}` (`{L1} {R1} {L2} {R2}` are the same shoulders
and triggers): `ui::Text` catalog sentences such as `Press {B} to go back.`; `drawLabel` and `drawTextBlock` (so every
modal body, empty-state text and page text) replace them with `ui::formatButtons(layout, text)`. Placeholders are
upper case and case-sensitive (a lower-case word is never a button: Classic's `formatButtonLabels` replaced a
lower-case "a" too), and the numbered `{1}` placeholders of `ui::format` are untouched. Prefer a hint to a sentence;
use a placeholder only where a sentence must name the button. The hint-size setting (Standard / Larger / Largest =
100 / 115 / 130 %) applies on top of the small size.

## States

```
normal          every hint fits at the chosen hint size
too long        step the text down one pixel at a time to the largest size that fits, never below Small;
                then drop non-essential hints from the right
modal on top    the modal's hints replace the screen's (below)
hold hint       "Hold X Favorite" with a thin bar under the line while the button is held
text entry      Start is listed with its action: "Start Search"
high contrast   bold hints, Primary tier, in a frame (2 px focus outline, surfaceHi, radius 6, padding 10 x 4,
                reaching 10 px past the left margin); the hold hint has its bar instead of the frame
```

Too long, full to fitted:

```
full     (+) Move   L2/R2 Jump   Hold X Favorite   Y Change view   A Play   Start Search   B Back
step     same line at the small size
fitted   (+) Move   Y Change view   A Play   B Back       <- non-essential hints dropped
```

Essential hints are A, B and Start where it is listed; they are never dropped. The version text at the
right reserves its width before the hint line is fitted.

Modal replaces the screen's hints:

```
screen   (+) Move   Y Change view   A Choose   B Back          shown while no modal is open
modal    A Close                                               the modal's list replaces it
```

Hold hint:

```
(+) Move   Hold X Favorite   A Choose   B Back
           ======.........         <- thin bar (6, accent) under the hint, grows while X is held
```

## Rules

- **Start rule:** list Start only where A is overloaded (text entry, a checklist with a review step)
  and name its action ("Start Search", "Start Review"). Never on pure confirm pages.
- **D-pad naming:** the `{DPAD}` icon for four-way movement; literal `^/v` or `<-/->` arrows for a
  single axis (ASCII stand-ins; the UI draws arrows). The words "D-pad" and a lone arrow are retired.
- **Dialogs draw no footer.** Status text ("Installing updates...") belongs to the body; hints carry
  keys only.
- **Hold:** written "Hold X <Action>" (no "2s"); uses the same hint size.
- **Back and Close:** both A and B closing a read-only dialog is listed once as `A/B Close`.
- Hint strings are logical (`A Choose   B Back`) in the source and formatted at render time; only
  `Hints` calls the formatter for the legend.

## Vocabulary (closed set)

One wording per action; new words are added here first.

| Group | Words | Use |
| --- | --- | --- |
| Way out | **Back** | leave a screen or a step |
| | **Leave** | leave the tab (B on the first level, where there is nothing to go back to) |
| | **Close** | dismiss a read-only dialog |
| | **Cancel** | abandon a choice or stop running work |
| | **Skip** | an optional step |
| | **Later** | postpone an offered action |
| Confirm | **Open** | go into something |
| | **Choose** | pick an option or run a row |
| | **Select** | toggle a checkbox |
| | **Turn on**, **Turn off** | flip a switch (A on a Switch row; the word names the result) |
| | **Change** | step a value to the next one (A on a cycling Value row) |
| | **Continue** | the next page of a flow (A on a Step page) |
| | **Start** | begin a job the flow described (A on its first page); not the Start key |
| | **Install** | put a downloaded package in place (A on its confirmation) |
| | **Restart** | restart the device (A on its confirmation) |
| | **Done** | finish entry or a flow |
| | **Save** | keep a tested or reviewed setup (A on a review page; its consequence is in the body) |
| | **Try again** | retry after a failure only |
| Movement | **Move**, **Scroll**, **Change <noun>**, **Jump** | |

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Bar height | 72 | shared bottom bar; hints vertically centered |
| Hint size | Small x hint setting (100 / 115 / 130 %) | steps down to Small |
| D-pad icon | hint icon size, gap 5, arm max(5, size / 3) | drawn by `Hints` only |
| Key separation | three spaces between hints | |
| High-contrast frame | pad 10 x 4, radius 6, 2 px `focus` | |
| Hold bar | thin ProgressBar (6) under the hold hint | |

## Behavior

- Reads `Hint` lists from the top modal, else the screen; `App` does not build strings.
- A hint exists only for an action the current state really offers; an unavailable action is left out,
  and the body says why.
- Re-fits only when the list, the layout, the hint size or the version text changes.

## Accessibility

- Hints follow the user's own layout, so the button named is the button on the controller.
- Small-size floor and Faint-or-better contrast (4.5 : 1); in high contrast bold with a frame.
- Fewer hints beat smaller hints: essential ones stay; non-essential ones drop before the text is
  squeezed below the floor.
- Hint order is the order the hands move; the same on every screen.
- Words from the closed vocabulary, one meaning each; the D-pad is an icon from the glossary and needs
  no label.

## API

```cpp
enum class HintKey { Dpad, UpDown, LeftRight, Shoulders, Triggers, X, Y, A, Start, B };
struct Hint { HintKey key; Text action; bool essential = false; bool hold = false; };   // hold: "Hold <key> <action>"
int drawHoldProgress(...);   // the running hold, see HoldProgress.md
using HintList = std::vector<Hint>;
std::string layoutHints(HintList, Theme&, int width);   // sorts by rank, fits, formats the layout
```

## Derived from / Used by

Derived from: `Screen::hints()` in each screen, `App::renderBottomBar` (Game options, hold hints,
update phases), `GameSearch::hints()` (and its `find("Y Change view")` test), in-dialog footers,
hint text in bodies.

Notes for the New GUI (strings that must change; application wording, not framework):

| Current | New |
| --- | --- |
| "Start game" (bar, Disc hints) | "Play game" (also fixes "(START)" rendering of "^ Start game") |
| "A / B  Dismiss" in the error panel | `A/B Close` in the bar |
| "A/B Back" (Disc settings) | `A/B Close` |
| "B Stop", "B Stop scanning", "B Cancel scan and go back", "B Cancel download" | `B Cancel` |
| "B Do this later", "B Skip for now", "B Not now" | `B Skip` or `B Later` |
| "A Reboot", "Reboot MiSTer" | Restart MiSTer |
| "A Set Linux time zone", "A Set this time zone" | `A Set time zone` |
| "A Install", "A Install package", "A Install update" | `A Install` |
| "Read more", "Read release notes", "Scroll" | `^/v Scroll` |
| "D-pad Choose key" | `{DPAD} Choose key` |
| "Show/Hide" without a key | name the key, for example `X Show password` |
| "Hold X 2s to favorite" and variants | `Hold X Favorite` |
| "Y Change game view" vs the search field's "Y Change view" test | one wording |
| hint order A before movement (Home, Games, Systems, Arcade, Disc, updater, Wi-Fi) | by key rank |

Building hints (implemented): `ui::hint(HintKey, "catalog.key", essential = false, hold = false)` in `src/ui-framework/kit/Components.h`
returns one `Hint`; nothing builds a `Hint` field by field. The application keeps the standard hint sets of its own
contexts (a game list, the search field) in `src/app/Hints.h`, so the same situation shows the same words everywhere. The
owner's wording decision for the application: A on a game is "Play" ("Start game" stays on the Game options and
Disc Start buttons).

Used by: the wizard (`Wizard::hints()`, `docs/components/Wizard.md`) and every dialog; Settings flows through `WizardFlowPage`.
