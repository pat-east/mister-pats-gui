#pragma once

#include <string>

#include "Favorites.h"
#include "HiddenSystems.h"
#include "History.h"
#include "Icons.h"
#include "ImageCache.h"
#include "Input.h"
#include "Launcher.h"
#include "Library.h"
#include "Preferences.h"
#include "Theme.h"

class Canvas;

// Shared services handed to every screen.
struct Context {
    Context(Theme &t, Library &l, Favorites &f, Launcher &la, ImageCache &i, Icons &ic,
            History &h, HiddenSystems &hs, Preferences &p)
        : theme(t), library(l), favorites(f), launcher(la), images(i), icons(ic), history(h),
          hiddenSystems(hs), preferences(p) {}

    Theme &theme;
    Library &library;
    Favorites &favorites;
    Launcher &launcher;
    ImageCache &images;
    Icons &icons;
    History &history;
    HiddenSystems &hiddenSystems;
    Preferences &preferences;

    // Prepared background, for restoring the area under a changed element.
    const Canvas *background = nullptr;

    std::string message;      // transient status line shown in the bottom bar
    float messageTimer = 0.0f;

    // Set after starting a game: the core needs the inputs and the screen.
    bool standDown = false;

    void notify(const std::string &text, float seconds = 3.5f) {
        message = text;
        messageTimer = seconds;
    }
};

class Screen {
public:
    virtual ~Screen() = default;

    virtual void update(float deltaSeconds) = 0;

    // `fullRedraw` is true when the caller has already wiped the whole area back to the
    // background this frame — a tab switch, returning from a detail view, anything that
    // invalidates whatever a screen thinks is still on screen. A screen that skips its own
    // repaint when nothing it tracks has changed must still repaint when this is true, or it
    // leaves the canvas showing whatever the wipe left behind: nothing.
    virtual void render(Canvas &canvas, const Rect &area, bool fullRedraw) = 0;

    virtual void handle(Action action) = 0;

    // Button legend for the bottom bar.
    virtual std::string hints() const = 0;

    // True when the screen restores the background under whatever it redraws.
    // Screens that return false get their whole area reset before rendering.
    virtual bool incremental() const { return false; }
};
