#include "TopBar.h"

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <time.h>

#include "Canvas.h"

namespace {

const char *kWeekdays[] = {"Sunday",   "Monday", "Tuesday",  "Wednesday",
                           "Thursday", "Friday", "Saturday"};

const char *kMonths[] = {"January", "February", "March",     "April",   "May",      "June",
                         "July",    "August",   "September", "October", "November", "December"};

struct TabEntry {
    Tab tab;
    const char *label;
};

const TabEntry kAllTabs[] = {
    {Tab::Home, "Home"},       {Tab::Favorites, "Favorites"}, {Tab::Systems, "Systems"},
    {Tab::Arcade, "Arcade"},   {Tab::Games, "Games"},         {Tab::Settings, "Settings"},
};

} // namespace

std::vector<Tab> TopBar::visibleTabs(bool showGames, bool showArcade) {
    std::vector<Tab> out;
    for (const TabEntry &entry : kAllTabs) {
        if (entry.tab == Tab::Games && !showGames) continue;
        if (entry.tab == Tab::Arcade && !showArcade) continue;
        out.push_back(entry.tab);
    }
    return out;
}

std::string TopBar::label(Tab tab) {
    for (const TabEntry &entry : kAllTabs)
        if (entry.tab == tab) return entry.label;
    return std::string();
}

void TopBar::refreshClock() {
    const time_t now = time(nullptr);
    tm local{};
    localtime_r(&now, &local);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", local.tm_hour, local.tm_min);
    time_ = buffer;

    std::snprintf(buffer, sizeof(buffer), "%s, %d. %s",
                  kWeekdays[local.tm_wday % 7], local.tm_mday, kMonths[local.tm_mon % 12]);
    date_ = buffer;
}

void TopBar::refreshNetwork() {
    link_ = "Offline";
    address_.clear();

    ifaddrs *list = nullptr;
    if (getifaddrs(&list) != 0) return;

    for (ifaddrs *it = list; it; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
        if (it->ifa_flags & IFF_LOOPBACK) continue;
        if (!(it->ifa_flags & IFF_UP)) continue;

        char buffer[INET_ADDRSTRLEN] = {};
        auto *addr = reinterpret_cast<sockaddr_in *>(it->ifa_addr);
        if (!inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer))) continue;

        address_ = buffer;
        link_ = (std::strncmp(it->ifa_name, "wl", 2) == 0) ? "Wi-Fi" : "Ethernet";
        break;
    }

    freeifaddrs(list);
}

void TopBar::update(float deltaSeconds) {
    clockTimer_ -= deltaSeconds;
    if (clockTimer_ <= 0.0f) {
        refreshClock();
        clockTimer_ = 1.0f;
    }

    networkTimer_ -= deltaSeconds;
    if (networkTimer_ <= 0.0f) {
        refreshNetwork();
        networkTimer_ = 10.0f;
    }
}

void TopBar::render(Canvas &canvas, Theme &theme, const Rect &area, Tab active, bool showGames,
                    bool showArcade, bool tabsEnabled) {
    Font &bold = theme.bold();
    Font &regular = theme.regular();

    // Left: time over date.
    const int left = area.x + theme.marginX();
    const int timeY = area.y + theme.px(16);
    bold.draw(canvas, left, timeY, time_, theme.sizeClock(), theme.textPrimary);
    const int dateY = timeY + bold.lineHeight(theme.sizeClock()) + theme.px(6);
    regular.draw(canvas, left, dateY, date_, theme.sizeSmall(),
                 theme.textMuted.withAlpha(170));

    // Centre: tabs.
    const std::vector<Tab> tabs = visibleTabs(showGames, showArcade);
    const int tabSize = theme.sizeTab();
    const int spacing = theme.px(44);
    int total = 0;
    for (size_t i = 0; i < tabs.size(); ++i) {
        total += bold.measure(label(tabs[i]), tabSize);
        if (i + 1 < tabs.size()) total += spacing;
    }

    int x = area.x + (area.w - total) / 2;
    const int tabY = area.y + (area.h - bold.lineHeight(tabSize)) / 2 - theme.px(4);

    for (size_t i = 0; i < tabs.size(); ++i) {
        const std::string &text = label(tabs[i]);
        const int width = bold.measure(text, tabSize);
        const bool isActive = tabs[i] == active;

        bold.draw(canvas, x, tabY, text, tabSize,
                  isActive ? theme.textPrimary : theme.textMuted.withAlpha(120));

        if (isActive) {
            // The underline glides to the active tab rather than jumping.
            if (indicatorX_ < 0) { indicatorX_ = float(x); indicatorW_ = float(width); }
            indicatorX_ += (float(x) - indicatorX_) * 0.28f;
            indicatorW_ += (float(width) - indicatorW_) * 0.28f;
        }

        x += width + spacing;
    }

    if (indicatorX_ >= 0) {
        const Rect underline{int(indicatorX_), tabY + bold.lineHeight(tabSize) + theme.px(4),
                             int(indicatorW_), theme.px(3)};
        canvas.fillRoundedRect(underline, theme.px(2),
                               tabsEnabled ? theme.accent : Color::rgb(0x808080));
    }

    // Right: link type over address.
    const bool online = !address_.empty();
    const std::string label = online ? link_ : std::string("Offline");
    const int labelWidth = regular.measure(label, theme.sizeBody());
    const int right = area.right() - theme.marginX();

    const int networkY = area.y + theme.px(18);
    regular.draw(canvas, right - labelWidth, networkY, label, theme.sizeBody(),
                 online ? theme.textMuted : theme.warning);

    if (online) {
        const int addressWidth = regular.measure(address_, theme.sizeSmall());
        regular.draw(canvas, right - addressWidth,
                     networkY + regular.lineHeight(theme.sizeBody()) + theme.px(6),
                     address_, theme.sizeSmall(), theme.textMuted.withAlpha(150));
    }
}
