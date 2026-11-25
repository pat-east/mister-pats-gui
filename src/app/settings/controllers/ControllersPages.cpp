#include "app/settings/controllers/ControllersPages.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "app/settings/StandardPages.h"
#include "app/settings/controllers/PadDiagram.h"
#include "app/settings/controllers/PadLogic.h"
#include "ui-framework/kit/List.h"
#include "ui-framework/kit/EmptyPage.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/Primitives.h"

namespace patsgui {
namespace controllers {

namespace {

using ui::Rect;

std::string tr(const char *key) { return ui::Text::key(key).str(); }
std::string whole(float v) { return std::to_string(long(std::lround(v))); }   // "%.0f"

std::string hex4(int v) {
    char b[16];
    std::snprintf(b, sizeof(b), "%04x", unsigned(v) & 0xFFFFu);
    return b;
}

// Classic's showError("Could not save", ...) / showError("Could not start", ...): an Error page with Classic's words.
void pushError(PageStack &stack, const char *titleKey, const char *bodyKey) {
    ResultSpec spec;
    spec.kind = ResultSpec::Error;
    spec.title = ui::Text::key(titleKey);
    spec.body = ui::Text::key(bodyKey);
    stack.push(std::unique_ptr<SettingsPage>(new ResultPage(spec)));
}

ui::Rect drawHeader(ui::Painter &p, const ui::Theme &t, const Rect &content, const std::string &title, const ui::Text &info = ui::Text()) {
    ui::PageHeaderSpec header;
    header.title = ui::Text::data(title);
    header.info = info;
    return ui::drawPageHeader(p, t, content, header);
}

bool sameSnapshot(const PadSnapshot &a, const PadSnapshot &b) {
    if (a.connected != b.connected || a.keys != b.keys || a.logicalKeys != b.logicalKeys || a.genericPad != b.genericPad ||
        a.axisDirX != b.axisDirX || a.axisDirY != b.axisDirY)
        return false;
    for (int i = 0; i < 6; ++i)
        if (a.axes[i].present != b.axes[i].present || a.axes[i].value != b.axes[i].value) return false;
    for (int i = 0; i < 8; ++i)
        if (a.hats[i].present != b.hats[i].present || a.hats[i].value != b.hats[i].value) return false;
    return true;
}

// ------------------------------------------------------------------------------------------------ list

// Classic: Mode::List (renderList, handleListAction).
class ListPage : public SettingsPage {
public:
    ListPage(ControllersService &s, std::shared_ptr<ListFlow> flow) : svc_(s), flow_(std::move(flow)) {
        bluetooth_ = svc_.bluetoothAvailable();   // sampled when the screen opens (refresh())
    }

    PagePresentation presentation() const override { return PagePresentation::Content; }

    void draw(ui::Painter &p, const ui::Theme &t, const Rect &content) override {
        const std::vector<ControllerInfo> &rows = svc_.controllers();
        const Rect body = drawHeader(p, t, content, tr("settings.controllers.list.title"),
                                     rows.empty() ? ui::Text() : ui::Text::data(ui::format(rows.size() == 1 ? "settings.controllers.list.count1" : "settings.controllers.list.count",
                                                                                            {std::to_string(rows.size())})));
        if (rows.empty() && !bluetooth_) {   // nothing to choose: why, and what to do (the same panel as every empty page)
            ui::EmptyPageSpec spec;
            spec.title = ui::Text::key("settings.controllers.list.empty");
            spec.why = ui::Text::key("settings.controllers.list.empty.why");
            spec.next = ui::Text::key("settings.controllers.list.empty.next");
            ui::drawEmptyPage(p, t, body, spec);
            return;
        }
        clamp();
        const int listWidth = std::min(body.w * 45 / 100, t.px(560));
        const Rect list{body.x, body.y, listWidth, body.h};
        const Rect detail{list.right() + t.gap(), body.y, body.w - listWidth - t.gap(), body.h};

        layout_.configure({list.x, list.y, std::max(0, list.w - ui::scrollbarGutter(t)), list.h}, ui::listItemFrameHeight(t, false), ui::listItemPitch(t, false));
        layout_.ensureVisible(flow_->cursor);
        const int count = itemCount();
        for (int i = 0; i < count; ++i) {
            if (!layout_.visible(i)) continue;
            ui::ListItemSpec spec;
            if (i < int(rows.size())) {
                spec.kind = ui::ListItemKind::Nav;      // A opens the input test
                spec.title = ui::Text::data(rows[size_t(i)].name);
            } else {
                spec.kind = ui::ListItemKind::Action;   // A starts the Bluetooth search
                spec.title = ui::Text::key("settings.controllers.list.bluetooth");
            }
            ui::ListItemState st;
            st.cursor = i == flow_->cursor;
            ui::drawListItem(p, t, layout_.row(i), spec, st);
        }
        ui::drawScrollbar(p, t, list, count, layout_.visibleRows(), layout_.scroll());

        ui::drawSurface(p, t, detail, ui::SurfaceRole::Faint, t.px(ui::Radius::panel));
        const Rect inner = detail.inset(t.px(26));
        const int step = t.px(10);
        int y = inner.y;
        const int colW = std::min(inner.w, ui::readingWidth(t));   // left-aligned at the panel's inset, never centred
        auto line = [&](const std::string &text, bool bold, ui::TextTier tier) {
            y += ui::drawTextBlock(p, t, {inner.x, y, colW, std::max(0, inner.bottom() - y)}, text, ui::TextRole::Body, tier, bold) + step;
        };
        if (flow_->cursor >= int(rows.size())) {
            if (bluetooth_) line(tr("settings.controllers.list.bluetoothHelp"), false, ui::TextTier::Muted);
            return;
        }
        const ControllerInfo &d = rows[size_t(flow_->cursor)];
        line(d.name, true, ui::TextTier::Primary);
        line(ui::format("settings.controllers.detail.ids", {hex4(d.vendor) + ":" + hex4(d.product), tr(d.hasHat ? "settings.controllers.yes" : "settings.controllers.no")}),
             false, ui::TextTier::Faint);
        if (d.sharesMapping) {
            line(tr("settings.controllers.detail.shares"), false, ui::TextTier::Primary);   // Classic: the accent line
        }
        y += t.px(8);
        line(d.hasDeadzone ? ui::format("settings.controllers.detail.deadzoneSet", {std::to_string(d.deadzone)})
                           : tr("settings.controllers.detail.deadzoneNone"), false, ui::TextTier::Primary);
        line(tr(d.hasMapping ? "settings.controllers.detail.mappingSet" : "settings.controllers.detail.mappingNone"), false,
             ui::TextTier::Primary);
    }

    bool handle(Event event, PageStack &stack) override {
        const std::vector<ControllerInfo> &rows = svc_.controllers();
        clamp();
        const int rowCount = int(rows.size());
        switch (event) {
            case Event::Up: if (flow_->cursor > 0) --flow_->cursor; break;
            case Event::Down: if (flow_->cursor + 1 < itemCount()) ++flow_->cursor; break;
            case Event::Back: stack.pop(); break;      // onClose_(): back to Settings
            case Event::Confirm:
            case Event::Start:                          // Start acts like A (App::dispatch)
                if (flow_->cursor == rowCount && bluetooth_) openBluetoothFlow(svc_, flow_, stack);
                else if (selected()) stack.push(makeInputTestPage(svc_, flow_, *selected()));
                break;
            case Event::X:                              // the left face button: the deadzone needs an analog stick
                if (const ControllerInfo *d = selected()) {
                    const PadSnapshot s = svc_.pad(d->deviceId);
                    if (s.connected && s.axes[code::kAbsX].present) stack.push(makeDeadzonePage(svc_, flow_, *d));
                    else stack.toast(ui::Text::key("settings.controllers.noStick"));
                }
                break;
            case Event::Y:                              // the top face button: button mapping
                if (selected()) openMappingFlow(svc_, flow_, *selected(), stack);
                break;
            default: break;
        }
        return true;
    }

    // Only what the focus really does: a controller (test, deadzone when it has a stick, set up buttons), the Bluetooth row
    // (search), nothing else. Classic shows one static line for every focus.
    std::vector<ui::Hint> hints() const override {
        std::vector<ui::Hint> h;
        if (const ControllerInfo *d = selected()) {
            if (d->hasAnalogStick) h.push_back(ui::hint(ui::HintKey::X, "settings.controllers.hint.deadzone"));
            h.push_back(ui::hint(ui::HintKey::Y, "settings.controllers.hint.setup"));
            h.push_back(ui::hint(ui::HintKey::A, "settings.controllers.hint.test", true));
        } else if (bluetooth_ && flow_->cursor >= int(svc_.controllers().size())) {
            h.push_back(ui::hint(ui::HintKey::A, "settings.controllers.hint.connect", true));
        }
        h.push_back(ui::hint(ui::HintKey::B, "hint.back", true));
        return h;
    }

private:
    int itemCount() const { return int(svc_.controllers().size()) + (bluetooth_ ? 1 : 0); }
    void clamp() { flow_->cursor = std::max(0, std::min(flow_->cursor, std::max(0, itemCount() - 1))); }
    const ControllerInfo *selected() const {
        const std::vector<ControllerInfo> &rows = svc_.controllers();
        return flow_->cursor >= 0 && flow_->cursor < int(rows.size()) ? &rows[size_t(flow_->cursor)] : nullptr;
    }

    ControllersService &svc_;
    std::shared_ptr<ListFlow> flow_;
    bool bluetooth_ = false;
    ui::ListLayout layout_;
};

// ------------------------------------------------------------------------------------------------ input test

// Classic: Mode::InputTest. Every input only lights up; the way out is Start held 5 s or 10 s without any input.
class InputTestPage : public SettingsPage {
public:
    InputTestPage(ControllersService &s, std::shared_ptr<ListFlow> flow, const ControllerInfo &d) : svc_(s), flow_(std::move(flow)), dev_(d) {
        pad_ = svc_.pad(dev_.deviceId);
        logic_.begin(pad_);
    }

    PagePresentation presentation() const override { return PagePresentation::Content; }

    bool update(float dt, PageStack &stack) override {
        const PadSnapshot now = svc_.pad(dev_.deviceId);
        const InputTestLogic::Outcome o = logic_.update(dt, now);
        if (o == InputTestLogic::Outcome::Disconnected) {   // a disconnect falls back to a fresh list
            svc_.refreshControllers();
            flow_->cursor = 0;
            stack.pop();
            return true;
        }
        if (o == InputTestLogic::Outcome::Exit) { stack.pop(); return true; }
        const long idleShown = logic_.idle() > InputTestLogic::kIdleWarningSeconds ? std::lround(InputTestLogic::kIdleSeconds - logic_.idle()) : -1;
        const bool holdShown = logic_.startHold() > 0.0f;
        const bool changed = !sameSnapshot(now, pad_) || idleShown != idleShown_ || holdShown != holdShown_ ||
                             (holdShown && int(logic_.startHold() * 20) != holdStep_);
        pad_ = now;
        idleShown_ = idleShown;
        holdShown_ = holdShown;
        holdStep_ = int(logic_.startHold() * 20);
        return changed;
    }

    void draw(ui::Painter &p, const ui::Theme &t, const Rect &content) override {
        // Close to firing, the countdown is the header's info (words, not colour): a constant one would be noise.
        ui::Text info;
        if (logic_.idle() > InputTestLogic::kIdleWarningSeconds)
            info = ui::Text::data(ui::format("settings.controllers.test.idle", {whole(InputTestLogic::kIdleSeconds - logic_.idle())}));
        const Rect body = drawHeader(p, t, content, ui::format("settings.controllers.test.title", {dev_.name}), info);
        // The page's one instruction, centred over the pad (the pad is centred too: one axis).
        const int lineH = p.lineHeight(ui::FontRole::Regular, t.size(ui::TextRole::Body)) + t.px(10);
        ui::drawLabel(p, t, {body.x, body.y, body.w, lineH}, tr("settings.controllers.test.instruction"), ui::TextRole::Body, ui::TextTier::Primary, ui::Align::Center);
        Rect pad{body.x, body.y + lineH, body.w, body.h - lineH};
        // Start held: the shared hold hint with its bar (HoldProgress), in a row of its own so the pad does not move.
        const int holdH = t.px(44);
        pad.h -= holdH;
        if (logic_.startHold() > 0.0f) {
            const int w = std::min(body.w, t.px(480));
            ui::drawHoldProgress(p, t, {body.x + (body.w - w) / 2, pad.bottom(), w, holdH}, ui::hint(ui::HintKey::Start, "settings.controllers.hint.exit", true, true),
                                 std::min(1.0f, logic_.startHold() / InputTestLogic::kHoldSeconds));
        }
        drawInputTestPad(p, t, pad, pad_);
    }

    bool handle(Event, PageStack &) override { return true; }   // nothing acts: every press only lights up

    // Classic: "Press buttons or move sticks to test   Hold Start 5s to exit" (the first half is the page's instruction line).
    std::vector<ui::Hint> hints() const override {
        return {ui::hint(ui::HintKey::Start, "settings.controllers.hint.exit", true, true)};
    }

private:
    ControllersService &svc_;
    std::shared_ptr<ListFlow> flow_;
    ControllerInfo dev_;
    InputTestLogic logic_;
    PadSnapshot pad_;
    long idleShown_ = -1;
    bool holdShown_ = false;
    int holdStep_ = 0;
};

// ------------------------------------------------------------------------------------------------ deadzone

// Classic: Mode::Deadzone (enterDeadzone, handleDeadzoneAction, renderDeadzone).
class DeadzonePage : public SettingsPage {
public:
    DeadzonePage(ControllersService &s, std::shared_ptr<ListFlow> flow, const ControllerInfo &d) : svc_(s), flow_(std::move(flow)), dev_(d) {
        int existing = 0;
        value_ = svc_.readDeadzone(dev_.deviceId, existing) ? existing : 25;   // read again from MiSTer.ini, else 25
        pad_ = svc_.pad(dev_.deviceId);
    }

    PagePresentation presentation() const override { return PagePresentation::Content; }

    bool update(float, PageStack &stack) override {
        const PadSnapshot now = svc_.pad(dev_.deviceId);
        if (!now.connected) {   // a disconnect falls back to a fresh list
            svc_.refreshControllers();
            flow_->cursor = 0;
            stack.pop();
            return true;
        }
        const bool changed = now.axes[code::kAbsX].value != pad_.axes[code::kAbsX].value || now.axes[code::kAbsX].present != pad_.axes[code::kAbsX].present;
        pad_ = now;
        return changed;
    }

    void draw(ui::Painter &p, const ui::Theme &t, const Rect &content) override {
        const Rect area = drawHeader(p, t, content, ui::format("settings.controllers.deadzone.title", {dev_.name}));
        // One column, the reading width, left-aligned at the header's edge.
        const Rect body{area.x, area.y, std::min(area.w, ui::readingWidth(t)), area.h};
        const int headH = p.lineHeight(ui::FontRole::Bold, t.size(ui::TextRole::Heading)) + t.px(8);
        ui::drawLabel(p, t, {body.x, body.y, body.w, headH}, ui::format("settings.controllers.deadzone.value", {std::to_string(value_)}),
                      ui::TextRole::Heading, ui::TextTier::Primary, ui::Align::Left, true);
        const Rect track{body.x, body.y + headH + t.px(24), body.w, t.px(20)};
        p.fillRoundedRect(track, t.px(10), t.palette.surface.withAlpha(160));
        p.fillRoundedRect({track.x, track.y, int(long(track.w) * value_ / 100), track.h}, t.px(10), t.palette.accent);
        const AxisReading &x = pad_.axes[code::kAbsX];
        if (x.present && x.maximum > x.minimum) {   // the stick's position: a mark that is a shape, not a colour
            const float fraction = float(x.value - x.minimum) / float(x.maximum - x.minimum);
            const int dotX = track.x + int(float(track.w) * fraction);
            p.fillRoundedRect({dotX - t.px(6), track.y - t.px(14), t.px(12), t.px(48)}, t.px(4), t.palette.textPrimary);
        }
        int y = track.bottom() + t.px(34);
        y += ui::drawTextBlock(p, t, {body.x, y, body.w, std::max(0, area.bottom() - y)}, tr("settings.controllers.deadzone.help"), ui::TextRole::Body,
                               ui::TextTier::Muted) + t.px(16);
        char vid[16];
        std::snprintf(vid, sizeof(vid), "0x%04x%04x", unsigned(dev_.vendor) & 0xFFFFu, unsigned(dev_.product) & 0xFFFFu);
        ui::drawTextBlock(p, t, {body.x, y, body.w, std::max(0, area.bottom() - y)},
                          ui::format("settings.controllers.deadzone.writes", {vid, std::to_string(value_)}), ui::TextRole::Small, ui::TextTier::Faint);
    }

    bool handle(Event event, PageStack &stack) override {
        switch (event) {
            case Event::Left: value_ = std::max(0, value_ - 1); break;
            case Event::Right: value_ = std::min(100, value_ + 1); break;
            case Event::Back: stack.pop(); break;   // nothing saved, no message
            case Event::Confirm:
            case Event::Start:
                if (svc_.saveDeadzone(dev_.deviceId, value_)) {
                    stack.toast(ui::Text::key("settings.controllers.deadzone.saved"));
                    stack.pop();
                } else {
                    pushError(stack, "settings.controllers.saveFailed", "settings.controllers.deadzone.failed");   // stays here
                }
                break;
            default: break;
        }
        return true;
    }

    std::vector<ui::Hint> hints() const override {
        return {ui::hint(ui::HintKey::LeftRight, "settings.controllers.hint.changeDeadzone"), ui::hint(ui::HintKey::A, "settings.controllers.hint.save", true),
                ui::hint(ui::HintKey::B, "hint.cancel", true)};
    }

private:
    ControllersService &svc_;
    std::shared_ptr<ListFlow> flow_;
    ControllerInfo dev_;
    int value_ = 25;
    PadSnapshot pad_;
};

}  // namespace

// ------------------------------------------------------------------------------------------------ factories, texts

std::unique_ptr<SettingsPage> makeListPage(ControllersService &s, std::shared_ptr<ListFlow> flow) { return std::unique_ptr<SettingsPage>(new ListPage(s, std::move(flow))); }
std::unique_ptr<SettingsPage> makeInputTestPage(ControllersService &s, std::shared_ptr<ListFlow> f, const ControllerInfo &d) { return std::unique_ptr<SettingsPage>(new InputTestPage(s, std::move(f), d)); }
std::unique_ptr<SettingsPage> makeDeadzonePage(ControllersService &s, std::shared_ptr<ListFlow> f, const ControllerInfo &d) { return std::unique_ptr<SettingsPage>(new DeadzonePage(s, std::move(f), d)); }

// The exact Classic English texts (src/legacy/ControllersScreen.cpp). `{A}`, `{START}`, ... are button placeholders the framework
// prints in the user's layout (Classic ran these strings through formatButtonLabels).
void registerPageStrings(ui::Catalog &c) {
    // List
    c.set("settings.controllers.list.title", "Controllers");
    c.set("settings.controllers.list.empty", "No controller connected");
    c.set("settings.controllers.list.empty.why", "There is nothing to test or set up yet.");
    c.set("settings.controllers.list.empty.next", "Connect a controller, then open Controllers again.");
    c.set("settings.controllers.list.count1", "{1} controller");
    c.set("settings.controllers.list.count", "{1} controllers");
    c.set("settings.controllers.hint.connect", "Connect controller");
    c.set("settings.controllers.list.bluetooth", "Connect new controller via Bluetooth");
    c.set("settings.controllers.list.bluetoothHelp", "Pair a new controller over Bluetooth. Put it into pairing mode first.");
    c.set("settings.controllers.detail.ids", "{1}  \xC2\xB7  hat: {2}");
    c.set("settings.controllers.yes", "yes");
    c.set("settings.controllers.no", "no");
    c.set("settings.controllers.detail.shares", "Shares its mapping with another identical controller below");
    c.set("settings.controllers.detail.deadzoneSet", "Deadzone: {1} (set)");
    c.set("settings.controllers.detail.deadzoneNone", "Deadzone: not set");
    c.set("settings.controllers.detail.mappingSet", "Button mapping: set");
    c.set("settings.controllers.detail.mappingNone", "Button mapping: not set");
    c.set("settings.controllers.noStick", "This controller has no analog stick to adjust");
    c.set("settings.controllers.hint.test", "Test controller");
    c.set("settings.controllers.hint.deadzone", "Stick deadzone");
    c.set("settings.controllers.hint.setup", "Set up buttons");
    // Input test
    c.set("settings.controllers.test.title", "Input test \xE2\x80\x94 {1}");
    c.set("settings.controllers.test.idle", "No input \xE2\x80\x94 leaving in {1}s");
    c.set("settings.controllers.test.instruction", "Press buttons or move sticks to test");
    c.set("settings.controllers.hint.exit", "to go back");   // "Hold Start to go back": reads as a sentence
    c.set("settings.controllers.test.stick1", "Analog left");
    c.set("settings.controllers.test.stick2", "Analog right");
    c.set("settings.controllers.pad.analogLeft", "Analog left");
    c.set("settings.controllers.pad.analogRight", "Analog right");
    c.set("settings.controllers.test.raw", "X {1} ({2} to {3})   Y {4} ({5} to {6})");
    // Pad parts (the wizard diagram and the test)
    c.set("settings.controllers.pad.l", "L");
    c.set("settings.controllers.pad.r", "R");
    c.set("settings.controllers.pad.home", "Home");
    c.set("settings.controllers.pad.up", "Up");
    c.set("settings.controllers.pad.down", "Down");
    c.set("settings.controllers.pad.left", "Left");
    c.set("settings.controllers.pad.right", "Right");
    // Deadzone
    c.set("settings.controllers.deadzone.title", "Deadzone \xE2\x80\x94 {1}");
    c.set("settings.controllers.deadzone.help", "Move the stick to see its position. It must move past this mark before MiSTer registers a direction.");
    c.set("settings.controllers.deadzone.value", "Deadzone: {1} of 100");
    c.set("settings.controllers.deadzone.writes", "Saved to MiSTer.ini as deadzone={1}, {2}");
    c.set("settings.controllers.hint.changeDeadzone", "Change deadzone");
    c.set("settings.controllers.hint.save", "Save");
    c.set("settings.controllers.deadzone.saved", "Stick deadzone saved");
    c.set("settings.controllers.saveFailed", "Could not save");
    c.set("settings.controllers.deadzone.failed", "MiSTer.ini could not be backed up or written. Nothing was changed.");
    c.set("settings.controllers.mapping.failed", "The mapping file could not be written.");
    c.set("settings.controllers.wizard.fallbackName", "Controller");
    c.set("settings.controllers.step.start", "{START}");
    c.set("settings.controllers.step.dpadRight", "D-pad Right");
    c.set("settings.controllers.step.dpadLeft", "D-pad Left");
    c.set("settings.controllers.step.dpadDown", "D-pad Down");
    c.set("settings.controllers.step.dpadUp", "D-pad Up");
    c.set("settings.controllers.step.faceBottom", "{A} (bottom)");
    c.set("settings.controllers.step.faceRight", "{B} (right)");
    c.set("settings.controllers.step.faceLeft", "{X} (left)");
    c.set("settings.controllers.step.faceTop", "{Y} (top)");
    c.set("settings.controllers.step.l", "L");
    c.set("settings.controllers.step.r", "R");
    c.set("settings.controllers.step.select", "{SELECT}");
    c.set("settings.controllers.step.stick1x", "Analog left: move it left and right");
    c.set("settings.controllers.step.stick1y", "Analog left: move it up and down");
    c.set("settings.controllers.step.stick2x", "Analog right: move it left and right");
    c.set("settings.controllers.step.stick2y", "Analog right: move it up and down");
    c.set("settings.controllers.step.menu", "Open the menu from inside a running game");
    c.set("settings.controllers.step.menuOk", "Confirm inside the menu (\"OK\")");
    c.set("settings.controllers.step.menuBack", "Cancel inside the menu (\"Back\")");
    registerFlowStrings(c);
}

}  // namespace controllers
}  // namespace patsgui
