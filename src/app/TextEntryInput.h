#pragma once

#include "app/Tab.h"
#include "ui-framework/kit/TextEntryDialog.h"

namespace patsgui {

// The one mapping from the shell's events to the text entry dialog (Classic's keyboard inputs): D-pad moves,
// A presses the key, X deletes, Y inserts a space, Start is Done, B cancels. Every other event is swallowed
// by the dialog (returns false): the keyboard locks the screen behind it.
inline bool mapTextEntryEvent(Event event, ui::TextEntryEvent &out) {
    switch (event) {
        case Event::Up:      out = ui::TextEntryEvent::Up; return true;
        case Event::Down:    out = ui::TextEntryEvent::Down; return true;
        case Event::Left:    out = ui::TextEntryEvent::Left; return true;
        case Event::Right:   out = ui::TextEntryEvent::Right; return true;
        case Event::Confirm: out = ui::TextEntryEvent::Press; return true;
        case Event::X:       out = ui::TextEntryEvent::Delete; return true;
        case Event::Y:       out = ui::TextEntryEvent::Space; return true;
        case Event::Start:   out = ui::TextEntryEvent::Done; return true;
        case Event::Back:    out = ui::TextEntryEvent::Cancel; return true;
        default: return false;
    }
}

}  // namespace patsgui
