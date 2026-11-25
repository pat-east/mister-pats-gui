#include "app/settings/library/LibraryPages.h"

#include "app/settings/StandardPages.h"

namespace patsgui {
namespace library {
namespace detail {
void openFlowAt(LibraryService *svc, PageStack &pages, int which);   // ScanFlow.cpp: 0 scan working, 1 scan done, 2 art working, 3 art done
void openCoreAt(LibraryService *svc, PageStack &pages, int which);   // CoreFlow.cpp: 0 intro, 1 checking, 2 list, 3 review, 4 installing, 5 result, 6 failed check
}  // namespace detail

std::vector<std::string> pageIds() {
    return {"scan.intro", "scan.working", "scan.done", "scan.stopped", "artwork.offer", "artwork.working", "artwork.done",
            "artwork.stopped", "cores.intro", "cores.intro.offline", "cores.checking", "cores.list", "cores.review",
            "cores.installing", "cores.result", "cores.failed", "ss.videos", "ss.user", "ss.password", "ss.saveError"};
}

// The pages as the host runner shows them. The sample service decides what they show (`--set library.<key>=...`);
// a page that needs a running task starts it on the service when the service is idle, as pressing A would have.
bool openPage(LibraryService &svc, PageStack &pages, const std::string &id) {
    LibraryService *s = &svc;
    auto push = [&pages](std::unique_ptr<SettingsPage> page) { pages.push(std::move(page)); return true; };
    if (id == "scan.intro") { openScanIntro(svc, pages); return true; }
    if (id == "scan.working") { detail::openFlowAt(s, pages, 0); return true; }
    if (id == "scan.done" || id == "scan.stopped") { detail::openFlowAt(s, pages, 1); return true; }
    if (id == "ss.videos") { openScrapedVideos(svc, pages); return true; }
    if (id == "artwork.offer") { openArtworkOffer(svc, pages); return true; }
    if (id == "artwork.working") { detail::openFlowAt(s, pages, 2); return true; }
    if (id == "artwork.done" || id == "artwork.stopped") { detail::openFlowAt(s, pages, 3); return true; }
    if (id == "cores.intro" || id == "cores.intro.offline") { openCoreUpdater(svc, pages); return true; }
    if (id == "cores.checking") { detail::openCoreAt(s, pages, 1); return true; }
    if (id == "cores.list") { detail::openCoreAt(s, pages, 2); return true; }
    if (id == "cores.review") { detail::openCoreAt(s, pages, 3); return true; }
    if (id == "cores.installing") { detail::openCoreAt(s, pages, 4); return true; }
    if (id == "cores.result") { detail::openCoreAt(s, pages, 5); return true; }
    if (id == "cores.failed") { detail::openCoreAt(s, pages, 6); return true; }   // the result of a failed check
    if (id == "ss.user") { openAccountEntry(svc, pages, true); return true; }
    if (id == "ss.password") { openAccountEntry(svc, pages, false); return true; }
    if (id == "ss.saveError") {
        ResultSpec failed;
        failed.kind = ResultSpec::Error;
        failed.title = ui::Text::key("settings.library.account.saveFailed.title");
        failed.body = ui::Text::key("settings.library.account.saveFailed.body");
        return push(std::unique_ptr<SettingsPage>(new ResultPage(failed)));
    }
    return false;
}

void registerPageStrings(ui::Catalog &c) {
    registerScanStrings(c);
    registerCoreStrings(c);
    registerAccountStrings(c);
    registerArtworkStrings(c);
    registerVideoStrings(c);
}

}  // namespace library
}  // namespace patsgui
