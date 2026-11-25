#include "app/settings/SystemSettings.h"

#include <algorithm>

#include "app/settings/StandardPages.h"
#include "app/settings/system/SystemFlows.h"
#include "app/settings/system/WifiFlow.h"

namespace patsgui {
namespace {

using namespace menu;

PATSGUI_OPTIONS(kTimeFormat, {"settings.opt.h24", "24-hour"}, {"settings.opt.h12", "12-hour AM/PM"});
PATSGUI_OPTIONS(kScreensaver, {"settings.opt.saverOff", "Disabled"}, {"settings.opt.s30", "30 seconds"},
     {"settings.opt.m5", "5 minutes"}, {"settings.opt.m10", "10 minutes"}, {"settings.opt.m20", "20 minutes"},
     {"settings.opt.m30", "30 minutes"}, {"settings.opt.m60", "60 minutes"});
PATSGUI_OPTIONS(kSaverStyle, {"settings.opt.dimmed", "Dimmed"}, {"settings.opt.slideshow", "Boxart slideshow"},
     {"settings.opt.collage", "Boxart collage"});

// Classic's cycle code: {0, 30, 300, 600, 1200, 1800, 3600} and dimmed -> boxart -> boxart-collage.
const int kDelays[] = {0, 30, 300, 600, 1200, 1800, 3600};
const char *const kStyles[] = {"dimmed", "boxart", "boxart-collage"};

// The Classic rows of this category, in Classic's order (src/legacy/SettingsScreen.cpp buildRows()).
const MenuCategory &table() {
    static const MenuCategory category = [] {
        MenuCategory system{"system", "System", Cond::Always, {}};
        system.rows = {
            [] {
                // Owner decision (2026-11-23): the row reads "Disc drive status" with the A button, like the other
                // action rows (Classic shows the value "A Status", which the framework cannot draw inside a value).
                MenuRow r = enabledWhen(action("system.disc", "Disc drive status",
                                               "Show the optical drive, disc status and detected PlayStation and Saturn discs.",
                                               "Classic: Disc drive status panel (drive, disc, Physical-Disc package files)"),
                                        Cond::DiscDrive);
                r.disabledValue = "settings.value.noDrive";
                r.reason = "No optical drive is connected.";
                return r;
            }(),
            status("system.disccores", "Install/update Disc cores",
                   "Download the pinned Physical Disc package, verify its checksums, back up managed files and MiSTer.ini, then configure only [CD-*]. Open it again during a download to see the progress or cancel.",
                   "Classic: confirmation, then download/verify/install; value is the package state"),
            action("system.info", "Info", "View details about this MiSTer, its hardware, storage, network and software.",
                   "Classic: System information panel (machine, networking, storage, library, display)"),
            cycle("system.timeformat", "Time format",
                  "Choose whether the clock uses 24-hour time or 12-hour time with AM and PM.", kTimeFormat,
                  PATSGUI_COUNT(kTimeFormat)),
            status("system.timezone", "Time zone",
                   "Choose the local time zone used by the clock and the MiSTer system.",
                   "Classic: two-step chooser, region then city; value is the zone or System default"),
            cycle("system.screensaver", "Screensaver",
                  "Choose how long the GUI waits without controller input before the screensaver starts.",
                  kScreensaver, PATSGUI_COUNT(kScreensaver)),
            cycle("system.saverstyle", "Screensaver style",
                  "Dimmed darkens the screen. Boxart slideshow shows one random cover at a time. Boxart collage keeps adding random covers until the screen fills up. Any controller input wakes the screen.",
                  kSaverStyle, PATSGUI_COUNT(kSaverStyle)),
            action("system.backup", "Back up GUI settings",
                   "Save your personal GUI settings as a backup on a connected USB drive. Restore a backup from the console.",
                   "Classic: starts the backup (onBackupSettings)"),
            status("system.icons", "Download missing icons",
                   "Download missing console and navigation icons. Existing icons are left alone.",
                   "Classic: starts the icon download; value is the icon status"),
            [] {
                MenuRow r = enabledWhen(action("system.wifi", "Connect to Wi-Fi\xE2\x80\xA6",
                                               "Choose a wireless network and enter its password. The connection is tested before settings are saved.",
                                               "Classic: Wi-Fi flow Scanning, Networks, name, password, country, test, review"),
                                        Cond::WifiAdapter);
                r.disabledValue = "settings.value.noAdapter";
                r.reason = "No Wi-Fi adapter is connected.";
                return r;
            }(),
            action("system.mistermenu", "Return to MiSTer menu",
                   "Close this GUI and return to the standard MiSTer menu.", "Classic: leaves the GUI at once"),
            action("system.reboot", "Reboot MiSTer",
                   "Restart the whole MiSTer system. You will leave this GUI until it starts again.",
                   "Classic: confirmation (A restart, B cancel)"),
        };
        return system;
    }();
    return category;
}

// Info: the labels Classic prints (SettingsScreen::facts()) as catalog keys; anything else (a volume path) is data.
struct FactLabel { const char *english, *key; };
const FactLabel kFactLabels[] = {
    {"Machine", "settings.system.fact.machine"}, {"Processor", "settings.system.fact.processor"},
    {"In use", "settings.system.fact.inUse"}, {"Memory", "settings.system.fact.memory"},
    {"Running for", "settings.system.fact.runningFor"}, {"Networking", "settings.system.fact.networking"},
    {"Ethernet", "settings.system.fact.ethernet"}, {"Wi-Fi", "settings.system.fact.wifi"},
    {"Wi-Fi network", "settings.system.fact.wifiNetwork"}, {"Storage", "settings.system.fact.storage"},
    {"Library", "settings.system.fact.library"}, {"Database", "settings.system.fact.database"},
    {"Drive missing", "settings.system.fact.driveMissing"}, {"Systems listed", "settings.system.fact.systemsListed"},
    {"Favorites", "settings.system.fact.favorites"}, {"Imported game list", "settings.system.fact.imported"},
    {"Display", "settings.system.fact.display"}, {"Resolution", "settings.system.fact.resolution"},
    {"Font", "settings.system.fact.font"}, {"Images cached", "settings.system.fact.images"},
};

ui::Text factLabel(const std::string &english) {
    for (const FactLabel &l : kFactLabels)
        if (english == l.english) return ui::Text::key(l.key);
    return ui::Text::data(english);
}

std::vector<ui::KeyValueEntry> infoEntries(SystemService &s) {
    std::vector<ui::KeyValueEntry> out;
    for (const SystemFact &f : s.facts()) {
        ui::KeyValueEntry e;
        if (f.label.empty() && f.value.empty()) e.kind = ui::KeyValueKind::Spacer;
        else if (f.value.empty()) { e.kind = ui::KeyValueKind::Heading; e.label = factLabel(f.label); }
        else { e.label = factLabel(f.label); e.value = ui::Text::data(f.value); }
        out.push_back(e);
    }
    return out;
}

ui::KeyValueEntry status3(const char *labelKey, ui::Text value, ui::Tone tone) {
    ui::KeyValueEntry e;
    e.label = ui::Text::key(labelKey);
    e.value = std::move(value);
    e.hasStatus = true;
    e.tone = tone;
    return e;
}

// Classic renderDiscDriveSettings(): eight lines, success (accent) or warning (orange) by the package state.
std::vector<ui::KeyValueEntry> discEntries(SystemService &s) {
    using ui::Tone;
    const DiscDriveInfo d = s.disc();
    auto key = [](const char *k) { return ui::Text::key(k); };
    auto data = [](const std::string &t) { return ui::Text::data(t); };
    auto fmt = [&](const char *k) { return data(ui::format(k, {d.packageVersion})); };
    std::vector<ui::KeyValueEntry> out;
    {
        ui::KeyValueEntry e;
        e.label = key("settings.system.disc.drive");
        e.hasStatus = true;
        if (d.device.empty()) { e.value = key("settings.system.disc.notAvailable"); e.tone = Tone::Warning; }
        else { e.value = data(d.device + "  " + d.model); e.tone = Tone::Done; }
        out.push_back(e);
    }
    {
        ui::KeyValueEntry e;
        e.label = key("settings.system.disc.status");
        static const char *const kStates[] = {"settings.system.disc.state.noDrive", "settings.system.disc.state.noDisc",
                                              "settings.system.disc.state.checking", "settings.system.disc.state.readable",
                                              "settings.system.disc.state.unsupported", "settings.system.disc.state.unreadable"};
        e.value = key(kStates[int(d.media)]);   // DiscDriveService::stateName(state)
        out.push_back(e);
    }
    // Classic colors a line by the "verified" flag alone (success when set, else warning), whatever the word says.
    out.push_back(status3("settings.system.disc.main",
                          !d.alternateMainInstalled ? key("settings.system.disc.missing")
                          : d.alternateMainVerified ? fmt("settings.system.disc.mainVerified") : key("settings.system.disc.presentUnverified"),
                          d.alternateMainVerified ? Tone::Done : Tone::Warning));
    out.push_back(status3("settings.system.disc.scoped", key(d.scopedMainConfigured ? "settings.system.disc.configured" : "settings.system.disc.missing"),
                          d.scopedMainConfigured ? Tone::Done : Tone::Warning));
    auto mgl = [&](const char *label, bool installed, bool compatible, bool verified) {
        return status3(label,
                       !installed ? key("settings.system.disc.missing") : !compatible ? key("settings.system.disc.incompatible")
                       : verified ? fmt("settings.system.disc.verified") : key("settings.system.disc.compatibleUnverified"),
                       verified ? Tone::Done : Tone::Warning);
    };
    auto rbf = [&](const char *label, bool installed) {
        return status3(label, key(installed ? "settings.system.disc.installed" : "settings.system.disc.missing"),
                       installed ? Tone::Done : Tone::Warning);
    };
    out.push_back(mgl("settings.system.disc.psxMgl", d.psxMglInstalled, d.psxMglCompatible, d.psxMglVerified));
    out.push_back(rbf("settings.system.disc.psxRbf", d.psxCoreInstalled));
    out.push_back(mgl("settings.system.disc.saturnMgl", d.saturnMglInstalled, d.saturnMglCompatible, d.saturnMglVerified));
    out.push_back(rbf("settings.system.disc.saturnRbf", d.saturnCoreInstalled));
    return out;
}

// The row text of "Install/update Disc cores" (Classic buildRows: first match wins).
std::string discCoresText(const DiscInstallInfo &inst, const DiscDriveInfo &d) {
    if (inst.recoveryBlocked) return ui::Catalog::get().tr("settings.system.disccores.blocked");
    if (inst.state == DiscInstallInfo::State::Installing) return ui::Catalog::get().tr("settings.system.disccores.installing");
    if (inst.state == DiscInstallInfo::State::Downloading || inst.state == DiscInstallInfo::State::Verifying)
        return ui::format("settings.system.disccores.working", {std::to_string(inst.bytes / 1024)});
    if (!inst.message.empty()) return inst.message;
    if (d.packageReady) return ui::format("settings.system.disccores.installed", {d.packageVersion});
    if (d.alternateMainInstalled || d.psxMglInstalled || d.saturnMglInstalled)
        return ui::format("settings.system.disccores.repair", {d.packageVersion});
    return ui::format("settings.system.disccores.install", {d.packageVersion});
}

// App::assetDownloadStatus() returns one of four texts; they are shown through the catalog (the count is data).
std::string iconsText(const std::string &status) {
    auto &catalog = ui::Catalog::get();
    if (status == "Downloading icons\xE2\x80\xA6") return catalog.tr("settings.system.icons.status.running");
    if (status == "Try download again") return catalog.tr("settings.system.icons.status.retry");
    if (status == "All icons ready") return catalog.tr("settings.system.icons.status.ready");
    const std::string tail = " icons missing";
    if (status.size() > tail.size() && status.compare(status.size() - tail.size(), tail.size(), tail) == 0)
        return ui::format("settings.system.icons.status.missing", {status.substr(0, status.size() - tail.size())});
    return status;
}

int indexOfDelay(int seconds) {   // Classic: `while (current < 6 && values[current] != delay) ++current;`
    int i = 0;
    while (i < 6 && kDelays[i] != seconds) ++i;
    return i;
}
int indexOfStyle(const std::string &style) { return style == "boxart" ? 1 : style == "boxart-collage" ? 2 : 0; }

void strings(ui::Catalog &c) {
    registerTable(c, table());
    c.set("settings.value.noDrive", "No drive");
    c.set("settings.value.noAdapter", "No adapter");
    c.set("settings.value.systemDefault", "System default");
    // Reboot MiSTer (Classic: "Reboot MiSTer?", "The system will restart now.", "A Restart MiSTer   B Cancel")
    // Disc drive panel (renderDiscDriveSettings)
    c.set("settings.system.disc.title", "Disc drive");
    c.set("settings.system.disc.drive", "Drive");
    c.set("settings.system.disc.status", "Disc status");
    c.set("settings.system.disc.main", "Physical-Disc Main");
    c.set("settings.system.disc.scoped", "Scoped [CD-*] setting");
    c.set("settings.system.disc.psxMgl", "PlayStation MGL");
    c.set("settings.system.disc.psxRbf", "PlayStation RBF");
    c.set("settings.system.disc.saturnMgl", "Saturn MGL");
    c.set("settings.system.disc.saturnRbf", "Saturn RBF");
    c.set("settings.system.disc.state.noDrive", "No drive detected");   // DiscDriveService::stateName()
    c.set("settings.system.disc.state.noDisc", "Please insert disc");
    c.set("settings.system.disc.state.checking", "Checking disc");
    c.set("settings.system.disc.state.readable", "Disc ready");
    c.set("settings.system.disc.state.unsupported", "No supported game detected");
    c.set("settings.system.disc.state.unreadable", "Disc could not be read");
    c.set("settings.system.disc.notAvailable", "Not available");
    c.set("settings.system.disc.missing", "Missing");
    c.set("settings.system.disc.mainVerified", "MiSTer-disc v{1} verified");
    c.set("settings.system.disc.presentUnverified", "Present, unverified");
    c.set("settings.system.disc.configured", "Configured");
    c.set("settings.system.disc.incompatible", "Incompatible");
    c.set("settings.system.disc.verified", "v{1} verified");
    c.set("settings.system.disc.compatibleUnverified", "Compatible, unverified");
    c.set("settings.system.disc.installed", "Installed");
    c.set("settings.system.disc.footer",
          "Use Install/update Disc cores in Settings to install or repair the scoped package. Global [MiSTer] main stays untouched.");
    // Install/update Disc cores (row text, confirmation, toasts)
    c.set("settings.system.disccores.blocked", "Recovery failed \xE2\x80\x94 package actions disabled");
    c.set("settings.system.disccores.installing", "Installing \xE2\x80\x94 finishing safely");
    c.set("settings.system.disccores.working", "Working \xE2\x80\x94 {1} KB");
    c.set("settings.system.disccores.installed", "Installed v{1}");
    c.set("settings.system.disccores.repair", "Repair/update v{1}");
    c.set("settings.system.disccores.install", "Install v{1}");
    // Info
    c.set("settings.system.info.title", "System information");
    c.set("settings.system.fact.machine", "Machine");
    c.set("settings.system.fact.processor", "Processor");
    c.set("settings.system.fact.inUse", "In use");
    c.set("settings.system.fact.memory", "Memory");
    c.set("settings.system.fact.runningFor", "Running for");
    c.set("settings.system.fact.networking", "Networking");
    c.set("settings.system.fact.ethernet", "Ethernet");
    c.set("settings.system.fact.wifi", "Wi-Fi");
    c.set("settings.system.fact.wifiNetwork", "Wi-Fi network");
    c.set("settings.system.fact.storage", "Storage");
    c.set("settings.system.fact.library", "Library");
    c.set("settings.system.fact.database", "Database");
    c.set("settings.system.fact.driveMissing", "Drive missing");
    c.set("settings.system.fact.systemsListed", "Systems listed");
    c.set("settings.system.fact.favorites", "Favorites");
    c.set("settings.system.fact.imported", "Imported game list");
    c.set("settings.system.fact.display", "Display");
    c.set("settings.system.fact.resolution", "Resolution");
    c.set("settings.system.fact.font", "Font");
    c.set("settings.system.fact.images", "Images cached");
    // Time zone
    // Backup
    // Icons
    c.set("settings.system.icons.status.running", "Downloading icons\xE2\x80\xA6");   // App::assetDownloadStatus()
    c.set("settings.system.icons.status.retry", "Try download again");
    c.set("settings.system.icons.status.ready", "All icons ready");
    c.set("settings.system.icons.status.missing", "{1} icons missing");
    registerSystemFlowStrings(c);
    registerWifiStrings(c);
}

}  // namespace

std::vector<VisibleRow> SystemSettings::rows(const MenuState &state) const { return visibleRows(table(), state); }

int SystemSettings::current(const std::string &rowId) const {
    SystemService *s = service();
    if (!s) return kNoValue;
    if (rowId == "system.timeformat") return s->use24HourClock() ? 0 : 1;
    if (rowId == "system.screensaver") return indexOfDelay(s->screensaverDelay());
    if (rowId == "system.saverstyle") return indexOfStyle(s->screensaverStyle());
    return kNoValue;
}

void SystemSettings::changed(const std::string &rowId, int value) {
    SystemService *s = service();
    if (!s) return;
    if (rowId == "system.timeformat") s->setUse24HourClock(value == 0);   // Classic: setUse24HourClock(!use24HourClock())
    else if (rowId == "system.screensaver") s->setScreensaverDelay(kDelays[std::max(0, std::min(value, 6))]);
    else if (rowId == "system.saverstyle") s->setScreensaverStyle(kStyles[std::max(0, std::min(value, 2))]);
}

bool SystemSettings::activate(const std::string &rowId, PageStack &pages) {
    SystemService *s = service();
    if (!s) return false;
    if (rowId == "system.disc") { openDiscPage(pages); return true; }
    if (rowId == "system.disccores") { openDiscInstallFlow(*s, pages); return true; }
    if (rowId == "system.info") { openInfoPage(pages); return true; }
    if (rowId == "system.timezone") { openTimeZoneFlow(*s, pages); return true; }
    if (rowId == "system.backup") { openBackupFlow(*s, pages, backupReporting_); return true; }
    if (rowId == "system.icons") { openIconsFlow(*s, pages); return true; }   // App::startAssetDownload()
    if (rowId == "system.wifi") { openWifiFlow(*s, pages); return true; }
    if (rowId == "system.mistermenu") { s->returnToMisterMenu(); return true; }
    if (rowId == "system.reboot") { openRebootFlow(*s, pages); return true; }
    return false;
}

// Classic: SettingsScreen::update() polls the Disc drive, the installer and the Wi-Fi adapter every frame, App::update()
// reports the backup and the icon download. The values and conditions are copied into MenuState here.
bool SystemSettings::refresh(bool force) {
    SystemService *s = service();
    if (!s || !attached()) return false;
    MenuState &st = env().state;
    bool changed = force;
    const DiscDriveInfo drive = s->disc();
    const bool hasDrive = drive.media != DiscDriveInfo::Media::NoDrive;
    if (st.discDrive != hasDrive) { st.discDrive = hasDrive; changed = true; }
    const bool wifi = s->wifiAvailable();
    if (st.wifiAdapter != wifi) { st.wifiAdapter = wifi; changed = true; }
    auto setStatus = [&](const char *id, const std::string &text) {
        std::string &slot = st.status[id];
        if (slot != text) { slot = text; changed = true; }
    };
    setStatus("system.disccores", discCoresText(s->discInstall(), drive));
    const std::string zone = s->timeZone();
    setStatus("system.timezone", zone.empty() ? ui::Catalog::get().tr("settings.value.systemDefault") : zone);
    setStatus("system.icons", iconsText(s->icons().statusText));
    return changed;
}

bool SystemSettings::update(float deltaSeconds) {
    SystemService *s = service();
    if (!s || !attached()) return false;
    s->update(deltaSeconds);
    bool changed = refresh(false);
    reportBackup(s);
    return changed;
}

void SystemSettings::onShow() { refresh(true); }

// Classic App::update(): the backup result is reported once. The backup flow reports its own run (it acknowledges the result),
// so this only catches a result nobody showed (Complete: a notice of 10 s, Failed: the failed page).
void SystemSettings::reportBackup(SystemService *s) {
    if (*backupReporting_) return;
    const BackupInfo b = s->backup();
    if (!b.unreported || b.state == BackupInfo::State::Running) return;
    s->acknowledgeBackup();
    if (b.state == BackupInfo::State::Complete) {
        const size_t slash = b.path.find_last_of('/');
        const std::string folder = slash == std::string::npos ? ui::Catalog::get().tr("settings.system.backup.usbFolder") : b.path.substr(0, slash);
        env().pages.toast(ui::Text::data(ui::format("settings.system.backup.done.changed", {folder})), 10.0f);
    } else if (b.state == BackupInfo::State::Failed) {
        openBackupStep(*s, env().pages, "failed", backupReporting_);
    }
}

void SystemSettings::openDiscPage(PageStack &pages) {
    SystemService *s = service();
    if (!s) return;
    InfoSpec spec;
    spec.title = ui::Text::key("settings.system.disc.title");
    spec.footer = ui::Text::key("settings.system.disc.footer");
    spec.footerAccent = ui::Text::data(s->disc().projectUrl);
    pages.push(InfoPage::facts(spec, [s]() { return discEntries(*s); }, 0.25f));   // live, like Classic's per-frame reads
}

void SystemSettings::openInfoPage(PageStack &pages) {
    SystemService *s = service();
    if (!s) return;
    InfoSpec spec;
    spec.title = ui::Text::key("settings.system.info.title");
    pages.push(InfoPage::facts(spec, [s]() { return infoEntries(*s); }, 0.5f));
}

std::vector<std::string> SystemSettings::pageIds() const {
    return {"reboot", "reboot.failed", "disc", "discinstall", "discinstall.working", "discinstall.done", "discinstall.failed",
            "discinstall.cancelled", "discinstall.blocked", "info", "timezone", "timezone.city", "timezone.failed", "timezone.nolist",
            "backup", "backup.done", "backup.failed", "icons", "icons.done", "icons.ready", "icons.failed",
            "wifi.scanning", "wifi.networks", "wifi.ssid", "wifi.password", "wifi.prompt", "wifi.testing", "wifi.review",
            "wifi.country", "wifi.failed"};
}

bool SystemSettings::openPage(const std::string &pageId, PageStack &pages) {
    SystemService *s = service();
    if (!s) return false;
    if (pageId == "disc") { openDiscPage(pages); return true; }
    if (pageId == "info") { openInfoPage(pages); return true; }
    if (pageId.compare(0, 5, "wifi.") == 0) return openWifiStepForTest(*s, pages, pageId.substr(5));
    return openSystemFlowStepForTest(*s, pages, pageId, backupReporting_);
}

PATSGUI_REGISTER_SETTINGS_CATEGORY(SystemSettings, strings);

}  // namespace patsgui
