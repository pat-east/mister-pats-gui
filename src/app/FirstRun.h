#pragma once

// The first start: Welcome, Jump Start and the overview (Classic: ScanScreen pages Welcome, JumpStart, the jump-start variants of
// Working and ArtworkWorking, Overview, and the failed Done page; started by App::init through openScan(true)). It is a wizard flow
// (ui::Wizard through WizardFlowPage) of ONE Medium frame, hosted by the shell and not by the Settings tab: while it is open it is
// drawn over the shell, takes every event, shows its own hints and locks the tabs; nothing underneath reacts (patsgui::handle /
// update / drawTab / shellFor / drawOverlays ask FirstRun::active()). The host starts it with startFirstRun(state) when the guide
// is not complete (src/legacy/bridge/PatsGuiScreen::startFirstRun, called by App::init); it ends itself.
//
//   Welcome -A-> Jump Start offer -A/Start-> [Assets (Busy): font, then icons; only when missing; B = Cancel] -> Working (scan) -(games)-> Covers (Busy) -(ended)-> Overview -A/B-> done
//      B = Skip       B = Skip                  B = Cancel   \-(none)-> Overview (no games)   B = Cancel
//   Working -(failed or stopped)-> Failed -A-> Working again | B -> done
//
// "done" marks the guide complete (OnboardingService::setComplete) and closes the flow; Skip on Welcome or the offer and Close on the
// failed page do the same (Classic: onOnboardingComplete_() + onClose_()). When the GUI closes in the middle (FirstRun::close) the guide
// stays incomplete (Classic marked it started, never complete) and a running scan or cover job is stopped. The working pages are the
// ones of "Build game list" and "Prepare box art" (src/app/settings/library), the jump-start quick scan is LibraryService::startJumpStart.

#include <memory>
#include <string>
#include <vector>

#include "app/Tab.h"
#include "app/settings/SettingsPage.h"

namespace patsgui {

class LibraryService;   // src/app/settings/LibraryService.h

class FirstRun {
public:
    FirstRun() = default;
    FirstRun(FirstRun &&) = default;
    FirstRun &operator=(FirstRun &&other) noexcept;
    ~FirstRun() { close(); }

    // Opens the flow at Welcome. False (nothing opens) when the host gives no Library or Onboarding service.
    bool start(Services &services);
    bool active() const { return !stack_.empty(); }

    // ---- The shell calls these while active() (and update() also to notice that it ended).
    bool update(float deltaSeconds);                 // steps the Library service, then the page; true = redraw
    bool handle(Event event);                        // every event is the flow's
    std::vector<ui::Hint> hints() const { return stack_.hints(); }
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content);
    // The GUI closes: the flow ends without completing the guide, a running scan or cover job is stopped.
    void close();

    // ---- Host runner and tests (app-render --first-run PAGE, src/app/tests)
    static std::vector<std::string> pageIds();       // "welcome", "offer", "working", "covers", "failed", "stopped", "overview", ...
    // Opens the flow at a page of pageIds(); starts the service job the page shows (working, covers). False: unknown id or no services.
    bool startAt(Services &services, const std::string &pageId);
    PageStack &stack() { return stack_; }

    struct Data;   // the flow's data (FirstRun.cpp)

private:
    bool open(Services &services, int firstPage, const std::string &pageId);

    PageStack stack_;
    std::shared_ptr<Data> data_;
    LibraryService *library_ = nullptr;
};

// Registers every word of the flow (firstrun.*).
void registerFirstRunStrings(ui::Catalog &c);

}  // namespace patsgui
