#pragma once

#include <cstdint>
#include <string>

#include "Library.h"
#include "Preferences.h"
#include "ScreenScraperService.h"
#include "VideoPlayer.h"

// The video preview of the game selected in a List view: the countdown, the lookup and download
// (through ScreenScraperService), the playback (through VideoPlayer) and the one line of status
// text that tells the user which step it is on. It is a state machine driven by two calls per
// frame; it never blocks, and every change of the selected game cancels all of it.
//
//   GamesScreen::update()  -> focus(system, game)   while a List view shows a game
//   App, once per frame    -> update(dt, suspended) right after the active screen's update
//
// Nothing happens unless the setting is on, the account check succeeded and the system is one
// ScreenScraper knows. A game whose stored answer is "no video" shows that at once, with no
// countdown and no request.
class GamePreview {
public:
    GamePreview(ScreenScraperService &service, VideoPlayer &player, Preferences &preferences);

    // The size available for the video (design pixels scaled to the screen). A video larger than
    // this is not played.
    void setSlotSize(int width, int height) { slotWidth_ = width; slotHeight_ = height; }

    // The selected game in a List view; call every frame the view is showing it.
    void focus(const GameSystem *system, const Game *game);
    // Once per frame, after the screens' update. `suspended`: something else owns the screen.
    void update(float deltaSeconds, bool suspended);
    // Stops playback and any lookup (leaving the view, starting a game).
    void stop();

    // The line to show under the video slot; empty when there is nothing to say.
    std::string statusText() const { return text_; }
    // Changes whenever the status text or whether a video is showing changes, so the screen
    // knows to repaint its panel.
    int revision() const { return revision_; }

    bool videoPlaying() const { return mode_ == Mode::Playing; }
    int videoWidth() const { return player_.width(); }
    int videoHeight() const { return player_.height(); }
    const uint32_t *acquireFrame() { return player_.acquire(); }
    void releaseFrame() { player_.release(); }

    // For the settings page: the account state and the stored results.
    ScreenScraperService &service() { return service_; }

    // True when a preview applies to the game last passed to focus() (even if it only says "no
    // video found"). The screen reserves a line for the status text only then.
    bool engaged() const { return mode_ != Mode::Off; }

private:
    enum class Mode {
        Off,         // no preview for this game
        Note,         // the feature is on but not applicable now; the line says why
        Countdown,   // waiting for the delay
        Known,       // stored answer: no video
        Working,     // looking up / downloading / verifying
        Opening,     // the video file exists; the player is opening it
        Playing,
        NotFound,    // just looked up: no video
        Failed,      // the lookup or download failed
        CannotPlay,  // the video exists but cannot be played here
        Done,        // played to the end
    };

    void begin(const GameSystem *system, const Game *game);
    void startNow();
    void setMode(Mode mode, const std::string &text);
    void setText(const std::string &text);
    // Whether a preview applies: not at all (the feature is off: silent), not for this game (the
    // status line says why: unsupported system, account not usable yet), or yes.
    enum class Gate { No, Note, Yes };
    Gate gate(const GameSystem *system, const Game *game, std::string &note);
    void deactivate();

    ScreenScraperService &service_;
    VideoPlayer &player_;
    Preferences &preferences_;

    Mode mode_ = Mode::Off;
    std::string text_;
    std::string key_;            // identifies the game this state belongs to
    std::string systemKey_, titleKey_, searchTitle_, title_;
    int systemId_ = 0;
    float remaining_ = 0.0f;
    unsigned requestId_ = 0;
    bool focusedThisFrame_ = false;
    int revision_ = 0;
    int slotWidth_ = 689, slotHeight_ = 624;
};
