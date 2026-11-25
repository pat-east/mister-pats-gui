#include "app/settings/LibrarySettings.h"

#include "app/settings/StandardPages.h"
#include "app/settings/library/LibraryPages.h"
#include "app/settings/library/Values.h"

namespace patsgui {
namespace {

using namespace menu;

PATSGUI_OPTIONS(kVideoSound, {"settings.opt.soundOff", "Off"}, {"settings.opt.v100", "100%"}, {"settings.opt.v75", "75%"},
     {"settings.opt.v50", "50%"}, {"settings.opt.v25", "25%"}, {"settings.opt.v10", "10%"});
PATSGUI_OPTIONS(kRegion, {"settings.opt.eu", "EU"}, {"settings.opt.us", "US"}, {"settings.opt.jp", "JP"});
PATSGUI_OPTIONS(kDelay, {"settings.opt.d5", "5 seconds"}, {"settings.opt.d10", "10 seconds"}, {"settings.opt.d15", "15 seconds"},
     {"settings.opt.d20", "20 seconds"}, {"settings.opt.d25", "25 seconds"}, {"settings.opt.d30", "30 seconds"});
PATSGUI_OPTIONS(kStorage, {"settings.opt.g256", "256 MB"}, {"settings.opt.g512", "512 MB"}, {"settings.opt.g1", "1 GB"},
     {"settings.opt.g2", "2 GB"}, {"settings.opt.g4", "4 GB"}, {"settings.opt.g8", "8 GB"},
     {"settings.opt.unlimited", "Unlimited"});

// The Classic rows of this category, in Classic's order (src/legacy/SettingsScreen.cpp buildRows()).
const MenuCategory &table() {
    static const MenuCategory category = [] {
        MenuCategory library{"library", "Library", Cond::Always, {}};
        library.rows = {
            action("library.build", "Build game list",
                   "Search connected drives for games and save a list the menus can load quickly. Run this after adding or removing games.",
                   "Classic: starts the game-list scan (onBuildDatabase)"),
            action("library.boxart", "Prepare box art",
                   "Download missing covers and make copies sized for each view. Covers already on your drives are kept.",
                   "Classic: starts the artwork job (onFetchArtwork)"),
            action("library.reload", "Reload saved game list",
                   "Reload the saved game list and rebuild the box-art index for the next startup. This does not search for newly added games.",
                   "Classic: reloads at once, shows a toast"),
            action("library.coreupdates", "Core and MRA updates",
                   "Check and update official Arcade, Console, Computer, Other, Service and Utility cores and Arcade MRAs.",
                   "Classic: modal flow Intro, Checking, List (filters, select), Review, Installing, Result"),
            action("library.arcade", "Arcade game status",
                   "See which Arcade games are ready to launch and why others are unavailable.",
                   "Classic: opens the Arcade status screen (onManageArcade)"),
            toggle("library.versions", "Organize game-versions",
                   "Group ROMs with region, revision or language suffixes under one title.", true),
            status("library.tgdb", "TheGamesDB",
                   "Looks up box art, screenshots and release year for PlayStation and Saturn discs, and up to four screenshots for games. Add your API key to /media/fat/mister-pat/thegamesdb-settings.ini as apikey=\"YOUR_KEY\", then select this row to test it. Without a key, game options use local images or one Libretro screenshot.",
                   "Classic: key check, result as toast; value is the check status"),
            heading("library.ss.heading", "ScreenScraper.fr"),
            toggle("library.ss.video", "Video previews",
                   "Play a short video of the selected game in the List view, taken from ScreenScraper.fr. It needs a free ScreenScraper account. Off means nothing is requested from ScreenScraper.",
                   false),
            status("library.ss.account", "Account",
                   "Shows whether your ScreenScraper login works. Select to check it again. The developer login of this software is read from /media/fat/mister-pat/screenscraper-fr-settings.ini.",
                   "Classic: checks the login again; value is the account state"),
            status("library.ss.user", "User name", "The user name of your own ScreenScraper account.",
                   "Classic: on-screen keyboard entry"),
            status("library.ss.password", "Password",
                   "The password of your ScreenScraper account. It is stored in the settings file on the card, which anyone with SSH or FTP access to the MiSTer can read, so use a password you use nowhere else.",
                   "Classic: on-screen keyboard entry (masked)"),
            cycle("library.ss.sound", "Video sound",
                  "The volume of the video previews: Off, 100%, 75%, 50%, 25% or 10%. This lowers the sound itself; the MiSTer's own volume is not changed. It takes effect with the next video you open. Off by default, because sound starting while you browse can be annoying.",
                  kVideoSound, PATSGUI_COUNT(kVideoSound)),
            cycle("library.ss.region", "Default region",
                  "The region whose spelling of a game title is shown when ScreenScraper knows several: EU, US or JP.",
                  kRegion, PATSGUI_COUNT(kRegion)),
            cycle("library.ss.delay", "Preview delay",
                  "How long a game must stay selected before its video is looked up, in steps of 5 seconds up to 30. Nothing is requested while you scroll.",
                  kDelay, PATSGUI_COUNT(kDelay)),
            action("library.ss.videos", "Scraped videos",
                   "See the videos downloaded so far with the space they use, and delete one or all of them. Deleted videos are downloaded again when you stay on their game.",
                   "New: list of the cached videos with delete"),
            cycle("library.ss.storage", "Video storage limit",
                  "The most space downloaded videos may use on the card. The oldest videos are deleted first when the limit is reached. Unlimited still keeps some free space on the card.",
                  kStorage, PATSGUI_COUNT(kStorage)),
        };
        return library;
    }();
    return category;
}

void strings(ui::Catalog &c) {
    registerTable(c, table());
    library::registerPageStrings(c);
    // Notices (Classic: Context::notify in SettingsScreen.cpp, with its seconds)
    c.set("settings.library.reload.working", "Reloading the saved game list\xE2\x80\xA6");
    c.set("settings.library.reload.done", "Saved game list loaded");
    c.set("settings.library.tgdb.addKey", "Add your key to {1}");
    c.set("settings.library.tgdb.checking", "Checking TheGamesDB API key\xE2\x80\xA6");
    c.set("settings.library.tgdb.works", "TheGamesDB key works: {1}.");
}

}  // namespace

std::vector<VisibleRow> LibrarySettings::rows(const MenuState &state) const { return visibleRows(table(), state); }

// The real value of a Switch (0/1) or of a cycling row (its option index), from the service. Classic reads the same
// Preferences getters in the row's value lambda (SettingsScreen.cpp:309, 663, 695, 709, 717, 724).
int LibrarySettings::current(const std::string &rowId) const {
    LibraryService *s = service();
    if (!s) return kNoValue;                                    // no service: the tab keeps the value in memory
    if (rowId == "library.versions") return s->organizeGameVersions() ? 1 : 0;
    if (rowId == "library.ss.video") return s->screenScraperEnabled() ? 1 : 0;
    if (rowId == "library.ss.sound") return library::volumeIndex(s->screenScraperVolume());
    if (rowId == "library.ss.region") return library::regionIndex(s->screenScraperRegion());
    if (rowId == "library.ss.delay") return library::delayIndex(s->screenScraperDelay());
    if (rowId == "library.ss.storage") return library::storageIndex(s->screenScraperStorageLimitMb());
    return kNoValue;
}

// The same Preferences setters Classic calls (SettingsScreen.cpp:310, 664, 704, 713, 720, 734). The tab has already
// stepped its in-memory value; the service validates and stores.
void LibrarySettings::changed(const std::string &rowId, int value) {
    LibraryService *s = service();
    if (!s) return;
    if (rowId == "library.versions") s->setOrganizeGameVersions(value != 0);   // + Context::onGameVersionSettingChanged (the adapter)
    else if (rowId == "library.ss.video") s->setScreenScraperEnabled(value != 0);   // on: forgetAccount + checkAccount; off: preview.stop()
    else if (rowId == "library.ss.sound") s->setScreenScraperVolume(library::volumeAt(value));
    else if (rowId == "library.ss.region") s->setScreenScraperRegion(library::regionAt(value));
    else if (rowId == "library.ss.delay") s->setScreenScraperDelay(library::delayAt(value));
    else if (rowId == "library.ss.storage") s->setScreenScraperStorageLimitMb(library::storageAt(value));
}

// What the rows show that the service knows (Classic draws these from closures at every frame), and the one
// per-frame hook of Classic's Settings screen for this category (SettingsScreen.cpp:1002-1012).
bool LibrarySettings::update(float deltaSeconds) {
    LibraryService *s = service();
    if (!s || !attached()) return false;
    s->update(deltaSeconds);
    MenuState &st = env().state;
    bool changed = false;
    auto setStatus = [&](const char *id, const std::string &text) {
        std::string &slot = st.status[id];
        if (slot != text) { slot = text; changed = true; }
    };
    const TheGamesDbStatus tg = s->theGamesDb();
    setStatus("library.tgdb", tg.text);
    // Checking -> Success: "TheGamesDB key works: <status>." for 7 s. Failures have no notice, only the row text.
    if (previousTgdb_ == TheGamesDbStatus::State::Checking && tg.state == TheGamesDbStatus::State::Success)
        env().pages.toast(ui::Text::data(ui::format("settings.library.tgdb.works", {tg.text})), 7.0f);
    previousTgdb_ = tg.state;
    setStatus("library.ss.account", library::accountValueText(s->account()));
    setStatus("library.ss.user", library::userText(s->screenScraperUser()));
    setStatus("library.ss.password", library::passwordText(s->screenScraperPasswordSet()));
    return changed;
}

bool LibrarySettings::activate(const std::string &rowId, PageStack &pages) {
    LibraryService *s = service();
    if (!s) return false;   // no service: the plain first level
    if (rowId == "library.build") { library::openScanIntro(*s, pages); return true; }
    if (rowId == "library.boxart") { library::openArtworkOffer(*s, pages); return true; }
    if (rowId == "library.reload") {   // SettingsScreen.cpp:292-300; the whole thing blocks, as in Classic
        pages.toast(ui::Text::key("settings.library.reload.working"), 6.0f);
        s->reloadSavedGameList();
        pages.toast(ui::Text::key("settings.library.reload.done"));
        return true;
    }
    if (rowId == "library.coreupdates") { library::openCoreUpdater(*s, pages); return true; }
    if (rowId == "library.arcade") {
        if (!env().openArcadeStatus) return false;
        env().openArcadeStatus();   // Classic: App::openArcadeGames(); here the Arcade tab's status screen
        return true;
    }
    if (rowId == "library.tgdb") {   // SettingsScreen.cpp:317-326
        const TheGamesDbStatus t = s->theGamesDb();
        if (!t.configured) pages.toast(ui::Text::data(ui::format("settings.library.tgdb.addKey", {t.settingsPath})), 8.0f);
        else if (!s->startTheGamesDbCheck()) pages.toast(ui::Text::data(s->theGamesDb().text), 5.0f);
        else pages.toast(ui::Text::key("settings.library.tgdb.checking"), 4.0f);
        return true;
    }
    if (rowId == "library.ss.videos") { library::openScrapedVideos(*s, pages); return true; }
    if (rowId == "library.ss.account") { s->checkAccount(); return true; }   // forgetAccount() + checkAccount()
    if (rowId == "library.ss.user") { library::openAccountEntry(*s, pages, true); return true; }
    if (rowId == "library.ss.password") { library::openAccountEntry(*s, pages, false); return true; }
    return false;
}

std::vector<std::string> LibrarySettings::pageIds() const { return library::pageIds(); }

bool LibrarySettings::openPage(const std::string &pageId, PageStack &pages) {
    LibraryService *s = service();
    return s && library::openPage(*s, pages, pageId);
}

PATSGUI_REGISTER_SETTINGS_CATEGORY(LibrarySettings, strings);

}  // namespace patsgui
