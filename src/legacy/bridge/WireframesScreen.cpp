#include "bridge/WireframesScreen.h"

#include "Canvas.h"
#include "bridge/LookBridge.h"

#include <arpa/inet.h>
#include <cstring>
#include <ctime>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/statvfs.h>

#include "ui-framework/kit/Components.h"
#include "ui-framework/render/soft/SoftPainter.h"

namespace {

constexpr float kSplashSeconds = 1.0f;

// The framework's catalog is filled once per run.
void registerStrings() {
    static bool done = false;
    if (done) return;
    done = true;
    ui::registerFrameworkStrings();
    ui::demo::registerStrings();
}

// The first IPv4 address that is up and not the loopback; empty when there is none.
std::string addressText(bool &wireless) {
    wireless = false;
    struct ifaddrs *list = nullptr;
    if (getifaddrs(&list) != 0) return "-";
    std::string result = "-";
    for (struct ifaddrs *i = list; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        if ((i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP)) continue;
        char buffer[INET_ADDRSTRLEN];
        const auto *sin = reinterpret_cast<const struct sockaddr_in *>(i->ifa_addr);
        if (inet_ntop(AF_INET, &sin->sin_addr, buffer, sizeof(buffer))) {
            result = buffer;
            wireless = std::strncmp(i->ifa_name, "wl", 2) == 0;   // as Classic: wl... is Wi-Fi
            break;
        }
    }
    freeifaddrs(list);
    return result;
}

}  // namespace

WireframesScreen::WireframesScreen(Context &context, std::function<void()> onClose)
    : context_(context), onClose_(std::move(onClose)) {}

void WireframesScreen::open() {
    state_ = ui::demo::State();
    {   // the demo starts in the user's look; its Appearance tab changes its own copy from there
        const patsgui::LookSettings look = classicLook(context_);
        state_.textScale = look.textScale;
        state_.dark = look.dark;
        state_.colorTheme = look.colorTheme >= 0 ? look.colorTheme : 0;
        state_.highContrast = look.highContrast;
        state_.highlight = look.highlight;
    }
    splash_ = true;
    splashSeconds_ = 0.0f;
    lastMinute_ = -1;
    dirty_ = true;
}

void WireframesScreen::refreshSystemValues() {
    const std::time_t now = std::time(nullptr);
    std::tm local;
    localtime_r(&now, &local);
    char clock[16], date[32];
    std::strftime(clock, sizeof(clock), "%H:%M", &local);
    std::strftime(date, sizeof(date), "%a %d %b", &local);
    values_.clock = clock;
    values_.date = date;
    values_.address = addressText(values_.wireless);
    lastMinute_ = local.tm_min;
}

void WireframesScreen::ensureResources(int width, int height) {
    if (canvas_ && canvas_->width() == width && canvas_->height() == height) return;
    registerStrings();
    regular_.reset(new ui::Font());
    bold_.reset(new ui::Font());
    // The same font files the GUI itself uses, so the demo reads like the real thing.
    appliedLook_ = classicLook(context_);
    patsgui::applyLookFonts(*regular_, *bold_, appliedLook_);
    theme_.reset(new ui::Theme(width, height));
    canvas_.reset(new ui::Canvas(width, height));
    assets_ = ui::demo::makeAssets(*theme_);
    refreshSystemValues();
    dirty_ = true;
}

void WireframesScreen::syncFonts() {
    if (!regular_ || !bold_) return;
    const patsgui::LookSettings look = classicLook(context_);
    if (look.sameFonts(appliedLook_)) return;
    patsgui::applyLookFonts(*regular_, *bold_, look);
    appliedLook_ = look;
    dirty_ = true;
}

void WireframesScreen::update(float deltaSeconds) {
    syncFonts();
    if (splash_) {
        splashSeconds_ += deltaSeconds;
        if (splashSeconds_ >= kSplashSeconds) splash_ = false;
        dirty_ = true;
        return;
    }
    if (ui::demo::update(state_, deltaSeconds)) dirty_ = true;
    // The clock changes once a minute; nothing else moves on its own.
    const std::time_t now = std::time(nullptr);
    std::tm local;
    localtime_r(&now, &local);
    if (local.tm_min != lastMinute_) {
        refreshSystemValues();
        dirty_ = true;
    }
}

void WireframesScreen::render(Canvas &, const Rect &, bool) {}

void WireframesScreen::renderFull(Canvas &canvas, bool fullRedraw) {
    ensureResources(canvas.width(), canvas.height());
    if (!dirty_ && !fullRedraw) return;

    ui::SoftPainter painter(*canvas_, *regular_, *bold_);
    if (splash_) {
        ui::drawSplash(painter, *theme_, assets_.logo.get(), splashSeconds_ / kSplashSeconds, std::string());
    } else {
        const ui::demo::Options options;
        ui::demo::applyAppearance(state_, *theme_);
        const ui::ShellSpec shell = ui::demo::shellFor(state_, options, values_, assets_);
        const ui::Rect content = ui::drawShell(painter, *theme_, shell);
        ui::demo::drawTab(painter, *theme_, state_, content, assets_);
        ui::demo::drawOverlays(painter, *theme_, state_, shell);
    }
    // The framework's canvas has the framebuffer's size: one copy puts it on the GUI's canvas.
    canvas.blit(canvas_->row(0), canvas_->width(), canvas_->height(), 0, 0);
    dirty_ = false;
}

void WireframesScreen::handle(Action action) {
    if (splash_) return;   // the splash is not interactive; Back works once the shell is up
    ui::demo::Event event;
    switch (action) {
        case Action::TabPrev: event = ui::demo::Event::TabPrev; break;
        case Action::TabNext: event = ui::demo::Event::TabNext; break;
        case Action::Up:      event = ui::demo::Event::Up; break;
        case Action::Down:    event = ui::demo::Event::Down; break;
        case Action::Left:    event = ui::demo::Event::Left; break;
        case Action::Right:   event = ui::demo::Event::Right; break;
        case Action::Confirm: event = ui::demo::Event::Confirm; break;
        case Action::CycleView: event = ui::demo::Event::CycleView; break;
        case Action::Back:    event = ui::demo::Event::Back; break;
        case Action::FaceXPress: event = ui::demo::Event::Delete; break;
        default: return;
    }
    if (ui::demo::handle(state_, event, assets_)) {
        if (onClose_) onClose_();
        return;
    }
    dirty_ = true;
}
