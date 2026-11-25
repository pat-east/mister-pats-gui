// The Disc tab: the optical drive and the disc in it, on the framework.
//
// One calm focus. Two kinds of page:
//   - a notice (no drive, no disc, reading, unsupported, unreadable): why, and what to do next;
//   - the disc page (readable): the artwork on the left; title, facts, overview, the one action and the
//     screenshots on the right. Focus is the Start button or one screenshot; A on a screenshot opens the
//     viewer (the framework's Modal::viewer).
// Everything comes through Library::disc() / discArt() / discScreenshot(); nothing blocks in draw.

#include <algorithm>
#include <string>
#include <vector>

#include "app/DetailView.h"
#include "app/ModalInput.h"
#include "app/Tab.h"
#include "ui-framework/kit/Components.h"
#include "ui-framework/kit/Data.h"
#include "ui-framework/kit/DetailPanel.h"
#include "ui-framework/kit/EmptyPage.h"
#include "ui-framework/kit/Input.h"
#include "ui-framework/kit/Modal.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/Primitives.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {
namespace {

using Media = DiscStatus::Media;
using Console = DiscStatus::Console;

constexpr int kMaxShots = 4;   // the strip shows the first four, as the Classic screen does

// ---- The tab

class DiscTab : public TabView {
public:
    explicit DiscTab(Services &s) : library_(s.library) {}

    // While the tab is the active one it keeps asking for the disc, so the library keeps watching the drive
    // and the artwork lookup, and a change (a disc inserted, artwork found) is drawn.
    bool update(float) override {
        if (shown_) library_.disc();
        return false;
    }
    void onHide() override { shown_ = false; }

    void onShow() override {
        shown_ = true;
        view_.reset();
        modals_ = ui::ModalStack();
        viewerOpen_ = false;
        infoOpen_ = false;
        sync(library_.disc());
    }

    bool handle(Event e) override {
        const DiscStatus disc = sync(library_.disc());
        if (viewerOpen_ || infoOpen_) return handleViewer(e);
        // Leaving the tab (or the GUI) abandons a start that is still re-checking, as the Classic screen does.
        if (disc.launching && (e == Event::TabPrev || e == Event::TabNext || e == Event::Back))
            library_.cancelDiscLaunch();
        if (disc.media == Media::Readable) {
            if (e == Event::Confirm || e == Event::Up || e == Event::Down || e == Event::Left || e == Event::Right) {
                const DetailView::Result r = view_.handle(e, subject(disc));
                if (r.action == DetailView::Action::Start) {
                    if (!disc.canStart() || disc.mglMissing) openWhy(disc);
                    else library_.startDisc();
                } else if (r.action == DetailView::Action::ShowShot) {
                    openViewer(disc, r.index);
                }
                return r.used;
            }
        } else if (e == Event::Confirm) {
            return false;
        }
        switch (e) {
            case Event::X:
                if (disc.launching) return false;
                library_.rescanDisc();
                return true;
            case Event::Y:
                if (disc.media != Media::Readable || disc.console == Console::Unknown) return false;
                library_.reloadDiscArt();
                return true;
            default: return false;
        }
    }

    bool locked() const override { return !modals_.empty(); }
    std::vector<ui::Hint> hints() const override {
        if (viewerOpen_ || infoOpen_) return modals_.hints();
        const DiscStatus disc = library_.disc();
        std::vector<ui::Hint> h;
        const int shots = shotCount(disc);
        if (disc.media == Media::Readable && shots > 0)
            h.push_back(ui::hint(ui::HintKey::UpDown, "hint.move"));
        if (view_.onShot() && shots > 1) h.push_back(ui::hint(ui::HintKey::LeftRight, "hint.choose"));
        if (!disc.launching) h.push_back(ui::hint(ui::HintKey::X, "disc.hint.check"));
        if (disc.media == Media::Readable && disc.console != Console::Unknown)
            h.push_back(ui::hint(ui::HintKey::Y, "disc.hint.reloadArt"));
        if (disc.media == Media::Readable && view_.onShot())
            h.push_back(ui::hint(ui::HintKey::A, "hint.viewShot", true));
        else if (disc.canStart() && !disc.mglMissing)
            h.push_back(ui::hint(ui::HintKey::A, "hint.startGame", true));
        else if (disc.media == Media::Readable)
            h.push_back(ui::hint(ui::HintKey::A, "disc.hint.why", true));
        return h;
    }

    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &area) override {
        const DiscStatus disc = sync(library_.disc());
        driveModel_ = disc.model;
        using namespace ui;
        PageHeaderSpec header;          // the page header, as on every tab: the title only
        header.title = Text::key("tab.disc");
        const Rect content = drawPageHeader(p, t, area, header);
        switch (disc.media) {
            case Media::NoDrive: {
                ui::EmptyPageSpec n;
                n.title = Text::key("disc.nodrive.title");
                n.why = Text::key("disc.nodrive.why");
                n.next = Text::key("disc.nodrive.next");
                drawCard(p, t, content, n);
                break;
            }
            case Media::NoDisc: {
                ui::EmptyPageSpec n;
                n.title = Text::key("disc.nodisc.title");
                n.why = Text::key("disc.nodisc.why");
                n.next = Text::key("disc.nodisc.next");
                drawCard(p, t, content, n);
                break;
            }
            case Media::Checking: {
                ui::EmptyPageSpec n;
                n.tone = Tone::Neutral;
                n.icon = IconId::Clock;
                n.title = Text::key("disc.checking.title");
                n.why = Text::key("disc.checking.why");
                drawCard(p, t, content, n);
                break;
            }
            case Media::Unsupported: {
                ui::EmptyPageSpec n;
                n.tone = Tone::Warning;
                n.icon = IconId::Alert;
                n.title = Text::key("disc.unsupported.title");
                n.why = Text::key("disc.unsupported.why");
                n.next = Text::key("disc.unsupported.next");
                drawCard(p, t, content, n);
                break;
            }
            case Media::Unreadable: {
                ui::EmptyPageSpec n;
                n.tone = Tone::Error;
                n.icon = IconId::Cross;
                n.title = Text::key("disc.unreadable.title");
                n.why = Text::key("disc.unreadable.why");
                n.next = Text::key("disc.unreadable.next");
                n.details = disc.error;
                drawCard(p, t, content, n);
                break;
            }
            case Media::Readable:
                drawDisc(p, t, content, disc);
                break;
        }
        if (viewerOpen_ || infoOpen_) {
            if (viewerOpen_) refreshViewer(disc);
            modals_.draw(p, t, p.bounds());
        }
    }

private:
    static int shotCount(const DiscStatus &d) { return std::max(0, std::min(d.screenshotCount, kMaxShots)); }

    // Reads the disc and resets the transient state when another disc was seen.
    DiscStatus sync(const DiscStatus &disc) {
        if (!haveGeneration_ || generation_ != disc.generation) {
            haveGeneration_ = true;
            generation_ = disc.generation;
            view_.reset();
            modals_ = ui::ModalStack();
            viewerOpen_ = false;
            infoOpen_ = false;
        }
        if (disc.media != Media::Readable) view_.reset();
        return disc;
    }

    void drawCard(ui::Painter &p, const ui::Theme &t, const ui::Rect &content, ui::EmptyPageSpec n) {
        if (!driveModel_.empty()) n.extra = ui::Text::key("disc.field.drive").str() + ": " + driveModel_;
        ui::drawEmptyPage(p, t, content, n);
    }

    // ---- the viewer
    bool handleViewer(Event e) {
        ui::ModalEvent me;
        if (!toModalEvent(e, me)) return true;   // tab switches and the other buttons wait while the viewer is open
        const ui::ModalResult r = modals_.handle(me);
        if (r == ui::ModalResult::Closed || r == ui::ModalResult::Cancelled || modals_.empty()) {
            viewerOpen_ = false;
            infoOpen_ = false;
            modals_ = ui::ModalStack();
        }
        return true;
    }

    // A on an unavailable Start button: the Classic screen showed a toast; here a dialog says why and what to do.
    void openWhy(const DiscStatus &disc) {
        const char *body = "disc.why.core";
        if (disc.packageBusy) body = "disc.why.busy";
        else if (disc.launching) body = "disc.why.launching";
        else if (disc.blocker == DiscStatus::Blocker::CoreMissing) body = "disc.why.core";
        else if (disc.mglMissing) body = "disc.why.mgl";
        else body = "disc.why.package";
        modals_ = ui::ModalStack();
        modals_.push(ui::Modal::info(ui::Text::key("disc.why.title"), ui::Text::key(body)));
        infoOpen_ = true;
    }

    // The pictures the viewer can show right now: the large size when it has arrived, the tile size otherwise.
    std::vector<ui::ImagePtr> viewerImages(const DiscStatus &disc, bool &complete) {
        std::vector<ui::ImagePtr> images;
        complete = true;
        for (int i = 0; i < shotCount(disc); ++i) {
            Art a = library_.discScreenshot(i, ArtSize::Detail);
            if (!a.image) {
                complete = false;
                a = library_.discScreenshot(i, ArtSize::Grid);
            }
            if (a.image) images.push_back(a.image);
        }
        return images;
    }

    void openViewer(const DiscStatus &disc, int index) {
        bool complete = false;
        std::vector<ui::ImagePtr> images = viewerImages(disc, complete);
        if (images.empty()) return;
        modals_ = ui::ModalStack();
        modals_.push(ui::Modal::viewer(images));
        for (int i = 0; i < index && i + 1 < int(images.size()); ++i) modals_.handle(ui::ModalEvent::Right);
        viewerOpen_ = true;
        viewerComplete_ = complete;
        viewerCount_ = int(images.size());
    }

    // While large pictures are still arriving the viewer is rebuilt in place, keeping the position.
    void refreshViewer(const DiscStatus &disc) {
        if (viewerComplete_) return;
        bool complete = false;
        std::vector<ui::ImagePtr> images = viewerImages(disc, complete);
        if (images.empty() || int(images.size()) < viewerCount_) return;
        const int at = modals_.topSelected();
        modals_.replaceTop(ui::Modal::viewer(images));
        for (int i = 0; i < at && i + 1 < int(images.size()); ++i) modals_.handle(ui::ModalEvent::Right);
        viewerComplete_ = complete;
    }

    // ---- the disc page
    std::string titleOf(const DiscStatus &disc) const {
        if (!disc.title.empty()) return disc.title;
        switch (disc.console) {
            case Console::PlayStation: return ui::Text::key("disc.title.psx").str();
            case Console::Saturn: return ui::Text::key("disc.title.saturn").str();
            default: return ui::Text::key("disc.title.generic").str();
        }
    }

    const char *startLabelKey(const DiscStatus &disc) const {
        if (disc.packageBusy) return "disc.button.packageBusy";
        if (disc.launching) return "disc.button.starting";
        if (!disc.coreReady)
            return disc.blocker == DiscStatus::Blocker::CoreMissing ? "disc.button.noCore" : "disc.button.noPackage";
        return "hint.startGame";
    }

    // What the shared DetailView shows for the disc in the drive.
    DetailSubject subject(const DiscStatus &disc) {
        DetailSubject s;
        const Art art = library_.discArt(ArtSize::Detail);
        s.picture = art.image;
        // "Missing" is final: the service looked and found nothing, so there is no 'loading' however the
        // image cache feels about a path that never existed. While searching (or before the first request)
        // the picture is on its way; once found, it loads from disk.
        s.pictureLoading = !art.image && disc.artState != DiscStatus::ArtState::Missing &&
                           (art.loading || disc.artState == DiscStatus::ArtState::Searching ||
                            disc.artState == DiscStatus::ArtState::Idle);
        s.pictureLoadingText = ui::Text::key("picture.loading");
        s.pictureNoneText = ui::Text::key("picture.none");
        s.title = titleOf(disc);
        auto add = [&](const char *labelKey, const std::string &value) {
            DetailFact f;
            f.label = ui::Text::key(labelKey);
            f.value = value;
            s.facts.push_back(f);
        };
        if (disc.console == Console::PlayStation || disc.console == Console::Saturn) {
            DetailFact f;
            f.label = ui::Text::key("disc.field.system");
            f.valueText = ui::Text::key(disc.console == Console::PlayStation ? "disc.console.psx" : "disc.console.saturn");
            s.facts.push_back(f);
        }
        add("disc.field.year", disc.year);
        add("disc.field.region", disc.region);
        add("disc.field.id", disc.identifier);
        add("disc.field.players", disc.players);
        add("disc.field.coop", disc.coop);
        add("disc.field.rating", disc.rating);
        add("disc.field.drive", disc.model);
        s.warning = disc.error;
        s.overviewLabel = ui::Text::key("disc.field.overview");
        s.overview = disc.overview;
        s.startLabel = ui::Text::key(startLabelKey(disc));
        s.startReady = disc.canStart();
        // Screenshots follow the same rule as the Game options: found ones only, "loading" while the lookup
        // runs, "none" when it found nothing.
        switch (disc.artState) {
            case DiscStatus::ArtState::Idle:
            case DiscStatus::ArtState::Searching: s.shotsPhase = ShotsPhase::Searching; break;
            case DiscStatus::ArtState::Ready:
                s.shotsPhase = disc.screenshotCount > 0 ? ShotsPhase::Ready : ShotsPhase::Missing;
                break;
            case DiscStatus::ArtState::Missing: s.shotsPhase = ShotsPhase::Missing; break;
        }
        s.shotCount = disc.screenshotCount;
        s.shot = [this](int index, ArtSize size) { return library_.discScreenshot(index, size); };
        return s;
    }

    void drawDisc(ui::Painter &p, const ui::Theme &t, const ui::Rect &c, const DiscStatus &disc) {
        view_.draw(p, t, c, subject(disc));
    }

    Library &library_;
    std::string driveModel_;
    bool infoOpen_ = false;            // the "why can't I start" dialog
    DetailView view_;                  // the shared detail layout: focus on the button or a screenshot
    bool shown_ = false;
    bool haveGeneration_ = false;
    uint64_t generation_ = 0;
    ui::ModalStack modals_;
    bool viewerOpen_ = false;
    bool viewerComplete_ = true;
    int viewerCount_ = 0;
};

void strings(ui::Catalog &c) {
    // Hints (the vocabulary: one word per concept)
    c.set("disc.hint.check", "Check again");
    c.set("disc.hint.reloadArt", "Reload artwork");
    c.set("disc.hint.why", "Why not?");
    c.set("disc.why.title", "Cannot start yet");
    // The texts of Classic's toasts for a Start that cannot go ahead (DiscScreen::handle), word for word.
    c.set("disc.why.busy", "Wait for the Physical Disc package update to finish.");
    c.set("disc.why.launching", "Checking disc and core before launch\xE2\x80\xA6");
    c.set("disc.why.core", "Install the matching PlayStation or Saturn core from the Downloader.");
    c.set("disc.why.package", "Install the scoped Physical Disc package in Settings.");
    c.set("disc.why.mgl", "The Physical Disc MGL is unavailable.");
    c.set("disc.field.drive", "Drive");
    c.set("disc.field.overview", "Overview");
    // Notices
    c.set("disc.nodrive.title", "No disc drive found");
    c.set("disc.nodrive.why", "A USB disc drive lets you play PlayStation and Sega Saturn discs.");
    c.set("disc.nodrive.next", "Connect a drive. It is noticed on its own.");
    c.set("disc.nodisc.title", "No disc in the drive");
    c.set("disc.nodisc.why", "Insert a PlayStation or Sega Saturn disc.");
    c.set("disc.nodisc.next", "The game shows up here as soon as the disc is read.");
    c.set("disc.checking.title", "Reading the disc");
    c.set("disc.checking.why", "This takes a few seconds.");
    c.set("disc.unsupported.title", "No game found on this disc");
    c.set("disc.unsupported.why", "Only PlayStation and Sega Saturn games can be started from here.");
    c.set("disc.unsupported.next", "Insert another disc.");
    c.set("disc.unreadable.title", "The disc could not be read");
    c.set("disc.unreadable.why", "It may be dirty or scratched. Nothing was changed.");
    c.set("disc.unreadable.next", "Clean the disc and check it again, or insert another one.");
    // The disc page
    c.set("disc.title.psx", "PlayStation disc detected");      // Classic's heading when the disc has no title
    c.set("disc.title.saturn", "Sega Saturn disc detected");
    c.set("disc.title.generic", "Supported disc detected");
    c.set("disc.console.psx", "PlayStation");
    c.set("disc.console.saturn", "Sega Saturn");
    c.set("disc.field.system", "System");
    c.set("disc.field.year", "Year");
    c.set("disc.field.region", "Region");
    c.set("disc.field.id", "Disc ID");
    c.set("disc.field.players", "Players");
    c.set("disc.field.coop", "Co-op");
    c.set("disc.field.rating", "Rating");
    c.set("disc.button.starting", "Rechecking disc and core\xE2\x80\xA6");   // the Start button's labels, as Classic words them
    c.set("disc.button.packageBusy", "Physical Disc package is updating");
    c.set("disc.button.noCore", "Install matching FPGA core");
    c.set("disc.button.noPackage", "Configure Physical Disc package");
}

}  // namespace

PATSGUI_REGISTER_TAB(Tab::Disc, DiscTab, strings);

}  // namespace patsgui
