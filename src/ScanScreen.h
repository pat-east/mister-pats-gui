#pragma once

#include <functional>
#include <string>
#include <vector>

#include "LibraryScan.h"
#include "MediaScraper.h"
#include "Screen.h"

// Guides building the game database, and shows its progress.
//
// It is the same screen in both situations on purpose: the first run, where nothing exists
// yet and the user needs to be told what is about to happen, and a later rebuild from the
// settings. Only the wording differs.
class ScanScreen : public Screen {
public:
    ScanScreen(Context &context, LibraryScan &scan, MediaScraper &scraper,
               std::function<void()> onFinished, std::function<void()> onClose,
               std::function<void()> onOnboardingComplete);

    // `firstRun` picks the wording for someone who has no database yet.
    void reset(bool firstRun);

    // Opens straight at the artwork step, for the settings entry.
    void resetForArtwork();

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    enum class Page { Welcome, JumpStart, Intro, Working, ArtworkOffer, ArtworkWorking,
                      Overview, Done };

    void renderWelcome(Canvas &canvas, const Rect &panel);
    void renderJumpStart(Canvas &canvas, const Rect &panel);
    void renderIntro(Canvas &canvas, const Rect &panel);
    void renderWorking(Canvas &canvas, const Rect &panel);
    void renderJumpStartWorking(Canvas &canvas, const Rect &panel);
    void renderArtworkOffer(Canvas &canvas, const Rect &panel);
    void renderArtworkWorking(Canvas &canvas, const Rect &panel);
    void renderOverview(Canvas &canvas, const Rect &panel);
    void renderDone(Canvas &canvas, const Rect &panel);
    void drawParagraph(Canvas &canvas, const Rect &area, const std::vector<std::string> &lines,
                       int size, Color color, int &y) const;

    Context &context_;
    LibraryScan &scan_;
    MediaScraper &scraper_;
    std::function<void()> onFinished_;
    std::function<void()> onClose_;
    std::function<void()> onOnboardingComplete_;

    Page page_ = Page::Intro;
    bool firstRun_ = true;
    bool jumpStartFlow_ = false;
    bool jumpStartNoGames_ = false;
    bool reported_ = false;          // reloaded the library once the scan finished
    // Whether a library scan actually ran during *this* visit to the wizard, as opposed to
    // an artwork-only visit that never touches `scan_` at all. The Done page used to read
    // `scan_.state()` unconditionally, so fetching box art on its own showed "Scan stopped"
    // — a scan's leftover state (or its absence) from earlier in the session, not anything
    // that happened just now.
    bool scanRan_ = false;
    size_t progressScroll_ = 0;
    size_t visibleProgressRows_ = 10;
    std::vector<std::string> roots_;
};
