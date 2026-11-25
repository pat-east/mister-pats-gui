#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ui-framework/kit/Components.h"

namespace ui {

// ---- Wizard: a multi-step flow of pages in ONE dialog frame (docs/components/Wizard.md).
//
// The flow owns its pages (WizardPage, registered by id) and the wizard owns what is common: the frame (scrim,
// panel, title row, optional progress text), routing by page id, the history for Back, the Back rule of each page
// kind, the hint bar and the reason line of a page that cannot continue. It draws every page in the same frame:
// the size is chosen once for the whole flow (Medium or Large) and never changes.
//
// The flow keeps Classic's state machine: its steps, transitions and messages are the pages' code (a page calls
// w.go(id) when it is done); only the frame, Back and the hints are the wizard's. Branching is fine.
//
//   Wizard w(WizardFrame::Medium);
//   w.add(kIntro, std::move(introPage));  w.add(kWork, ...);  w.add(kDone, ...);
//   w.open(kIntro);                       // enter() of the first page runs now
//   // every frame:  w.update(dt);  w.draw(painter, theme, canvas);  hints: w.hints()
//   // input:        w.handle(WizardEvent::Confirm);     // returns true while the wizard is open (it takes all input)
//   // finished when w.active() is false (the flow went to kClose, or Back left the first page).

using PageId = int;
constexpr PageId kClosed = -1;     // no page is open (the state after the flow ended)
constexpr PageId kPrevious = -2;   // a target: the page before this one (kClose when there is none)
constexpr PageId kClose = -3;      // a target: leave the flow
constexpr PageId kNoPage = -4;     // "no target" in a spec (a Result without a retry page)

// The one size of a flow, in design pixels at 1080p, clamped to the canvas minus the margin:
//   Medium  1120 x 760   texts, a short list, a progress page, a result, the on-screen keyboard (field + status line + keys = 400)
//   Large   1812 x 860   a list of 12 one-line rows (52 each) with a status line, a review that scrolls, filter tabs
// Choose it once, from the flow's largest page. The frame does not change between pages, so the dialog never jumps.
enum class WizardFrame { Medium, Large };

// What the host maps its controller input onto. ShoulderPrev/Next are the shoulder buttons (the tab buttons of the
// shell), TriggerPrev/Next the triggers (jump to a letter, page a list).
enum class WizardEvent { Up, Down, Left, Right, Confirm, Back, X, Y, Start, ShoulderPrev, ShoulderNext, TriggerPrev, TriggerNext };

class Wizard;

class WizardPage {
public:
    // Step    asks or informs         A continues (the page calls w.go), B goes to back() (the previous page; the first page closes)
    // Busy    work is running         B cancels (cancel(), leave(true)); if it cannot be cancelled B is ignored and has no hint
    // Result  the outcome             A is the primary action (Done / Try again), B closes
    // Capture takes raw input         every event goes to the page; Back is NOT interpreted (the page keeps a visible way out)
    enum class Kind { Step, Busy, Result, Capture };

    virtual ~WizardPage() = default;

    virtual Kind kind() const { return Kind::Step; }
    // What the page is: the dialog's title (a heading). Never the question; the question is in the body.
    virtual Text title() const = 0;
    // Optional "2 / 5" at the right of the title row, small and muted. Only for repeated, homogeneous steps
    // (the same question N times); a branching flow shows none, since a total would be wrong. Empty = none.
    virtual std::string progress() const { return std::string(); }
    // The frame's outline (Neutral: none). A Result page has the tone of its outcome.
    virtual Tone tone() const { return Tone::Neutral; }

    // The page became the current one: reset its cursor, scroll, entered text and snapshots (nothing is kept implicitly).
    virtual void enter(Wizard &w) { (void)w; }
    // The page was left. `cancelled`: by Back, by Cancel on a Busy page, or because the GUI closed; false when the
    // page moved on by itself (w.go) or the flow finished. Undo side effects and wipe secrets here.
    virtual void leave(bool cancelled) { (void)cancelled; }
    // Once per frame while the page is current: poll background work (never block) and call w.go(id) when it ends.
    // Returns true when what the page shows changed (the screen needs a redraw).
    virtual bool update(float deltaSeconds, Wizard &w) { (void)deltaSeconds; (void)w; return false; }
    // Draws the body: `body` is the rect below the title row, inside the frame's padding (see Wizard::bodyRect).
    virtual void draw(Painter &p, const Theme &t, const Rect &body) = 0;
    // An input event, after the wizard applied the Back rule of the kind (so a Step never sees Back, a Busy page never
    // sees Back, a Result sees neither Back; a Capture sees everything) and after canContinue (A and Start are not
    // delivered to a Step page that cannot continue).
    virtual void handle(WizardEvent event, Wizard &w) { (void)event; (void)w; }
    // The page's own hints (movement, A and Start words, X/Y actions). The wizard adds B by kind: Back (Close on the
    // first page), Cancel on a Busy page, Close on a Result page; it removes A and Start while canContinue is false.
    // A page that lists its own B hint (e.g. "Skip") keeps it.
    virtual std::vector<Hint> hints() const { return std::vector<Hint>(); }
    // False: the continue hint is removed, A and Start are ignored and `reason` is shown as one line at the bottom of
    // the frame ("Select at least one system."). Step and Capture pages only.
    virtual bool canContinue(Text &reason) const { (void)reason; return true; }
    // The key that moves a Step or Capture page on: A (default) or Start where A is overloaded (a checklist: A selects,
    // Start continues). While canContinue is false the wizard swallows this key and removes its hint.
    virtual HintKey continueKey() const { return HintKey::A; }
    // Where Back goes on a Step page (and where Cancel goes on a Busy page): kPrevious (default), kClose or a page id.
    virtual PageId back() const { return kPrevious; }
    // Busy page: false while the work cannot be stopped (an install): B is swallowed and no hint is shown.
    virtual bool cancellable() const { return true; }
    // Busy page, B pressed. Default: leave(true) and go to back(). A page that must wait for its worker (a scan that
    // stops asynchronously) overrides it: tell the service to stop, switch to a "Stopping" state with cancellable()
    // false, and call w.go(...) from update() when the worker is done.
    virtual void cancel(Wizard &w);
    // Capture page that reads the pad itself (the controller mapping): the host stops translating pad input to events.
    virtual bool rawInput() const { return false; }
};

class Wizard {
public:
    explicit Wizard(WizardFrame frame = WizardFrame::Medium) : frame_(frame) {}
    Wizard(const Wizard &) = delete;
    Wizard &operator=(const Wizard &) = delete;

    WizardFrame frame() const { return frame_; }
    void setFrame(WizardFrame frame) { frame_ = frame; }

    // Registers a page under an id (>= 0). The wizard owns it. Returns the page for further setup.
    template <class T>
    T *add(PageId id, std::unique_ptr<T> page) {
        T *raw = page.get();
        pages_[id] = std::move(page);
        return raw;
    }
    WizardPage *page(PageId id) const;

    // ---- Flow control (called by pages and by the host).
    void open(PageId first);                // start the flow at `first`; clears the history
    void go(PageId target);                 // forward: remembers a Step/Capture page for Back; also kPrevious, kClose
    void cancelTo(PageId target);           // Back or Cancel: like go(), but the page is left with leave(true)
    void close() { go(kClose); }            // the flow is finished (leave(false))
    void abort();                           // the host closes the GUI: leave(true) on the current page, the flow ends
    void clearHistory() { history_.clear(); }   // after a commit: Back must not return across it

    bool active() const { return page_ != kClosed; }
    PageId pageId() const { return page_; }
    WizardPage *current() const { return active() ? page(page_) : nullptr; }
    size_t historySize() const { return history_.size(); }

    // ---- Per frame.
    // Applies the Back rule of the page kind and routes the event. Returns true while the wizard is open (it takes all
    // input, like any dialog); false once it has ended.
    bool handle(WizardEvent event);
    bool update(float deltaSeconds);        // true = redraw
    // The hint bar: the page's hints, A/Start removed while it cannot continue, B added by kind.
    std::vector<Hint> hints() const;
    bool wantsRawInput() const;             // a Capture page with rawInput()
    // Scrim, panel, title row, progress text, the page body and the reason line.
    void draw(Painter &p, const Theme &t, const Rect &canvas) const;

    // Geometry (design pixels scale with the theme): the panel in `canvas`, and the body rect handed to a page.
    static Rect panelRect(const Theme &t, const Rect &canvas, WizardFrame frame);
    Rect bodyRect(Painter &p, const Theme &t, const Rect &canvas) const;

private:
    void transition(PageId target, bool cancelled, bool remember);
    int reasonStrip(Painter &p, const Theme &t) const;

    WizardFrame frame_;
    std::map<PageId, std::unique_ptr<WizardPage>> pages_;
    PageId page_ = kClosed;
    std::vector<PageId> history_;           // depth 8
};

// ---- FnPage: a page defined with lambdas (a flow is a few lambdas around a shared struct). Every hook is optional
//      except `onDraw`. The hooks mirror WizardPage; kind, title, tone and progress are plain fields or functions.
class FnPage : public WizardPage {
public:
    explicit FnPage(Kind kind, const Text &title) : kind_(kind), title_(title) {}

    Kind kind_;
    Text title_;
    Tone tone_ = Tone::Neutral;
    std::function<std::string()> onProgress;
    std::function<void(Wizard &)> onEnter;
    std::function<void(bool cancelled)> onLeave;
    std::function<bool(float, Wizard &)> onUpdate;
    std::function<void(Painter &, const Theme &, const Rect &body)> onDraw;
    std::function<void(WizardEvent, Wizard &)> onEvent;
    std::function<std::vector<Hint>()> onHints;
    std::function<bool(Text &reason)> onCanContinue;
    std::function<PageId()> onBack;
    std::function<bool()> onCancellable;
    std::function<void(Wizard &)> onCancel;
    std::function<bool()> onRawInput;
    HintKey continueKey_ = HintKey::A;

    Kind kind() const override { return kind_; }
    Text title() const override { return title_; }
    std::string progress() const override { return onProgress ? onProgress() : std::string(); }
    Tone tone() const override { return tone_; }
    void enter(Wizard &w) override { if (onEnter) onEnter(w); }
    void leave(bool cancelled) override { if (onLeave) onLeave(cancelled); }
    bool update(float dt, Wizard &w) override { return onUpdate ? onUpdate(dt, w) : false; }
    void draw(Painter &p, const Theme &t, const Rect &body) override { if (onDraw) onDraw(p, t, body); }
    void handle(WizardEvent e, Wizard &w) override { if (onEvent) onEvent(e, w); }
    std::vector<Hint> hints() const override { return onHints ? onHints() : std::vector<Hint>(); }
    bool canContinue(Text &reason) const override { return onCanContinue ? onCanContinue(reason) : true; }
    PageId back() const override { return onBack ? onBack() : kPrevious; }
    bool cancellable() const override { return onCancellable ? onCancellable() : true; }
    void cancel(Wizard &w) override { if (onCancel) onCancel(w); else WizardPage::cancel(w); }
    bool rawInput() const override { return onRawInput ? onRawInput() : false; }
    HintKey continueKey() const override { return continueKey_; }
};

}  // namespace ui
