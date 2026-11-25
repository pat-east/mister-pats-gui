#pragma once

// WizardFlowPage: hosts a ui::Wizard (src/ui-framework/kit/Wizard.h) as one Settings page. A Settings flow with two or
// more steps (Wi-Fi, build the game list, core updates, ...) is this page: every step is a ui::WizardPage in the
// wireframes' dialog frame, the flow keeps Classic's state machine in its page code, and the page forwards input, hints,
// update and draw to the wizard. It is a Modal page and locks the tabs; it pops itself when the flow ends.
//
//   struct MyFlow { bool ran = false; };                                     // the flow's data, one struct
//   auto data = std::make_shared<MyFlow>();
//   std::unique_ptr<WizardFlowPage> flow(new WizardFlowPage(ui::WizardFrame::Medium));   // one size for the whole flow
//   WizardFlowPage *self = flow.get();                                       // stable: the stack keeps the page
//
//   enum { kIntro, kWork, kDone };
//   std::unique_ptr<ui::FnPage> intro(new ui::FnPage(ui::WizardPage::Kind::Step, ui::Text::key("settings.x.title")));
//   intro->onDraw = [](ui::Painter &p, const ui::Theme &t, const ui::Rect &body) {
//       ui::drawWizardParagraphs(p, t, body, {{ui::Text::key("settings.x.intro"), ui::TextTier::Primary, false}}); };
//   intro->onHints = []() { return std::vector<ui::Hint>{ui::hint(ui::HintKey::A, "hint.start", true)}; };
//   intro->onEvent = [](ui::WizardEvent e, ui::Wizard &w) { if (e == ui::WizardEvent::Confirm) w.go(kWork); };
//   flow->add(kIntro, std::move(intro));
//
//   std::unique_ptr<ui::BusyPage> work(new ui::BusyPage(ui::Text::key("settings.x.title")));
//   work->status = ui::Text::key("settings.x.working");
//   work->onUpdate = [svc, self](float, ui::Wizard &w) {                     // poll the service, never block
//       const Progress s = svc->progress();
//       if (s.finished) { self->toast(ui::Text::key("settings.x.saved")); w.go(kDone); }
//       return true; };
//   flow->add(kWork, std::move(work));
//   flow->add(kDone, makeResultPage(...));                                  // ui::ResultPage + ui::WizardResult
//   WizardFlowPage::launch(pages, std::move(flow), kIntro);                  // opens the first page and pushes the flow
//
// Callbacks reach the rest of the GUI through the flow: stack() (push/replace/pop other SettingsPages), toast(), handOff().

#include <functional>
#include <memory>

#include "app/settings/SettingsPage.h"
#include "ui-framework/kit/Wizard.h"
#include "ui-framework/kit/WizardParts.h"

namespace patsgui {

// The shell's events as the wizard's: the D-pad, A, B, X, Y, Start, the shoulders (TabPrev/TabNext) and the triggers
// (LetterPrev/LetterNext). Select, XHold and BHold have no meaning in a flow; returns false for them.
bool toWizardEvent(Event event, ui::WizardEvent &out);

class WizardFlowPage : public SettingsPage {
public:
    explicit WizardFlowPage(ui::WizardFrame frame = ui::WizardFrame::Medium) : wizard_(frame) {}

    // The flow's pages, by id (see ui::Wizard::add). Register them all before launch().
    template <class T>
    T *add(ui::PageId id, std::unique_ptr<T> page) { return wizard_.add(id, std::move(page)); }
    ui::Wizard &wizard() { return wizard_; }

    // Opens `first` (its enter() runs now, with stack() available) and pushes the flow. Call it from a row's activate()
    // or from another page's callback; both are safe (the push is applied after the current call).
    static void launch(PageStack &stack, std::unique_ptr<WizardFlowPage> flow, ui::PageId first);

    // ---- For the pages' callbacks.
    // The stack the flow lives on: push() / replace() other SettingsPages (a ConfirmPage, a keyboard page of a standard
    // page), pop() it, toast(). Valid from launch() on.
    PageStack &stack() { return *stack_; }
    // A confirmation or a notice in the shell's toast (not for failures: those are Result pages).
    void toast(const ui::Text &text, float seconds = 3.5f) { if (stack_) stack_->toast(text, seconds); }
    void toast(const std::string &data, float seconds = 3.5f) { if (stack_) stack_->toast(data, seconds); }
    // The flow ends (leave(false) on the current page) and `next` takes its place (Bluetooth found a pad: the mapping
    // wizard opens). Without this the flow ends with w.close() / Back from the first page and the page pops itself.
    void handOff(std::unique_ptr<SettingsPage> next);
    // True while a Capture page wants the pad's raw input (the host stops translating pad input to events).
    bool wantsRawInput() const { return wizard_.wantsRawInput(); }
    // Called with true when a Capture page that reads the pad itself becomes current and with false when it is left, the flow
    // ends or the GUI closes (the Controllers flows forward it to ControllersService::setRawInputWanted -> SettingsHost::rawInputWanted).
    std::function<void(bool wanted)> onRawInput;

    // ---- SettingsPage
    PagePresentation presentation() const override { return PagePresentation::Modal; }
    bool update(float deltaSeconds, PageStack &stack) override;
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event, PageStack &stack) override;
    std::vector<ui::Hint> hints() const override { return wizard_.hints(); }
    bool locked() const override { return true; }
    // The GUI closed or the Settings tab was left while the flow ran: leave(true) on the current page (undo side effects,
    // wipe secrets). A flow that ended normally has nothing left to do.
    void onClose() override { wizard_.abort(); syncRawInput(); }

private:
    void finishIfEnded(PageStack &stack);
    void syncRawInput();

    ui::Wizard wizard_;
    PageStack *stack_ = nullptr;
    bool handedOff_ = false;
    bool rawWanted_ = false;
};

}  // namespace patsgui
