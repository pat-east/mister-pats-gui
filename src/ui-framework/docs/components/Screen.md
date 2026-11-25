# Screen

The page frame every view is built on: top bar, content area, bottom bar, safe area, the three
non-happy states (loading, empty, error), the focus owner and the screen stack. Use it for every
tab view and every full-screen takeover view. Dialogs are `Modal`, not Screen.

Layer: container. Status: **proposal.** Values marked (M0) wait for the owner's decision.

The frame's six regions (tabs, nav items left and right, help, footer, content window) are defined by
[Shell](Shell.md); this page covers the content window's screen: states, focus owner, the screen
stack.

## Anatomy

The whole 1920 x 1080 design canvas.

```
+--------------------------------------------------------------------------------------------+
| 10:42  Tue 22 Nov            Home   Games   Consoles   Arcade   Disc   Settings     Wi-Fi   |  top bar
|                                                                                            |  96
+--------------------------------------------------------------------------------------------+
| 72 |                                                                                  | 72 |  ^ margin 32
|    |   +----------------------------------------------------------------------+       |    |
|    |   | PageHeader (58)                                                      |       |    |
|    |   +----------------------------------------------------------------------+       |    |
|    |   |                                                                      |       |    |  content
|    |   |                      content area: 1776 wide                         |       |    |  area
|    |   |                      (the screen draws here)                         |       |    |
|    |   |                                                                      |       |    |
|    |   +----------------------------------------------------------------------+       |    |
|    |                                                                                  |    |  v margin 32
+--------------------------------------------------------------------------------------------+
| {DPAD} Move   A Open   B Back                          ~toast pill~          v1.4.2       |  bottom bar
+--------------------------------------------------------------------------------------------+  72
```

Per frame, in order: background, top bar, bottom bar, `screen.render(content, fullRedraw)`, then modals
on top. The content rect is `x 72, y 96 + 32, w 1776, h = bottom.y - top.bottom - 2 x 32`. Important
content stays inside it (the safe area).

## Variants

```
Tab screen            View (stays tabbed)          Takeover (own frame, no tabs)
top bar: tabs live    top bar: tabs live           top bar: tabs locked
e.g. Home, Games      e.g. an item detail          e.g. a first-run flow, controller setup
dropped never         dropped when a tab changes   pushed and popped explicitly
```

- **Tab screen:** the base of the stack; chosen by the tab row (LB/RB).
- **View:** pushed over a tab screen; the tabs still work and dropping the tab drops the view.
- **Takeover:** pushed over everything; blocks tab navigation until popped.

## States

The frame draws the three non-happy states; the screen only supplies the texts.

```
content                      loading                        empty
 (the screen draws)           +----------------------+       +----------------------+
                              |  ProgressPanel       |       |  EmptyState          |
                              |  title, bar, 1 line  |       |  why + next action   |
                              +----------------------+       +----------------------+

error (inline, full screen)                   with a modal on top
 +--------------------------------------+      the scrim covers the whole canvas incl. the top bar;
 |  What went wrong in plain words.     |      the bottom bar is redrawn above the scrim and shows
 |  Nothing was changed.                |      the TOP modal's hints
 |  ~Details: short technical line~     |
 +--------------------------------------+
   A Try again   B Back
```

- A failure the user must notice is a `Modal` (Error). An inline error page is for a screen that cannot
  show its content at all.
- Empty states are body-size, wrapped, never small, and name the next action.

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Top bar | 96 | clock, tabs, network; non-interactive |
| Bottom bar | 72 | hints, version, toast slot |
| Side margin | 72 | `Space.marginX` |
| Vertical margin | 32 | `Space.marginY`, between bars and content |
| Content width | 1776 | at 1080p; scaled by `px()` on other framebuffers |
| Page header | 58 | see `PageHeader` |
| Scrim | one dialog level (M0: alpha 210), covers the full canvas | |

## Behavior

- **Focus owner:** the top of the stack owns the cursor; a `Modal` on the `ModalStack` takes input first.
  A container inside a screen owns its own cursor and scroll (two integers); a screen never keeps per-row
  state.
- **Stack:** `ScreenStack` holds the base tab screen and any Views and Takeovers.

```
push(Takeover)  -> onEnter()    pop() -> onExit(), generation++
selectTab()     -> dropViews()  blocksTabs() = any Takeover on the stack
```

- **Input claims:** `claims() -> {back, start, raw}` tells the app whether the screen wants Back for an
  inner overlay, Start as a field action, or raw device input (controller mapping). It replaces five
  separate predicates.
- **Full repaint:** the app compares `(top, viewId, generation, modals.revision)` with the last frame
  and sets `fullRedraw` when it changed. A screen that returns `incremental() == true` may skip its own
  repaint when nothing it tracks changed, but still repaints when `fullRedraw` is true; modals force the
  non-incremental path until the base snapshot exists.
- **Quit** is handled once at the top: modals clean up, then the app stops.
- **Interruptions:** a screen can set `blocksInterruptions` (raw capture, a running scan) so a service
  prompt waits until it ends.
- **Hints** come from the top of the stack (modal, else screen); the top bar never shows hints.

## Accessibility

- The hint bar is always visible and always lists what the current state offers; it is never covered.
- Exactly one focus target is visible at any time; Back always leaves the current context.
- Opening a modal blocks tab switching (LB/RB), so the user cannot change the screen behind a dialog.
- Content stays inside the safe area; no essential element sits in a margin.
- Bars have fixed heights; text in them elides or steps down (hints) and never overlaps.

## Largest text and high contrast

```
Largest: bars keep their height, text sizes scale; the hint line steps down to the Small size before
dropping hints; the content area is unchanged

| {DPAD} Move   A Open   B Back   X Favorite       v1.4.2 |   hint size steps 130 -> 100, never below small
```

High contrast: black background, top bar text and the active tab underline in the highlight color; the
hint line is drawn in a bordered frame.

## API

```cpp
enum class ScreenKind { Takeover, View };
struct InputClaims { bool back = false, start = false, raw = false; };

class Screen {
public:
    virtual ~Screen() = default;
    virtual void update(float dt) = 0;
    virtual void render(Painter&, const Rect& content, bool fullRedraw) = 0;
    virtual void handle(Action) = 0;
    virtual void hints(Hints&) const = 0;
    virtual InputClaims claims() const { return {}; }
    virtual int  viewId() const { return 0; }
    virtual bool incremental() const { return false; }
    virtual bool blocksInterruptions() const { return false; }
    virtual void onEnter() {}
    virtual void onExit() {}
};
class ScreenStack {
public:
    void setBase(Screen*);
    void push(Screen&, ScreenKind = ScreenKind::Takeover);
    void pop();
    void dropViews();
    Screen& top() const;
    bool blocksTabs() const;
    unsigned generation() const;
};
```

## Derived from / Used by

Derived from: the app frame in `App.cpp`, `Screen.h`, the seven takeover flags with their
`open*/close*` pairs, `activeScreen()` and `tabNavigationAvailable()`, `hasBackOverlay` /
`usesStartAction` / `wantsBack` / `wantsRawInput` (inventory C1, C2; modal report section 3).

Used by: *none yet.*
