// Host runner of the UI Wireframes demo: renders the splash and every tab to PNG files.
//
//   ui-demo [--out DIR] [--height 1080] [--text 100|115|130] [--pseudo] [--warning] [--layout xbox|playstation|nintendo]
//
// The same demo code runs on the device through the framebuffer presenter (a later step).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "ui-framework/demo/Demo.h"
#include "ui-framework/kit/Contrast.h"
#include "ui-framework/kit/TextEntryDialog.h"
#include "ui-framework/platform/host/PngWriter.h"
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

int main(int argc, char **argv) {
    std::string out = "build/out";
    int height = 1080, textScale = 100;
    bool pseudo = false;
    ButtonLayout layout = ButtonLayout::Xbox;
    bool contrastReport = false;
    demo::Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--out" && i + 1 < argc) out = argv[++i];
        else if (a == "--height" && i + 1 < argc) height = std::atoi(argv[++i]);
        else if (a == "--text" && i + 1 < argc) textScale = std::atoi(argv[++i]);
        else if (a == "--pseudo") pseudo = true;
        else if (a == "--layout" && i + 1 < argc) layout = buttonLayoutFromName(argv[++i]);
        else if (a == "--warning") options.forceWarning = true;
        else if (a == "--contrast") contrastReport = true;
        else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
    }
    const int width = height * 16 / 9;
    mkdir(out.c_str(), 0755);

    if (contrastReport) {
        // The contrast of every color pair, for every color theme in dark and light mode and for high contrast
        // with every highlight color. Exits with 1 when a pair falls short.
        int failures = 0, configs = 0;
        auto run = [&](const std::string &name, Theme &th) {
            ++configs;
            const std::vector<ContrastCheck> checks = checkContrast(th);
            std::string bad;
            int count = 0;
            for (const ContrastCheck &c : checks)
                if (!c.ok()) {
                    char line[160];
                    std::snprintf(line, sizeof(line), "    %-34s %.2f : 1 (needs %.1f)\n", c.name.c_str(), c.ratio, c.minimum);
                    bad += line;
                    ++count;
                }
            failures += count;
            std::printf("%-28s %s\n%s", name.c_str(), count ? (std::to_string(count) + " pair(s) fall short").c_str() : "ok", bad.c_str());
        };
        Theme probe(1920, 1080);
        for (int ct = 0; ct < probe.colorThemeCount(); ++ct)
            for (int dark = 1; dark >= 0; --dark) {
                probe.setHighContrast(false);
                probe.setColorTheme(ct);
                probe.setDarkMode(dark != 0);
                run(std::string(probe.colorThemeName(ct)) + (dark ? " (dark)" : " (light)"), probe);
            }
        int hcount = 0;
        const HighlightColor *colors = highlightColors(hcount);
        for (int h = 0; h < hcount; ++h) {
            probe.setHighContrast(true, h);
            run(std::string("High contrast, ") + colors[h].name, probe);
        }
        std::printf("\n%d configurations, %d pair(s) fall short\n", configs, failures);
        return failures ? 1 : 0;
    }

    Font regular, bold;
    if (!loadFonts(regular, bold)) std::fprintf(stderr, "note: no TrueType font found, using the built-in face\n");

    registerFrameworkStrings();
    demo::registerStrings();
    Catalog::get().setPseudo(pseudo);

    Theme theme(width, height);
    theme.setTextScale(textScale);
    theme.setButtonLayout(layout);

    Canvas canvas(width, height);
    SoftPainter painter(canvas, regular, bold);
    demo::Assets assets = demo::makeAssets(theme);

    auto save = [&](const std::string &name) {
        const std::string path = out + "/" + name;
        if (!writePng(canvas, path)) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return false; }
        std::printf("wrote %s\n", path.c_str());
        return true;
    };

    // Splash: a normal start, and a start known to be long (one calm line).
    drawSplash(painter, theme, assets.logo.get(), 0.62f, "");
    if (!save("splash.png")) return 1;
    Catalog::get().set("demo.splash.long", "Getting your games ready");
    drawSplash(painter, theme, assets.logo.get(), 0.4f, Text::key("demo.splash.long").str());
    if (!save("splash-long.png")) return 1;

    // The shell with each tab, and a few states that show the live settings and the grid's scroll.
    auto render = [&](const std::string &name, demo::State state) {
        state.textScale = textScale;
        state.pseudo = pseudo;
        state.forceWarning = state.forceWarning || options.forceWarning;
        demo::applyAppearance(state, theme);
        const ShellSpec shell = demo::shellFor(state, options, demo::SystemValues(), assets);
        const Rect content = drawShell(painter, theme, shell);
        demo::drawTab(painter, theme, state, content, assets);
        demo::drawOverlays(painter, theme, state, shell);
        return save(name);
    };
    static const char *names[demo::TabCount] = {"overview", "text", "lists", "data", "grids", "dialogs", "flows", "input", "appearance"};
    for (int tab = 0; tab < demo::TabCount; ++tab) {
        demo::State state;
        state.tab = tab;
        if (!render(std::string("shell-") + names[tab] + ".png", state)) return 1;
    }
    {
        demo::State state;
        state.tab = demo::Grids;
        state.cursor[demo::Grids] = 26;   // the grid scrolls and keeps the focus row visible
        if (!render("shell-grids-scrolled.png", state)) return 1;
        state.viewMode = 1;
        if (!render("shell-grids-small.png", state)) return 1;
        state.viewMode = 2;
        if (!render("shell-grids-gridlist.png", state)) return 1;
        state.viewMode = 0;
        // Light mode in a color theme, and high contrast: the same screens in another look.
        {
            demo::State look;
            look.tab = demo::Lists;
            look.dark = false;
            look.colorTheme = 2;
            if (!render("shell-lists-light.png", look)) return 1;
            look.tab = demo::Dialogs;
            look.cursor[demo::Dialogs] = 2;
            demo::openDemoDialog(look, 2, assets);
            if (!render("shell-dialog-error-light.png", look)) return 1;
            demo::State hc;
            hc.tab = demo::Lists;
            hc.highContrast = true;
            if (!render("shell-lists-highcontrast.png", hc)) return 1;
            hc.tab = demo::Grids;
            hc.viewMode = 2;
            hc.cursor[demo::Grids] = 5;
            if (!render("shell-gridlist-highcontrast.png", hc)) return 1;
        }
        // The toast: a confirmation, a one-line notice, and the confirmation in high contrast.
        {
            demo::State tt;
            tt.tab = demo::TextTab;
            demo::handle(tt, demo::Event::Confirm, assets);
            if (!render("shell-toast.png", tt)) return 1;
            demo::handle(tt, demo::Event::CycleView, assets);
            if (!render("shell-toast-notice.png", tt)) return 1;
            tt.highContrast = true;
            if (!render("shell-toast-highcontrast.png", tt)) return 1;
        }
        // Flows: the pages of the Medium flow, the pages of the Large flow, and the crash screen.
        {
            using demo::Event;
            demo::State fl;
            fl.tab = demo::Flows;
            if (!render("shell-flows.png", fl)) return 1;
            // Medium flow: Step (text), Step (single choice with a status line), Busy, Result.
            demo::handle(fl, Event::Confirm, assets);         // start the flow: page 0
            if (!render("shell-wizard-start.png", fl)) return 1;
            demo::handle(fl, Event::Confirm, assets);         // page 1, the choice
            if (!render("shell-wizard-choose.png", fl)) return 1;
            demo::handle(fl, Event::Confirm, assets);         // the first target: the busy page
            demo::update(fl, 1.5f);
            if (!render("shell-wizard-busy.png", fl)) return 1;
            demo::update(fl, 2.0f);
            if (!render("shell-wizard-done.png", fl)) return 1;
            demo::handle(fl, Event::Confirm, assets);         // Done closes the flow
            // The second target ends in the failure page with Try again; the third asks for raw input first.
            demo::handle(fl, Event::Confirm, assets);         // start again
            demo::handle(fl, Event::Confirm, assets);         // choice
            demo::handle(fl, Event::Down, assets);
            demo::handle(fl, Event::Confirm, assets);         // the second target
            demo::update(fl, 3.5f);
            if (!render("shell-wizard-error.png", fl)) return 1;
            demo::handle(fl, Event::Confirm, assets);         // Try again: the choice
            demo::handle(fl, Event::Down, assets);
            demo::handle(fl, Event::Confirm, assets);         // the third target: the capture page
            if (!render("shell-wizard-capture.png", fl)) return 1;
            demo::handle(fl, Event::Back, assets);            // raw input: Back is not interpreted, the page ignores it...
            demo::handle(fl, Event::Start, assets);           // ...and Start is its way out
            demo::handle(fl, Event::Back, assets);            // Back on the choice: the first page
            demo::handle(fl, Event::Back, assets);            // Back on the first page closes the flow
            // Large flow: multiple choice (nothing selected: Continue is off and says why), selected, keyboard, review,
            // busy without cancel, warning result.
            demo::State lf;
            lf.tab = demo::Flows;
            lf.cursor[demo::Flows] = 1;
            demo::handle(lf, Event::Confirm, assets);
            if (!render("shell-wizard-large-list-reason.png", lf)) return 1;
            demo::handle(lf, Event::Confirm, assets);         // "All items" on
            demo::handle(lf, Event::Down, assets);
            demo::handle(lf, Event::Confirm, assets);         // item 1 on, "All items" off
            for (int i = 0; i < 16; ++i) demo::handle(lf, Event::Down, assets);
            if (!render("shell-wizard-large-list.png", lf)) return 1;
            demo::handle(lf, Event::Start, assets);           // the keyboard page
            demo::handle(lf, Event::Confirm, assets);
            if (!render("shell-wizard-large-entry.png", lf)) return 1;
            demo::handle(lf, Event::Start, assets);           // Done with a typed letter
            if (!render("shell-wizard-large-review.png", lf)) return 1;
            demo::handle(lf, Event::Confirm, assets);         // Install
            demo::update(lf, 1.5f);
            if (!render("shell-wizard-large-busy.png", lf)) return 1;
            demo::update(lf, 2.0f);
            if (!render("shell-wizard-large-result.png", lf)) return 1;
            demo::State cr;
            cr.tab = demo::Flows;
            cr.crashPreview = true;
            if (!render("shell-crash.png", cr)) return 1;
        }
        // The Input tab: idle, and entering text in the dialog (typing, then a masked field).
        {
            demo::State in;
            in.tab = demo::Input;
            in.field[0] = "Sample name";
            if (!render("shell-input.png", in)) return 1;
            in.cursor[demo::Input] = 1;
            demo::handle(in, demo::Event::Confirm, assets);
            in.entry.handle(TextEntryEvent::Right);
            in.entry.handle(TextEntryEvent::Down);
            demo::handle(in, demo::Event::Confirm, assets);
            demo::handle(in, demo::Event::Confirm, assets);
            if (!render("shell-input-keyboard.png", in)) return 1;
        }
        // The shared PageHeader on the Grids tab: field idle (with a query), field focused, dialog open,
        // at 130 % text, in the pseudo-locale, light, and high contrast.
        {
            demo::State g;
            g.tab = demo::Grids;
            if (!render("shell-header.png", g)) return 1;
            g.gridSearch = "sample";
            g.searchFocused = true;
            if (!render("shell-header-focused.png", g)) return 1;
            demo::handle(g, demo::Event::Confirm, assets);
            g.entry.handle(TextEntryEvent::Down);
            demo::handle(g, demo::Event::Confirm, assets);
            if (!render("shell-header-dialog.png", g)) return 1;
            demo::State gl = g;
            gl.entryTarget = -1;
            gl.dark = false;
            gl.colorTheme = 2;
            if (!render("shell-header-light.png", gl)) return 1;
            demo::State gh = g;
            gh.entryTarget = -1;
            gh.highContrast = true;
            if (!render("shell-header-highcontrast.png", gh)) return 1;
            gh.entryTarget = 2;
            gh.entry = TextEntryDialog(TextEntrySpec());
            if (!render("shell-dialog-highcontrast.png", gh)) return 1;
        }
        // Each dialog variant over the Dialogs tab, and a dialog over a dialog.
        static const char *dialogNames[demo::kDialogCount] = {"info", "confirm", "error", "result", "progress", "choose", "viewer", "nested"};
        for (int i = 0; i < demo::kDialogCount; ++i) {
            demo::State d;
            d.tab = demo::Dialogs;
            d.cursor[demo::Dialogs] = i;
            demo::openDemoDialog(d, i, assets);
            if (!render(std::string("shell-dialog-") + dialogNames[i] + ".png", d)) return 1;
        }
        state.tab = demo::Appearance;
        state.tabDisplay = TabDisplay::Icons;
        if (!render("shell-appearance-icons.png", state)) return 1;
        state.tabDisplay = TabDisplay::Both;
        state.forceWarning = true;
        if (!render("shell-appearance-both-warning.png", state)) return 1;
    }
    // An icon sheet: every embedded icon, large, with its name (a check of the SVG renderer).
    {
        painter.clear(theme.palette.background);
        static const char *sheet[] = {"arrow-up", "check-lg", "clock", "collection", "controller", "diagram-3", "ethernet",
                                      "exclamation-circle", "fonts", "gear", "grid-3x3-gap", "hdd", "heart", "house",
                                      "info-circle", "joystick", "keyboard", "list-ul", "palette", "search", "star-fill",
                                      "wifi", "window", "x-lg"};
        const int columns = 8, cell = 225, size = 112, count = int(sizeof(sheet) / sizeof(sheet[0]));
        for (int i = 0; i < count; ++i) {
            const int x = 60 + (i % columns) * cell, y = 60 + (i / columns) * 300;
            drawSurface(painter, theme, {x, y, cell - 20, 270}, SurfaceRole::Faint, theme.px(Radius::panel));
            BootstrapIcon(sheet[i], size, size).drawIn(painter, {x + (cell - 20 - size) / 2, y + 30, size, size}, theme.palette.textPrimary);
            BootstrapIcon(sheet[i], 24, 24).drawIn(painter, {x + (cell - 20) / 2 - 12, y + 170, 24, 24}, theme.palette.accent);
            drawLabel(painter, theme, {x, y + 210, cell - 20, 40}, sheet[i], TextRole::Small, TextTier::Muted, Align::Center);
        }
        if (!save("icons.png")) return 1;
    }
    // One sheet with every theme in every mode: buttons, text tiers and the cursor row side by side.
    {
        struct Cfg { int theme; bool dark; bool hc; int highlight; std::string name; };
        std::vector<Cfg> configs;
        for (int ct = 0; ct < theme.colorThemeCount(); ++ct)
            for (int dark = 1; dark >= 0; --dark)
                configs.push_back({ct, dark != 0, false, 0, std::string(theme.colorThemeName(ct)) + (dark ? " dark" : " light")});
        int hcount = 0;
        const HighlightColor *colors = highlightColors(hcount);
        for (int h = 0; h < hcount; ++h) configs.push_back({0, true, true, h, std::string("HC ") + colors[h].name});
        // 18 configurations per sheet (6 columns, 3 rows): themes.png, themes-2.png, ...
        const int cols = 6, rowsPerSheet = 3, perSheet = cols * rowsPerSheet, cw = width / cols, ch = height / rowsPerSheet;
        const int sheets = int((configs.size() + perSheet - 1) / perSheet);
        for (int sheet = 0; sheet < sheets; ++sheet) {
        painter.clear(Color::rgb(0x000000));
        for (int k = 0; k < perSheet && size_t(sheet * perSheet + k) < configs.size(); ++k) {
            const size_t i = size_t(sheet * perSheet + k);
            const Cfg &c = configs[i];
            theme.setHighContrast(false);
            theme.setColorTheme(c.theme);
            theme.setDarkMode(c.dark);
            if (c.hc) theme.setHighContrast(true, c.highlight);
            const int x = (k % cols) * cw, y = (k / cols) * ch;
            painter.fillRect({x, y, cw, ch}, theme.palette.background);
            const int pad = theme.px(10);
            const Rect inner{x + pad, y + pad, cw - 2 * pad, ch - 2 * pad};
            drawLabel(painter, theme, {inner.x, inner.y, inner.w, theme.px(26)}, c.name, TextRole::Small, TextTier::Primary, Align::Left, true);
            int yy = inner.y + theme.px(30);
            drawActionButton(painter, theme, {inner.x, yy, inner.w, theme.px(48)}, "Start game", ButtonState::Ready, true);
            yy += theme.px(56);
            drawActionButton(painter, theme, {inner.x, yy, inner.w, theme.px(48)}, "Not ready", ButtonState::Unavailable, true);
            yy += theme.px(56);
            drawLabel(painter, theme, {inner.x, yy, inner.w, theme.px(26)}, "Primary text", TextRole::Small, TextTier::Primary);
            yy += theme.px(26);
            drawLabel(painter, theme, {inner.x, yy, inner.w, theme.px(26)}, "Muted text", TextRole::Small, TextTier::Muted);
            yy += theme.px(26);
            drawLabel(painter, theme, {inner.x, yy, inner.w, theme.px(26)}, "Faint text", TextRole::Small, TextTier::Faint);
            yy += theme.px(26);
            drawLabelColor(painter, theme, {inner.x, yy, inner.w, theme.px(26)}, "Accent text", TextRole::Small, theme.palette.accentText);
            yy += theme.px(30);
            ListItemSpec row;
            row.kind = ListItemKind::Action;
            row.title = Text::data("Cursor row");
            ListItemState st;
            st.cursor = true;
            drawListItem(painter, theme, {inner.x, yy, inner.w, listItemFrameHeight(theme, false)}, row, st);
        }
        if (!save(sheet == 0 ? "themes.png" : "themes-" + std::to_string(sheet + 1) + ".png")) return 1;
        }
    }
    return 0;
}
