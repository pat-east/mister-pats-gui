#pragma once

// DetailView: the one detail layout of the application. A picture on the left (the box art, or the video
// preview in its place) with an optional status line under it; on the right the title, facts, an optional
// list of versions, the one action (the Start button) and a strip of screenshots. It draws inside any
// rectangle: the Disc tab shows it inline, the Game options show it inside a modal frame.
//
// The view holds no data. The caller fills a DetailSubject every time it draws (and when it handles an
// event) and the view draws and handles it: the focus (Start button <-> versions <-> screenshots), the
// selected version, and what each press means (Result). The caller acts on the result (start, open the
// viewer) and owns everything with a consequence.
//
// Screenshots follow one rule everywhere: only the thumbnails that exist are drawn (never empty frames for
// slots beyond the found count); a thumbnail whose picture has not arrived is a calm empty tile; while the
// lookup runs the strip says "Loading screenshots..."; when it found none, "No screenshots available".

#include <functional>
#include <string>
#include <vector>

#include "app/Library.h"
#include "app/Tab.h"
#include "ui-framework/kit/List.h"

namespace patsgui {

struct DetailFact {
    ui::Text label;
    std::string value;           // data; a fact with an empty value is not shown
    ui::Text valueText;          // used instead of `value` when it is not empty (a catalog word)
};

enum class ShotsPhase { None, Searching, Ready, Missing };   // None: no strip at all

struct DetailSubject {
    // The picture.
    ui::ImagePtr picture;
    bool pictureIsVideo = false;         // drawn with square corners, like PreviewSlot::redrawVideo() repaints it
    bool pictureLoading = false;         // the placeholder says "loading" rather than "none"
    ui::Text pictureLoadingText, pictureNoneText;   // empty: the view's own words
    std::string pictureStatus;           // the preview's status line under the picture (data)

    // The text column.
    std::string title;
    std::vector<DetailFact> facts;
    std::string warning;                 // a technical reason in warning tone (data), e.g. a read error
    ui::Text overviewLabel;
    std::string overview;                // data; shown when there is room

    // Versions (empty: no list). The selected one says `selectedWord`.
    ui::Text versionsLabel, selectedWord;
    std::vector<std::string> versions;   // data

    // The action.
    ui::Text startLabel;
    bool startReady = true;              // false: drawn as unavailable (it still takes focus)

    // Screenshots. `shot(i, size)` returns thumbnail i (ArtSize::Grid) or the large picture.
    ShotsPhase shotsPhase = ShotsPhase::None;
    int shotCount = 0;                   // how many exist (Ready); at most kMaxShots are shown
    ui::Text shotsLabel;                 // empty: no label above the strip
    std::function<Art(int index, ArtSize size)> shot;
};

class DetailView {
public:
    static constexpr int kMaxShots = 4;

    enum class Action { None, Start, ShowShot, VersionChosen };
    struct Result {
        bool used = false;               // the view used the event (a move or a press)
        Action action = Action::None;
        int index = 0;                   // ShowShot: the thumbnail; VersionChosen: the version
    };

    void reset();                        // focus on the Start button, first version selected
    void setSelectedVersion(int index) { selected_ = index; }
    int selectedVersion() const { return selected_; }

    // Focus, for hints: 0 the button, 1.. a version row, kShotBase.. a screenshot.
    bool onButton() const { return cursor_ == 0; }
    bool onShot() const { return cursor_ >= kShotBase; }
    int shotFocus() const { return onShot() ? cursor_ - kShotBase : -1; }

    // Up/Down/Left/Right move the focus; Confirm presses what is focused. Other events are not used.
    Result handle(Event event, const DetailSubject &subject);

    // The generic hints for the focus: move, browse, and A: start / select version / view screenshot.
    std::vector<ui::Hint> hints(const DetailSubject &subject) const;

    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &area, const DetailSubject &subject);

    // The picture's rectangle as last drawn (real pixels): the slot a video must fit into.
    const ui::Rect &pictureRect() const { return picture_; }

    static void registerStrings(ui::Catalog &catalog);

private:
    static constexpr int kShotBase = 100;
    int shownShots(const DetailSubject &s) const;
    bool shotsFocusable(const DetailSubject &s) const { return s.shotsPhase == ShotsPhase::Ready && shownShots(s) > 0; }

    int cursor_ = 0;
    int selected_ = 0;
    ui::ListLayout list_;
    ui::Rect picture_;
};

}  // namespace patsgui
