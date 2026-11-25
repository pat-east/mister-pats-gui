# Wizard

A multi-step flow of pages: each page has a title, a body, its own hints and a rule for Back. Use it for
any sequence the user walks through (set up, check and install, find and connect, build and prepare).
Do not use it for a single question (a `Modal`) or for a screen that only reflects live state.

Layer: flow. Status: **implemented** (`kit/Wizard.h`, `kit/WizardParts.h`; the demo's Flows tab shows every page kind in
a Medium flow and a Large flow).

**Decisions (owner, 2026-11-23).** Every page of a flow is drawn in a centred dialog of **one size for the whole flow**
(Medium or Large, chosen once from the largest page), content at the top, the frame never changes. A flow keeps its own
state machine (steps, transitions, Back rules, messages; branching is fine); only the frame, Back, the hints and the Result
layout come from the Wizard. Pages are made of framework components, never private widgets.

## Anatomy

A page inside the wizard frame (a `Modal` look: pad 28, title gap 16). No footer; the hints are in the bar.

```
+------------------------------------------------------------------------+
|<28>                                                                    |
|  **Page title**                                         ~2 / 5~        |   optional progress,
|  <16>                                                                  |   small, muted, only
|  Body: text, ChooseList, Result, status slot, ProgressPanel ...        |   for repeated steps
|                                                                        |   (page.progress())
|  ~reason why A or Start is off~                                        |   one line, Step/Capture only
+------------------------------------------------------------------------+
 hint bar: A Continue   B Back              (wizard.hints(): the page's hints + B by kind)
```

## Frame sizes (one per flow)

| Frame | Size (design px at 1080p) | Holds |
| --- | --- | --- |
| Medium | 1120 x 760 | text, a short list, a progress page, a result, the keyboard (field 56 + status line + keys 270 = about 400) |
| Large | 1812 x 860 | a list of 12 one-line rows (52 each) with a lead line and a status line (13 rows fit; 10 two-line rows of 68), a review that scrolls, filter tabs |

The frame is clamped to the canvas minus the margin (36) and centred; sizes scale with the theme. The body handed to a page
is the frame inside the padding, below the title row, minus one reason line (Step and Capture pages). Choose Large only
when the largest page needs it: a Medium flow that has to show a long list scrolls it instead.

## Variants

Four page kinds. The kind decides what Back means and what the page may do.

```
Step      asks or informs        A continues (the page calls go), B goes to back() (default: previous page; the first page closes)
Busy      work is running        B cancels (cancel(): leave(true), then back()); if it cannot be cancelled, B is swallowed and has no hint
Result    outcome                A primary (Done or Try again), B closes. One layout (below)
Capture   takes raw input        every event goes to the page; Back is NOT interpreted; the page keeps a visible way out
```

The text-entry page (keyboard inside the frame) is a Capture page (`EntryPage`): its B is the keyboard's Cancel.

## Result layout (one for every outcome)

`WizardResult` and `drawWizardResult` (also the ready-made `ResultPage`):

```
+------------------------------------------------------------------------+
|  Page title                                                            |
|  (!) Warning                      status: icon AND word, tone colours it   (Done / Warning / Error / Info)
|  2 of 3 items updated             headline: plain words, Heading, at most two lines
|  One item could not be installed. The others are in place.   what changed (Body, Primary)
|  | Item 01                                     v Installed |            optional items (ResultItem rows,
|  | Item 03  ~verify: checksum ...~             x Failed    |            scroll with the shared scrollbar)
|  Try again, or close and update it later.                    next step (Body, Muted)
|  ~Details: verify: checksum did not match~                   raw service text only here (Small, Muted)
+------------------------------------------------------------------------+
 hints: A Try again   B Close            (retry page set)     or   A/B Done   (no retry)
```

Rules: the status word is never missing (a Warning or Error without a word is a bug); the headline says what happened, not what the
service said; "what changed" says whether anything was changed ("Nothing was changed."); the raw text is `details` and nothing
else; a failure offers Try again (a page id) when one exists. The frame outline carries the tone as well. Success is not a toast.

## Body building blocks (`kit/WizardParts.h`)

| Block | Use | Rules |
| --- | --- | --- |
| `wizardTextColumn`, `drawWizardParagraphs` | running text | left edge = the title's edge; at most `readingWidth` (900) wide; Primary for the point, Muted for the explanation; no more than three paragraphs |
| `drawWizardCentered` | one thing per page: a start page, an "empty" page, a question | symbol (112), headline (Heading, bold, at most two lines), one or two short sentences (Body, muted), all centred horizontally and vertically in the body inside the reading width; no more than two sentences; the symbol illustrates the headline and is dropped first when the text is Largest |
| `BusyPage::centered` | work that runs, filled page | the phase phrase large (Heading, bold), the bar, the "what now" line, the small elapsed-time line and the wait note under it, the block centred in the body; the bar is as wide as the reading column; every line has its place reserved (see ProgressPanel.md "Busy page in a wizard") |
| `WizardScrollText` | a Confirm or Review body longer than the frame | Up/Down one line, triggers and Left/Right a page; shared scrollbar |
| `drawWizardStatus` | the status slot: one line, icon + word (tone) + muted text | under a Choose list, or between the field and the keyboard; the height (`wizardStatusHeight`) is reserved even when empty |
| `ChooseList` | pick one or several | below |
| `BusyPage` | work that runs | `ProgressPanel` without its title (the frame is the title), in the text column |
| `ResultPage` | the outcome | above |
| `EntryPage` | the keyboard in the frame | `TextEntryDialog`'s field and keys, a status slot between them, the dialog's own hints |

**ChooseList.** Rows are `ListItemKind::Choice` rows with a status word at the right (`ChooseItem::status` + `statusTone`;
the word carries the state, the tone only colours it). Rows are one-line (46 / 52) unless some row has a `detail` (then all
rows are two-line, 62 / 68). Rows end `scrollbarGutter` before the right edge; the shared scrollbar appears when there is more
than one page. A disabled row is muted, stays focusable, and A does nothing on it. Up/Down move, the triggers page; Left/Right
are free (a flow may use them for filter tabs).

| Mode | The dot means | A does | Returns |
| --- | --- | --- | --- |
| `Single` | the option in use (the current value); the cursor starts there (`setCurrent`) | `Chosen` on the cursor row; the flow moves on | `Chosen` |
| `Multiple` | a checkbox: the row is selected | toggles the row; the flow's Continue or Start key moves on (set `continueKey` to Start) | `Toggled` |

`setExclusiveFirst(true)` (Multiple): the first row is exclusive. Selecting it clears every other row; selecting another row
clears it; clearing the last other row selects it again; A on it while it is the only selected row does nothing. What the first
row means ("All systems") is the flow's: the framework only keeps the markers consistent. A flow with other rules reads
`Toggled`, changes `items()` itself and ignores this flag.
`setJumpByLetter(true)` (an alphabetical list of hundreds of rows, the time-zone cities): TriggerPrev/Next jump to the first row of the previous / next first letter instead of paging; show the hint `Triggers` "Jump to letter".

**Hints, Back and the reason.** `page.hints()` lists the page's own hints (movement, A, Start, X, Y). The wizard adds B by kind:
Step `Back` (`Close` on the first page or when `back()` is `kClose`), Busy `Cancel` (none when not cancellable), Result `A/B Done`
when the page names no A or B; a Capture page names everything. A page that lists its own B hint keeps it ("Skip"). While
`canContinue(reason)` is false the hint of the page's `continueKey()` (A, or Start) is removed, the key is swallowed and the
reason is the one line at the bottom of the frame. `Start` is listed only where A is overloaded and names its action.

## States

```
closed          page == kClosed (-1); the flow object exists, no dialog is open (active() is false)
running a page  one page active; update(dt) polls its background work and may call go(id)
auto-advance    a finished task moves the flow on (Checking -> List, Working -> Done)
cancelling      Busy page + B: cancel() = leave(true) (restore state), then back() (default previous, or kClose)
finished        Result page shown, then close(); leave(false); flow data (passwords) wiped on leave
```

## Measurements

| Item | Value | Notes |
| --- | --- | --- |
| Frame | pad 28, title gap 16, Medium 1120 x 760 or Large 1812 x 860 | **one size for the whole flow**; the dialog never jumps |
| Progress text | small, `textMuted`, right of the title row | only for repeated, homogeneous steps; the title gives way to it |
| Reason line | body, `textMuted`, bottom of the frame | Step and Capture pages; the strip is reserved while the page is current |
| Text column | left edge of the body, at most 900 wide | reading-width rule; lists, bars and tables may use the whole body |
| Page ids | `int`; `kClosed` -1, `kPrevious` -2, `kClose` -3, `kNoPage` -4 | each flow defines its own enum of ids >= 0 |
| History depth | 8 | only Step and Capture pages are Back targets; a Busy or Result page never is |

## Behavior

- **Closed encoding:** one way only, `page != kClosed` (`active()`). No separate open flag and no `None` inside a page enum.
- **Back by kind:** Step goes to `back()` and leaves its page with `leave(true)`; Busy cancels; Result closes; Capture does not
  interpret Back. `go(id)` is forward (`leave(false)`, the page is remembered if it is a Step or Capture); `cancelTo(id)` is Back or
  Cancel (`leave(true)`, nothing is remembered); `close()` ends the flow; `abort()` is the host closing the GUI (`leave(true)`).
- **Cancelling work that stops asynchronously:** a Busy page overrides `cancel(w)`: tell the service to stop, show "Stopping" (a new
  status), return false from `cancellable()` so the hint goes, and call `w.go(...)` from `update()` when the worker is done.
- **Result page:** one layout for every outcome; A is Done or Try again, B closes.
- **Step counter rule:** none by default. A "n / m" (`page.progress()`) appears only for repeated, homogeneous steps (the same
  question N times). Flows that branch (skipped, inserted or optional pages) show no total, since it would be wrong; a long wait
  shows a ProgressPanel instead. No "confirm on cancel": the Back hint states the consequence.
- **enter() resets** the cursor, scroll and entered text of the page; return to a page keeps nothing implicit. `clearHistory()`
  after a commit keeps Back from returning across it.
- **Flow-owned data:** snapshots, entered text live in one struct captured by the page lambdas (no `std::any`); secrets are wiped
  in `leave` (`EntryPage` does it for the typed text).
- **canContinue(reason):** false removes the continue hint, swallows the key and shows the reason as one line.

## Accessibility

- Every page says what it is (title), what comes next (hint) and why it cannot continue (reason).
- Status is never colour alone: a Result and a status slot carry icon and word (Done, Warning, Error, Info); a list row's state is a word.
- A long or unknown wait is a Busy page with one calm status line; it never takes input hostage:
  cancelable work says "B Cancel". A Busy page answers four questions: which phase (`status`, one phrase per phase), what exactly
  now (`detail`, may change, shown once a second at most: `setDetail`), how long it has run ("Running for 1:25", once a second) and,
  after 10 s without any visible change, what it is waiting for and that nothing is stuck (`waitNote`, the flow's text per phase; a
  generic "Still working" line when it gives none). No per-file counters faster than once a second, no ETA guess unless a service gives a real one.
- Errors are Result pages with plain words, what changed, a Details line and a controller next step.
- No timeouts hidden in a page; a countdown that must exist is named in the body and can be restarted. Capture pages keep a visible way out.
- Text sizes: lines wrap or elide, never overlap; the body scrolls with the shared Scrollbar (list, review, result items).

## Largest text and high contrast

Same as `Modal`. A Step page with a List scrolls inside the frame; the title elides before the progress text. High contrast
applies to the frame (white border), every body component and the scrollbar.

## API

```cpp
using PageId = int;
constexpr PageId kClosed = -1, kPrevious = -2, kClose = -3, kNoPage = -4;
enum class WizardFrame { Medium, Large };
enum class WizardEvent { Up, Down, Left, Right, Confirm, Back, X, Y, Start, ShoulderPrev, ShoulderNext, TriggerPrev, TriggerNext };

class WizardPage {
public:
    enum class Kind { Step, Busy, Result, Capture };
    virtual Kind kind() const;                         // Step
    virtual Text title() const = 0;
    virtual std::string progress() const;              // "2 / 5" or empty
    virtual Tone tone() const;                         // the frame's outline
    virtual void enter(Wizard &);                      // reset cursor, scroll, snapshot
    virtual void leave(bool cancelled);                // undo side effects, wipe secrets
    virtual bool update(float dt, Wizard &);           // poll work; w.go(id); true = redraw
    virtual void draw(Painter &, const Theme &, const Rect &body) = 0;
    virtual void handle(WizardEvent, Wizard &);        // after the Back rule and canContinue
    virtual std::vector<Hint> hints() const;           // the page's own; the wizard adds B by kind
    virtual bool canContinue(Text &reason) const;
    virtual HintKey continueKey() const;               // A (default) or Start
    virtual PageId back() const;                       // kPrevious
    virtual bool cancellable() const;                  // Busy
    virtual void cancel(Wizard &);                     // Busy: B
    virtual bool rawInput() const;                     // Capture: the page reads the pad itself
};
class Wizard {
public:
    explicit Wizard(WizardFrame);
    template <class T> T *add(PageId, std::unique_ptr<T>);
    void open(PageId first);  void go(PageId);  void cancelTo(PageId);  void close();  void abort();  void clearHistory();
    bool active() const;  PageId pageId() const;  WizardPage *current() const;
    bool handle(WizardEvent);  bool update(float dt);
    std::vector<Hint> hints() const;  bool wantsRawInput() const;
    void draw(Painter &, const Theme &, const Rect &canvas) const;
};
class FnPage;        // a page with std::function hooks: onDraw, onEvent, onHints, onEnter, onLeave, onUpdate, onCanContinue, onBack, ...
```

Defining a flow: ids in an enum, pages from `FnPage` (lambdas around a shared struct), `BusyPage`, `ResultPage`, `EntryPage`;
`add` each, then `open(first)`; every frame `update(dt)`, `draw`, and `hints()` into the help bar; route input with `handle`.

```cpp
enum { kIntro, kPick, kWork, kDone };
Wizard w(WizardFrame::Medium);
auto list = std::make_shared<ChooseList>(ChooseMode::Single);
std::unique_ptr<FnPage> pick(new FnPage(WizardPage::Kind::Step, Text::key("flow.pick.title")));
pick->onEnter = [list](Wizard &) { list->setItems(makeItems()); list->setCurrent(0); };
pick->onDraw = [list](Painter &p, const Theme &t, const Rect &body) { list->draw(p, t, body); };
pick->onHints = [] { return std::vector<Hint>{hint(HintKey::UpDown, "hint.move"), hint(HintKey::A, "hint.choose", true)}; };
pick->onEvent = [list](WizardEvent e, Wizard &w) { if (list->handle(e) == ChooseOutcome::Chosen) w.go(kWork); };
w.add(kPick, std::move(pick));
// ... a BusyPage (onUpdate polls and ends with w.go(kDone)), a ResultPage with WizardResult, then:
w.open(kIntro);
```

The Settings host is `WizardFlowPage` (src/app/settings/WizardFlowPage.h).

## Derived from / Used by

Derived from: seven hand-built flows with four state encodings (inventory C7; modal consolidation
report, section 4). Application mapping, as page lists:

```
Core updater   Intro -A(online)-> Checking(Busy) -auto-> List -Start(>=1)-> Review -A->
                 Installing (no cancel) -auto-> Result                                           (Large)
                 Checking: B = cancel and close;  List: Y = re-check -> Checking
                 check failed -> Result(Retry -> Intro)
Wi-Fi          Scanning(Busy) -> Networks -> [Ssid] -> [Password] -> TestPrompt -> Testing(Busy)
                 -> TestReview -(country needed)-> CountryEntry -> save;  TestFailed(Retry)
                 Back on Testing/TestReview = cancel and restore the old connection (leave(true))
Time zone      Regions -A-> Cities -A-> set      B on Cities = previous level (2-page Choose)
Scan           first run: Welcome -> JumpStart -> Working(Busy) -> ArtworkWorking(Busy) -> Overview
               settings:  Intro -> Working -> ArtworkOffer -> ArtworkWorking -> Done
               artwork:   ArtworkOffer -> ArtworkWorking -> Done   (route chosen in the page, as data)
Controller map Opening -A-> Capture x N (one page type, index; progress "n / 19") -> save -> Result   (raw input claimed)
Bluetooth      Idle -> Scanning(Busy) -> Found -A-> hands off to Controller map
                 Scanning -> Timeout(Retry -> Idle) | NoBluetooth(Info)
```

Do not fit, stay screens or plain modals: the **Disc screen** (state derived from the drive and core
snapshots every frame, not a sequence; only its viewer and install confirm become modals), the
**update installer** (download progress is shown inline in the settings rows and cancel is global;
only its confirm and success prompts become modals), and **account entry** (one `Input` modal).

Used by: the demo's Flows tab; Settings flows through `WizardFlowPage`.

## Implementation

src/ui-framework/kit/Wizard.h, Wizard.cpp: `Wizard`, `WizardPage`, `FnPage` (frame, routing by page id, history depth 8, Back by kind, hints). src/ui-framework/kit/WizardParts.h,
WizardParts.cpp: text, scroll text, status slot, `ChooseList`, Result layout and `ResultPage`, `BusyPage`, `EntryPage`, `drawWizardCentered`. Shown in the demo's Flows tab
(`shell-wizard-*.png`, `shell-wizard-large-*.png` in build/out). Application host: src/app/settings/WizardFlowPage.h (`make -C src/app run ARGS="--tab settings --page standard/wizard --keys a,a"`).
