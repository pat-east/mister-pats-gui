// Host runner of the new GUI: renders the splash and every tab on top of the SampleLibrary to PNG files.
//
//   app-render [--out DIR] [--height 1080] [--tab NAME] [--keys a,b,c] [--suffix NAME] [--hold favorite|remove] [--options] [--list] [--seconds N] [--partial N] [--layout xbox|playstation|nintendo] [--disc nodrive|nodisc|checking|readable|unsupported|unreadable]
//       toast: [--notify TEXT]   (what the device does for every Classic notify(): the shell's toast)
//       disc tab: [--disc-tab-off]   (the drive goes away after the keys: the tab leaves the row, an active Disc tab goes to Home)
//       look: [--look text=130,hint=115,contrast=2,dark=0,theme=3]   interface: [--start] [--own-sounds]
//       first start: [--first-run PAGE] [--list-first-run]   (the Welcome / Jump Start flow over the Home tab; PAGE as --list-first-run prints it)
//       settings tab: [--category ID] [--page PAGE] [--set key=value ...] [--after N] [--list-pages] [--list-samples]
//       bench: [--bench N] replays --keys N times; per key prints the cost of a full repaint (BEFORE) and of the damage path
//       (RetainedFrame, AFTER): pixels touched, glyphs, ms on this machine, repainted share; every frame is compared pixel by pixel
//       layout check: [--check-layout] prints "layout: <picture>: <kind>: ..." for text outside the screen, cut by its clip, overlapping other text
//       or elided with "..." (exit code 4 when any is found; src/app/tests/README.md)
//
// No device and no src/legacy/ code involved; this is how tabs are developed and checked (src/app/README.md).

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "app/GamesTab.h"
#include "app/Look.h"
#include "app/PatsGui.h"
#include "app/SampleLibrary.h"
#include "app/SettingsTab.h"
#include "app/settings/SampleSettingsServices.h"
#include "ui-framework/kit/Components.h"
#include "ui-framework/platform/host/PngWriter.h"
#include "ui-framework/render/soft/RetainedFrame.h"
#include "ui-framework/render/soft/SoftPainter.h"

using namespace ui;

static bool loadFonts(Font &regular, Font &bold) {
    std::vector<std::string> r, b;
    if (const char *e = std::getenv("UI_FONT_REGULAR")) r.push_back(e);
    if (const char *e = std::getenv("UI_FONT_BOLD")) b.push_back(e);
    r.push_back("/System/Library/Fonts/Supplemental/Arial.ttf");
    b.push_back("/System/Library/Fonts/Supplemental/Arial Bold.ttf");
    r.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    b.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    const bool okR = regular.load(r);
    const bool okB = bold.load(b);
    return okR && okB;
}

// The event a --keys word names: up down left right a(confirm) b(back) x y start xhold bhold lprev lnext select tabprev tabnext.
static bool eventNamed(const std::string &w, patsgui::Event &e) {
    using E = patsgui::Event;
    static const struct { const char *name; E event; } table[] = {
        {"up", E::Up}, {"down", E::Down}, {"left", E::Left}, {"right", E::Right}, {"a", E::Confirm}, {"b", E::Back},
        {"x", E::X}, {"y", E::Y}, {"start", E::Start}, {"xhold", E::XHold}, {"bhold", E::BHold}, {"lprev", E::LetterPrev},
        {"lnext", E::LetterNext}, {"select", E::Select}, {"tabprev", E::TabPrev}, {"tabnext", E::TabNext}};
    for (const auto &row : table)
        if (w == row.name) { e = row.event; return true; }
    return false;
}

// The layout check over the text runs one frame drew (SoftPainter's recorder): what a person sees at once as a defect.
// Returns the number of findings, each printed as "layout: NAME: kind: text".
static int checkLayout(const std::vector<SoftPainter::TextRun> &runs, const Rect &screen, const std::string &name) {
    int found = 0;
    auto report = [&](const char *kind, const SoftPainter::TextRun &r, const std::string &more) {
        std::printf("layout: %s: %s: \"%s\" at %d,%d %dx%d%s\n", name.c_str(), kind, r.text.c_str(), r.box.x, r.box.y, r.box.w, r.box.h, more.c_str());
        ++found;
    };
    std::vector<Rect> visible(runs.size());
    for (size_t i = 0; i < runs.size(); ++i) {
        const SoftPainter::TextRun &r = runs[i];
        if (r.text.empty() || r.box.w <= 0) continue;
        visible[i] = r.shown.intersect(screen);
        if (r.box.x < screen.x || r.box.y < screen.y || r.box.right() > screen.right() || r.box.bottom() > screen.bottom()) {
            if (!visible[i].empty()) report("offscreen", r, "");
        } else if (!visible[i].empty() && r.box.intersect(r.clip).w < r.box.w - 1) {
            report("cut", r, " (clip " + std::to_string(r.clip.w) + "px wide)");   // horizontally cut by its clip
        }
        if (r.elided && !visible[i].empty()) report("elided", r, "");
    }
    for (size_t i = 0; i < runs.size(); ++i)
        for (size_t j = i + 1; j < runs.size(); ++j) {
            if (visible[i].empty() || visible[j].empty()) continue;
            if (runs[i].text == runs[j].text && runs[i].box.x == runs[j].box.x && runs[i].box.y == runs[j].box.y) continue;   // drawn twice
            const Rect o = visible[i].intersect(visible[j]);
            const int minH = std::min(visible[i].h, visible[j].h), minW = std::min(visible[i].w, visible[j].w);
            // Real overlap: more than a few pixels both ways and a good part of the smaller run (tight line boxes touch).
            if (o.w >= 3 && o.h >= 3 && o.h * 10 >= minH * 4 && o.w * 10 >= std::min(minW, 40) * 4) {
                report("overlap", runs[i], " with \"" + runs[j].text + "\" at " + std::to_string(runs[j].box.x) + "," + std::to_string(runs[j].box.y));
            }
        }
    return found;
}

int main(int argc, char **argv) {
    std::string out = "build/out", only;
    int height = 1080;
    bool options = false, listView = false;
    ui::ButtonLayout layout = ui::ButtonLayout::Xbox;   // --layout: the face-button layout of the hints
    float seconds = 0.0f;
    int bench = 0;           // --bench N: see the usage line
    int partialFrames = 0;   // --partial N: after the full frame, N video-only frames, each checked against a full redraw
    float favoriteHold = 0.0f, removeHold = 0.0f;
    // Settings tab: --category focuses a category's rows, --page opens one of its pages (--list-pages names them),
    // --set key=value puts a sample service into a state (--list-samples names the keys), --after lets time pass
    // after the keys (the sample state machines move on).
    std::string category, page;
    std::vector<std::pair<std::string, std::string>> sets;
    std::string firstRunPage;   // --first-run PAGE: the first-start flow opened at that page (src/app/FirstRun.h)
    bool listFirstRun = false;
    bool listPages = false, listSamples = false, pseudo = false, checkLayoutOn = false;
    int layoutFindings = 0;
    // --start: open on the user's Start tab (the Interface sample's start tab; the device's PatsGuiScreen::open does the
    // same). --own-sounds: the GUI plays Navigation and Start itself (State::ownsSounds), as when it is the primary program.
    bool useStart = false, ownSounds = false;
    bool discTabOff = false;   // --disc-tab-off: after the keys the drive is gone: the Disc tab leaves the row
    float after = 0.0f;
    // --look text=130,hint=115,contrast=2,dark=0,theme=3: the look settings Classic keeps in Preferences, applied with
    // the same function the device uses (src/app/Look.h). contrast=N turns high contrast on with highlight N.
    patsgui::LookSettings look;
    std::string notify;   // --notify TEXT: a toast after the keys, as the host bridge shows a Classic notify()
    std::string keys, suffix;   // --keys: events sent to the tab before the picture is taken (see eventNamed); --suffix: file name part
    patsgui::DiscStatus::Media media = patsgui::DiscStatus::Media::Readable;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--out" && i + 1 < argc) out = argv[++i];
        else if (a == "--height" && i + 1 < argc) height = std::atoi(argv[++i]);
        else if (a == "--hold" && i + 1 < argc) {   // favorite|remove: the bottom bar with a hold at 60 %
            const std::string h = argv[++i];
            if (h == "favorite") favoriteHold = 0.6f; else if (h == "remove") removeHold = 0.6f;
            else { std::fprintf(stderr, "unknown hold: %s\n", h.c_str()); return 2; }
        }
        else if (a == "--layout" && i + 1 < argc) layout = ui::buttonLayoutFromName(argv[++i]);
        else if (a == "--options") options = true;      // Games tab: the Game options panel
        else if (a == "--list") listView = true;        // Games tab: Grid-List with the detail panel
        else if (a == "--seconds" && i + 1 < argc) seconds = float(std::atof(argv[++i]));   // let time pass first (video preview)
        else if (a == "--bench" && i + 1 < argc) bench = std::atoi(argv[++i]);
        else if (a == "--partial" && i + 1 < argc) partialFrames = std::atoi(argv[++i]);
        else if (a == "--tab" && i + 1 < argc) only = argv[++i];
        else if (a == "--start") useStart = true;
        else if (a == "--own-sounds") ownSounds = true;
        else if (a == "--keys" && i + 1 < argc) keys = argv[++i];
        else if (a == "--suffix" && i + 1 < argc) suffix = argv[++i];
        else if (a == "--category" && i + 1 < argc) category = argv[++i];
        else if (a == "--page" && i + 1 < argc) page = argv[++i];
        else if (a == "--look" && i + 1 < argc) {
            std::string list = argv[++i];
            for (size_t at = 0; at < list.size();) {
                const size_t comma = list.find(',', at);
                const std::string kv = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                at = comma == std::string::npos ? list.size() : comma + 1;
                const size_t eq = kv.find('=');
                const std::string key = kv.substr(0, eq), value = eq == std::string::npos ? std::string() : kv.substr(eq + 1);
                if (key == "text") look.textScale = std::atoi(value.c_str());
                else if (key == "hint") look.hintScale = std::atoi(value.c_str());
                else if (key == "contrast") { look.highContrast = true; look.highlight = std::atoi(value.c_str()); }
                else if (key == "dark") look.dark = value != "0";
                else if (key == "theme") look.colorTheme = std::atoi(value.c_str());
                else { std::fprintf(stderr, "unknown --look key: %s\n", key.c_str()); return 2; }
            }
        }
        else if (a == "--notify" && i + 1 < argc) notify = argv[++i];
        else if (a == "--after" && i + 1 < argc) after = float(std::atof(argv[++i]));
        else if (a == "--list-pages") listPages = true;
        else if (a == "--first-run" && i + 1 < argc) firstRunPage = argv[++i];
        else if (a == "--list-first-run") listFirstRun = true;
        else if (a == "--check-layout") checkLayoutOn = true;
        else if (a == "--pseudo") pseudo = true;   // the pseudo-locale (en-XA): accented, 40 % longer text
        else if (a == "--list-samples") listSamples = true;
        else if (a == "--set" && i + 1 < argc) {
            const std::string kv = argv[++i];
            const size_t eq = kv.find('=');
            if (eq == std::string::npos) { std::fprintf(stderr, "--set wants key=value: %s\n", kv.c_str()); return 2; }
            sets.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
        }
        else if (a == "--disc-tab-off") discTabOff = true;
        else if (a == "--disc" && i + 1 < argc) {
            const std::string d = argv[++i];
            using M = patsgui::DiscStatus::Media;
            if (d == "nodrive") media = M::NoDrive;
            else if (d == "nodisc") media = M::NoDisc;
            else if (d == "checking") media = M::Checking;
            else if (d == "readable") media = M::Readable;
            else if (d == "unsupported") media = M::Unsupported;
            else if (d == "unreadable") media = M::Unreadable;
            else { std::fprintf(stderr, "unknown disc state: %s\n", d.c_str()); return 2; }
        } else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
    }
    if (listFirstRun) {
        for (const std::string &id : patsgui::FirstRun::pageIds()) std::printf("%s\n", id.c_str());
        return 0;
    }
    if (!firstRunPage.empty() && only.empty()) only = "home";
    const int width = height * 16 / 9;
    mkdir(out.c_str(), 0755);

    Font regular, bold;
    if (!loadFonts(regular, bold)) std::fprintf(stderr, "note: no TrueType font found, using the built-in face\n");
    registerFrameworkStrings();
    patsgui::registerStrings();
    if (pseudo) ui::Catalog::get().setPseudo(true);

    Theme theme(width, height);
    look.buttonLayout = layout;
    patsgui::applyLook(theme, look);
    Canvas canvas(width, height);
    SoftPainter painter(canvas, regular, bold);
    patsgui::Assets assets = patsgui::makeAssets(theme, nullptr);

    patsgui::SampleLibrary library;
    library.setDisc(media);
    patsgui::Services services{library};
    patsgui::SampleSettingsServices sampleSettings;   // the Settings tab's services: fakes with settable state machines
    services.settings = &sampleSettings.services();
    if (listSamples) {
        for (const std::string &line : sampleSettings.help()) std::printf("%s\n", line.c_str());
        return 0;
    }
    // The Accessibility sample starts from --look and drives the look afterwards (the device's look bridge does the same
    // from Preferences): sizes, high contrast and its color change the theme at every draw, and the font being off or
    // missing shows the built-in face, as on the device.
    sampleSettings.accessibility.fontScale = look.textScale;
    sampleSettings.accessibility.hintScale = look.hintScale;
    sampleSettings.accessibility.contrast = look.highContrast;
    sampleSettings.accessibility.contrastColor = look.highlight;
    sampleSettings.interfaceUi.theme = look.colorTheme;   // the Interface sample starts from --look too
    sampleSettings.interfaceUi.dark = look.dark;
    bool hostFaceBuiltin = false;
    auto applySampleLook = [&]() {
        const patsgui::SampleAccessibilityService &acc = sampleSettings.accessibility;
        look.textScale = acc.fontScale;
        look.hintScale = acc.hintScale;
        look.highContrast = acc.contrast;
        look.highlight = acc.contrastColor;
        look.colorTheme = sampleSettings.interfaceUi.theme;   // Settings > Interface: color theme and dark mode
        look.dark = sampleSettings.interfaceUi.dark;
        patsgui::applyLook(theme, look);
        const bool builtin = !(acc.installed && acc.enabled);
        if (builtin != hostFaceBuiltin) {
            hostFaceBuiltin = builtin;
            if (builtin) { regular.load({}); bold.load({}); } else loadFonts(regular, bold);
        }
    };
    for (const auto &kv : sets)
        if (!sampleSettings.set(kv.first, kv.second)) { std::fprintf(stderr, "unknown sample state: %s=%s (see --list-samples)\n", kv.first.c_str(), kv.second.c_str()); return 2; }
    patsgui::SystemValues values;
    values.clock = "10:42"; values.date = "Tue 22 Nov"; values.disks = {{"fat", 7, "128GB", false}, {"usb0", 80, "512GB", false}, {"usb1", 97, "64GB", true}}; values.address = "192.168.1.20"; values.link = patsgui::NetworkLink::Wifi; values.version = "0.9.1";

    static const char *names[int(patsgui::Tab::Count)] = {"disc", "home", "favorites", "systems", "arcade", "games", "settings"};
    for (int i = 0; i < int(patsgui::Tab::Count); ++i) {
        if (!only.empty() && only != names[i]) continue;
        if (useStart && i != 0) continue;   // --start renders the one tab the Start tab names
        patsgui::State state;
        state.tab = patsgui::Tab(i);
        state.useStartTab = useStart;
        state.ownsSounds = ownSounds;
        state.favoriteHold = favoriteHold;
        state.removeHold = removeHold;
        patsgui::attach(state, services);
        patsgui::update(state, 0.0f);
        if (!firstRunPage.empty() && !state.firstRun.startAt(services, firstRunPage)) {
            std::fprintf(stderr, "no such first-run page: %s (see --list-first-run)\n", firstRunPage.c_str());
            return 2;
        }
        auto *games = dynamic_cast<patsgui::GamesTab *>(state.views[size_t(patsgui::Tab::Games)].get());
        if (games && listView) games->setViewForTest(2);
        auto *settings = dynamic_cast<patsgui::SettingsTab *>(state.views[size_t(patsgui::Tab::Settings)].get());
        if (settings && listPages && state.tab == patsgui::Tab::Settings) {
            for (const std::string &id : settings->pageIdsForTest()) std::printf("%s\n", id.c_str());
            return 0;
        }
        if (settings && state.tab == patsgui::Tab::Settings) {
            if (!category.empty()) settings->focusCategoryForTest(category);
            if (!page.empty()) {
                // --page [CATEGORY/]PAGE: a page id as --list-pages prints it, or just the page with --category
                std::string cat = category, id = page;
                const size_t slash = page.find('/');
                if (slash != std::string::npos) { cat = page.substr(0, slash); id = page.substr(slash + 1); }
                if (cat.empty() || !settings->openPageForTest(cat, id)) {
                    std::fprintf(stderr, "no such page: %s (see --list-pages)\n", page.c_str());
                    return 2;
                }
            }
        }
        auto drawAll = [&]() {
            applySampleLook();
            // Settings > Interface: tab display and the cover settings (the device reads them from Preferences).
            const int display = sampleSettings.interfaceUi.display;
            state.tabDisplay = display == 1 ? ui::TabDisplay::Icons : display == 2 ? ui::TabDisplay::Both : ui::TabDisplay::Names;
            library.setCoverSettings(sampleSettings.interfaceUi.boxArt, sampleSettings.interfaceUi.smallGrid);
            const ShellSpec shell = patsgui::shellFor(state, values, assets);
            const Rect content = drawShell(painter, theme, shell);
            patsgui::drawTab(painter, theme, state, content);
            patsgui::drawOverlays(painter, theme, state, shell);
        };
        drawAll();   // the first draw tells the tab its sizes and loads its first window
        if (games && options && state.tab == patsgui::Tab::Games) games->openGameOptions();
        drawAll();
        for (float t = 0; t < seconds; t += 0.1f) { library.update(0.1f); patsgui::update(state, 0.1f); }
        drawAll();
        for (size_t at = 0; at < keys.size() && !keys.empty() && bench == 0;) {   // --keys: drive the tab like the controller would
            const size_t comma = keys.find(',', at);
            const std::string word = keys.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
            at = comma == std::string::npos ? keys.size() : comma + 1;
            patsgui::Event e;
            if (!eventNamed(word, e)) { std::fprintf(stderr, "unknown key: %s\n", word.c_str()); return 2; }
            if (patsgui::handle(state, e)) {   // true = the shell asks to be closed (never today)
                std::printf("closed: the GUI asked to leave at '%s'\n", word.c_str());
                break;
            }
            library.update(0.1f);
            patsgui::update(state, 0.1f);
            drawAll();
            drawAll();
        }
        if (discTabOff) {
            library.setDiscTabAvailable(false);
            patsgui::update(state, 0.0f);
            drawAll();
            drawAll();
        }
        if (!notify.empty()) {
            state.notice.toast(ui::Text::data(notify));
            patsgui::update(state, 0.0f);
            drawAll();
            drawAll();
        }
        if (!keys.empty()) std::printf("after the keys: tab %s\n", names[int(state.tab)]);
        if (sampleSettings.developer.openedWireframes > 0)
            std::printf("sample developer: Wireframes opened %d time(s)\n", sampleSettings.developer.openedWireframes);
        for (float t = 0; t < after; t += 0.1f) { patsgui::update(state, 0.1f); }
        if (after > 0.0f) { drawAll(); drawAll(); }
        if (bench > 0) {
            using Clock = std::chrono::steady_clock;
            auto ms = [](Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
            std::vector<std::string> words;
            for (size_t at = 0; at < keys.size() && !keys.empty();) {
                const size_t comma = keys.find(',', at);
                words.push_back(keys.substr(at, comma == std::string::npos ? std::string::npos : comma - at));
                at = comma == std::string::npos ? keys.size() : comma + 1;
            }
            Canvas shown(width, height);   // what the damage path keeps on screen
            RetainedFrame frame(width, height, regular, bold);
            Canvas::Work fullWork, damageWork;
            struct Row { double fullMs = 0, recordMs = 0, replayMs = 0, dirtyPx = 0, calls = 0, replayed = 0; Canvas::Work full, damage; int differ = 0, fullRepaints = 0; };
            std::vector<Row> rows(words.size());
            auto fullFrame = [&](Canvas::Work *work) {
                canvas.setWork(work);
                const ShellSpec shell = patsgui::shellFor(state, values, assets);
                const Rect content = drawShell(painter, theme, shell);
                patsgui::drawTab(painter, theme, state, content);
                patsgui::drawOverlays(painter, theme, state, shell);
                canvas.setWork(nullptr);
            };
            auto damageFrame = [&](Canvas::Work *work, double &record, double &replay) {
                const auto t0 = Clock::now();
                frame.begin();
                const ShellSpec shell = patsgui::shellFor(state, values, assets);
                const Rect content = drawShell(frame, theme, shell);
                patsgui::drawTab(frame, theme, state, content);
                patsgui::drawOverlays(frame, theme, state, shell);
                const auto t1 = Clock::now();
                shown.setWork(work);
                frame.commit(shown);
                shown.setWork(nullptr);
                record = ms(t0, t1);
                replay = ms(t1, Clock::now());
            };
            double r0, r1;
            applySampleLook();
            fullFrame(nullptr);
            damageFrame(nullptr, r0, r1);   // the first frame is a full one for both
            for (int round = 0; round < bench; ++round)
                for (size_t k = 0; k < words.size(); ++k) {
                    patsgui::Event e;
                    if (!eventNamed(words[k], e)) { std::fprintf(stderr, "unknown key: %s\n", words[k].c_str()); return 2; }
                    patsgui::handle(state, e);
                    library.update(0.1f);
                    patsgui::update(state, 0.1f);
                    applySampleLook();
                    Row &row = rows[k];
                    Canvas::Work w1, w2;
                    const auto t0 = Clock::now();
                    fullFrame(&w1);
                    row.fullMs += ms(t0, Clock::now());
                    double rec, rep;
                    damageFrame(&w2, rec, rep);
                    row.recordMs += rec; row.replayMs += rep;
                    row.dirtyPx += double(frame.stats().dirtyPixels);
                    row.calls += frame.stats().calls; row.replayed += frame.stats().replayed;
                    if (frame.stats().full) ++row.fullRepaints;
                    row.full.opaque += w1.opaque; row.full.blended += w1.blended; row.full.copied += w1.copied; row.full.glyphs += w1.glyphs;
                    row.damage.opaque += w2.opaque; row.damage.blended += w2.blended; row.damage.copied += w2.copied; row.damage.glyphs += w2.glyphs;
                    for (int y = 0; y < height; ++y)
                        if (std::memcmp(canvas.row(y), shown.row(y), size_t(width) * 4) != 0) { ++row.differ; break; }
                }
            std::printf("bench %s, %d round(s), %dx%d; per interaction (average): pixels = filled + blended + copied, in millions\n", names[i], bench, width, height);
            std::printf("%-8s | %-34s | %-52s | %s\n", "key", "BEFORE: full repaint", "AFTER: damage path (record + repaint)", "check");
            std::printf("%-8s | %7s %7s %7s %7s | %7s %7s %7s %7s %6s %6s %5s | %s\n", "", "ms", "Mpix", "blend", "glyphs", "ms", "rec ms", "Mpix", "glyphs", "dirty%", "calls", "full", "");
            const double n = bench;
            for (size_t k = 0; k < words.size(); ++k) {
                const Row &r = rows[k];
                const double fp = double(r.full.opaque + r.full.blended + r.full.copied) / n / 1e6, fb = double(r.full.blended) / n / 1e6;
                const double dp = double(r.damage.opaque + r.damage.blended + r.damage.copied) / n / 1e6;
                std::printf("%-8s | %7.2f %7.2f %7.2f %7.0f | %7.2f %7.2f %7.2f %7.0f %6.1f %6.0f %5d | %s\n", words[k].c_str(),
                            r.fullMs / n, fp, fb, double(r.full.glyphs) / n, (r.recordMs + r.replayMs) / n, r.recordMs / n, dp,
                            double(r.damage.glyphs) / n, 100.0 * r.dirtyPx / n / (double(width) * height), r.calls / n, r.fullRepaints,
                            r.differ ? "DIFFERS" : "identical");
                if (r.differ) return 3;
            }
        }
        if (partialFrames > 0) {
            // Video-only redraw check: the canvas keeps the last full frame; each new video frame is painted by
            // redrawPreview() alone and must equal a full redraw (drawn into a second canvas) pixel for pixel.
            using Clock = std::chrono::steady_clock;
            auto ms = [](Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
            Canvas reference(width, height);
            SoftPainter refPainter(reference, regular, bold);
            auto drawFull = [&](SoftPainter &pp) {
                const ShellSpec shell = patsgui::shellFor(state, values, assets);
                const Rect content = drawShell(pp, theme, shell);
                patsgui::drawTab(pp, theme, state, content);
                patsgui::drawOverlays(pp, theme, state, shell);
            };
            int mismatched = 0, fellBack = 0, revisionChanged = 0, partials = 0;
            double fullMs = 0, partialMs = 0, damagePixels = 0;
            for (int k = 0; k < partialFrames; ++k) {
                const uint64_t rev = library.revision(), serial = library.previewFrameSerial();
                library.update(1.0f / 15.0f);
                patsgui::update(state, 1.0f / 15.0f);
                if (library.revision() != rev) { ++revisionChanged; drawAll(); continue; }
                if (library.previewFrameSerial() == serial) continue;   // no new frame
                Rect damage;
                const auto t0 = Clock::now();
                const bool ok = patsgui::redrawPreview(state, painter, theme, damage);
                const auto t1 = Clock::now();
                if (!ok) { ++fellBack; drawAll(); continue; }
                partialMs += ms(t0, t1);
                damagePixels += double(damage.w) * damage.h;
                ++partials;
                const auto t2 = Clock::now();
                drawFull(refPainter);
                fullMs += ms(t2, Clock::now());
                for (int y = 0; y < height; ++y)
                    if (std::memcmp(canvas.row(y), reference.row(y), size_t(width) * 4) != 0) {
                        if (mismatched == 0)
                            for (int x = 0; x < width; ++x)
                                if (canvas.row(y)[x] != reference.row(y)[x]) {
                                    std::printf("first difference at %d,%d (damage %d,%d %dx%d)\n", x, y, damage.x, damage.y, damage.w, damage.h);
                                    break;
                                }
                        ++mismatched;
                        break;
                    }
            }
            std::printf("%s partial check: %d partial frames, %d fell back to full, %d revision changes, %d differ from a full redraw\n",
                        names[i], partials, fellBack, revisionChanged, mismatched);
            if (partials > 0)
                std::printf("  full redraw avg %.2f ms, partial redraw avg %.3f ms (damage avg %.0f px of %d = %.1f %%)\n",
                            fullMs / partials, partialMs / partials, damagePixels / partials, width * height,
                            100.0 * damagePixels / partials / (double(width) * height));
            if (mismatched > 0 || partials == 0) return 3;
        }
        std::string name = std::string(useStart ? "start-" : "") + names[useStart ? int(state.tab) : i];
        if (state.tab == patsgui::Tab::Settings && (!category.empty() || !page.empty())) {
            if (!category.empty() && page.find('/') == std::string::npos) name += "-" + category;   // app-settings-system-reboot.png
            if (!page.empty()) { std::string id = page; for (char &ch : id) if (ch == '/') ch = '-'; name += "-" + id; }
        }
        if (!firstRunPage.empty()) { std::string id = firstRunPage; for (char &ch : id) if (ch == '.') ch = '-'; name += "-first-" + id; }
        if (checkLayoutOn) {   // one more frame, recorded: text outside the screen, cut, overlapping, elided
            std::vector<SoftPainter::TextRun> runs;
            painter.setTextRecorder(&runs);
            drawAll();
            painter.setTextRecorder(nullptr);
            layoutFindings += checkLayout(runs, canvas.bounds(), name + suffix);
        }
        const std::string path = out + "/app-" + name + suffix + ".png";
        if (!writePng(canvas, path)) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return 1; }
        std::printf("wrote %s\n", path.c_str());
    }
    if (layoutFindings > 0) { std::printf("layout: %d finding(s)\n", layoutFindings); return 4; }
    return 0;
}
