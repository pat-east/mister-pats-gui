# EmptyState

What a screen shows instead of its content when there is none: why it is empty, and what the user can
do next. Also the structure of the full-screen **Error** and **Success** pages. Use it whenever a
list, grid, table or panel has nothing to show. Do not use it for transient confirmations (see
`Toast`) or for running work (see `ProgressPanel`).

Layer: container. Status: **implemented in part** (`src/ui-framework/kit/EmptyPage.h`, 2026-11-23): the in-place page (title, why, next, a tone icon, a
Details line, one more small line) on the faint panel; the `EmptyKind` variants below are the planned shape. Values marked (M0) wait for the
owner's decision.

## Anatomy

```
 +----------------------------------------------------------------------------------+
 |                                                                                  |
 |                                                                                  |
 |                          Nothing here yet.                                       |  why: heading size,
 |                                                                                  |  Muted, wrapped
 |                  Add items in Settings to see them here.                         |  next action:
 |                                                                                  |  body, Muted, wrapped
 |                                                                                  |
 +----------------------------------------------------------------------------------+
    ^ centered in the content area (not the whole screen); max width about 60 characters
```

Two parts: **why** (heading size, one sentence) and **next action** (body size, one sentence naming
what to do and, if a button helps, which one). Never small, never dimmed below `Muted`, never
left-aligned in a corner.

## Variants

```
Empty         there is nothing yet                       "No items yet."
              next: what to do                          "Add items in Settings."
NoResults     a search or filter matches nothing         "No results for "abc"."
              next: how to change it                     "Press B to clear the search."
NothingToDo   everything is current (a good outcome)     "Everything is up to date."
              next: leave or check again                 "Press A to check again."
Error page    something failed (full screen)             see below
Success page  an action completed (full screen)          see below
```

The first three are in-place states of a container. **Error page** and **Success page** are
full-screen results; the same structure appears inside a `Modal` (variants `Error` and `Result`).

## States

**Error page** (AGENTS.md rules: plain words, whether anything changed, what to try, a small
Details line, a controller-operable next step):

```
 +----------------------------------------------------------------------------------+
 |                                                                                  |
 |                     (x)  Could not load the list                                 |  headline
 |                                                                                  |  heading, Primary
 |                     Nothing on your device was changed.                          |  what changed
 |                     Check the connection and try again.                          |  what to try
 |                                                                                  |
 |                     ~Details: the server did not answer~                         |  small, Faint
 |                                                                                  |
 +----------------------------------------------------------------------------------+
   bottom bar:   A Try again   B Close
```

**Success page** (the accent, never green; confirms what happened and offers a way on):

```
 |                     (v)  Settings saved                                          |
 |                     Your changes are active now.                                 |
   bottom bar:   A Done
```

The raw error string, a path or an `errno` text never appears in the headline.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Position | centered in the content area | vertical center about 45 % (slightly above the middle) |
| Why line | heading size, `Muted`, not bold | wrapped, max width about 60 characters |
| Next action | body size, `Muted` | wrapped; same width |
| Error / Success headline | heading, bold, `Primary`, icon 28 at the left | icon shape per tone |
| Details | small, `Faint` | one line, elided |
| Line step | line height + 9 (M0) | |
| Gap why to next | 16 | |

The caller supplies: why, next action (both plain sentences), tone and icon for Error / Success,
optional Details. The component decides layout, wrapping, tiers.

## Behavior

- No cursor and no focus target: the page is read-only; its actions are hints in the bottom bar
  (`A Try again   B Close`, `A Done`, `B Back`). It never draws buttons.
- The hint is stated once, in the bar; the body does not repeat it as a key name (it says what to do
  in words and the bar says which key).
- The container behind it keeps working: leaving a filter or search state shows the content again.
- A transient state (loading) is not an EmptyState; use `ProgressPanel`.

## Accessibility

- Always says why and what next (rules 3.2, 3.3); an empty screen is never blank.
- Body size or larger, `Muted` or brighter (contrast registry); no alpha-dimmed hint text (the Home
  rows' small, dim hint is replaced).
- Error and Success use icon + word + tone; Success is the accent, Error is red with a cross.
- Wraps and grows at Largest text; nothing clips; controller-only (no pointer actions).
- Language: short, plain sentences; one term per concept; technical detail only in Details.

## Largest text and high contrast

```
Largest: the sentences wrap to more lines, the block stays centered

            Nothing here yet.
      Add items in Settings to see
      them here.
```

High contrast: icon and headline use the highlight color for Success; Error keeps its distinct hue
plus the cross and the word.

## API

```cpp
enum class EmptyKind { Empty, NoResults, NothingToDo, Error, Success };

struct EmptyStateSpec {
    EmptyKind kind;
    std::string_view why;            // one sentence (headline for Error / Success)
    std::string_view next;           // one sentence: what to do
    std::string_view details;        // Error only, optional
};
void drawEmptyState(Painter &, Theme &, const Rect &contentArea, const EmptyStateSpec &);
```

No state. The owner supplies the hints (`A Try again   B Close`) through `Hints`.

### Implemented today

```cpp
ui::EmptyPageSpec page;                       // title, why, next (Text), tone + icon, details, extra (data)
page.title = Text::key("games.empty.none.title");  page.why = ...;  page.next = ...;
ui::drawEmptyPage(p, theme, area, page);      // the faint panel over `area`, the block centered inside it
ui::drawEmptyState(p, theme, area, title, why, next);   // the same block with no panel (inside another surface)
```

One frame for every list, grid, table and notice page; the block is at most 900 px wide, title wrapped, why Muted,
next Primary, then Details and the extra line small and Faint. A notice page (a state the user can act on) adds the
tone icon (`Tone::Warning` + `IconId::Alert`, `Tone::Error` + `IconId::Cross`, `IconId::Clock` for "checking").

## Derived from / Used by

Derived from: the seven empty-state looks (Games, Systems, Arcade, visibility, Home rows, Disc,
Controllers, updater "Nothing to show here"), the `ErrorModal` structure, and the AGENTS.md rules for
empty, error and success screens. Survey: inventory C4d.

Used by: the New GUI's Systems, Arcade, Home, Favorites, Games and Disc tabs (`src/app/EmptyPages.h` builds the shared search texts).
