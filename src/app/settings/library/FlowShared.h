#pragma once

// What the pages of the Library flows share. "Build game list" (ScanFlow.cpp) and "Prepare box art" (ArtworkFlow.cpp) are
// two flows; the scan ends in the box-art offer and both end in the same Done result.

#include <functional>
#include <memory>

#include "app/settings/LibraryService.h"
#include "app/settings/SettingsPage.h"
#include "app/settings/WizardFlowPage.h"

namespace patsgui {
namespace library {

// What happened in this visit to the flows (Classic: ScanScreen::scanRan_ and the artwork state). The shared
// LibraryScan / MediaScraper keep their last state between visits, so a page must not read an old result as its own.
struct Visit {
    bool scanRan = false, artworkRan = false;
    bool artworkCancelled = false;   // the user pressed B on the box-art job (a stopped job ends Failed in the service)
    bool scanCancelled = false;   // the user pressed B on the scan (a stopped scan ends Failed in the service; this tells it apart)
};
using VisitPtr = std::shared_ptr<Visit>;

// ONE frame size for both Library flows (the box-art offer holds a system list).
constexpr ui::WizardFrame kLibraryFlowFrame = ui::WizardFrame::Medium;

// ScanFlow.cpp: the Result page both flows end in (scan and/or box-art outcome of this visit), page id `id`.
// A retry after a failed or stopped scan goes to `retryScan` (kNoPage: A is Done); likewise `retryArtwork`.
void addDonePage(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId id,
                 ui::PageId retryScan = ui::kNoPage, ui::PageId retryArtwork = ui::kNoPage);
// ScanFlow.cpp: a choice page of both flows: the question (Heading), one muted line, then the list, top-aligned under the title
// (the list may scroll). `reserveBottom` keeps room under the list (e.g. for a status line).
void drawQuestionList(ui::Painter &p, const ui::Theme &t, const ui::Rect &body, const ui::Text &question, const ui::Text &line,
                      ui::ChooseList &list, int reserveBottom);
// ArtworkFlow.cpp: adds the box-art offer (id `offerId`) and the working page (id `offerId + 1`) to `flow`. Start goes to the
// working page; Skip/B and the end of the working page go to `doneId`.
void addArtworkPages(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId offerId, ui::PageId doneId);

// ScanFlow.cpp: the scan's Busy page (centred status phrase, bar, one counts line; B stops the scan and shows "Stopping"). Done
// goes to `afterScan(progress)` when given, else `doneId`; Failed (also a stopped scan) goes to `failedId`. `title`: the dialog's
// title (empty: "Build game list"). Shared by "Build game list" and the first-run Jump Start (src/app/FirstRun.cpp).
void addScanWorking(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId id, ui::PageId doneId, ui::PageId failedId,
                    const ui::Text &title = ui::Text(), std::function<ui::PageId(const ScanProgress &)> afterScan = nullptr);
// ArtworkFlow.cpp: the box-art Busy page alone (the job is already started): the end of the job (Done or Failed) and B ("Stopping",
// or at once when the job stopped) go to `doneId`. `title` empty: "Prepare box art".
void addArtworkWorking(WizardFlowPage &flow, LibraryService *svc, VisitPtr visit, ui::PageId id, ui::PageId doneId,
                       const ui::Text &title = ui::Text());

}  // namespace library
}  // namespace patsgui
