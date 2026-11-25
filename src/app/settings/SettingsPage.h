#pragma once

// Pages: everything a Settings row can open beyond its own first level (a chooser, a confirmation, a progress
// view, a wizard step, a keyboard, a text viewer). A page is one screenful of state with the same four
// duties as a tab: update, draw, handle an event, name its hints. The PageStack owns the open pages, routes
// all input to the top page, shows its hints and locks the tabs (the same as every overlay in src/app/).
//
//   class MyPage : public SettingsPage {
//   public:
//       void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override { ... }
//       bool handle(Event e, PageStack &stack) override {
//           if (e == Event::Back) { stack.pop(); return true; }        // closes this page
//           if (e == Event::Confirm) { stack.push(std::unique_ptr<SettingsPage>(new OtherPage)); return true; }
//           return false;                                              // not used
//       }
//       std::vector<ui::Hint> hints() const override { ... }
//   };
//   // from a category:  pages.push(std::unique_ptr<SettingsPage>(new MyPage));
//
// Rules (src/app/README.md applies): no blocking in draw/handle/update, colors and sizes from ui::Theme, every
// word a ui::Text key (register it in the category's strings function), components only from src/ui-framework/kit.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/Tab.h"
#include "ui-framework/kit/Components.h"

namespace patsgui {

class PageStack;

// Where a page draws.
//   Content  in the tab's content rectangle, instead of the three panes (a viewer, a list, a progress view).
//            The page draws its own frame; use drawSettingsPageFrame() (StandardPages.h) for the standard one.
//   Modal    over the three panes (a dialog, the keyboard). The page draws its own scrim, the way ui::ModalStack
//            and ui::TextEntryDialog do, and may use the whole canvas (p.bounds()).
enum class PagePresentation { Content, Modal };

class SettingsPage {
public:
    virtual ~SettingsPage() = default;

    virtual PagePresentation presentation() const { return PagePresentation::Modal; }

    // Once per frame while the page is the top page (poll a service here, advance a countdown). Returns true
    // when what the page shows changed and the screen needs a redraw. Never blocks.
    virtual bool update(float deltaSeconds, PageStack &stack) { (void)deltaSeconds; (void)stack; return false; }

    // `content` is the tab's content rectangle (the same one SettingsTab::draw receives).
    virtual void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) = 0;

    // Returns true when the event was used. A Back the page does not use closes it (the stack pops it), so a
    // page cannot trap the user; a page that must not be left (an install in progress) uses Back and
    // says so in its hints. The page closes itself with stack.pop() and opens another with stack.push() /
    // stack.replace(): both take effect after handle() returns.
    virtual bool handle(Event event, PageStack &stack) = 0;

    // The page's own hints: they are the whole help line while the page is open (the shell hides "change
    // tab" and "leave" because locked() is true).
    virtual std::vector<ui::Hint> hints() const = 0;

    // True: the tabs are locked and TabPrev/TabNext are swallowed. A page that lets the shoulder buttons
    // through returns false (then its hints are shown together with the tab's).
    virtual bool locked() const { return true; }

    // Called once when the page leaves the stack (closed by itself, by Back, or because the GUI closes or the
    // Settings tab is left): stop the work only this page started, wipe a typed secret.
    virtual void onClose() {}
};

// The pages that are open, bottom first. Owned by SettingsTab; a category receives it in activate().
class PageStack {
public:
    // ---- For pages and categories (safe to call from handle() and update(): applied afterwards).
    void push(std::unique_ptr<SettingsPage> page);
    void pop();                                              // closes the top page
    void replace(std::unique_ptr<SettingsPage> page);        // pop + push
    void clear();                                            // closes every page, top first (applied at once)
    // A confirmation or a notice (Classic: Context::notify), shown by the shell's toast (Services::notice).
    // `seconds` is Classic's: the line stays at least that long. Lost when no shell gave the stack a sink.
    void toast(const ui::Text &text, float seconds = 3.5f) { notice(sink_, text, ui::Tone::Done, seconds); }
    void toast(const std::string &data, float seconds = 3.5f) { notice(sink_, ui::Text::data(data), ui::Tone::Done, seconds); }

    // ---- For the tab.
    void setNotice(Notice *sink) { sink_ = sink; }
    bool empty() const { return pages_.empty(); }
    size_t size() const { return pages_.size(); }
    SettingsPage *top() { return pages_.empty() ? nullptr : pages_.back().get(); }
    bool locked() const;
    std::vector<ui::Hint> hints() const;
    bool update(float deltaSeconds);                          // the top page; true = redraw
    // True when the top page is a dialog over the panes (the shell then holds the toast back).
    bool dialogOpen() const;
    // Routes the event to the top page. Returns true when it was consumed (a locked page consumes every
    // event, used or not). Back that the page did not use pops it.
    bool handle(Event event);
    // True when the first-level panes are covered by a Content page.
    bool coversPanes() const;
    // Draws the open pages (bottom first; Content pages replace what is below them).
    // The tab draws its three panes first unless coversPanes().
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content);

private:
    enum class OpKind { Push, Pop };
    struct Op {
        OpKind kind;
        std::unique_ptr<SettingsPage> page;
    };
    void apply();
    void closeTop();

    std::vector<std::unique_ptr<SettingsPage>> pages_;
    std::vector<Op> pending_;
    bool inCall_ = false;      // a page is running: operations are queued
    Notice *sink_ = nullptr;   // the shell's toast (Services::notice)
};

}  // namespace patsgui
