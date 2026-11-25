#pragma once

// What the Accessibility category needs from the host (Classic: Preferences + Theme setters + Context::onThemeChanged,
// App::activateSpaceGrotesk()/spaceGroteskStatus(), SpaceGroteskDownload). Owned and extended by the Accessibility
// agent. Every setter writes the preference and the Classic theme and asks the Classic App to redraw; the new GUI's
// look (framework theme and fonts) follows from the same preferences through src/app/Look.h, so nothing here touches it.

#include <string>

namespace patsgui {

// The state of App's one SpaceGroteskDownload (read-only; the first-start Jump Start waits for it).
struct FontDownloadInfo {
    bool running = false;                                // SpaceGroteskDownload::running()
    bool failed = false;                                 // its state is Failed
    std::string error;                                   // SpaceGroteskDownload::error() when failed
};

class AccessibilityService {
public:
    virtual ~AccessibilityService() = default;
    virtual bool available() const = 0;
    virtual void update(float deltaSeconds) = 0;

    // ---- Space Grotesk font (Classic: App::spaceGroteskStatus() / activateSpaceGrotesk())
    virtual bool fontInstalled() const = 0;              // Theme::spaceGroteskAvailable()
    virtual bool fontEnabled() const = 0;                // Preferences::useSpaceGrotesk()
    virtual std::string fontStatus() const = 0;          // the row's value text ("On", "Off", "Download font", progress, "Try font download again")
    virtual FontDownloadInfo fontDownload() const { return FontDownloadInfo(); }   // live (not cached) state of the download
    virtual void activateFont() = 0;                     // toggle, or start the download when the files are missing
    // The messages Classic shows with Context::notify (text and seconds exactly as App::activateSpaceGrotesk() and the
    // download's completion say them). The device adapter returns none: the shell turns Context::notify into a toast
    // (PatsGuiScreen). A host without that bridge (the host runner's sample) returns them and the category toasts them.
    // True while one is pending; call until false.
    virtual bool takeNotice(std::string &text, float &seconds) { (void)text; (void)seconds; return false; }

    // ---- Sizes (percent: 100, 115, 130)
    virtual int fontSizeScale() const = 0;
    virtual void setFontSizeScale(int percent) = 0;
    virtual int hintSizeScale() const = 0;
    virtual void setHintSizeScale(int percent) = 0;

    // ---- High contrast
    virtual bool highContrast() const = 0;
    virtual void setHighContrast(bool on) = 0;
    virtual int highContrastColor() const = 0;
    virtual void setHighContrastColor(int index) = 0;
};

}  // namespace patsgui
