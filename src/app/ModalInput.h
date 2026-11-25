#pragma once

#include "app/Tab.h"
#include "ui-framework/kit/Modal.h"

namespace patsgui {

// The one mapping from the shell's events to a modal dialog (ui::Modal / ui::ModalStack): the D-pad moves,
// A confirms, B backs out. Every other event returns false: a modal swallows it, tab switching included.
// Every tab and view that holds a ModalStack uses this, never its own switch. `startConfirms`: Start acts like A
// (the Settings pages, as in Classic's Settings dialogs).
inline bool toModalEvent(Event event, ui::ModalEvent &out, bool startConfirms = false) {
    switch (event) {
        case Event::Up:      out = ui::ModalEvent::Up; return true;
        case Event::Down:    out = ui::ModalEvent::Down; return true;
        case Event::Left:    out = ui::ModalEvent::Left; return true;
        case Event::Right:   out = ui::ModalEvent::Right; return true;
        case Event::Confirm: out = ui::ModalEvent::Confirm; return true;
        case Event::Back:    out = ui::ModalEvent::Back; return true;
        case Event::Start:
            if (!startConfirms) return false;
            out = ui::ModalEvent::Confirm;
            return true;
        default: return false;
    }
}

}  // namespace patsgui
