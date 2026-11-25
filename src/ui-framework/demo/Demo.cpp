#include "ui-framework/kit/Data.h"
#include "ui-framework/demo/Demo.h"


#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {
namespace demo {

void registerStrings() {
    Catalog &c = Catalog::get();
    c.set("demo.tab.overview", "Overview");
    c.set("demo.tab.text", "Text");
    c.set("demo.tab.lists", "Lists");
    c.set("demo.tab.grids", "Grids");
    c.set("demo.tab.data", "Data");
    c.set("demo.tab.dialogs", "Dialogs");
    c.set("demo.tab.flows", "Flows");
    c.set("demo.tab.input", "Input");
    c.set("demo.tab.appearance", "Appearance");
    c.set("demo.footer.version", "Version 0.9.0-dev");
    c.set("demo.nav.disk", "Disk usage");
    c.set("demo.nav.diskLow", "Low space");
    c.set("demo.nav.ethernet", "Ethernet");
    c.set("demo.nav.wifi", "Wi-Fi");
    c.set("demo.nav.offline", "Offline");

    c.set("demo.input.fields", "Text fields");
    c.set("demo.input.name", "Name");
    c.set("demo.input.password", "Password");
    c.set("demo.input.start", "Start");
    c.set("demo.grids.search", "Search items");
    c.set("demo.grids.searchTitle", "Search");
    c.set("demo.input.notReady", "Not ready: choose a name first");
    c.set("demo.input.about", "Input");
    c.set("demo.input.aboutText",
          "A text field shows its value; A opens the keyboard. On the keyboard the D-pad chooses a key and A "
          "presses it. A password shows one star per character. The large buttons run an action; a button that "
          "cannot run says why.");
    c.set("hint.edit", "Edit");
    c.set("demo.data.table", "Table");
    c.set("demo.data.kv", "Key-value list");
    c.set("demo.data.results", "Result list");
    c.set("demo.data.progress", "Progress panel");
    c.set("demo.data.empty", "Empty state");
    c.set("demo.col.name", "Name");
    c.set("demo.col.year", "Year");
    c.set("demo.col.maker", "Maker");
    c.set("demo.col.works", "Works");
    c.set("demo.kv.system", "System");
    c.set("demo.kv.version", "Version");
    c.set("demo.kv.storage", "Storage free");
    c.set("demo.kv.network", "Network");
    c.set("demo.kv.connection", "Connection");
    c.set("demo.kv.connected", "Connected");
    c.set("demo.kv.address", "Address");
    c.set("demo.res.uptodate", "Up to date");
    c.set("demo.res.update", "Update available");
    c.set("demo.res.failed", "Failed");
    c.set("demo.res.working", "Checking...");
    c.set("demo.res.itemA", "First item");
    c.set("demo.res.itemB", "Second item");
    c.set("demo.res.itemC", "Third item");
    c.set("demo.res.itemD", "Fourth item");
    c.set("demo.res.reason", "Details: the file could not be read (example)");
    c.set("demo.res.size", "12.4 MB");
    c.set("demo.prog.title", "Looking for items");
    c.set("demo.prog.status", "Reading the folders");
    c.set("demo.prog.detail", "12 items found");
    c.set("demo.empty.title", "Nothing here yet");
    c.set("demo.empty.why", "No items match the current filter.");
    c.set("demo.empty.next", "Press {B} to clear the filter.");
    c.set("demo.flows.title", "Flows");
    c.set("demo.flows.wizard", "Set-up flow, Medium frame (text, choose, capture, busy, result)");
    c.set("demo.flows.large", "Update flow, Large frame (multiple choice, keyboard, review, busy, result)");
    c.set("demo.flows.crash", "Failure screen (preview)");
    c.set("demo.flows.about", "Flows and takeovers");
    c.set("demo.flows.aboutText",
          "A flow walks through pages: a question, a choice, work that runs, and an outcome. Every page of a flow "
          "is drawn in the same frame, so the dialog never jumps. B goes back one page, and the help always says "
          "what A does. In the first flow, choose the second target to see the failure page with Try again, and "
          "the third to see a page that takes raw input. The second flow shows a list with several selections, "
          "the keyboard, a review that scrolls, work that cannot be cancelled and an outcome with a warning. The "
          "failure screen replaces the whole screen when nothing can continue; here A or B returns.");
    c.set("demo.wizard.start.title", "Set up a connection");
    c.set("demo.wizard.start.headline", "Connect to a target");
    c.set("demo.wizard.start.body", "This flow asks for one choice and then does the work.");
    c.set("demo.wizard.choose.title", "Choose a target");
    c.set("demo.wizard.busy.title", "Setting up");
    c.set("demo.wizard.busy.status", "Reading your folders");
    c.set("demo.wizard.busy.wait", "Big collections take a few minutes. Nothing is stuck.");
    c.set("demo.wizard.done.title", "All set");
    c.set("demo.wizard.done.status", "Done");
    c.set("demo.wizard.done.body", "The connection is ready to use.");
    c.set("demo.wizard.fail.title", "Could not connect");
    c.set("demo.wizard.fail.what", "The target did not answer.");
    c.set("demo.wizard.fail.todo", "Nothing was changed. Choose another target, or try again.");
    c.set("demo.wizard.fail.details", "Details: no answer after 10 seconds (example)");
    c.set("demo.wizard.fail.reason", "no answer after 10 seconds (example)");
    c.set("demo.wizard.start.note", "Nothing is changed before you confirm. You can go back at any step.");
    c.set("demo.wizard.choose.note.ready", "Ready to use");
    c.set("demo.wizard.choose.note.slow", "Answers slowly; the next step may fail.");
    c.set("demo.wizard.choose.note.pair", "Needs a button to be pressed first.");
    c.set("demo.wizard.target.one", "First target");
    c.set("demo.wizard.target.two", "Second target");
    c.set("demo.wizard.target.three", "Third target");
    c.set("demo.wizard.target.detail", "Example text for the target");
    c.set("demo.wizard.status.ready", "Ready");
    c.set("demo.wizard.status.slow", "Slow");
    c.set("demo.wizard.status.pair", "Needs setup");
    c.set("demo.wizard.note", "Note");
    c.set("demo.wizard.busy.detail", "Folder 3 of 12: Super Nintendo");
    c.set("demo.wizard.busy.facts", "3 folders, 412 items");
    c.set("demo.wizard.done.headline", "All set");
    c.set("demo.wizard.done.changed", "The connection is ready to use.");
    c.set("demo.wizard.done.next", "You can change the target later in the settings.");
    c.set("demo.wizard.capture.title", "Pair a button");
    c.set("demo.wizard.capture.prompt", "Press the button you want to use for Up.");
    c.set("demo.wizard.capture.note", "This page reads the buttons itself. {START} cancels.");
    c.set("demo.wizard.large.list.title", "Choose what to update");
    c.set("demo.wizard.large.list.lead", "Select one or more items. The first row selects all of them.");
    c.set("demo.wizard.large.list.reason", "Select at least one item to continue.");
    c.set("demo.wizard.large.list.all", "All items");
    c.set("demo.wizard.large.list.selected", "{1} of {2} selected");
    c.set("demo.wizard.large.status.new", "Not installed");
    c.set("demo.wizard.large.status.old", "Update available");
    c.set("demo.wizard.large.status.ok", "Up to date");
    c.set("demo.wizard.large.name.title", "Name this selection");
    c.set("demo.wizard.large.name.placeholder", "Selection name");
    c.set("demo.wizard.large.name.status", "Up to 24 characters.");
    c.set("demo.wizard.large.name.empty", "Type a name first.");
    c.set("demo.wizard.large.review.title", "Review");
    c.set("demo.wizard.large.review.lead", "These items will be downloaded and installed. Nothing else on the device is changed.");
    c.set("demo.wizard.large.review.line", "Item {1}, about 3 MB, from the official source.");
    c.set("demo.wizard.large.install", "Install");
    c.set("demo.wizard.large.busy.title", "Installing");
    c.set("demo.wizard.large.busy.status", "Installing updates");
    c.set("demo.wizard.large.busy.detail", "Keep the device powered on.");
    c.set("demo.wizard.large.result.title", "Update");
    c.set("demo.wizard.large.result.headline", "2 of 3 items updated");
    c.set("demo.wizard.large.result.changed", "One item could not be installed. The others are in place.");
    c.set("demo.wizard.large.result.next", "Try again, or close and update it later.");
    c.set("demo.wizard.large.result.details", "verify: checksum did not match (example)");
    c.set("demo.wizard.large.result.ok", "Installed");
    c.set("demo.wizard.large.result.failed", "Failed");
    c.set("demo.crash.title", "Something went wrong");
    c.set("demo.crash.what", "The interface stopped and could not continue.");
    c.set("demo.crash.changed", "Nothing on your device was changed.");
    c.set("demo.crash.details", "Details: failed while loading the list (code 12, example)");
    c.set("demo.crash.next", "{A}  Try again      {B}  Quit");
    c.set("demo.overview.title", "Main-screen layout");
    c.set("demo.overview.text",
          "Every screen of an application is built in the same frame. Tabs sit at the top center, "
          "nav items at the top left and right. The bottom left always shows what the controller can do "
          "right now; the bottom right shows static text such as the version.");
    c.set("demo.region.tabs", "Tabs");
    c.set("demo.region.tabs.where", "top center");
    c.set("demo.region.navLeft", "Nav items, left");
    c.set("demo.region.navLeft.where", "top left");
    c.set("demo.region.navRight", "Nav items, right");
    c.set("demo.region.navRight.where", "top right");
    c.set("demo.region.help", "Help");
    c.set("demo.region.help.where", "bottom left, always");
    c.set("demo.region.footer", "Footer");
    c.set("demo.region.footer.where", "bottom right, static");
    c.set("demo.region.content", "Content window");
    c.set("demo.region.content.where", "middle");

    c.set("demo.text.tiers", "Text tiers");
    c.set("demo.text.primary", "Primary: what the user reads to decide");
    c.set("demo.text.muted", "Muted: supporting text");
    c.set("demo.text.faint", "Faint: secondary detail, the floor for readable text");
    c.set("demo.text.disabled", "Disabled: only with a second cue");
    c.set("demo.text.status", "Status words");
    c.set("demo.text.progress", "Progress");
    c.set("demo.text.paragraph", "A paragraph");
    c.set("demo.text.longText",
          "Text is wrapped at a comfortable width and uses one line step everywhere. Nothing is cut off at "
          "the largest text size: a long word is shortened with an ellipsis, a paragraph grows downwards.");
    c.set("demo.status.done", "Up to date");
    c.set("demo.status.action", "Update available");
    c.set("demo.status.warning", "Low space");
    c.set("demo.status.error", "Failed");
    c.set("demo.status.neutral", "Not set");
    c.set("demo.hint.toast", "Show confirmation");
    c.set("demo.hint.notice", "Show notice");
    c.set("demo.toast.done", "Time zone set to Berlin");
    c.set("demo.toast.notice", "Another task is running. Try again when it ends.");

    c.set("demo.lists.settings", "Settings-style list");
    c.set("demo.lists.pane", "Pane without focus");
    c.set("demo.item.nav", "Open a section");
    c.set("demo.item.value", "Text size");
    c.set("demo.item.value.larger", "Larger");
    c.set("demo.item.switch", "Dark mode");
    c.set("demo.item.choice", "A file that can be chosen");
    c.set("demo.item.choice.detail", "folder/subfolder/name");
    c.set("demo.item.action", "Run an action");
    c.set("demo.item.info", "Version");
    c.set("demo.item.info.value", "0.9.0-dev");
    c.set("demo.item.heading", "Display");
    c.set("demo.item.disabled", "Not available right now");

    c.set("demo.placeholder.title", "Coming next");
    c.set("demo.placeholder.why", "This tab shows its components once they are built.");
    c.set("demo.placeholder.next", "Components are added one tab at a time.");

    c.set("hint.leave", "Leave");

    c.set("demo.appearance.title", "Live settings");
    c.set("demo.appearance.tabDisplay", "Tab display");
    c.set("demo.appearance.names", "Names");
    c.set("demo.appearance.icons", "Icons");
    c.set("demo.appearance.both", "Names and icons");
    c.set("demo.appearance.textSize", "Text size");
    c.set("demo.appearance.pseudo", "Pseudo-language (longer text)");
    c.set("demo.appearance.warning", "Show warning state");
    c.set("demo.appearance.dark", "Dark mode");
    c.set("demo.appearance.colorTheme", "Color theme");
    c.set("demo.appearance.highContrast", "High contrast");
    c.set("demo.appearance.highlight", "Highlight color");
    c.set("demo.appearance.about", "About these switches");
    c.set("demo.appearance.text",
          "These change the demo live. Text size scales every text and layout. The pseudo-language makes every "
          "text about 40 percent longer and accents its letters: anything that is not translatable, or that "
          "clips or overlaps, shows up at once. The warning state shows how nav items look when something needs "
          "attention.");
    c.set("demo.grids.title", "Grid views");

    c.set("demo.dialogs.title", "Open a dialog");
    c.set("demo.dialogs.about", "About dialogs");
    c.set("demo.dialogs.aboutText",
          "A dialog sits over the screen on a dimmed layer. It has a title and a body but no buttons: what A and B do "
          "is named in the help at the bottom left. A dialog over a dialog dims the first one further, and only the "
          "top one reacts. The tabs stay locked until the last dialog is closed.");
    c.set("demo.dialogs.info", "Information");
    c.set("demo.dialogs.confirm", "Confirmation");
    c.set("demo.dialogs.error", "Error with details");
    c.set("demo.dialogs.result", "Result");
    c.set("demo.dialogs.progress", "Progress");
    c.set("demo.dialogs.choose", "Choose from a list");
    c.set("demo.dialogs.viewer", "Picture viewer");
    c.set("demo.dialogs.nested", "A dialog over a dialog");
    c.set("demo.dialog.info.title", "Information");
    c.set("demo.dialog.info.body", "A short message with nothing to decide. {A} or {B} closes it.");
    c.set("demo.dialog.confirm.title", "Apply the change?");
    c.set("demo.dialog.confirm.body", "The setting is applied when you confirm. Nothing is written before that.");
    c.set("demo.dialog.error.title", "Could not save");
    c.set("demo.dialog.error.what", "The file could not be written.");
    c.set("demo.dialog.error.todo", "Nothing was changed. Check that the storage is connected, then try again.");
    c.set("demo.dialog.error.details", "no space left on device (example)");
    c.set("demo.dialog.result.title", "Saved");
    c.set("demo.dialog.result.status", "Done");
    c.set("demo.dialog.result.body", "The change has been applied.");
    c.set("demo.dialog.progress.title", "Installing");
    c.set("demo.dialog.progress.status", "Copying files…");
    c.set("demo.dialog.choose.title", "Choose an option");
    c.set("demo.dialog.choose.first", "First option");
    c.set("demo.dialog.choose.second", "Second option");
    c.set("demo.dialog.choose.third", "Third option");
    c.set("demo.dialog.choose.detail", "A short line that says what it does");
    c.set("demo.view.grid", "Grid");
    c.set("demo.view.small", "Grid small");
    c.set("demo.view.list", "Grid list");
    c.set("demo.detail.status", "No video preview");
    c.set("hint.changeView", "Change view");
    c.set("hint.page", "Page");
}

namespace {

const char *kTabKeys[TabCount] = {"demo.tab.overview", "demo.tab.text", "demo.tab.lists", "demo.tab.data", "demo.tab.grids",
                                  "demo.tab.dialogs", "demo.tab.flows", "demo.tab.input", "demo.tab.appearance"};

// A list item: one row per volume, the columns lined up (label left, percent right, size left). A nearly full
// volume is coloured and says so in words as well.
NavItemSpec volumesItem(bool withLowVolume) {
    NavItemSpec item;
    item.primary = Text::key("demo.nav.disk");
    item.priority = 5;
    auto row = [&](const char *label, const char *percent, const char *note, Tone tone) {
        NavRow r;
        r.label = Text::data(label);
        r.value = Text::data(percent);
        r.note = Text::data(note);
        r.tone = tone;
        item.rows.push_back(r);
    };
    row("fat:", "7%", "of 128GB", Tone::Neutral);
    row("usb0:", "80%", "of 512GB", Tone::Neutral);
    if (withLowVolume) row("usb1:", "97%", "of 64GB (!)", Tone::Warning);
    return item;
}

NavItemSpec navItem(const char *primaryKey, const std::string &secondary, IconId icon, Tone tone, int priority) {
    NavItemSpec item;
    item.primary = Text::key(primaryKey);
    item.secondary = Text::data(secondary);
    item.icon = icon;
    item.tone = tone;
    item.priority = priority;
    return item;
}

}  // namespace

ShellSpec shellFor(const State &state, const Options &options, const SystemValues &values, const Assets &assets) {
    const int tab = state.tab;
    ShellSpec spec;
    spec.tabDisplay = state.tabDisplay;
    for (int i = 0; i < TabCount; ++i) {
        TabSpec ts;
        ts.label = Text::key(kTabKeys[i]);
        if (size_t(i) < assets.tabIcons.size()) ts.icon = assets.tabIcons[size_t(i)];
        spec.tabs.push_back(ts);
    }
    spec.activeTab = tab;

    NavItemSpec clock;
    clock.primary = Text::data(values.clock);
    clock.secondary = Text::data(values.date);
    clock.isClock = true;
    clock.priority = 10;
    spec.left.push_back(clock);

    // Generic system properties; on a device the platform provider supplies real values.
    if (options.forceWarning || state.forceWarning) {
        spec.right.push_back(volumesItem(true));
        spec.right.push_back(navItem("demo.nav.offline", "No network", IconId::Network, Tone::Neutral, 4));
    } else {
        spec.right.push_back(volumesItem(false));
        spec.right.push_back(navItem(values.wireless ? "demo.nav.wifi" : "demo.nav.ethernet", values.address,
                                     values.wireless ? IconId::Wifi : IconId::Network, Tone::Done, 4));
    }

    // Help follows the active tab; the demo's shortcuts are the same on every tab.
    // The help is honest: only what works on this tab right now.
    if (tab == Overview || tab == Lists) spec.help.push_back(hint(HintKey::UpDown, "hint.move"));
    if (tab == Dialogs) {
        spec.help.push_back(hint(HintKey::UpDown, "hint.move"));
        spec.help.push_back(hint(HintKey::A, "hint.open", true));
    }
    if (tab == Grids && state.searchFocused) {
        spec.help.push_back(hint(HintKey::UpDown, "hint.move"));
        spec.help.push_back(hint(HintKey::A, "hint.edit", true));
    } else if (tab == Grids) {
        if (state.viewMode == 2) {
            spec.help.push_back(hint(HintKey::UpDown, "hint.move"));
            spec.help.push_back(hint(HintKey::LeftRight, "hint.page"));
        } else {
            spec.help.push_back(hint(HintKey::Dpad, "hint.move"));
        }
        spec.help.push_back(hint(HintKey::Y, "hint.changeView", true));
    }
    if (tab == Data) spec.help.push_back(hint(HintKey::UpDown, "hint.move"));
    if (tab == TextTab) {
        spec.help.push_back(hint(HintKey::A, "demo.hint.toast"));
        spec.help.push_back(hint(HintKey::Y, "demo.hint.notice"));
    }
    if (tab == Flows) {
        spec.help.push_back(hint(HintKey::UpDown, "hint.move"));
        spec.help.push_back(hint(HintKey::A, "hint.open", true));
    }
    if (state.entryTarget >= 0) {
        spec.help = state.entry.hints();
        FooterTextSpec v;
        v.text = Text::key("demo.footer.version");
        spec.footer.push_back(v);
        spec.tabsLocked = true;
        return spec;
    }
    if (tab == Input) {
        spec.help.push_back(hint(HintKey::UpDown, "hint.move"));
        spec.help.push_back(hint(HintKey::A, "hint.edit", true));
    }
    if (tab == Appearance) {
        spec.help.push_back(hint(HintKey::UpDown, "hint.move"));
        spec.help.push_back(hint(HintKey::A, "hint.change", true));
    }
    spec.help.push_back(hint(HintKey::Shoulders, "hint.changeTab", true));
    spec.help.push_back(hint(HintKey::B, "hint.leave", true));

    FooterTextSpec version;
    version.text = Text::key("demo.footer.version");
    spec.footer.push_back(version);

    // An open dialog: its help replaces the screen's, and the tabs are locked.
    if (!state.modals.empty()) {
        spec.help = state.modals.hints();
        spec.tabsLocked = true;
    }
    if (state.flow && state.flow->active()) {
        spec.help = state.flow->hints();
        spec.tabsLocked = true;
    }
    return spec;
}

namespace {

// A card with a heading, returning the rect inside it.
Rect card(Painter &p, const Theme &t, const Rect &r, const char *titleKey) {
    drawSurface(p, t, r, SurfaceRole::Faint, t.px(Radius::panel));
    const int pad = t.px(Space::pad);
    const int head = p.lineHeight(FontRole::Bold, t.size(TextRole::Body));
    drawLabel(p, t, {r.x + pad, r.y + t.px(18), r.w - 2 * pad, head}, Text::key(titleKey).str(), TextRole::Body,
              TextTier::Muted, Align::Left, true);
    const int top = r.y + t.px(18) + head + t.px(Space::titleGap);
    return {r.x + pad, top, r.w - 2 * pad, r.bottom() - top - t.px(18)};
}

void drawOverview(Painter &p, const Theme &t, const Rect &c, int cursor) {
    const int gap = t.gap();
    const int leftW = (c.w - gap) * 2 / 5;
    const Rect left{c.x, c.y, leftW, c.h};
    const Rect right{c.x + leftW + gap, c.y, c.w - leftW - gap, c.h};

    const Rect inner = card(p, t, left, "demo.overview.title");
    drawTextBlock(p, t, inner, Text::key("demo.overview.text").str(), TextRole::Body, TextTier::Primary);

    const Rect list = card(p, t, right, "demo.overview.title");
    static const char *regions[6][2] = {
        {"demo.region.tabs", "demo.region.tabs.where"},
        {"demo.region.navLeft", "demo.region.navLeft.where"},
        {"demo.region.navRight", "demo.region.navRight.where"},
        {"demo.region.help", "demo.region.help.where"},
        {"demo.region.footer", "demo.region.footer.where"},
        {"demo.region.content", "demo.region.content.where"}};
    int y = list.y;
    for (int i = 0; i < 6; ++i) {
        ListItemSpec spec;
        spec.kind = ListItemKind::Info;
        spec.title = Text::key(regions[i][0]);
        spec.value = Text::key(regions[i][1]);
        ListItemState state;
        state.cursor = i == cursor;
        drawListItem(p, t, {list.x, y, list.w, listItemFrameHeight(t, false)}, spec, state);
        y += listItemPitch(t, false);
    }
}

void drawTextTab(Painter &p, const Theme &t, const Rect &c) {
    const int gap = t.gap();
    const int colW = (c.w - gap) / 2;
    const Rect colL{c.x, c.y, colW, c.h};
    const Rect colR{c.x + colW + gap, c.y, colW, c.h};

    // Left: tiers, status words, progress.
    const Rect tiers = card(p, t, {colL.x, colL.y, colL.w, colL.h}, "demo.text.tiers");
    const int step = p.lineHeight(FontRole::Regular, t.size(TextRole::Body)) + t.px(10);
    int y = tiers.y;
    const char *keys[4] = {"demo.text.primary", "demo.text.muted", "demo.text.faint", "demo.text.disabled"};
    const TextTier tierList[4] = {TextTier::Primary, TextTier::Muted, TextTier::Faint, TextTier::Disabled};
    for (int i = 0; i < 4; ++i) {
        drawLabel(p, t, {tiers.x, y, tiers.w, step}, Text::key(keys[i]).str(), TextRole::Body, tierList[i]);
        y += step;
    }
    y += t.px(Space::pad);
    drawLabel(p, t, {tiers.x, y, tiers.w, step}, Text::key("demo.text.status").str(), TextRole::Body, TextTier::Muted,
              Align::Left, true);
    y += step + t.px(6);
    const Tone tones[5] = {Tone::Done, Tone::Action, Tone::Warning, Tone::Error, Tone::Neutral};
    const char *words[5] = {"demo.status.done", "demo.status.action", "demo.status.warning", "demo.status.error",
                            "demo.status.neutral"};
    for (int i = 0; i < 5; ++i) {
        drawStatus(p, t, tiers.x, y, tones[i], Text::key(words[i]).str(), TextRole::Body);
        y += step;
    }
    y += t.px(Space::pad);
    drawLabel(p, t, {tiers.x, y, tiers.w, step}, Text::key("demo.text.progress").str(), TextRole::Body, TextTier::Muted,
              Align::Left, true);
    y += step + t.px(6);
    const float fractions[3] = {0.0f, 0.35f, 1.0f};
    for (int i = 0; i < 3; ++i) {
        drawProgressBar(p, t, {tiers.x, y, tiers.w, t.px(12)}, fractions[i], false);
        y += t.px(12) + t.px(18);
    }

    // Right: a paragraph, and an empty state.
    const int paraH = c.h * 2 / 5;
    const Rect para = card(p, t, {colR.x, colR.y, colR.w, paraH}, "demo.text.paragraph");
    drawTextBlock(p, t, para, Text::key("demo.text.longText").str(), TextRole::Body, TextTier::Primary);
    const Rect emptyCard{colR.x, colR.y + paraH + gap, colR.w, c.h - paraH - gap};
    drawSurface(p, t, emptyCard, SurfaceRole::Faint, t.px(Radius::panel));
    drawEmptyState(p, t, emptyCard.inset(t.px(Space::pad)), Text::key("demo.placeholder.title").str(),
                   Text::key("demo.placeholder.why").str(), Text::key("demo.placeholder.next").str());
}

void drawListsTab(Painter &p, const Theme &t, const Rect &c, int cursor) {
    const int gap = t.gap();
    const int colW = (c.w - gap) / 2;

    auto build = [](std::vector<ListItemSpec> &items) {
        ListItemSpec s;
        s.kind = ListItemKind::Heading; s.title = Text::key("demo.item.heading"); items.push_back(s);
        s = ListItemSpec(); s.kind = ListItemKind::Nav; s.title = Text::key("demo.item.nav"); items.push_back(s);
        s = ListItemSpec(); s.kind = ListItemKind::Value; s.title = Text::key("demo.item.value");
        s.value = Text::key("demo.item.value.larger"); items.push_back(s);
        s = ListItemSpec(); s.kind = ListItemKind::Switch; s.title = Text::key("demo.item.switch"); s.checked = true;
        items.push_back(s);
        s = ListItemSpec(); s.kind = ListItemKind::Switch; s.title = Text::key("demo.item.disabled");
        s.checked = false; s.disabled = true; items.push_back(s);
        s = ListItemSpec(); s.kind = ListItemKind::Action; s.title = Text::key("demo.item.action"); items.push_back(s);
        s = ListItemSpec(); s.kind = ListItemKind::Info; s.title = Text::key("demo.item.info");
        s.value = Text::key("demo.item.info.value"); items.push_back(s);
        s = ListItemSpec(); s.kind = ListItemKind::Choice; s.title = Text::key("demo.item.choice");
        s.detail = Text::key("demo.item.choice.detail"); s.status = Text::key("demo.status.action");
        s.statusTone = Tone::Action; s.checked = true; items.push_back(s);
    };

    for (int pane = 0; pane < 2; ++pane) {
        const Rect col{c.x + pane * (colW + gap), c.y, colW, c.h};
        const Rect inner = card(p, t, col, pane == 0 ? "demo.lists.settings" : "demo.lists.pane");
        std::vector<ListItemSpec> items;
        build(items);
        int y = inner.y;
        for (size_t i = 0; i < items.size(); ++i) {
            const bool two = items[i].kind == ListItemKind::Choice;
            const bool heading = items[i].kind == ListItemKind::Heading;
            const int h = heading ? t.px(36) : listItemFrameHeight(t, two);
            if (y + h > inner.bottom()) break;
            ListItemState state;
            state.cursor = int(i) == cursor;
            state.paneActive = pane == 0;     // the second pane remembers it without focus
            drawListItem(p, t, {inner.x, y, inner.w, h}, items[i], state);
            y += heading ? h : (two ? listItemPitch(t, true) : listItemPitch(t, false));
        }
    }
}

void drawPlaceholder(Painter &p, const Theme &t, const Rect &c) {
    drawSurface(p, t, c, SurfaceRole::Faint, t.px(Radius::panel));
    drawEmptyState(p, t, c.inset(t.px(Space::pad)), Text::key("demo.placeholder.title").str(),
                   Text::key("demo.placeholder.why").str(), Text::key("demo.placeholder.next").str());
}

}  // namespace

// ---------------------------------------------------------------- Grids tab

namespace {

const char *viewModeKey(int mode) {
    return mode == 0 ? "demo.view.grid" : mode == 1 ? "demo.view.small" : "demo.view.list";
}

// The sample item i: its label and flags (generic data, not application content).
std::string sampleTitle(int i) {
    char label[40];
    std::snprintf(label, sizeof(label), "Sample title %02d", i + 1);
    return label;
}

void drawGridView(Painter &p, const Theme &t, State &state, const Rect &area, const Assets &assets, bool small) {
    GridSpec spec;
    if (small) { spec.artW = spec.artH = 128; spec.caption = Caption::None; }
    else spec.caption = Caption::TitleSub;
    state.grid.configure(p, t, area, spec);
    state.grid.ensureVisible(state.cursor[Grids]);

    p.pushClip(area.inset(-t.px(6)));
    const int first = state.grid.firstVisible();
    const int last = std::min(state.gridCount, first + state.grid.columns() * state.grid.visibleRows());
    const std::vector<ImagePtr> &art = small ? assets.samplesSmall : assets.samples;
    for (int i = first; i < last; ++i) {
        GridItemSpec item;
        item.label = Text::data(sampleTitle(i));
        item.sublabel = Text::data("Example system");
        item.favorite = i % 4 == 1;
        if (i % 7 == 3) {
            item.kind = GridItemKind::Placeholder;
        } else if (!small && i % 7 == 5 && !assets.bigIcons.empty()) {
            item.kind = GridItemKind::Icon;
            item.icon = assets.bigIcons[size_t(i) % assets.bigIcons.size()];
        } else if (!art.empty()) {
            item.art = art[size_t(i) % art.size()];
        }
        drawGridItem(p, t, state.grid, state.grid.cell(i), spec, item, !state.searchFocused && i == state.cursor[Grids]);
    }
    p.popClip();
}

void drawListView(Painter &p, const Theme &t, State &state, const Rect &area, const Assets &assets) {
    const GridListRects split = splitGridList(t, area);
    const Density density = Density::Dense;
    const Rect rows{split.list.x, split.list.y, split.list.w - t.px(20), split.list.h};
    state.list.configure(rows, listItemFrameHeight(t, false, density), listItemPitch(t, false, density));
    state.list.ensureVisible(state.cursor[Grids]);

    p.pushClip(split.list.inset(-t.px(4)));
    const int first = state.list.scroll();
    const int last = std::min(state.gridCount, first + state.list.visibleRows());
    for (int i = first; i < last; ++i) {
        ListItemSpec entry;
        entry.kind = ListItemKind::Entry;
        entry.title = Text::data(sampleTitle(i));
        entry.favorite = i % 4 == 1;
        ListItemState st;
        st.cursor = !state.searchFocused && i == state.cursor[Grids];
        drawListItem(p, t, state.list.row(i), entry, st);
    }
    drawScrollbar(p, t, split.list, state.gridCount, state.list.visibleRows(), state.list.scroll());
    p.popClip();

    DetailSpec detail;
    const int cursor = state.cursor[Grids];
    if (!assets.samplesDetail.empty()) detail.art = assets.samplesDetail[size_t(cursor) % assets.samplesDetail.size()];
    detail.title = Text::data(sampleTitle(cursor));
    detail.subtitle = Text::data("Example system");
    detail.status = Text::key("demo.detail.status");
    drawDetailPanel(p, t, split.detail, detail);
}

void drawGridsTab(Painter &p, const Theme &t, State &state, const Rect &c, const Assets &assets) {
    // The shared header (title, centered search field, view mode and position), then the active view.
    char position[48];
    std::snprintf(position, sizeof(position), " \xC2\xB7 %d of %d", state.cursor[Grids] + 1, state.gridCount);
    PageHeaderSpec header;
    header.title = Text::key("demo.grids.title");
    header.info = Text::data(Text::key(viewModeKey(state.viewMode)).str() + position);
    header.hasSearch = true;
    header.search.placeholder = Text::key("demo.grids.search");
    header.search.value = state.gridSearch;
    header.search.focused = state.searchFocused;
    const Rect area = drawPageHeader(p, t, c, header);
    if (state.viewMode == 2) drawListView(p, t, state, area, assets);
    else drawGridView(p, t, state, area, assets, state.viewMode == 1);
}

// ---------------------------------------------------------------- Dialogs tab

void drawDialogsTab(Painter &p, const Theme &t, const State &state, const Rect &c) {
    const int gap = t.gap();
    const int leftW = (c.w - gap) * 3 / 5;
    const Rect left{c.x, c.y, leftW, c.h};
    const Rect right{c.x + leftW + gap, c.y, c.w - leftW - gap, c.h};

    const Rect list = card(p, t, left, "demo.dialogs.title");
    static const char *labels[kDialogCount] = {"demo.dialogs.info", "demo.dialogs.confirm", "demo.dialogs.error",
                                               "demo.dialogs.result", "demo.dialogs.progress", "demo.dialogs.choose",
                                               "demo.dialogs.viewer", "demo.dialogs.nested"};
    int y = list.y;
    for (int i = 0; i < kDialogCount; ++i) {
        ListItemSpec row;
        row.kind = ListItemKind::Action;
        row.title = Text::key(labels[i]);
        ListItemState st;
        st.cursor = i == state.cursor[Dialogs] && state.modals.empty();
        drawListItem(p, t, {list.x, y, list.w, listItemFrameHeight(t, false)}, row, st);
        y += listItemPitch(t, false);
    }
    const Rect about = card(p, t, right, "demo.dialogs.about");
    drawTextBlock(p, t, about, Text::key("demo.dialogs.aboutText").str(), TextRole::Body, TextTier::Primary);
}

// ---------------------------------------------------------------- Data tab

namespace {

void drawDataTab(Painter &p, const Theme &t, const State &state, const Rect &c) {
    const int gap = t.gap();
    const int topH = (c.h - gap) * 11 / 20, botH = c.h - gap - topH;
    const int leftW = (c.w - gap) * 3 / 5;
    auto key = [](const char *k) { return Text::key(k); };

    const Rect tab = card(p, t, {c.x, c.y, leftW, topH}, "demo.data.table");
    std::vector<TableColumn> cols(4);
    cols[0].header = key("demo.col.name"); cols[0].weight = 40;
    cols[1].header = key("demo.col.year"); cols[1].weight = 12;
    cols[2].header = key("demo.col.maker"); cols[2].weight = 28;
    cols[3].header = key("demo.col.works"); cols[3].weight = 20; cols[3].kind = CellKind::Flag;
    static const char *names[] = {"Sample item A", "Sample item B", "Sample item C", "Sample item D", "Sample item E"};
    static const char *years[] = {"1991", "1992", "1993", "1994", "1995"};
    static const char *makers[] = {"Maker One", "Maker Two", "Maker Three", "Maker One", "Maker Two"};
    std::vector<TableRow> rows;
    for (int i = 0; i < 5; ++i) {
        TableCell n, y, m, f;
        n.text = names[i]; y.text = years[i]; m.text = makers[i]; f.flag = i % 3 != 2;
        rows.push_back({n, y, m, f});
    }
    drawTable(p, t, tab, cols, rows, state.modals.empty() ? state.cursor[Data] : -1);

    const Rect kvArea = card(p, t, {c.x + leftW + gap, c.y, c.w - leftW - gap, topH}, "demo.data.kv");
    std::vector<KeyValueEntry> kv;
    auto head = [&](const char *k) { KeyValueEntry e; e.kind = KeyValueKind::Heading; e.label = key(k); kv.push_back(e); };
    auto row = [&](const char *l, const Text &v, bool status = false) {
        KeyValueEntry e; e.label = key(l); e.value = v; e.hasStatus = status; e.tone = Tone::Done; kv.push_back(e);
    };
    head("demo.kv.system");
    row("demo.kv.version", Text::data("1.4.2"));
    row("demo.kv.storage", Text::data("7.9 / 120 GB"));
    KeyValueEntry sp; sp.kind = KeyValueKind::Spacer; kv.push_back(sp);
    head("demo.kv.network");
    row("demo.kv.connection", key("demo.kv.connected"), true);
    row("demo.kv.address", Text::data("192.168.0.12"));
    drawKeyValueList(p, t, kvArea, kv);

    const int w3 = (c.w - 2 * gap) / 3;
    const int by = c.y + topH + gap;
    const Rect resArea = card(p, t, {c.x, by, w3, botH}, "demo.data.results");
    ResultItemSpec items[4];
    items[0].label = key("demo.res.itemA"); items[0].tone = Tone::Done; items[0].status = key("demo.res.uptodate");
    items[1].label = key("demo.res.itemB"); items[1].tone = Tone::Action; items[1].status = key("demo.res.update");
    items[1].count = key("demo.res.size");
    items[2].label = key("demo.res.itemC"); items[2].tone = Tone::Error; items[2].status = key("demo.res.failed");
    items[2].details = key("demo.res.reason");
    items[3].label = key("demo.res.itemD"); items[3].tone = Tone::Neutral; items[3].status = key("demo.res.working");
    int y = resArea.y;
    for (const ResultItemSpec &it : items) {
        const int h = resultItemHeight(t, it);
        drawResultItem(p, t, {resArea.x, y, resArea.w, h}, it);
        y += h;
    }

    const Rect progArea = card(p, t, {c.x + w3 + gap, by, w3, botH}, "demo.data.progress");
    ProgressPanelSpec pp;
    pp.title = key("demo.prog.title"); pp.status = key("demo.prog.status"); pp.detail = key("demo.prog.detail");
    pp.fraction = 0.45f;
    drawProgressPanel(p, t, progArea, pp);

    const Rect emptyArea = card(p, t, {c.x + 2 * (w3 + gap), by, c.w - 2 * (w3 + gap), botH}, "demo.data.empty");
    drawEmptyState(p, t, emptyArea, key("demo.empty.title").str(), key("demo.empty.why").str(), key("demo.empty.next").str());
}

}  // namespace

// ---------------------------------------------------------------- Flows tab

namespace {

bool flowOpen(const State &state) { return state.flow && state.flow->active(); }

void drawFlowsTab(Painter &p, const Theme &t, const State &state, const Rect &c) {
    const int gap = t.gap();
    const int leftW = (c.w - gap) * 3 / 5;
    const Rect list = card(p, t, {c.x, c.y, leftW, c.h}, "demo.flows.title");
    static const char *labels[3] = {"demo.flows.wizard", "demo.flows.large", "demo.flows.crash"};
    int y = list.y;
    for (int i = 0; i < 3; ++i) {
        ListItemSpec row;
        row.kind = ListItemKind::Action;
        row.title = Text::key(labels[i]);
        ListItemState st;
        st.cursor = i == state.cursor[Flows] && state.modals.empty() && !flowOpen(state);
        drawListItem(p, t, {list.x, y, list.w, listItemFrameHeight(t, false)}, row, st);
        y += listItemPitch(t, false);
    }
    const Rect about = card(p, t, {c.x + leftW + gap, c.y, c.w - leftW - gap, c.h}, "demo.flows.about");
    drawTextBlock(p, t, about, Text::key("demo.flows.aboutText").str(), TextRole::Body, TextTier::Primary);
}

// ---- The two demo flows. Each is a Wizard with a frame chosen once, pages registered by id, and the flow's data in one
//      shared struct that the lambdas capture (the pattern a Settings flow uses).

Text k(const char *key) { return Text::key(key); }

struct MediumData {
    int target = 0;        // the chosen row
    float busy = 0.0f;     // seconds the work has run
    int pressed = 0;       // buttons captured
};
enum MediumPage { kIntro, kChoose, kCapture, kBusy, kDone, kError };

std::shared_ptr<Wizard> makeMediumFlow() {
    std::shared_ptr<Wizard> w(new Wizard(WizardFrame::Medium));
    std::shared_ptr<MediumData> data(new MediumData);
    std::shared_ptr<ChooseList> list(new ChooseList(ChooseMode::Single));

    // 1. Step: a text in the reading column; A continues; B closes (the first page).
    {
        std::unique_ptr<FnPage> pg(new FnPage(WizardPage::Kind::Step, k("demo.wizard.start.title")));
        pg->onDraw = [](Painter &p, const Theme &t, const Rect &body) {
            // One thing per page, centred: symbol, headline, two short sentences (drawWizardCentered).
            WizardCentered c;
            c.icon = IconId::Search;
            c.headline = k("demo.wizard.start.headline");
            c.lines = {k("demo.wizard.start.body"), k("demo.wizard.start.note")};
            drawWizardCentered(p, t, body, c);
        };
        pg->onHints = []() { return std::vector<Hint>{hint(HintKey::A, "hint.continue", true)}; };
        pg->onEvent = [](WizardEvent e, Wizard &wz) { if (e == WizardEvent::Confirm) wz.go(kChoose); };
        w->add(kIntro, std::move(pg));
    }
    // 2. Step: a Single choose list with a status word per row and the status slot under it.
    {
        std::unique_ptr<FnPage> pg(new FnPage(WizardPage::Kind::Step, k("demo.wizard.choose.title")));
        pg->onEnter = [list, data](Wizard &) {
            std::vector<ChooseItem> items(3);
            const char *titles[3] = {"demo.wizard.target.one", "demo.wizard.target.two", "demo.wizard.target.three"};
            const char *status[3] = {"demo.wizard.status.ready", "demo.wizard.status.slow", "demo.wizard.status.pair"};
            const Tone tones[3] = {Tone::Done, Tone::Warning, Tone::Action};
            for (int i = 0; i < 3; ++i) {
                items[size_t(i)].title = k(titles[i]);
                items[size_t(i)].detail = k("demo.wizard.target.detail");
                items[size_t(i)].status = k(status[i]);
                items[size_t(i)].statusTone = tones[i];
            }
            list->setItems(items);
            list->setCurrent(data->target);
        };
        pg->onDraw = [list](Painter &p, const Theme &t, const Rect &body) {
            const int statusH = wizardStatusHeight(p, t);
            list->draw(p, t, {body.x, body.y, body.w, body.h - statusH - t.px(8)});
            static const char *notes[3] = {"demo.wizard.choose.note.ready", "demo.wizard.choose.note.slow", "demo.wizard.choose.note.pair"};
            static const Tone tones[3] = {Tone::Done, Tone::Warning, Tone::Action};
            const int c = list->cursor();
            drawWizardStatus(p, t, {body.x, body.bottom() - statusH, body.w, statusH}, tones[c],
                             c == 0 ? Text() : k("demo.wizard.note"), k(notes[c]));
        };
        pg->onHints = []() { return std::vector<Hint>{hint(HintKey::UpDown, "hint.move"), hint(HintKey::A, "hint.choose", true)}; };
        pg->onEvent = [list, data](WizardEvent e, Wizard &wz) {
            if (list->handle(e) != ChooseOutcome::Chosen) return;
            data->target = list->cursor();
            list->setCurrent(data->target);
            wz.go(data->target == 2 ? kCapture : kBusy);
        };
        w->add(kChoose, std::move(pg));
    }
    // 3. Capture: the page reads the buttons itself; Back is not interpreted, Start is the visible way out. "n / m" only
    //    because the step repeats (the same question three times).
    {
        std::unique_ptr<FnPage> pg(new FnPage(WizardPage::Kind::Capture, k("demo.wizard.capture.title")));
        pg->onProgress = [data]() { return std::to_string(data->pressed + 1) + " / 3"; };
        pg->onEnter = [data](Wizard &) { data->pressed = 0; };
        pg->onDraw = [](Painter &p, const Theme &t, const Rect &body) {
            std::vector<WizardParagraph> paras(2);
            paras[0].text = k("demo.wizard.capture.prompt");
            paras[0].bold = true;
            paras[1].text = k("demo.wizard.capture.note");
            paras[1].tier = TextTier::Muted;
            drawWizardParagraphs(p, t, body, paras);
        };
        pg->onHints = []() { return std::vector<Hint>{hint(HintKey::Start, "hint.cancel", true)}; };
        pg->onEvent = [data](WizardEvent e, Wizard &wz) {
            if (e == WizardEvent::Start) { wz.cancelTo(kPrevious); return; }
            if (e == WizardEvent::Up || e == WizardEvent::Down) return;
            if (++data->pressed >= 3) wz.go(kBusy);
        };
        pg->onRawInput = []() { return true; };
        w->add(kCapture, std::move(pg));
    }
    // 4. Busy: a bar, one status line, a detail line; B cancels and returns to the choice.
    {
        std::unique_ptr<BusyPage> pg(new BusyPage(k("demo.wizard.busy.title")));
        pg->status = k("demo.wizard.busy.status");
        pg->backTo = kPrevious;
        pg->centered = true;
        pg->waitNote = k("demo.wizard.busy.wait");
        pg->waitNoteAfter = 2.0f;   // the real default is 10 s; shortened so the three-second demo can show the line
        BusyPage *raw = pg.get();
        pg->onEnter = [data, raw](Wizard &) { data->busy = 0.0f; raw->fraction = 0.0f; raw->detail = Text(); raw->facts = Text(); };
        pg->onUpdate = [data, raw](float dt, Wizard &wz) {
            data->busy += dt;
            raw->fraction = std::min(1.0f, data->busy / 3.0f);
            if (data->busy > 0.5f) raw->setDetail(k("demo.wizard.busy.detail"), k("demo.wizard.busy.facts"));
            if (data->busy >= 3.0f) wz.go(data->target == 1 ? kError : kDone);
            return true;
        };
        w->add(kBusy, std::move(pg));
    }
    // 5. Result (Done) and 6. Result (Error with Try again back to the choice): one layout.
    {
        std::unique_ptr<ResultPage> pg(new ResultPage(k("demo.wizard.start.title")));
        WizardResult r;
        r.tone = Tone::Done;
        r.headline = k("demo.wizard.done.headline");
        r.changed = k("demo.wizard.done.changed");
        r.next = k("demo.wizard.done.next");
        pg->set(r);
        w->add(kDone, std::move(pg));
    }
    {
        std::unique_ptr<ResultPage> pg(new ResultPage(k("demo.wizard.start.title")));
        WizardResult r;
        r.tone = Tone::Error;
        r.headline = k("demo.wizard.fail.title");
        r.changed = k("demo.wizard.fail.what");
        r.next = k("demo.wizard.fail.todo");
        r.details = k("demo.wizard.fail.reason");
        r.retry = kChoose;
        pg->set(r);
        w->add(kError, std::move(pg));
    }
    w->open(kIntro);
    return w;
}

struct LargeData {
    float busy = 0.0f;
    std::string name;
};
enum LargePage { kList, kName, kReview, kInstall, kOutcome };

std::shared_ptr<Wizard> makeLargeFlow() {
    std::shared_ptr<Wizard> w(new Wizard(WizardFrame::Large));
    std::shared_ptr<LargeData> data(new LargeData);
    std::shared_ptr<ChooseList> list(new ChooseList(ChooseMode::Multiple));
    std::shared_ptr<WizardScrollText> review(new WizardScrollText);
    list->setExclusiveFirst(true);

    // 1. Step: multiple choice (the first row is "all"), the status slot says how many; Start continues, and cannot
    //    while nothing is selected (the reason is the one line at the bottom of the frame).
    {
        std::unique_ptr<FnPage> pg(new FnPage(WizardPage::Kind::Step, k("demo.wizard.large.list.title")));
        pg->continueKey_ = HintKey::Start;
        pg->onEnter = [list](Wizard &) {
            std::vector<ChooseItem> items;
            ChooseItem all;
            all.title = k("demo.wizard.large.list.all");
            items.push_back(all);
            const char *status[3] = {"demo.wizard.large.status.new", "demo.wizard.large.status.old", "demo.wizard.large.status.ok"};
            const Tone tones[3] = {Tone::Action, Tone::Warning, Tone::Done};
            for (int i = 1; i <= 15; ++i) {
                ChooseItem it;
                it.title = Text::data("Item " + std::string(i < 10 ? "0" : "") + std::to_string(i));
                it.status = k(status[i % 3]);
                it.statusTone = tones[i % 3];
                items.push_back(it);
            }
            list->setItems(items);
            list->setCursor(0);
        };
        pg->onDraw = [list](Painter &p, const Theme &t, const Rect &body) {
            const int lead = drawTextBlock(p, t, wizardTextColumn(t, body), k("demo.wizard.large.list.lead").str(), TextRole::Body,
                                           TextTier::Muted) + t.px(12);
            const int statusH = wizardStatusHeight(p, t);
            list->draw(p, t, {body.x, body.y + lead, body.w, body.h - lead - statusH - t.px(8)});
            const std::string n = format("demo.wizard.large.list.selected", {std::to_string(list->checkedCount()),
                                                                           std::to_string(int(list->items().size()))});
            drawWizardStatus(p, t, {body.x, body.bottom() - statusH, body.w, statusH}, Tone::Neutral, Text(), Text::data(n));
        };
        pg->onHints = []() {
            return std::vector<Hint>{hint(HintKey::UpDown, "hint.move"), hint(HintKey::A, "hint.select", true),
                                     hint(HintKey::Start, "hint.continue", true)};
        };
        pg->onCanContinue = [list](Text &reason) {
            if (list->checkedCount() > 0) return true;
            reason = k("demo.wizard.large.list.reason");
            return false;
        };
        pg->onEvent = [list](WizardEvent e, Wizard &wz) {
            if (e == WizardEvent::Start) wz.go(kName);
            else list->handle(e);
        };
        w->add(kList, std::move(pg));
    }
    // 2. Entry (a Capture page): the keyboard in the same frame, the status slot between field and keys.
    {
        TextEntrySpec spec;
        spec.title = k("demo.wizard.large.name.title");
        spec.placeholder = k("demo.wizard.large.name.placeholder");
        spec.maxLength = 24;
        spec.trim = true;
        std::unique_ptr<EntryPage> pg(new EntryPage(k("demo.wizard.large.name.title"), spec));
        pg->statusLine = k("demo.wizard.large.name.status");
        pg->validate = [](const std::string &v) { return v.empty() ? k("demo.wizard.large.name.empty") : Text(); };
        pg->onSubmit = [data](const std::string &v, Wizard &wz) { data->name = v; wz.go(kReview); };
        w->add(kName, std::move(pg));
    }
    // 3. Step: a review that scrolls.
    {
        std::unique_ptr<FnPage> pg(new FnPage(WizardPage::Kind::Step, k("demo.wizard.large.review.title")));
        pg->onEnter = [review](Wizard &) {
            std::vector<WizardParagraph> paras;
            WizardParagraph lead;
            lead.text = k("demo.wizard.large.review.lead");
            paras.push_back(lead);
            for (int i = 1; i <= 24; ++i) {
                WizardParagraph line;
                line.text = Text::data(format("demo.wizard.large.review.line", {std::to_string(i)}));
                line.tier = TextTier::Muted;
                paras.push_back(line);
            }
            review->set(paras);
        };
        pg->onDraw = [review](Painter &p, const Theme &t, const Rect &body) { review->draw(p, t, body); };
        pg->onHints = []() {
            return std::vector<Hint>{hint(HintKey::UpDown, "hint.scroll"), hint(HintKey::A, "demo.wizard.large.install", true)};
        };
        pg->onEvent = [review](WizardEvent e, Wizard &wz) {
            if (e == WizardEvent::Confirm) wz.go(kInstall);
            else review->handle(e);
        };
        w->add(kReview, std::move(pg));
    }
    // 4. Busy that cannot be cancelled: no hint, B is swallowed.
    {
        std::unique_ptr<BusyPage> pg(new BusyPage(k("demo.wizard.large.busy.title")));
        pg->status = k("demo.wizard.large.busy.status");
        pg->detail = k("demo.wizard.large.busy.detail");
        pg->cancelable = false;
        BusyPage *raw = pg.get();
        pg->onEnter = [data, raw](Wizard &) { data->busy = 0.0f; raw->fraction = 0.0f; };
        pg->onUpdate = [data, raw](float dt, Wizard &wz) {
            data->busy += dt;
            raw->fraction = std::min(1.0f, data->busy / 3.0f);
            if (data->busy >= 3.0f) wz.go(kOutcome);
            return true;
        };
        w->add(kInstall, std::move(pg));
    }
    // 5. Result with a warning: what is in place, which item failed (with its own Details), a next step, Try again.
    {
        std::unique_ptr<ResultPage> pg(new ResultPage(k("demo.wizard.large.result.title")));
        WizardResult r;
        r.tone = Tone::Warning;
        r.headline = k("demo.wizard.large.result.headline");
        r.changed = k("demo.wizard.large.result.changed");
        for (int i = 1; i <= 3; ++i) {
            ResultItemSpec item;
            item.label = Text::data("Item 0" + std::to_string(i));
            item.tone = i == 3 ? Tone::Error : Tone::Done;
            item.status = k(i == 3 ? "demo.wizard.large.result.failed" : "demo.wizard.large.result.ok");
            if (i == 3) item.details = k("demo.wizard.large.result.details");
            r.items.push_back(item);
        }
        r.next = k("demo.wizard.large.result.next");
        r.details = k("demo.wizard.large.result.details");
        r.retry = kReview;
        pg->set(r);
        w->add(kOutcome, std::move(pg));
    }
    w->open(kList);
    return w;
}

}  // namespace

// ---------------------------------------------------------------- Input tab

namespace {

void drawInputTab(Painter &p, const Theme &t, const State &state, const Rect &c) {
    const int gap = t.gap();
    const bool editing = state.entryTarget >= 0;
    const int leftW = (c.w - gap) * 3 / 5;
    const Rect left{c.x, c.y, leftW, c.h};
    const Rect right{c.x + leftW + gap, c.y, c.w - leftW - gap, c.h};

    const Rect in = card(p, t, left, "demo.input.fields");
    const int fieldH = t.px(56), step = fieldH + t.px(12);
    for (int i = 0; i < 2; ++i) {
        TextFieldSpec f;
        f.placeholder = Text::key(i == 0 ? "demo.input.name" : "demo.input.password");
        f.value = state.field[i];
        f.masked = i == 1;
        f.focused = state.cursor[Input] == i;
        f.editing = false;   // the TextEntryDialog shows the text while it is edited
        drawTextField(p, t, {in.x, in.y + i * step, in.w, fieldH}, f);
    }
    const int bh = t.px(64);
    const bool ready = !state.field[0].empty();
    drawActionButton(p, t, {in.x, in.y + 2 * step + t.px(8), in.w, bh}, Text::key("demo.input.start").str(),
                     ready ? ButtonState::Ready : ButtonState::Unavailable, state.cursor[Input] == 2 && !editing);
    if (!ready)
        drawLabel(p, t, {in.x, in.y + 2 * step + t.px(8) + bh + t.px(8), in.w, t.px(30)},
                  Text::key("demo.input.notReady").str(), TextRole::Small, TextTier::Muted);

    const Rect about = card(p, t, right, "demo.input.about");
    drawTextBlock(p, t, about, Text::key("demo.input.aboutText").str(), TextRole::Body, TextTier::Primary);
}

}  // namespace

// ---------------------------------------------------------------- Appearance tab

const char *tabDisplayKey(TabDisplay d) {
    switch (d) {
        case TabDisplay::Names: return "demo.appearance.names";
        case TabDisplay::Icons: return "demo.appearance.icons";
        case TabDisplay::Both:  return "demo.appearance.both";
    }
    return "demo.appearance.names";
}

void drawAppearanceTab(Painter &p, const Theme &t, const State &state, const Rect &c) {
    const int gap = t.gap();
    const int leftW = (c.w - gap) * 3 / 5;
    const Rect left{c.x, c.y, leftW, c.h};
    const Rect right{c.x + leftW + gap, c.y, c.w - leftW - gap, c.h};

    const Rect list = card(p, t, left, "demo.appearance.title");
    char scale[16];
    std::snprintf(scale, sizeof(scale), "%d %%", state.textScale);
    ListItemSpec rows[8];
    rows[0].kind = ListItemKind::Value;  rows[0].title = Text::key("demo.appearance.tabDisplay");
    rows[0].value = Text::key(tabDisplayKey(state.tabDisplay));
    rows[1].kind = ListItemKind::Value;  rows[1].title = Text::key("demo.appearance.textSize");
    rows[1].value = Text::data(scale);
    rows[2].kind = ListItemKind::Switch; rows[2].title = Text::key("demo.appearance.pseudo");
    rows[2].checked = state.pseudo;
    rows[3].kind = ListItemKind::Switch; rows[3].title = Text::key("demo.appearance.warning");
    rows[3].checked = state.forceWarning;
    rows[4].kind = ListItemKind::Switch; rows[4].title = Text::key("demo.appearance.dark");
    rows[4].checked = state.dark;
    rows[5].kind = ListItemKind::Value;  rows[5].title = Text::key("demo.appearance.colorTheme");
    rows[5].value = Text::data(t.colorThemeName(state.colorTheme));
    rows[6].kind = ListItemKind::Switch; rows[6].title = Text::key("demo.appearance.highContrast");
    rows[6].checked = state.highContrast;
    {
        int count = 0;
        const HighlightColor *colors = highlightColors(count);
        rows[7].kind = ListItemKind::Value; rows[7].title = Text::key("demo.appearance.highlight");
        rows[7].value = Text::data(colors[state.highlight % count].name);
        rows[7].disabled = !state.highContrast;   // only meaningful with high contrast
    }
    int y = list.y;
    for (int i = 0; i < 8; ++i) {
        ListItemState st;
        st.cursor = i == state.cursor[Appearance];
        drawListItem(p, t, {list.x, y, list.w, listItemFrameHeight(t, false)}, rows[i], st);
        y += listItemPitch(t, false);
    }

    const Rect about = card(p, t, right, "demo.appearance.about");
    drawTextBlock(p, t, about, Text::key("demo.appearance.text").str(), TextRole::Body, TextTier::Primary);
}

}  // namespace

void startFlow(State &state, int which) { state.flow = which == 0 ? makeMediumFlow() : makeLargeFlow(); }

void applyAppearance(const State &state, Theme &theme) {
    theme.setTextScale(state.textScale);
    theme.setDarkMode(state.dark);
    theme.setColorTheme(state.colorTheme);
    theme.setHighContrast(state.highContrast, state.highlight);
    Catalog::get().setPseudo(state.pseudo);
}

bool update(State &state, float deltaSeconds) {
    const bool toastGone = state.toast.update(deltaSeconds, !state.modals.empty() || state.entryTarget >= 0 || state.crashPreview ||
                                                                (state.flow && state.flow->active()));
    if (!state.flow || !state.flow->active()) return toastGone;
    const bool changed = state.flow->update(deltaSeconds);
    return toastGone || changed;
}

bool handle(State &state, Event event, const Assets &assets) {
    auto moveRow = [&](int lo, int hi, int delta) {
        int &c = state.cursor[state.tab];
        c = std::max(lo, std::min(hi, c + delta));
    };
    auto moveGrid = [&](int dx, int dy) {
        int &c = state.cursor[Grids];
        c = state.grid.move(c, state.gridCount, dx, dy);
        state.grid.ensureVisible(c);
    };
    auto moveList = [&](int delta) {
        int &c = state.cursor[Grids];
        c = ListLayout::move(c, state.gridCount, delta);
        state.list.ensureVisible(c);
    };
    const bool listMode = state.tab == Grids && state.viewMode == 2;

    // The crash screen preview: A or B returns to the demo.
    if (state.crashPreview) {
        if (event == Event::Confirm || event == Event::Back) state.crashPreview = false;
        return false;
    }
    // Entering text: the TextEntryDialog takes the event; Back cancels the entry and does not leave the demo.
    if (state.entryTarget >= 0 && state.modals.empty()) {
        TextEntryEvent te;
        switch (event) {
            case Event::Up:        te = TextEntryEvent::Up; break;
            case Event::Down:      te = TextEntryEvent::Down; break;
            case Event::Left:      te = TextEntryEvent::Left; break;
            case Event::Right:     te = TextEntryEvent::Right; break;
            case Event::Confirm:   te = TextEntryEvent::Press; break;
            case Event::Delete:    te = TextEntryEvent::Delete; break;
            case Event::CycleView: te = TextEntryEvent::Space; break;
            case Event::Start:     te = TextEntryEvent::Done; break;
            case Event::Back:      te = TextEntryEvent::Cancel; break;
            default: return false;
        }
        const TextEntryResult r = state.entry.handle(te);
        std::string &target = state.entryTarget == 2 ? state.gridSearch : state.field[state.entryTarget];
        if (r != TextEntryResult::None) target = state.entry.value();   // Changed live, Submitted, or the old value back
        if (r == TextEntryResult::Submitted || r == TextEntryResult::Cancelled) state.entryTarget = -1;
        return false;
    }
    // An open flow takes every event; Back and the page's keys are the wizard's business.
    if (state.flow && state.flow->active()) {
        WizardEvent we = WizardEvent::Up;
        switch (event) {
            case Event::Up:        we = WizardEvent::Up; break;
            case Event::Down:      we = WizardEvent::Down; break;
            case Event::Left:      we = WizardEvent::Left; break;
            case Event::Right:     we = WizardEvent::Right; break;
            case Event::Confirm:   we = WizardEvent::Confirm; break;
            case Event::Back:      we = WizardEvent::Back; break;
            case Event::Delete:    we = WizardEvent::X; break;
            case Event::CycleView: we = WizardEvent::Y; break;
            case Event::Start:     we = WizardEvent::Start; break;
            case Event::TabPrev:   we = WizardEvent::ShoulderPrev; break;
            case Event::TabNext:   we = WizardEvent::ShoulderNext; break;
        }
        state.flow->handle(we);
        return false;
    }
    // An open dialog takes the event; the tabs stay locked and the screen behind does not react.
    if (!state.modals.empty()) {
        ModalEvent me;
        switch (event) {
            case Event::Up:      me = ModalEvent::Up; break;
            case Event::Down:    me = ModalEvent::Down; break;
            case Event::Left:    me = ModalEvent::Left; break;
            case Event::Right:   me = ModalEvent::Right; break;
            case Event::Confirm: me = ModalEvent::Confirm; break;
            case Event::Back:    me = ModalEvent::Back; break;
            default: return false;   // tab changes and view changes wait until the dialog is closed
        }
        state.modals.handle(me);
        return false;
    }
    switch (event) {
        case Event::TabPrev: state.tab = (state.tab + TabCount - 1) % TabCount; break;
        case Event::TabNext: state.tab = (state.tab + 1) % TabCount; break;
        case Event::Up:
            if (state.tab == Overview) moveRow(0, 5, -1);
            else if (state.tab == Lists) moveRow(1, 7, -1);
            else if (state.tab == Appearance) moveRow(0, 7, -1);
            else if (state.tab == Input) moveRow(0, 2, -1);
            else if (state.tab == Flows) moveRow(0, 2, -1);
            else if (state.tab == Data) moveRow(0, 4, -1);
            else if (state.tab == Dialogs) moveRow(0, kDialogCount - 1, -1);
            else if (state.tab == Grids && state.searchFocused) break;
            else if (state.tab == Grids && state.cursor[Grids] < (listMode ? 1 : state.grid.columns())) state.searchFocused = true;
            else if (listMode) moveList(-1);
            else if (state.tab == Grids) moveGrid(0, -1);
            break;
        case Event::Down:
            if (state.tab == Overview) moveRow(0, 5, 1);
            else if (state.tab == Lists) moveRow(1, 7, 1);
            else if (state.tab == Appearance) moveRow(0, 7, 1);
            else if (state.tab == Input) moveRow(0, 2, 1);
            else if (state.tab == Flows) moveRow(0, 2, 1);
            else if (state.tab == Data) moveRow(0, 4, 1);
            else if (state.tab == Dialogs) moveRow(0, kDialogCount - 1, 1);
            else if (state.tab == Grids && state.searchFocused) state.searchFocused = false;
            else if (listMode) moveList(1);
            else if (state.tab == Grids) moveGrid(0, 1);
            break;
        case Event::Left:
            if (state.searchFocused) break;
            if (listMode) moveList(-state.list.pageStep());   // in a list, Left and Right page
            else if (state.tab == Grids) moveGrid(-1, 0);
            break;
        case Event::Right:
            if (state.searchFocused) break;
            if (listMode) moveList(state.list.pageStep());
            else if (state.tab == Grids) moveGrid(1, 0);
            break;
        case Event::CycleView:
            if (state.tab == Grids) state.viewMode = (state.viewMode + 1) % 3;
            if (state.tab == TextTab) state.toast.show(Text::key("demo.toast.notice"), Tone::Neutral);
            break;
        case Event::Confirm:
            if (state.tab == TextTab) state.toast.show(Text::key("demo.toast.done"), Tone::Done);
            if (state.tab == Input && state.cursor[Input] < 2) {
                const int i = state.cursor[Input];
                TextEntrySpec spec;
                spec.title = Text::key(i == 0 ? "demo.input.name" : "demo.input.password");
                spec.placeholder = spec.title;
                spec.value = state.field[i];
                spec.masked = i == 1;
                spec.live = false;
                spec.maxLength = 64;
                spec.trim = true;
                state.entry = TextEntryDialog(spec);
                state.entryTarget = i;
            }
            if (state.tab == Grids && state.searchFocused) {
                TextEntrySpec spec;
                spec.title = Text::key("demo.grids.searchTitle");
                spec.placeholder = Text::key("demo.grids.search");
                spec.value = state.gridSearch;
                spec.live = true;
                spec.maxLength = 64;
                spec.trim = true;
                state.entry = TextEntryDialog(spec);
                state.entryTarget = 2;
            }
            if (state.tab == Flows) {
                if (state.cursor[Flows] < 2) startFlow(state, state.cursor[Flows]);
                else state.crashPreview = true;
            }
            if (state.tab == Dialogs) openDemoDialog(state, state.cursor[Dialogs], assets);
            if (state.tab == Appearance) {
                switch (state.cursor[Appearance]) {
                    case 0:
                        state.tabDisplay = state.tabDisplay == TabDisplay::Names ? TabDisplay::Icons
                                           : state.tabDisplay == TabDisplay::Icons ? TabDisplay::Both : TabDisplay::Names;
                        break;
                    case 1: state.textScale = state.textScale == 100 ? 115 : state.textScale == 115 ? 130 : 100; break;
                    case 2: state.pseudo = !state.pseudo; break;
                    case 3: state.forceWarning = !state.forceWarning; break;
                    case 4: state.dark = !state.dark; break;
                    case 5: state.colorTheme = (state.colorTheme + 1) % 8; break;
                    case 6: state.highContrast = !state.highContrast; break;
                    case 7: state.highlight = (state.highlight + 1) % 8; break;
                }
            }
            break;
        case Event::Delete: case Event::Start: break;
        case Event::Back:
            if (state.tab == Grids && state.searchFocused) { state.searchFocused = false; break; }
            return true;
    }
    return false;
}

void drawTab(Painter &p, const Theme &t, State &state, const Rect &content, const Assets &assets) {
    p.pushClip(content.inset(-t.px(4)));
    switch (state.tab) {
        case Overview:   drawOverview(p, t, content, state.cursor[Overview]); break;
        case TextTab:    drawTextTab(p, t, content); break;
        case Lists:      drawListsTab(p, t, content, state.cursor[Lists]); break;
        case Grids:      drawGridsTab(p, t, state, content, assets); break;
        case Dialogs:    drawDialogsTab(p, t, state, content); break;
        case Appearance: drawAppearanceTab(p, t, state, content); break;
        case Input:      drawInputTab(p, t, state, content); break;
        case Flows:      drawFlowsTab(p, t, state, content); break;
        case Data:       drawDataTab(p, t, state, content); break;
        default:         drawPlaceholder(p, t, content); break;
    }
    p.popClip();
}

void openDemoDialog(State &state, int index, const Assets &assets) {
    auto key = [](const char *k) { return Text::key(k); };
    switch (index) {
        case 0:
            state.modals.push(Modal::info(key("demo.dialog.info.title"), key("demo.dialog.info.body")));
            break;
        case 1:
            state.modals.push(Modal::confirm(key("demo.dialog.confirm.title"), key("demo.dialog.confirm.body")));
            break;
        case 2:
            state.modals.push(Modal::error(key("demo.dialog.error.title"), key("demo.dialog.error.what"),
                                           key("demo.dialog.error.todo"), key("demo.dialog.error.details"), true));
            break;
        case 3:
            state.modals.push(Modal::result(Tone::Done, key("demo.dialog.result.title"), key("demo.dialog.result.status"),
                                            key("demo.dialog.result.body")));
            break;
        case 4:
            state.modals.push(Modal::progress(key("demo.dialog.progress.title"), key("demo.dialog.progress.status"), 0.62f));
            break;
        case 5: {
            std::vector<ModalChoice> choices(3);
            choices[0].title = key("demo.dialog.choose.first");
            choices[1].title = key("demo.dialog.choose.second");
            choices[2].title = key("demo.dialog.choose.third");
            for (ModalChoice &c : choices) c.detail = key("demo.dialog.choose.detail");
            choices[0].checked = true;
            state.modals.push(Modal::choose(key("demo.dialog.choose.title"), choices));
            break;
        }
        case 6:
            state.modals.push(Modal::viewer(assets.samplesDetail));
            break;
        case 7: {
            // A list, and over it a confirmation: the list stays visible, dimmed further; only the top reacts.
            std::vector<ModalChoice> choices(3);
            choices[0].title = key("demo.dialog.choose.first");
            choices[1].title = key("demo.dialog.choose.second");
            choices[2].title = key("demo.dialog.choose.third");
            for (ModalChoice &c : choices) c.detail = key("demo.dialog.choose.detail");
            state.modals.push(Modal::choose(key("demo.dialog.choose.title"), choices));
            state.modals.push(Modal::confirm(key("demo.dialog.confirm.title"), key("demo.dialog.confirm.body")));
            break;
        }
    }
}

void drawOverlays(Painter &p, const Theme &t, State &state, const ShellSpec &spec) {
    if (state.crashPreview) {
        drawCrashScreen(p, t, Text::key("demo.crash.title").str(), Text::key("demo.crash.what").str(),
                        Text::key("demo.crash.changed").str(), Text::key("demo.crash.details").str(),
                        Text::key("demo.crash.next").str());
        return;
    }
    if (state.entryTarget >= 0) {
        state.entry.draw(p, t, p.bounds());
    } else if (state.flow && state.flow->active()) {
        state.flow->draw(p, t, p.bounds());
    } else {
        if (state.modals.empty()) {
            state.toast.draw(p, t, p.bounds());   // over the page, never over a dialog
            return;
        }
        state.modals.draw(p, t, p.bounds());
    }
    // The help bar is redrawn above the scrim: it shows the top dialog's help.
    const Rect all = p.bounds();
    p.pushClip({0, all.h - t.bottomBarHeight(), all.w, t.bottomBarHeight()});
    drawPage(p, t, all);
    drawShellBottomBar(p, t, spec);
    p.popClip();
}

// ---------------------------------------------------------------- generated pictures

namespace {

// Converts hue (0..1), saturation and value to an opaque ARGB pixel.
uint32_t hsv(float h, float s, float v) {
    h = h - std::floor(h);
    const float f = h * 6.0f;
    const int i = int(f);
    const float ff = f - float(i);
    const float pp = v * (1 - s), q = v * (1 - s * ff), tt = v * (1 - s * (1 - ff));
    float r = v, g = tt, b = pp;
    switch (i % 6) {
        case 0: r = v;  g = tt; b = pp; break;
        case 1: r = q;  g = v;  b = pp; break;
        case 2: r = pp; g = v;  b = tt; break;
        case 3: r = pp; g = q;  b = v;  break;
        case 4: r = tt; g = pp; b = v;  break;
        case 5: r = v;  g = pp; b = q;  break;
    }
    return 0xFF000000u | (uint32_t(r * 255) << 16) | (uint32_t(g * 255) << 8) | uint32_t(b * 255);
}

// Cover-like sample art: a diagonal gradient, a large disc and stripes. Generated at the exact art size,
// so the grid never scales a picture (docs: box art is fixed-size).
ImagePtr makeSample(int w, int h, int seed) {
    ImagePtr img(new Image(w, h));
    const float hue = 0.07f + 0.16f * float(seed);
    for (int y = 0; y < h; ++y) {
        uint32_t *row = img->row(y);
        for (int x = 0; x < w; ++x) {
            const float d = float(x + y) / float(w + h);
            const float cx = (x - w * 0.62f) / w, cy = (y - h * 0.38f) / h;
            const bool disc = cx * cx + cy * cy < 0.055f;
            const bool stripe = y > h * 0.72f && ((y / std::max(1, h / 14)) % 2 == 0);
            row[x] = disc ? hsv(hue + 0.5f, 0.35f, 0.95f)
                          : stripe ? hsv(hue + 0.02f, 0.8f, 0.30f)
                                   : hsv(hue + 0.10f * d, 0.72f, 0.92f - 0.55f * d);
        }
    }
    img->setOpaque(true);
    return img;
}

}  // namespace

Assets makeAssets(const Theme &t) {
    Assets a;
    a.logo = makeLogo(t);
    // Tab icons: Bootstrap Icons through the SVG renderer, one per tab, as coverage masks.
    static const char *tabIconNames[TabCount] = {"house", "fonts", "list-ul", "table", "grid-3x3-gap",
                                                  "window", "diagram-3", "controller", "palette"};
    for (int i = 0; i < TabCount; ++i) a.tabIcons.push_back(makeBootstrapIcon(tabIconNames[i], t.px(26), t.px(26)));
    const int artW = t.px(245), artH = t.px(245);
    for (int i = 0; i < 6; ++i) a.samples.push_back(makeSample(artW, artH, i));
    for (int i = 0; i < 6; ++i) a.samplesSmall.push_back(makeSample(t.px(128), t.px(128), i));
    for (int i = 0; i < 6; ++i) a.samplesDetail.push_back(makeSample(t.px(360), t.px(360), i));
    static const char *bigIconNames[4] = {"controller", "joystick", "gear", "collection"};
    for (int i = 0; i < 4; ++i) a.bigIcons.push_back(makeBootstrapIcon(bigIconNames[i], t.px(160), t.px(160)));
    return a;
}

ImagePtr makeLogo(const Theme &t) {
    const int n = 128;
    ImagePtr img(new Image(n, n));
    const Color top = t.palette.accent, bottom = Color::lerp(t.palette.accent, t.palette.backgroundLo, 0.55f);
    for (int y = 0; y < n; ++y) {
        const Color row = Color::lerp(top, bottom, float(y) / (n - 1));
        for (int x = 0; x < n; ++x) img->row(y)[x] = row.argb();
    }
    auto fill = [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) img->row(y)[x] = Color::rgb(0xFFFFFF).argb();
    };
    // A simple "P": a stem and a bowl.
    fill(38, 28, 54, 100);
    fill(38, 28, 88, 44);
    fill(38, 62, 88, 78);
    fill(74, 28, 90, 78);
    img->setOpaque(true);
    return img;
}

}  // namespace demo
}  // namespace ui
