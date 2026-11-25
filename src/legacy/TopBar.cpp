#include "TopBar.h"

#include <sys/statvfs.h>

#include <algorithm>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <time.h>

#include "Canvas.h"
#include "TabIcons.h"

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
    {Tab::Disc, "Disc"},        {Tab::Home, "Home"},          {Tab::Favorites, "Favorites"},
    {Tab::Systems, "Consoles"},
    {Tab::Arcade, "Arcade"},   {Tab::Games, "Games"},         {Tab::Settings, "Settings"},
};

} // namespace

std::vector<Tab> TopBar::visibleTabs(bool showGames, bool showArcade, bool showDisc) {
    std::vector<Tab> out;
    for (const TabEntry &entry : kAllTabs) {
        if (entry.tab == Tab::Games && !showGames) continue;
        if (entry.tab == Tab::Arcade && !showArcade) continue;
        if (entry.tab == Tab::Disc && !showDisc) continue;
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
    if (use24HourClock_) {
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", local.tm_hour, local.tm_min);
        time_ = buffer;
    } else {
        ::strftime(buffer, sizeof(buffer), "%I:%M %p", &local);
        time_ = buffer;
        if (!time_.empty() && time_[0] == '0') time_.erase(0, 1);
    }

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

// "7.9 / 120 GB": one unit for both numbers, chosen by the size of the volume.
static std::string diskNumbers(unsigned long long used, unsigned long long total) {
    const double gib = 1024.0 * 1024.0 * 1024.0, mib = 1024.0 * 1024.0;
    const bool inGb = double(total) >= gib;
    const double unit = inGb ? gib : mib;
    char buffer[64];
    const double u = double(used) / unit, t = double(total) / unit;
    if (t >= 100.0) std::snprintf(buffer, sizeof(buffer), "%.0f / %.0f %s", u, t, inGb ? "GB" : "MB");
    else std::snprintf(buffer, sizeof(buffer), "%.1f / %.1f %s", u, t, inGb ? "GB" : "MB");
    return buffer;
}

// Reads the mount list (a virtual file, no storage access) and asks each of /media/fat and
// /media/usbN for its size. statvfs answers from the file system's own counters, it does not
// scan the card; this runs every 30 seconds, not every frame.
void TopBar::refreshDisks() {
    std::vector<DiskUsage> found;
    if (std::FILE *mounts = std::fopen("/proc/mounts", "r")) {
        char line[512];
        while (std::fgets(line, sizeof(line), mounts)) {
            char device[256], point[256];
            if (std::sscanf(line, "%255s %255s", device, point) != 2) continue;
            const std::string path = point;
            std::string name;
            if (path == "/media/fat") name = "fat";
            else if (path.compare(0, 10, "/media/usb") == 0 && path.size() > 10 &&
                     path.find_first_not_of("0123456789", 10) == std::string::npos)
                name = path.substr(7);
            if (name.empty()) continue;
            struct statvfs info {};
            if (statvfs(path.c_str(), &info) != 0 || info.f_blocks == 0) continue;
            DiskUsage usage;
            usage.name = name;
            usage.total = static_cast<unsigned long long>(info.f_blocks) * info.f_frsize;
            const unsigned long long free = static_cast<unsigned long long>(info.f_bfree) * info.f_frsize;
            const unsigned long long available = static_cast<unsigned long long>(info.f_bavail) * info.f_frsize;
            usage.used = usage.total > free ? usage.total - free : 0;
            usage.warning = available * 20 <= usage.total;   // 5 % or less is left
            usage.numbers = diskNumbers(usage.used, usage.total);
            found.push_back(usage);
        }
        std::fclose(mounts);
    }
    // fat first, then usb0, usb1, ... in number order.
    std::sort(found.begin(), found.end(), [](const DiskUsage &a, const DiskUsage &b) {
        if (a.name == "fat" || b.name == "fat") return a.name == "fat" && b.name != "fat";
        return std::atoi(a.name.c_str() + 3) < std::atoi(b.name.c_str() + 3);
    });
    disks_ = std::move(found);
}

void TopBar::update(float deltaSeconds, bool use24HourClock) {
    constexpr float kFadeSeconds = 0.045f;
    if (indicatorFade_ < 0) {
        indicatorOpacity_ = std::max(0.0f, indicatorOpacity_ - deltaSeconds / kFadeSeconds);
        if (indicatorOpacity_ <= 0.0f) indicatorFade_ = 1;
    } else if (indicatorFade_ > 0) {
        indicatorOpacity_ = std::min(1.0f, indicatorOpacity_ + deltaSeconds / kFadeSeconds);
        if (indicatorOpacity_ >= 1.0f) indicatorFade_ = 0;
    }
    if (use24HourClock_ != use24HourClock) {
        use24HourClock_ = use24HourClock;
        clockTimer_ = 0.0f;
    }
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

    diskTimer_ -= deltaSeconds;
    if (diskTimer_ <= 0.0f) {
        refreshDisks();
        diskTimer_ = 30.0f;
    }
}

void TopBar::render(Canvas &canvas, Theme &theme, const Rect &area, Tab active, bool showGames,
                    bool showArcade, bool showDisc, bool tabsEnabled, TabDisplay display,
                    const TabIcons &icons) {
    Font &bold = theme.bold();
    Font &regular = theme.regular();

    // Left: time over date.
    const int left = area.x + theme.marginX();
    const int timeY = area.y + theme.px(16);
    bold.draw(canvas, left, timeY, time_, theme.sizeClock(), theme.topBarText);
    const int dateY = timeY + bold.lineHeight(theme.sizeClock()) + theme.px(6);
    regular.draw(canvas, left, dateY, date_, theme.sizeSmall(),
                 theme.topBarMuted.withAlpha(205));

    // Centre: tabs.
    const std::vector<Tab> tabs = visibleTabs(showGames, showArcade, showDisc);
    if (!indicatorTabSet_) {
        indicatorTab_ = active;
        indicatorTabSet_ = true;
    } else if (active != indicatorTab_) {
        indicatorTab_ = active;
        indicatorFade_ = -1;
        indicatorOpacity_ = 1.0f;
    }
    const int tabSize = theme.sizeTab();
    const bool wantsIcons = display != TabDisplay::Names;
    const TabDisplay effectiveDisplay = wantsIcons && !icons.allAvailable()
        ? TabDisplay::Names : display;
    const int iconSize = theme.px(TabIcons::kIconSize);
    const int iconGap = theme.px(10);
    const int spacing = theme.px(effectiveDisplay == TabDisplay::Icons ? 54 : 44);
    const auto tabWidth = [&](Tab tab) {
        const int textWidth = bold.measure(label(tab), tabSize);
        if (effectiveDisplay == TabDisplay::Icons) return iconSize;
        if (effectiveDisplay == TabDisplay::Both) return iconSize + iconGap + textWidth;
        return textWidth;
    };
    int total = 0;
    for (size_t i = 0; i < tabs.size(); ++i) {
        total += tabWidth(tabs[i]);
        if (i + 1 < tabs.size()) total += spacing;
    }

    int x = area.x + (area.w - total) / 2;
    const int tabY = area.y + (area.h - bold.lineHeight(tabSize)) / 2 - theme.px(4);

    for (size_t i = 0; i < tabs.size(); ++i) {
        const std::string &text = label(tabs[i]);
        const int width = tabWidth(tabs[i]);
        const bool isActive = tabs[i] == active;

        // Decorative separator between the browsing tabs and Settings.
        if (i > 0 && tabs[i] == Tab::Settings) {
            const int dotSize = theme.px(6);
            const Rect dot{x - spacing / 2 - dotSize / 2,
                           tabY + (bold.lineHeight(tabSize) - dotSize) / 2,
                           dotSize, dotSize};
            canvas.fillRoundedRect(dot, dotSize / 2, theme.topBarMuted.withAlpha(175));
        }

        if (effectiveDisplay != TabDisplay::Names) {
            const int imageX = effectiveDisplay == TabDisplay::Icons
                ? x + (width - iconSize) / 2 : x;
            const int imageY = tabY + (bold.lineHeight(tabSize) - iconSize) / 2;
            if (const Image *image = icons.imageFor(tabs[i]))
                canvas.drawImage(*image, {imageX, imageY, iconSize, iconSize},
                                 isActive ? 255 : 215);
        }
        if (effectiveDisplay != TabDisplay::Icons) {
            const int textX = effectiveDisplay == TabDisplay::Both ? x + iconSize + iconGap : x;
            bold.draw(canvas, textX, tabY, text, tabSize,
                      isActive ? theme.topBarText : theme.topBarMuted);
        }

        if (isActive) {
            if (indicatorX_ < 0 || indicatorFade_ >= 0) {
                indicatorX_ = float(x);
                indicatorW_ = float(width);
            }
        }

        x += width + spacing;
    }

    if (indicatorX_ >= 0) {
        const Rect underline{int(indicatorX_), tabY + bold.lineHeight(tabSize) + theme.px(4),
                             int(indicatorW_), theme.px(3)};
        canvas.fillRoundedRect(underline, theme.px(2),
                               (tabsEnabled ? theme.topBarFocus : Color::rgb(0xA0A0A0))
                                   .withAlpha(uint8_t(255.0f * indicatorOpacity_)));
    }

    // Right: link type over address.
    const bool online = !address_.empty();
    const std::string label = online ? link_ : std::string("Offline");
    const int labelWidth = regular.measure(label, theme.sizeBody());
    const int right = area.right() - theme.marginX();

    const int networkY = area.y + theme.px(18);
    regular.draw(canvas, right - labelWidth, networkY, label, theme.sizeBody(),
                 online ? theme.topBarMuted : theme.warning);

    int networkWidth = labelWidth;
    if (online) {
        const int addressWidth = regular.measure(address_, theme.sizeSmall());
        networkWidth = std::max(networkWidth, addressWidth);
        regular.draw(canvas, right - addressWidth,
                     networkY + regular.lineHeight(theme.sizeBody()) + theme.px(6),
                     address_, theme.sizeSmall(), theme.topBarMuted);
    }

    // Left of the network block, only while a volume is nearly full (5 % or less left):
    //   Disk space
    //   fat:  7.9 / 120 GB
    //   usb0: 432 / 477 GB (!)
    // One line per volume, the labels in one column and the numbers in another, so they line up.
    // All volumes are listed so the warning has context; the red (!) marks the full ones. The block
    // may use the empty margin under the bar (the app restores that strip every frame); more
    // volumes than fit are cut with "...". It is left out when there is no room beside the tabs.
    bool anyWarning = false;
    for (const DiskUsage &disk : disks_) anyWarning = anyWarning || disk.warning;
    if (anyWarning) {
        const int small = theme.sizeSmall();
        const int blockRight = right - networkWidth - theme.px(40);
        const int available = blockRight - (area.x + (area.w + total) / 2 + theme.px(24));
        const int titleHeight = regular.lineHeight(theme.sizeBody());
        const int lineHeight = regular.lineHeight(small);
        const int firstLineY = networkY + titleHeight + theme.px(6);
        const int limitY = area.bottom() + theme.marginY() - theme.px(4);
        size_t room = limitY > firstLineY ? size_t((limitY - firstLineY) / std::max(1, lineHeight)) : 0;
        size_t count = std::min(room, disks_.size());
        const bool cut = count < disks_.size();
        if (cut && count > 0) --count;   // the last line says that more volumes exist

        int labelWidth = 0, valueWidth = 0;
        for (size_t i = 0; i < count; ++i) {
            labelWidth = std::max(labelWidth, regular.measure(disks_[i].name + ":", small));
            valueWidth = std::max(valueWidth, regular.measure(disks_[i].numbers, small) +
                                                   (disks_[i].warning ? regular.measure(" (!)", small) : 0));
        }
        const int title = regular.measure("Disk space", theme.sizeBody());
        const int columnGap = theme.px(10);
        const int blockWidth = std::max(title, labelWidth + columnGap + valueWidth);
        if (count > 0 && blockWidth <= available) {   // never covers a tab
            const int blockLeft = blockRight - blockWidth;
            regular.draw(canvas, blockLeft, networkY, "Disk space", theme.sizeBody(), theme.topBarMuted);
            int y = firstLineY;
            for (size_t i = 0; i < count; ++i) {
                regular.draw(canvas, blockLeft, y, disks_[i].name + ":", small, theme.topBarMuted);
                const int valueX = blockLeft + labelWidth + columnGap;
                regular.draw(canvas, valueX, y, disks_[i].numbers, small, theme.topBarMuted);
                if (disks_[i].warning)
                    regular.draw(canvas, valueX + regular.measure(disks_[i].numbers, small), y, " (!)",
                                 small, theme.warning);
                y += lineHeight;
            }
            if (cut) regular.draw(canvas, blockLeft, y, "...", small, theme.topBarMuted);
        }
    }
}
