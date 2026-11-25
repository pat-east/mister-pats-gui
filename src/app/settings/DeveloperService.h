#pragma once

// What the Developer category needs from the host. Classic: App::openWireframes() (SettingsScreen's onOpenWireframes).
// Owned by the Developer agent. Classic's second Developer row, "MiSTer Pats GUI (UI-Framework)" (App::openPatsGui()),
// opens this GUI from Classic and does not exist inside the new GUI, so the interface has no method for it.

namespace patsgui {

class DeveloperService {
public:
    virtual ~DeveloperService() = default;
    virtual bool available() const = 0;
    // Classic has no build flag: the category is always shown (SettingsScreen::buildRows). The host answers true; the
    // owner's decision D44 (hidden developer tool or removed in 0.9.0) may change it.
    virtual bool developmentBuild() const = 0;
    // Opens the framework demo (the Wireframes) over the new GUI. Its Back returns to the new GUI.
    virtual void openWireframes() = 0;
};

}  // namespace patsgui
