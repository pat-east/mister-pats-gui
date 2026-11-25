#pragma once

// The one place a page says "this happened" in a few words: "Mega Man added to favorites", "Games starting with S",
// "Time zone set to Berlin". The shell shows it as a toast (ui::Toast) above the bottom bar, the same on every tab.
// A page reaches it through `Services::notice` and never draws a message itself.
//
//   notice(services_.notice, ui::Text::data(ui::format("note.favorite.added", {title})));
//
// Rules (src/ui-framework/docs/components/Toast.md, ACCESSIBILITY.md 2.4): only a confirmation of something the screen does not
// show by itself, or one line explaining a blocked action; never progress, never a failure (that is an Error
// dialog), never the only copy of something the user must read or act on. One notice at a time; a newer one
// replaces the showing one. The notice never takes focus and is not shown over a dialog.

#include <string>

#include "ui-framework/kit/Text.h"
#include "ui-framework/kit/Tokens.h"

namespace patsgui {

class Notice {
public:
    virtual ~Notice() = default;
    // Tone::Done (accent, check icon) for a change that happened; Tone::Neutral for one line of explanation.
    // `minSeconds` (0 = the shell's policy) keeps a line up at least that long (Classic's notify() seconds).
    virtual void toast(const ui::Text &text, ui::Tone tone = ui::Tone::Done, float minSeconds = 0.0f) = 0;
};

// Null-safe call for pages that may run without a shell (a test, a host that has none).
inline void notice(Notice *sink, const ui::Text &text, ui::Tone tone = ui::Tone::Done, float minSeconds = 0.0f) {
    if (sink) sink->toast(text, tone, minSeconds);
}

// The shared messages: one wording on every tab (the keys are in src/app/Strings.cpp).
void noteFavorite(Notice *sink, const std::string &title, bool nowFavorite);   // "{1} added to favorites" / "... removed from favorites"
void noteRemovedRecent(Notice *sink, const std::string &title);                // "{1} removed from Recently played"

}  // namespace patsgui
