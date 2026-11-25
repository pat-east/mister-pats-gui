#pragma once

// The rig of the flow tests: a SettingsTab on the SampleLibrary and the sample Settings services, input by key name,
// time that advances by hand, and questions about the top page (id, kind, title, hints). Everything a test needs to
// drive one flow like the controller would, without a screen. See src/app/tests/README.md.

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/PatsGui.h"
#include "app/SampleLibrary.h"
#include "app/SettingsTab.h"
#include "app/settings/SampleSettingsServices.h"
#include "app/settings/WizardFlowPage.h"
#include "ui-framework/render/soft/Canvas.h"
#include "ui-framework/render/soft/Font.h"
#include "ui-framework/render/soft/SoftPainter.h"

namespace tt {

using namespace patsgui;

// One hint as the bar would name it: the key and the English word ("A", "Start"). hold: "Hold X Favorite".
struct HintView {
    ui::HintKey key;
    std::string word;
    bool hold = false;
};

const char *keyName(ui::HintKey key);
// The names the tests (and `--keys` of app-render) use: up down left right a b x y start xhold bhold lprev lnext select tabprev tabnext.
bool eventNamed(const std::string &name, Event &out);

class Rig {
public:
    Rig();   // registers the strings once, shows the Settings tab

    // ---- The pieces
    SampleSettingsServices ss;
    SampleLibrary library;
    Services services;
    SettingsTab tab;
    SampleLibraryService &lib() { return ss.library; }
    SampleSystemService &sys() { return ss.system; }
    SampleUpdatesService &upd() { return ss.updates; }
    SampleControllersService &pads() { return ss.controllers; }
    PageStack &stack() { return firstRun_ ? state_->firstRun.stack() : tab.pages(); }
    SampleOnboardingService &onboarding() { return ss.onboarding; }

    // ---- Setup. `set` takes the `--set key=value` words of app-render (CHECK-fails on an unknown key).
    void set(const std::string &key, const std::string &value);
    // Opens a flow like a row's A does ("library", "library.build"); the stack size before is the flow's base.
    void open(const std::string &category, const std::string &rowId);
    // Opens a page by its `--list-pages` id ("library/scan.working").
    bool openPage(const std::string &categoryAndPage);
    // The first-start guide (src/app/FirstRun.h), hosted by the shell as in the app: from now on every key, wait and question goes
    // to the shell's State (patsgui::handle / update) and stack() is the guide's. onboardingStarted is not modelled (App sets it).
    void openFirstRun();
    // Same, at a page of FirstRun::pageIds() (the host runner's --first-run).
    bool openFirstRunAt(const std::string &pageId);
    patsgui::State *shell() { return state_.get(); }
    size_t base() const { return base_; }
    void setBase(size_t n) { base_ = n; }

    // ---- Hooks of the flow specific rules (Generic.h): called just before and after every key press and every frame of wait().
    std::function<void(Rig &, bool before)> watch;
    std::map<std::string, long> mem;   // what a watch remembers between calls

    // ---- Input. `keys("a,down,b")` sends the events in order; every press is followed by one short frame.
    void press(Event e);
    void keys(const std::string &list);
    // Lets time pass in 0.1 s frames (the sample services run their clocks from tab.update()).
    void wait(float seconds);
    // The first page: update(0) once, as the host's first frame.
    void frame() { tab.update(0.0f); }
    // Writes a short text into the failure context (shown with every failed CHECK).
    void note(const std::string &text);
    const std::string &trace() const { return trace_; }

    // ---- Questions about the open flow (the pages above the base). All safe when nothing is open.
    bool active() { return stack().size() > base_; }   // the flow page is on the stack
    size_t depth() { return stack().size(); }
    SettingsPage *top() { return stack().top(); }
    WizardFlowPage *flow();                                  // the top page as a wizard flow, or nullptr
    int id();                                                // the wizard page id, -9 when the top page is not a wizard flow
    ui::WizardPage *wpage();                                 // the current wizard page, or nullptr
    ui::WizardPage::Kind kind();                             // Step when there is no wizard page
    std::string title();
    std::vector<HintView> hints();                           // the hint bar (the wizard added B by kind)
    std::string hintLine();                                  // "UD Move | A Start | B Close"
    bool hasHint(ui::HintKey key);
    std::string hintWord(ui::HintKey key);                   // "" when the key is not listed
    ui::WizardResult *result();                              // the outcome of a Result page, nullptr on any other page
    bool cancellableBusy();                                  // a Busy page that B can stop
    bool rawInputWanted();                                   // what the wizard page asks the host for (flow()->wantsRawInput())
    // The service-side view of "the pad is captured": what setRawInputWanted() last received.
    bool padRawFlag() { return ss.controllers.rawInputWanted; }

    // ---- Service calls (SampleSystemService::calls). Count of the calls that start with `prefix`.
    int calls(const std::string &prefix);
    std::string callsText();

    // ---- Drawing: the whole tab onto a 1920x1080 canvas with the built-in face. The check is that nothing crashes.
    void draw();

    // The tab leaves (the GUI closes): every page is closed, their onClose runs.
    void guiCloses() { if (firstRun_) state_->firstRun.close(); else tab.onHide(); }

private:
    ui::Canvas canvas_;
    ui::Font regular_, bold_;
    std::unique_ptr<ui::SoftPainter> painter_;
    ui::Theme theme_;
    size_t base_ = 0;
    std::string trace_;
    std::unique_ptr<patsgui::State> state_;   // the shell, only while the first-start guide is the subject
    bool firstRun_ = false;
    patsgui::Assets assets_;
};

}  // namespace tt
