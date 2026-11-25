#pragma once

// The building blocks of a Wizard page body and the ready-made pages built from them (docs/components/Wizard.md).
// A page's draw() receives the body rect of the frame; these functions draw into it. All of them use framework
// components only, no private widgets, and follow the reading-width rule (running text is not full width).
//
//   Text         wizardTextColumn, drawWizardParagraphs, WizardScrollText (a Confirm/Review body that scrolls)
//   Status       drawWizardStatus (one line: icon + word + text) for Choose lists and text-entry pages
//   Choose       ChooseList: single or multiple selection, status per row, scrolling, scrollbar gutter
//   Centred      drawWizardCentered (symbol + headline + short lines, centred in the body), BusyPage::centered
//   Result       WizardResult + drawWizardResult (the one layout of every outcome) and ResultPage
//   Busy         BusyPage (ProgressPanel without its title: the frame carries it)
//   Entry        EntryPage (the on-screen keyboard inside the frame; a Capture page)

#include <functional>
#include <string>
#include <vector>

#include "ui-framework/kit/Data.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/TextEntryDialog.h"
#include "ui-framework/kit/Wizard.h"

namespace ui {

// ---- Text ---------------------------------------------------------------------------------------------------------

// The column running text uses in a wizard page: the body's left edge (the title's edge: one left edge per dialog),
// at most `readingWidth` (900) wide. Lists, tables and bars may use the whole body width.
Rect wizardTextColumn(const Theme &t, const Rect &body);

struct WizardParagraph {
    Text text;
    TextTier tier = TextTier::Primary;     // Primary for the point, Muted for the explanation
    bool bold = false;
};
// Draws wrapped paragraphs in the text column with the shared line step and 10 px between paragraphs. Returns the
// height used. Text that does not fit is cut at the bottom of `body`: size the frame, or use WizardScrollText.
int drawWizardParagraphs(Painter &p, const Theme &t, const Rect &body, const std::vector<WizardParagraph> &paragraphs);

// A body of text that can be longer than the frame (release notes, a review): Up/Down scroll one line, the
// triggers (and Left/Right) a page; the shared scrollbar sits at the right edge of `area`.
class WizardScrollText {
public:
    void set(const std::vector<WizardParagraph> &paragraphs) { source_ = paragraphs; scroll_ = 0; }
    void reset() { scroll_ = 0; }
    // True when the event scrolled or tried to (so the page does not use it for something else).
    bool handle(WizardEvent event);
    void draw(Painter &p, const Theme &t, const Rect &area);
    bool scrolls() const { return total_ > visible_; }      // after the first draw
    bool atEnd() const { return scroll_ + visible_ >= total_; }
    int scroll() const { return scroll_; }

private:
    struct Line { std::string text; TextTier tier; bool bold; };
    std::vector<WizardParagraph> source_;
    std::vector<Line> lines_;
    int scroll_ = 0, visible_ = 1, total_ = 0;
};

// ---- Centred page ---------------------------------------------------------------------------------------------------

// One thing per page, large and centred: a symbol, a headline and one or two short sentences, centred horizontally and
// vertically in the body (a start page, an "empty" page, a question without a list). The symbol illustrates the
// headline; it never carries the meaning alone. Text is centred inside the reading width. When the block does not fit
// (Largest text) the symbol goes first, then the lines are cut at the bottom of the body.
struct WizardCentered {
    IconId icon = IconId::None;
    Tone tone = Tone::Neutral;         // the symbol's colour: Neutral = accent; Warning, Error, Done as in a status
    Text headline;                     // short, plain words, Heading size, bold (at most two lines)
    std::vector<Text> lines;           // each a short sentence in Body, muted (the first may be Primary: see `firstPrimary`)
    bool firstPrimary = false;
};
// Draws it into `body`; returns the height of the block.
int drawWizardCentered(Painter &p, const Theme &t, const Rect &body, const WizardCentered &spec);

// ---- Status slot ---------------------------------------------------------------------------------------------------

// One line: the status word with its icon (never colour alone) in the tone, then the muted text. `word` empty: only
// the text. Draws into `area` (one line high) and elides. Use it under a Choose list or between the field and the
// keyboard of a text entry page. The height is wizardStatusHeight(); reserve it also when the line is empty, so the
// page does not jump when a status appears.
int wizardStatusHeight(Painter &p, const Theme &t);
void drawWizardStatus(Painter &p, const Theme &t, const Rect &area, Tone tone, const Text &word, const Text &line);

// ---- Choose list ----------------------------------------------------------------------------------------------------

struct ChooseItem {
    Text title;
    Text detail;                   // optional second line; one row height for the whole list (any detail = two-line rows)
    Text status;                   // optional status word at the right ("Not installed"); the tone colours it, the word carries it
    Tone statusTone = Tone::Neutral;
    bool checked = false;          // Single: this is the option in use. Multiple: the row is selected.
    bool disabled = false;         // muted, focusable, A does nothing
};
enum class ChooseMode {
    Single,     // the dot marks the option in use (the current value); A returns Chosen and the flow moves on
    Multiple    // the dot is a checkbox; A toggles the row and returns Toggled; the flow's Continue/Start key moves on
};
enum class ChooseOutcome { None, Moved, Toggled, Chosen };

class ChooseList {
public:
    ChooseList() = default;
    explicit ChooseList(ChooseMode mode) : mode_(mode) {}

    void setMode(ChooseMode mode) { mode_ = mode; }
    // Multiple only. The first row is exclusive (generic "All" row): selecting it clears every other row, selecting
    // another row clears it, and clearing the last other row selects it again. A on it while it is the only row
    // selected does nothing. The flow's meaning of the row is its own.
    void setExclusiveFirst(bool on) { exclusiveFirst_ = on; }
    // A long list in alphabetical order (the time-zone cities): TriggerPrev/Next jump to the first row of the previous / next
    // first letter (case-insensitive) instead of paging. The flow shows the hint "Jump" (HintKey::Triggers, "hint.jump").
    void setJumpByLetter(bool on) { jumpByLetter_ = on; }

    void setItems(std::vector<ChooseItem> items);            // keeps the cursor in range
    std::vector<ChooseItem> &items() { return items_; }
    const std::vector<ChooseItem> &items() const { return items_; }
    void setCursor(int index);
    int cursor() const { return cursor_; }
    void setCurrent(int index);                              // Single: marks `index` as in use and puts the cursor there
    int checkedCount() const;
    std::vector<int> checked() const;

    // Up/Down move, TriggerPrev/Next page, Confirm acts. Left/Right are not used (a flow may use them for filters).
    // Returns what happened; for Toggled and Chosen the row is cursor().
    ChooseOutcome handle(WizardEvent event);

    // Draws the rows in `area` (the flow takes the status line, if any, out of the body first). Rows stop
    // `scrollbarGutter` before the right edge; the scrollbar appears when there is more than one page.
    void draw(Painter &p, const Theme &t, const Rect &area);
    // The height all rows need (without scrolling), for a page that centres a short list; valid for the current theme.
    int contentHeight(const Theme &t) const;
    int visibleRows() const { return layout_.visibleRows(); }   // after the first draw
    int scroll() const { return layout_.scroll(); }

private:
    bool twoLine() const;
    int letterJump(int direction) const;
    std::vector<ChooseItem> items_;
    ChooseMode mode_ = ChooseMode::Single;
    bool exclusiveFirst_ = false;
    bool jumpByLetter_ = false;
    int cursor_ = 0;
    ListLayout layout_;
};

// ---- Result: the one layout of every outcome ------------------------------------------------------------------------

// Status icon and word (Done, Warning, Error, Info), the headline in plain words, what changed, optional outcome items,
// the next step, and the raw service text only as small muted Details. The primary action and Close are hints.
struct WizardResult {
    Tone tone = Tone::Done;            // Done, Warning, Error; Neutral (or Action) is Info
    Text status;                       // the status word; empty = the tone's word (Done, Warning, Error, Info)
    Text headline;                     // what happened, in plain words ("Library ready", "Could not connect")
    Text changed;                      // what changed or did not ("3 systems and 1872 games added." / "Nothing was changed.")
    std::vector<ResultItemSpec> items; // optional outcome list (which file failed, with its own status word and Details)
    Text next;                         // the next step as a sentence ("Choose another network, or try again.")
    Text details;                      // raw service text; drawn small and muted after "Details:"; never the headline
    PageId retry = kNoPage;            // a page to go to with A "Try again" (Error and Warning); kNoPage: A is Done
    Text primary;                      // replaces the words of A ("Try again" / "Done")
};
// Draws it into `body`. `firstItem` is the scroll position of the items (clamped); returns how many items are fully shown.
int drawWizardResult(Painter &p, const Theme &t, const Rect &body, const WizardResult &result, int &firstItem);

// The ready-made Result page: A is the primary action (go to `retry`, else finish the flow), B closes. Set the
// outcome with set() when the page is about to be shown (from the previous page's code or from onEnter).
class ResultPage : public WizardPage {
public:
    explicit ResultPage(const Text &title) : title_(title) {}
    Text title_;
    std::function<void(Wizard &)> onEnter;               // build the outcome from the service snapshot here
    std::function<void()> onFinished;                    // A or B ended the flow (the page was left)

    void set(const WizardResult &r) { result_ = r; first_ = 0; shown_ = 1 << 20; }
    WizardResult &result() { return result_; }

    Kind kind() const override { return Kind::Result; }
    Text title() const override { return title_; }
    Tone tone() const override { return result_.tone == Tone::Action ? Tone::Neutral : result_.tone; }
    void enter(Wizard &w) override { first_ = 0; shown_ = 1 << 20; if (onEnter) onEnter(w); }
    void leave(bool) override { if (onFinished) onFinished(); }
    void draw(Painter &p, const Theme &t, const Rect &body) override { shown_ = drawWizardResult(p, t, body, result_, first_); }
    void handle(WizardEvent event, Wizard &w) override;
    std::vector<Hint> hints() const override;

private:
    WizardResult result_;
    int first_ = 0, shown_ = 1 << 20;   // items fully shown by the last draw (all of them until the first draw)
};

// ---- Busy -----------------------------------------------------------------------------------------------------------

// Work is running. Top to bottom (docs/components/ProgressPanel.md): the phase phrase (`status`, one per phase), the bar,
// `detail` (what is being worked on right now: one line, service text is fine here), a small muted line with the elapsed
// time ("Running for 1:25", once a second; `facts` follow it on the same line), and the wait note: a small muted sentence the page shows by
// itself when status, detail and bar did not change for `waitNoteAfter` seconds, naming what is being waited for and
// that nothing is stuck. Every line has its place reserved from the first frame, so nothing jumps. B cancels unless
// `cancelable` is false (no hint then).
class BusyPage : public WizardPage {
public:
    explicit BusyPage(const Text &title) : title_(title) {}
    Text title_;
    Text status;                       // one phrase per phase ("Reading your game folders"); not per item
    Text detail;                       // what is worked on now ("System 12 of 46: Super Nintendo"); setDetail() limits it to 1 Hz
    Text facts;                        // optional small facts in front of the elapsed time ("46 systems  \xC2\xB7  10485 games")
    float fraction = -1.0f;            // 0..1; negative: unknown (a static minimum-fill bar)
    bool showElapsed = true;           // "Running for 1:25"
    Text waitNote;                     // shown after `waitNoteAfter` quiet seconds; empty: the generic "Still working ..." line
    float waitNoteAfter = 10.0f;
    bool cancelable = true;
    bool centered = false;             // a large status phrase, the bar and the lines centred in the body (not top-left)
    PageId backTo = kClose;            // where B goes when the page cancels itself (default: leave the flow)
    std::function<void(Wizard &)> onEnter;
    std::function<bool(float, Wizard &)> onUpdate;       // poll the service; set status/detail/fraction; w.go(...) at the end
    std::function<void(bool cancelled)> onLeave;          // undo side effects when cancelled
    std::function<void(Wizard &)> onCancel;               // overrides the default (leave(true) and go to backTo)

    // Sets detail and facts, but at most once a second (the first value and an emptied line at once, a newer value waits).
    // Call it from onUpdate as often as the service changes.
    void setDetail(const Text &line, const Text &factsLine = Text());

    // Whole seconds the page has been shown, and the text of the elapsed line ("Running for 1:25").
    int elapsedSeconds() const { return int(elapsed_ + 1e-6); }
    std::string elapsedText() const;
    bool waitNoteVisible() const { return quiet_ + 1e-6 >= double(waitNoteAfter); }

    Kind kind() const override { return Kind::Busy; }
    Text title() const override { return title_; }
    void enter(Wizard &w) override;
    void leave(bool cancelled) override { if (onLeave) onLeave(cancelled); }
    bool update(float dt, Wizard &w) override;
    void draw(Painter &p, const Theme &t, const Rect &body) override;
    bool cancellable() const override { return cancelable; }
    PageId back() const override { return backTo; }
    void cancel(Wizard &w) override { if (onCancel) onCancel(w); else WizardPage::cancel(w); }

private:
    double elapsed_ = 0.0, quiet_ = 0.0, sinceDetail_ = 1.0;   // double: frames of 0.1 s must add up to whole seconds
    float lastFraction_ = -1.0f;
    bool hasPending_ = false;
    Text pendingDetail_, pendingFacts_;
    std::string lastStatus_, lastDetail_, lastFacts_;
};

// "0:05", "1:25", "61:00": minutes and seconds, minutes not wrapped at 60 (a long job stays readable as it is).
std::string formatElapsed(int seconds);

// ---- Text entry -----------------------------------------------------------------------------------------------------

// The on-screen keyboard inside the frame: the same field and keys as ui::TextEntryDialog, a status slot between them
// (the dialog's message line), the dialog's own hints. A Capture page: Back is the dialog's Cancel and goes to back().
// Start submits. A wrong value stays in the keyboard and `validate` says why in the status slot.
class EntryPage : public WizardPage {
public:
    EntryPage(const Text &title, const TextEntrySpec &spec) : title_(title), spec_(spec), dialog_(spec) {}
    Text title_;
    // Status slot, always reserved: "Enter the password for X" (Neutral, no word) or a note with a word.
    Tone statusTone = Tone::Neutral;
    Text statusWord, statusLine;
    // The value a Submit tests; returns the message when it is not acceptable, an empty Text when it is.
    std::function<Text(const std::string &)> validate;
    std::function<void(const std::string &value, Wizard &)> onSubmit;
    std::function<void(Wizard &)> onEnter;
    PageId backTo = kPrevious;

    void setSpec(const TextEntrySpec &spec) { spec_ = spec; dialog_ = TextEntryDialog(spec); invalid_ = Text(); }
    const std::string &value() const { return dialog_.value(); }

    Kind kind() const override { return Kind::Capture; }
    Text title() const override { return title_; }
    void enter(Wizard &w) override;
    void leave(bool) override { dialog_.clear(); }        // a typed secret does not outlive the page
    void draw(Painter &p, const Theme &t, const Rect &body) override;
    void handle(WizardEvent event, Wizard &w) override;
    std::vector<Hint> hints() const override { return dialog_.hints(); }
    PageId back() const override { return backTo; }

private:
    TextEntrySpec spec_;
    TextEntryDialog dialog_;
    Text invalid_;
};

}  // namespace ui
