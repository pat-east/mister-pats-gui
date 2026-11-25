#include "app/settings/WizardFlowPage.h"

namespace patsgui {

bool toWizardEvent(Event event, ui::WizardEvent &out) {
    switch (event) {
        case Event::Up:         out = ui::WizardEvent::Up; return true;
        case Event::Down:       out = ui::WizardEvent::Down; return true;
        case Event::Left:       out = ui::WizardEvent::Left; return true;
        case Event::Right:      out = ui::WizardEvent::Right; return true;
        case Event::Confirm:    out = ui::WizardEvent::Confirm; return true;
        case Event::Back:       out = ui::WizardEvent::Back; return true;
        case Event::X:          out = ui::WizardEvent::X; return true;
        case Event::Y:          out = ui::WizardEvent::Y; return true;
        case Event::Start:      out = ui::WizardEvent::Start; return true;
        case Event::TabPrev:    out = ui::WizardEvent::ShoulderPrev; return true;
        case Event::TabNext:    out = ui::WizardEvent::ShoulderNext; return true;
        case Event::LetterPrev: out = ui::WizardEvent::TriggerPrev; return true;
        case Event::LetterNext: out = ui::WizardEvent::TriggerNext; return true;
        default: return false;
    }
}

void WizardFlowPage::launch(PageStack &stack, std::unique_ptr<WizardFlowPage> flow, ui::PageId first) {
    if (!flow) return;
    flow->stack_ = &stack;
    flow->wizard_.open(first);
    flow->syncRawInput();
    stack.push(std::move(flow));
}

void WizardFlowPage::handOff(std::unique_ptr<SettingsPage> next) {
    if (!stack_ || handedOff_) return;
    handedOff_ = true;
    wizard_.close();
    syncRawInput();
    stack_->replace(std::move(next));
}

void WizardFlowPage::syncRawInput() {
    const bool wanted = wizard_.wantsRawInput();
    if (wanted == rawWanted_) return;
    rawWanted_ = wanted;
    if (onRawInput) onRawInput(wanted);
}

void WizardFlowPage::finishIfEnded(PageStack &stack) {
    if (!wizard_.active() && !handedOff_) {
        handedOff_ = true;   // the pop is queued; do not queue another
        stack.pop();
    }
}

bool WizardFlowPage::update(float deltaSeconds, PageStack &stack) {
    stack_ = &stack;
    const bool changed = wizard_.update(deltaSeconds);
    syncRawInput();
    finishIfEnded(stack);
    return changed;
}

void WizardFlowPage::draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &) {
    wizard_.draw(p, t, p.bounds());
}

bool WizardFlowPage::handle(Event event, PageStack &stack) {
    stack_ = &stack;
    ui::WizardEvent we;
    if (toWizardEvent(event, we)) wizard_.handle(we);   // a locked page takes the rest silently
    syncRawInput();
    finishIfEnded(stack);
    return true;
}

}  // namespace patsgui
