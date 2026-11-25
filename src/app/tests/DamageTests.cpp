// The damage path (ui::RetainedFrame) against a full repaint: after every step of a scenario the canvas the damage path
// keeps must equal, pixel for pixel, the canvas of a frame drawn completely with SoftPainter. Covers what the owner feels
// (keyboard moves, Input test, list cursors, tab switch, Grid), dialogs opening and closing, a page changing behind a
// dialog (the Busy page's clock, the shell's clock and disk usage under a scrim, a toast), a look change and a resize.
// What the damage path must NOT do is also checked: an unchanged frame repaints nothing and a keyboard move repaints a
// small share of the screen. See src/app/README.md "Rendering and damage".

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "app/GamesTab.h"
#include "app/Look.h"
#include "app/PatsGui.h"
#include "app/SampleLibrary.h"
#include "app/SettingsTab.h"
#include "app/settings/SampleSettingsServices.h"
#include "app/tests/Rig.h"
#include "app/tests/Test.h"
#include "ui-framework/kit/Components.h"
#include "ui-framework/render/soft/Canvas.h"
#include "ui-framework/render/soft/Font.h"
#include "ui-framework/render/soft/RetainedFrame.h"
#include "ui-framework/render/soft/SoftPainter.h"

namespace tt {
namespace {

bool loadFace(ui::Font &regular, ui::Font &bold) {
    const char *r[] = {"/System/Library/Fonts/Supplemental/Arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
    const char *b[] = {"/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"};
    return regular.load({r[0], r[1]}) && bold.load({b[0], b[1]});
}

struct Scene {
    int width, height;
    ui::Font regular, bold;
    ui::Theme theme;
    patsgui::SampleLibrary library;
    patsgui::SampleSettingsServices samples;
    patsgui::Services services;
    patsgui::State state;
    patsgui::Assets assets;
    patsgui::SystemValues values;
    patsgui::LookSettings look;
    ui::Canvas reference, shown;                 // a full repaint every step / what the damage path keeps
    std::unique_ptr<ui::RetainedFrame> frame;
    int steps = 0, mismatches = 0;
    double lastShare = 0;                        // share of the screen the last step repainted, percent
    int lastRects = 0;

    explicit Scene(int h = 1080, patsgui::Tab tab = patsgui::Tab::Settings)
        : width(h * 16 / 9), height(h), theme(h * 16 / 9, h), services{library}, reference(h * 16 / 9, h), shown(h * 16 / 9, h) {
        ui::registerFrameworkStrings();
        patsgui::registerStrings();
        loadFace(regular, bold);
        patsgui::applyLook(theme, look);
        assets = patsgui::makeAssets(theme, nullptr);
        services.settings = &samples.services();
        values.clock = "10:42"; values.date = "Tue 22 Nov"; values.address = "192.168.1.20";
        values.link = patsgui::NetworkLink::Wifi; values.version = "0.9.1";
        values.disks = {{"fat", 7, "128GB", false}, {"usb0", 80, "512GB", false}};
        state.tab = tab;
        patsgui::attach(state, services);
        patsgui::update(state, 0.0f);
        frame.reset(new ui::RetainedFrame(width, height, regular, bold));
        step("first frame");
    }

    patsgui::SettingsTab *settings() { return dynamic_cast<patsgui::SettingsTab *>(state.views[size_t(patsgui::Tab::Settings)].get()); }

    void draw(ui::Painter &p) {
        const ui::ShellSpec shell = patsgui::shellFor(state, values, assets);
        const ui::Rect content = ui::drawShell(p, theme, shell);
        patsgui::drawTab(p, theme, state, content);
        patsgui::drawOverlays(p, theme, state, shell);
    }

    // One frame both ways, then the comparison. A new look must call lookChanged() first (what the bridge's syncLook does).
    void step(const std::string &what) {
        ++steps;
        ui::SoftPainter painter(reference, regular, bold);
        draw(painter);
        frame->begin();
        draw(*frame);
        frame->commit(shown);
        lastShare = 100.0 * double(frame->stats().dirtyPixels) / (double(width) * height);
        lastRects = 0;
        context() += " [" + what + "]";
        for (int y = 0; y < height; ++y)
            if (std::memcmp(reference.row(y), shown.row(y), size_t(width) * 4) != 0) {
                int x = 0;
                while (reference.row(y)[x] == shown.row(y)[x]) ++x;
                ++mismatches;
                CHECK_MSG(false, "step '" + what + "': the damage path differs from a full repaint at " + std::to_string(x) + "," + std::to_string(y));
                return;
            }
        CHECK(true);
    }

    void send(const std::string &keys) {
        for (size_t at = 0; at < keys.size();) {
            const size_t comma = keys.find(',', at);
            const std::string word = keys.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
            at = comma == std::string::npos ? keys.size() : comma + 1;
            patsgui::Event e;
            CHECK(eventNamed(word, e));
            patsgui::handle(state, e);
            library.update(0.1f);
            patsgui::update(state, 0.1f);
            step(word);
        }
    }
    void wait(float seconds, const std::string &what) {
        for (float t = 0; t < seconds; t += 0.1f) {
            library.update(0.1f);
            patsgui::update(state, 0.1f);
            step(what);
        }
    }
    void openPage(const std::string &category, const std::string &page) {
        settings()->focusCategoryForTest(category);
        CHECK(settings()->openPageForTest(category, page.substr(page.find('/') + 1)));   // "system/wifi.password" as --list-pages prints it
        step("open " + page);
    }
    void lookChanged() { patsgui::applyLook(theme, look); frame->invalidate(); }
};

TEST(damage_keyboard_moves_and_typing) {
    Scene s;
    s.openPage("system", "system/wifi.password");
    s.send("right,right,down,a,a,left,up,x,y,down,down,left,left,a,right,right,right,up,up,a");
    CHECK(s.mismatches == 0);
    s.send("left");   // a key move repaints a small part of the screen, not the page behind the dialog
    CHECK_MSG(s.lastShare < 5.0, "repainted " + std::to_string(s.lastShare) + " % for one key move");
    s.wait(0.2f, "idle");
    CHECK_MSG(s.lastShare == 0.0, "an unchanged frame repainted " + std::to_string(s.lastShare) + " %");
}

TEST(damage_input_test_page) {
    Scene s;
    s.samples.set("controllers.script", "test");
    s.openPage("controllers", "controllers/test");
    s.wait(8.0f, "input test");
    s.samples.set("controllers.pad.keys", "south,tl,dpadup");
    s.samples.set("controllers.pad.stick", "x:0.7,y:-0.4");
    s.wait(0.5f, "keys held");
    s.samples.set("controllers.pad.keys", "east");
    s.samples.set("controllers.pad.stick", "x:-0.2,y:0.9,lt:0.5");
    s.wait(0.5f, "other keys");
}

TEST(damage_settings_list_and_tabs) {
    Scene s;
    s.send("right,down,down,down,up,left,down,down,right,a,b,left,up");
    s.state.tab = patsgui::Tab::Settings;
    s.send("tabnext,tabnext,tabprev,tabprev,tabprev,tabnext");
}

TEST(damage_grid_cursor) {
    Scene s(1080, patsgui::Tab::Games);
    s.send("right,right,down,down,left,up,up,right,lnext,lnext,lprev,down,a,b");
    s.send("y,down,y,up");
}

TEST(damage_dialogs_open_and_close) {
    Scene s;
    s.openPage("system", "system/reboot");
    s.send("b");
    s.openPage("library", "library/scan.intro");
    s.send("a,a,b,b");
    s.openPage("system", "system/wifi.ssid");
    s.send("a,right,a,b,b");
    s.openPage("standard", "standard/confirm");
    s.send("left,right,b");
}

TEST(damage_page_behind_a_dialog_changes) {
    Scene s;
    s.openPage("library", "library/scan.working");
    s.wait(4.0f, "busy clock");   // the Busy page counts seconds
    // The shell behind the scrim changes under an open dialog: the clock, the disk usage, the network.
    s.openPage("system", "system/wifi.password");
    s.values.clock = "10:43";
    s.step("clock behind the dialog");
    s.values.disks[0].percentUsed = 91;
    s.values.disks[0].low = true;
    s.step("disk usage behind the dialog");
    s.values.link = patsgui::NetworkLink::Offline;
    s.values.address.clear();
    s.step("network behind the dialog");
    // A toast over a page without a dialog, and while it fades.
    s.send("b,b");
    s.state.notice.toast(ui::Text::data("Time zone set to Berlin"));
    s.wait(5.0f, "toast");
    s.state.tab = patsgui::Tab::Home;
    s.step("other tab");
}

TEST(damage_look_change) {
    Scene s;
    s.openPage("system", "system/wifi.password");
    s.look.textScale = 130;
    s.lookChanged();
    s.step("text 130");
    s.send("right,down");
    s.look.highContrast = true;
    s.lookChanged();
    s.step("high contrast");
    s.look.highContrast = false; s.look.dark = false; s.look.colorTheme = 3;
    s.lookChanged();
    s.step("light, theme 3");
    s.send("left,up");
    // The font face: the same text and geometry, other glyphs. Only invalidate() can know.
    s.regular.load({}); s.bold.load({});
    s.lookChanged();
    s.step("built-in face");
    loadFace(s.regular, s.bold);
    s.lookChanged();
    s.step("TrueType face");
    s.look.buttonLayout = ui::ButtonLayout::PlayStation;
    s.lookChanged();
    s.step("PlayStation labels");
}

TEST(damage_resize) {
    Scene big(1080), small(720);   // a new size is a new frame (the bridge builds one in ensureResources)
    big.openPage("system", "system/wifi.password");
    small.openPage("system", "system/wifi.password");
    big.send("right,down");
    small.send("right,down,a");
    CHECK(big.mismatches == 0 && small.mismatches == 0);
}

}  // namespace
}  // namespace tt
