# ModalStack

The single owner of "what is on top": it draws the open modals in order, gives input to the top one
only, and supplies the hint bar. Use it for every overlay. Do not keep a flag per dialog in a screen,
and do not route Back, Confirm or Quit by hand.

Layer: overlay. Status: **proposal.** Values marked (M0) wait for the owner's decision.

## Anatomy

What is drawn, from back to front, and who gets input.

```
 front   +--------------------------------------+
   ^     | hint bar (redrawn above the scrim)   |  shows the TOP modal's hints
   |     +--------------------------------------+
   |     | Alert / Viewer      (layer 2)        |  scrim alpha 120 under it, only if layer 1 open
   |     +--------------------------------------+
   |     | Dialog              (layer 1)        |  scrim alpha 210 (viewer 230)
   |     +--------------------------------------+
   |     | top bar + content screen             |  dimmed, no input, no hold gestures
 back    +--------------------------------------+
```

Stack contents and routing:

```
stack (capacity 4, non-owning pointers)      input
 [3]  Alert          <- top                  Action ---> top modal only
 [2]  Dialog (Large)                         update(dt) ---> every modal (pages keep advancing)
 [1]  ...                                    Quit ------> onQuit() of each, top first, then stop
 [0]  (empty = screen has input)
```

## Variants

The stack is generic; only two nesting cases are allowed in practice.

```
Alert over anything          +-- Error (x) ----------+      raised by a screen or a service
                             over a Large dialog            queue: up to 3, duplicates dropped
Viewer over a Large dialog   +-- image 2 / 4 --------+      closes back to the dialog beneath
Keyboard inside Input        part of the Input body, NOT a stack entry
```

- **Service-driven prompts** (an update is ready, a restart is needed): the application pushes the
  modal when the service's phase changes and pops it when the phase leaves. The modal holds no state
  except its scroll position. Services never know the stack.
- **Deferred prompts:** while the current screen declares `blocksInterruptions` (raw input capture,
  a scan that is working) a service-driven prompt waits; Alerts do not wait.

## States

```
empty             screen has input; hold gestures (favorite, remove) allowed; tabs allowed
one modal         screen dimmed, no input, tabs blocked, hold gestures cancelled
modal over modal  lower modal dimmed again (alpha 120); only the top acts
alert queued      next error shown after the current one is closed
closing           revision counter increments; the next frame repaints the whole canvas
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Capacity | 4 | enough for dialog + viewer + alert |
| Error queue | 3 | duplicates (same title and text) dropped |
| Scrim | 210 dialog / 120 over a modal / 230 viewer | tokens (M0) |
| Redraw, phase 1 | full canvas every frame while any modal is open | same cost as today |
| Redraw, phase 2 | one snapshot of screen + scrim, then only the panel rect per frame | one 8 MB copy, same size as the existing background (M0) |

## Behavior

- **Input:** `handle(Action)` goes to the top modal only. Tab switching (LB/RB) and screen-level hold
  gestures are blocked while the stack is not empty; the top bar is dimmed with the screen.
- **Hints:** `hints(Hints&)` returns the top modal's list; the screen's hints are not shown beneath
  a modal. A modal with no hints of its own is a bug (the bar would show nothing true).
- **Back and Confirm** are decided by the modal's variant (see `Modal`), never by the screen.
- **Quit** is handled once at the top of dispatch: every modal's `onQuit()` runs (cancel a pending
  download, dismiss a prompt), then the application stops. A modal never swallows Quit.
- **Raising an error:** `alert(title, plainText, details, retry)` is the single entry point (the
  application's context forwards to it). Failures never use the toast.
- **Hold-hint gating:** the hold bar replaces the hint bar only when the stack is empty and the
  screen reports a hold target; with a modal open the hold is cancelled.
- **Repaint:** `revision()` changes on push, pop and phase-driven changes; the application compares
  it with the last frame and forces a full redraw when it differs.
- **Ownership:** the application owns the stack; screens own their flow objects and push pointers.

## Accessibility

- Exactly one focus target is visible: the top modal's. The dimmed screen shows none, so focus is
  never ambiguous; it returns to the same item when the last modal closes.
- The hint bar always matches the top modal, so what is shown is what works.
- An Alert is never hidden by another dialog and never lost: a second error queues instead of
  overwriting the first. Nothing important disappears by itself.
- No timeouts; a prompt stays until it is answered or its service leaves the phase.

## Largest text and high contrast

No layout of its own. The stack only places the modals; text size and high contrast apply inside each
`Modal`. The hint bar fits at Largest by stepping down, then dropping non-essential hints from the
right (see `Hints`), so the top modal's A and B are always visible.

## API

```cpp
class ModalStack {
public:
    void push(Modal&);                       // non-owning; capacity 4
    void alert(std::string title, std::string plain,
               std::string details = {}, std::function<void()> retry = {});
    bool empty() const;
    void update(float dt);                   // all modals
    void render(Painter&, Theme&, const Rect &area);
    void handle(Action);                     // top only
    void onQuit();
    void hints(Hints&) const;                // top modal's list
    unsigned revision() const;
};
struct InputClaims { bool back = false, start = false, raw = false; };   // per Screen
```

## Derived from / Used by

Derived from: separate modal flags and handlers per screen, the 10-term back predicate, hint chains
that mirror the flags, the error modal's own state, the update prompt routed around the screen
(inventory C5b; modal consolidation report, sections 1-3). Fixes found there: the error hides the
dialog beneath it, tab switching works behind a dialog, hints under an error are stale, a second
error overwrites the first.

Used by: *none yet.*
