#include "bridge/PatsGuiScreen.h"

#include "Canvas.h"
#include "DebugLog.h"
#include "Version.h"
#include "bridge/AppLibraryAdapter.h"
#include "bridge/LookBridge.h"
#include "app/SampleLibrary.h"

#include <arpa/inet.h>
#include <ctime>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/statvfs.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unistd.h>

#include "Image.h"
#include "UiSounds.h"
#include "SplashImage.h"
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
    patsgui::registerStrings();
}

// "128GB" / "512MB": the size of a volume, in the unit that fits.
std::string sizeText(double bytes) {
    char text[32];
    if (bytes >= 1e9) std::snprintf(text, sizeof(text), "%.0fGB", bytes / 1e9);
    else std::snprintf(text, sizeof(text), "%.0fMB", bytes / 1e6);
    return text;
}

// The mounted volumes (Classic TopBar::refreshDisks): /media/fat and /media/usbN, read from the mount list (a virtual
// file) and sized with statvfs, which answers from the file system's own counters. "fat" first, then usb0, usb1, ...
std::vector<patsgui::DiskVolume> diskVolumes() {
    std::vector<patsgui::DiskVolume> found;
    std::FILE *mounts = std::fopen("/proc/mounts", "r");
    if (!mounts) return found;
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
        struct statvfs vfs;
        if (statvfs(path.c_str(), &vfs) != 0 || vfs.f_blocks == 0) continue;
        const double total = double(vfs.f_blocks) * double(vfs.f_frsize);
        const double available = double(vfs.f_bavail) * double(vfs.f_frsize);
        const double used = total - double(vfs.f_bfree) * double(vfs.f_frsize);
        patsgui::DiskVolume volume;
        volume.name = name;
        volume.percentUsed = int(std::lround(used * 100.0 / total));
        volume.size = sizeText(total);
        volume.low = available * 20.0 <= total;   // 5 % or less is left
        found.push_back(volume);
    }
    std::fclose(mounts);
    std::sort(found.begin(), found.end(), [](const patsgui::DiskVolume &a, const patsgui::DiskVolume &b) {
        if (a.name == "fat" || b.name == "fat") return a.name == "fat" && b.name != "fat";
        return std::atoi(a.name.c_str() + 3) < std::atoi(b.name.c_str() + 3);
    });
    return found;
}

// The first IPv4 address that is up and not the loopback, and whether it belongs to a wireless interface ("wl...",
// as Classic's TopBar::refreshNetwork decides); offline when there is none.
void networkState(std::string &address, patsgui::NetworkLink &link) {
    address.clear();
    link = patsgui::NetworkLink::Offline;
    struct ifaddrs *list = nullptr;
    if (getifaddrs(&list) != 0) return;
    for (struct ifaddrs *i = list; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        if ((i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP)) continue;
        char buffer[INET_ADDRSTRLEN];
        const auto *sin = reinterpret_cast<const struct sockaddr_in *>(i->ifa_addr);
        if (!inet_ntop(AF_INET, &sin->sin_addr, buffer, sizeof(buffer))) continue;
        address = buffer;
        link = std::strncmp(i->ifa_name, "wl", 2) == 0 ? patsgui::NetworkLink::Wifi : patsgui::NetworkLink::Ethernet;
        break;
    }
    freeifaddrs(list);
}

}  // namespace

PatsGuiScreen::PatsGuiScreen(Context &context, std::function<void()> onClose)
    : context_(context), onClose_(std::move(onClose)) {}

void PatsGuiScreen::open(bool atStart) {
    // Fall back to generic sample data when the real library is not ready (nothing scanned yet).
    if (AppLibraryAdapter::ready(context_)) library_.reset(new AppLibraryAdapter(context_));
    else library_.reset(new patsgui::SampleLibrary());
    library_->refresh();
    services_.reset(new patsgui::Services{*library_});
    AdapterEnv adapterEnv{context_, [this] { return theme_.get(); }, [this] { dirty_ = true; }};
    settingsAdapters_.reset(new SettingsAdapters(adapterEnv));
    services_->settings = &settingsAdapters_->services();
    noticeSerial_ = context_.messageSerial;   // what Classic said before the GUI opened is not shown again
    state_ = patsgui::State();
    state_.useStartTab = true;   // Settings > Interface > Start tab (Classic: App::init); falls back to Home for a hidden tab
    patsgui::attach(state_, *services_);
    drawnRevision_ = library_->revision();
    drawnSerial_ = library_->previewFrameSerial();
    previewDirty_ = false;
    stats_ = PreviewStats();
    if (frame_) frame_->invalidate();
    splash_ = !atStart;
    splashSeconds_ = 0.0f;
    lastMinute_ = -1;
    firstRunSeen_ = false;
    dirty_ = true;
}

void PatsGuiScreen::startFirstRun() {
    firstRunSeen_ = patsgui::startFirstRun(state_);
    dirty_ = true;
}

void PatsGuiScreen::refreshSystemValues() {
    const std::time_t now = std::time(nullptr);
    std::tm local;
    localtime_r(&now, &local);
    char clock[16], date[48];
    // Classic's TopBar::refreshClock(): the Time format setting (24-hour "09:05", 12-hour "9:05 PM") and the date line
    // "Monday, 23. November" (English names, no locale). The zone is the process TZ that Preferences::applyTimeZone() sets.
    static const char *const kWeekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *const kMonths[] = {"January", "February", "March", "April", "May", "June", "July", "August",
                                          "September", "October", "November", "December"};
    lastUse24_ = context_.preferences.use24HourClock();
    lastZone_ = context_.preferences.timeZone();
    if (lastUse24_) {
        std::snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
    } else {
        std::strftime(clock, sizeof(clock), "%I:%M %p", &local);
        if (clock[0] == '0') std::memmove(clock, clock + 1, std::strlen(clock));   // "09:05 PM" -> "9:05 PM"
    }
    std::snprintf(date, sizeof(date), "%s, %d. %s", kWeekdays[local.tm_wday % 7], local.tm_mday, kMonths[local.tm_mon % 12]);
    values_.clock = clock;
    values_.date = date;
    values_.disks = diskVolumes();
    values_.version = kDisplayVersion;
    networkState(values_.address, values_.link);
    lastMinute_ = local.tm_min;
}

void PatsGuiScreen::ensureResources(int width, int height) {
    if (canvas_ && canvas_->width() == width && canvas_->height() == height) return;
    registerStrings();
    regular_.reset(new ui::Font());
    bold_.reset(new ui::Font());
    theme_.reset(new ui::Theme(width, height));
    // The settings the user has saved (also from the last run) apply from the first frame; the fonts are the
    // GUI's own files (Space Grotesk when enabled and installed), as in Classic.
    appliedLook_ = classicLook(context_);
    patsgui::applyLook(*theme_, appliedLook_);
    patsgui::applyLookFonts(*regular_, *bold_, appliedLook_);
    canvas_.reset(new ui::Canvas(width, height));
    frame_.reset(new ui::RetainedFrame(width, height, *regular_, *bold_));
    // The real application icon, decoded like the Classic splash does and copied into a framework image.
    ui::ImagePtr logo;
    const char *tempPath = "/tmp/mister-pat-splash-ui.jpg";
    if (FILE *out = std::fopen(tempPath, "wb")) {
        std::fwrite(splash_image::kData, 1, splash_image::kSize, out);
        std::fclose(out);
        if (ImagePtr decoded = Image::load(tempPath)) {
            logo = std::make_shared<ui::Image>(decoded->width(), decoded->height());
            for (int y = 0; y < decoded->height(); ++y)
                std::copy(decoded->row(y), decoded->row(y) + decoded->width(), logo->row(y));
            logo->setOpaque(decoded->opaque());
        }
        unlink(tempPath);
    }
    assets_ = patsgui::makeAssets(*theme_, logo);
    refreshSystemValues();
    dirty_ = true;
}

void PatsGuiScreen::syncLook() {
    if (!theme_ || !regular_ || !bold_) return;   // ensureResources() applies the look when it creates them
    const patsgui::LookSettings look = classicLook(context_);
    if (look == appliedLook_) return;
    if (!look.sameTheme(appliedLook_)) patsgui::applyLook(*theme_, look);
    if (!look.sameFonts(appliedLook_)) patsgui::applyLookFonts(*regular_, *bold_, look);
    appliedLook_ = look;
    if (frame_) frame_->invalidate();   // a new look or font changes what unchanged draw calls paint
    dirty_ = true;
}

void PatsGuiScreen::update(float deltaSeconds) {
    if (input_.start != 0) logInputStats(false);
    // The first-start guide ended (not because the GUI closes): the library it built replaces the sample data the screen was opened
    // with (open() chooses the real library when it is ready), and the user lands on the start tab without a second splash.
    if (firstRunSeen_ && !patsgui::firstRunActive(state_)) {
        firstRunSeen_ = false;
        open(true);
    }
    syncLook();
    // Every Classic notify() (an App-level task finishing, a service the new GUI's pages started) is a toast here:
    // the Classic bottom bar that draws them is not shown while the new GUI owns the screen.
    if (context_.messageSerial != noticeSerial_) {
        noticeSerial_ = context_.messageSerial;
        if (context_.messageTimer > 0.0f && !context_.message.empty()) {
            state_.notice.toast(ui::Text::data(context_.message));
            dirty_ = true;
        }
    }
    // Progress of the X / B holds, for the shell's bottom bar. The App only lets a hold run while the
    // active tab gave a key for it (favoritePath / removablePath above).
    {
        const TabDisplay pref = context_.preferences.tabDisplay();
        const ui::TabDisplay wanted = pref == TabDisplay::Icons ? ui::TabDisplay::Icons
                                      : pref == TabDisplay::Both ? ui::TabDisplay::Both : ui::TabDisplay::Names;
        if (wanted != state_.tabDisplay) {
            state_.tabDisplay = wanted;
            dirty_ = true;
        }
    }
    if (context_.input && !splash_) {
        const float fav = context_.input->favoriteHoldActive() && !patsgui::favoriteKey(state_).empty()
                              ? context_.input->favoriteHoldProgress() : 0.0f;
        const float rem = context_.input->removeHoldActive() && !patsgui::removeKey(state_).empty()
                              ? context_.input->removeHoldProgress() : 0.0f;
        if (fav != state_.favoriteHold || rem != state_.removeHold) {
            state_.favoriteHold = fav;
            state_.removeHold = rem;
            dirty_ = true;
        }
    }
    if (library_) {
        // The Disc tab watches the drive every frame while it is the active tab (the Classic screen polls
        // in its update()); the library only polls and requests artwork for what a tab asked about.
        if (state_.tab == patsgui::Tab::Disc && !splash_) library_->disc();
        library_->update(deltaSeconds);
        if (library_->revision() != drawnRevision_) {
            drawnRevision_ = library_->revision();
            dirty_ = true;
        }
        // A new video frame alone is not a redraw of the screen: renderFull() repaints just the picture.
        if (library_->previewFrameSerial() != drawnSerial_) previewDirty_ = true;
        if (patsgui::update(state_, deltaSeconds)) dirty_ = true;
    }
    if (splash_) {
        splashSeconds_ += deltaSeconds;
        if (splashSeconds_ >= kSplashSeconds) {
            splash_ = false;
            patsgui::playStartSound(state_);   // Classic: after the splash; plays only when this GUI owns the sounds (it does not while hosted by App)
        }
        dirty_ = true;
        return;
    }
    // The clock changes once a minute, at once when the Time format or the time zone changes (Classic: TopBar::update
    // refreshes when the format preference differs); nothing else moves on its own.
    const std::time_t now = std::time(nullptr);
    std::tm local;
    localtime_r(&now, &local);
    if (local.tm_min != lastMinute_ || context_.preferences.use24HourClock() != lastUse24_ ||
        context_.preferences.timeZone() != lastZone_) {
        refreshSystemValues();
        dirty_ = true;
    }
}

void PatsGuiScreen::render(Canvas &, const Rect &, bool) {}

namespace {
double nowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

void PatsGuiScreen::notePresent(double milliseconds) {
    if (input_.start != 0) {
        const double now = nowSeconds();
        ++input_.presents;
        input_.presentMs += milliseconds;
        input_.presentMax = std::max(input_.presentMax, milliseconds);
        for (double stamp : inFlight_) {
            const double latency = (now - stamp) * 1000.0;
            ++input_.latencies;
            input_.latencySum += latency;
            input_.latencyMax = std::max(input_.latencyMax, latency);
        }
    }
    inFlight_.clear();
    if (stats_.start == 0) return;
    ++stats_.presents;
    stats_.presentMs += milliseconds;
}

// One line per ~5 s window that saw key presses: how many redraws they caused, what a frame cost in its three steps, what
// App's present cost, how long a key took from handle() to the presented frame, and what the sound calls cost the frame loop.
void PatsGuiScreen::logInputStats(bool force) {
    if (input_.start == 0) return;
    const double span = nowSeconds() - input_.start;
    if (span < 5.0 && !force) return;
    const UiSounds::Stats sounds = UiSounds::takeStats();
    auto avg = [](double sum, int n) { return n ? sum / n : 0.0; };
    char line[480];
    std::snprintf(line, sizeof(line),
                  "pats-gui input: %d keys, %d redraws (%d whole screen, repainted %.1f %% of the screen on average); "
                  "draw %.2f/%.2f ms avg/max, paint %.2f/%.2f, copy %.2f/%.2f, present %.2f/%.2f (%d); "
                  "key to screen %.1f/%.1f ms avg/max (%d); sounds %d calls %.2f ms in calls (max %.2f), %d dropped; window %.1f s",
                  input_.keys, input_.frames, input_.fullFrames,
                  input_.frames ? 100.0 * input_.dirtyPixels / input_.frames / (double(canvas_ ? canvas_->width() : 1) * (canvas_ ? canvas_->height() : 1)) : 0.0,
                  avg(input_.recordMs, input_.frames), input_.recordMax, avg(input_.paintMs, input_.frames), input_.paintMax,
                  avg(input_.copyMs, input_.frames), input_.copyMax, avg(input_.presentMs, input_.presents), input_.presentMax,
                  input_.presents, avg(input_.latencySum, input_.latencies), input_.latencyMax, input_.latencies,
                  sounds.calls, sounds.totalMs, sounds.maxMs, sounds.dropped, span);
    DebugLog::info(line);
    input_ = InputStats();
}

// One line every ~5 s while a video plays: the frames shown per second and what a frame costs.
void PatsGuiScreen::logPreviewStats() {
    const double now = nowSeconds();
    if (stats_.start == 0) { stats_.start = now; return; }
    const double span = now - stats_.start;
    if (span < 5.0) return;
    if (stats_.partial + stats_.full > 0) {
        char line[256];
        std::snprintf(line, sizeof(line),
                      "pats-gui video: %.1f frames/s shown; partial redraw+copy %.2f ms avg (%d), present %.2f ms avg (%d); "
                      "full redraws %d (%.1f ms avg); window %.1f s",
                      (stats_.partial + stats_.full) / span, stats_.partial ? stats_.partialMs / stats_.partial : 0.0,
                      stats_.partial, stats_.presents ? stats_.presentMs / stats_.presents : 0.0, stats_.presents,
                      stats_.full, stats_.full ? stats_.fullMs / stats_.full : 0.0, span);
        DebugLog::info(line);
    }
    stats_ = PreviewStats();
    stats_.start = now;
}

const std::vector<ui::Rect> &PatsGuiScreen::paintFrame(bool everything) {
    if (everything) frame_->invalidate();
    const double t0 = nowSeconds();
    frame_->begin();
    if (splash_) {
        ui::drawSplash(*frame_, *theme_, assets_.logo.get(), splashSeconds_ / kSplashSeconds, std::string());
    } else {
        const ui::ShellSpec shell = patsgui::shellFor(state_, values_, assets_);
        const ui::Rect content = ui::drawShell(*frame_, *theme_, shell);
        patsgui::drawTab(*frame_, *theme_, state_, content);
        patsgui::drawOverlays(*frame_, *theme_, state_, shell);
    }
    const double t1 = nowSeconds();
    const std::vector<ui::Rect> &damage = frame_->commit(*canvas_);
    const double t2 = nowSeconds();
    if (input_.start != 0) {
        ++input_.frames;
        if (frame_->stats().full) ++input_.fullFrames;
        input_.dirtyPixels += double(frame_->stats().dirtyPixels);
        input_.recordMs += (t1 - t0) * 1000.0;
        input_.recordMax = std::max(input_.recordMax, (t1 - t0) * 1000.0);
        input_.paintMs += (t2 - t1) * 1000.0;
        input_.paintMax = std::max(input_.paintMax, (t2 - t1) * 1000.0);
    }
    return damage;
}

void PatsGuiScreen::renderFull(Canvas &canvas, bool fullRedraw) {
    ensureResources(canvas.width(), canvas.height());
    if (!dirty_ && !fullRedraw && !previewDirty_) return;

    ui::SoftPainter painter(*canvas_, *regular_, *bold_);
    const double begin = nowSeconds();
    // Only the video moved: repaint its picture on the last full frame and copy just that rectangle.
    if (!dirty_ && !fullRedraw && !splash_ && library_) {
        ui::Rect damage;
        if (patsgui::redrawPreview(state_, painter, *theme_, damage)) {
            canvas.blitRect(canvas_->row(0), canvas_->width(), Rect{damage.x, damage.y, damage.w, damage.h});
            drawnSerial_ = library_->previewFrameSerial();
            previewDirty_ = false;
            ++stats_.partial;
            stats_.partialMs += (nowSeconds() - begin) * 1000.0;
            logPreviewStats();
            return;
        }
    }
    // Everything else: the display list is rebuilt and only the tiles whose draw calls changed are repainted and copied.
    std::vector<ui::Rect> changed = paintFrame(fullRedraw);
    // The video picture is drawn outside the display list (it changes without the page changing): when a new frame is
    // waiting, paint it on top of the repainted tiles; if that is not possible (a toast over it), repaint everything.
    if (previewDirty_ && !splash_ && library_) {
        ui::Rect video;
        if (patsgui::redrawPreview(state_, painter, *theme_, video)) changed.push_back(video);
        else changed = paintFrame(true);
    }
    const double copyBegin = nowSeconds();
    if (fullRedraw) canvas.blit(canvas_->row(0), canvas_->width(), canvas_->height(), 0, 0);
    else
        for (const ui::Rect &r : changed) canvas.blitRect(canvas_->row(0), canvas_->width(), Rect{r.x, r.y, r.w, r.h});
    if (input_.start != 0) {
        const double copyMs = (nowSeconds() - copyBegin) * 1000.0;
        input_.copyMs += copyMs;
        input_.copyMax = std::max(input_.copyMax, copyMs);
        inFlight_.insert(inFlight_.end(), keyStamps_.begin(), keyStamps_.end());
    }
    keyStamps_.clear();
    dirty_ = false;
    if (library_) {
        drawnSerial_ = library_->previewFrameSerial();
        if (previewDirty_ && library_->preview().playing) {   // a frame shown for a video frame
            ++stats_.full;
            stats_.fullMs += (nowSeconds() - begin) * 1000.0;
        }
        previewDirty_ = false;
        if (library_->preview().playing) logPreviewStats();
        else stats_ = PreviewStats();
    }
}

void PatsGuiScreen::handle(Action action) {
    if (splash_) return;   // the splash is not interactive; Back works once the shell is up
    {   // latency probe: stamped here, closed in notePresent() when the frame that shows the result is on the framebuffer
        if (input_.start == 0) input_.start = nowSeconds();
        ++input_.keys;
        if (keyStamps_.size() < 16) keyStamps_.push_back(nowSeconds());
    }
    patsgui::Event event;
    switch (action) {
        case Action::TabPrev: event = patsgui::Event::TabPrev; break;
        case Action::TabNext: event = patsgui::Event::TabNext; break;
        case Action::Up:      event = patsgui::Event::Up; break;
        case Action::Down:    event = patsgui::Event::Down; break;
        case Action::Left:    event = patsgui::Event::Left; break;
        case Action::Right:   event = patsgui::Event::Right; break;
        case Action::Confirm: event = patsgui::Event::Confirm; break;
        case Action::Back:    event = patsgui::Event::Back; break;
        case Action::ToggleFavorite: event = patsgui::Event::XHold; break;
        case Action::RemoveRecent: event = patsgui::Event::BHold; break;
        case Action::JumpPrev: event = patsgui::Event::LetterPrev; break;
        case Action::JumpNext: event = patsgui::Event::LetterNext; break;
        case Action::CycleView: event = patsgui::Event::Y; break;
        case Action::FaceXPress:
            // The press arrives the moment X goes down. On a game it only starts the 2 s hold (XHold follows),
            // so it is not a short action there; elsewhere (keyboard, nothing focused) it is a plain X.
            if (!patsgui::favoriteKey(state_).empty()) return;
            event = patsgui::Event::X;
            break;
        case Action::Start:   event = patsgui::Event::Start; break;
        case Action::Select:  event = patsgui::Event::Select; break;
        default: return;
    }
    if (patsgui::handle(state_, event)) {
        if (onClose_) onClose_();
        return;
    }
    dirty_ = true;
}
