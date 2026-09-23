#include "ImageCache.h"

#include <algorithm>
#include <cstdio>
#include <unistd.h>

#include "Input.h"   // nowMs

namespace {

// How long a miss sticks before it is worth trying again. Long enough that scrolling past a
// game with genuinely no artwork does not retry it every frame; short enough that a drive
// still settling right after boot recovers within a few seconds rather than for the rest of
// the session. ~3 s at the app's ~30 fps frame pace.
constexpr uint64_t kFailureRetryTicks = 90;

} // namespace

void ImageCache::beginFrame(int budgetMs) {
    ++tick_;
    budgetMs_ = budgetMs;
    decodedMs_ = 0;
}

void ImageCache::evictIfNeeded() {
    while (entries_.size() > maxEntries_) {
        auto oldest = entries_.begin();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
            if (it->second.lastUsed < oldest->second.lastUsed) oldest = it;
        entries_.erase(oldest);
    }
}

ImagePtr ImageCache::get(const std::string &path, int width, int height) {
    if (path.empty() || width <= 0 || height <= 0) return nullptr;

    const std::string key = path + "@" + std::to_string(width) + "x" + std::to_string(height);

    auto it = entries_.find(key);
    if (it != entries_.end()) {
        const bool staleFailure =
            it->second.failed && tick_ - it->second.failedAtTick >= kFailureRetryTicks;
        if (!staleFailure) {
            it->second.lastUsed = tick_;
            return it->second.failed ? nullptr : it->second.image;
        }
    }

    // Decode only while this frame has not spent its decoding allowance yet.
    if (decodedMs_ >= budgetMs_) return nullptr;
    const int64_t startedAt = nowMs();

    Entry entry;
    entry.lastUsed = tick_;

    ImagePtr source = Image::load(path);

    if (!source || !source->valid()) {
        // A drive touched for the first time this session can lose an early read even though
        // the file is genuinely there — a debug build that happened to add a few ms of
        // incidental delay around this exact call made the failure stop reproducing. One
        // short, deliberate pause and a retry turns that into nothing anyone sees, rather
        // than depending on logging (or anything else) to accidentally provide the delay.
        usleep(5000);
        source = Image::load(path);
    }

    if (!source || !source->valid()) {
        entry.failed = true;
        entry.failedAtTick = tick_;
    } else {
        // Fit inside the box without distorting: whichever side is relatively longer
        // determines the scale.
        int targetW = width;
        int targetH = height;
        if (long(source->width()) * height > long(source->height()) * width)
            targetH = std::max(1, int(long(source->height()) * width / source->width()));
        else
            targetW = std::max(1, int(long(source->width()) * height / source->height()));

        entry.image = source->scaledTo(targetW, targetH);
        if (!entry.image) { entry.failed = true; entry.failedAtTick = tick_; }
    }

    decodedMs_ += nowMs() - startedAt;

    const bool failed = entry.failed;
    ImagePtr result = entry.image;
    if (!failed) ++generation_;
    entries_[key] = std::move(entry);   // assignment, not emplace: a retry replaces the stale miss
    evictIfNeeded();

    return failed ? nullptr : result;
}

ImagePtr ImageCache::get(const std::string &path, const std::string &fallback, int width,
                         int height) {
    if (ImagePtr image = get(path, width, height)) return image;
    if (fallback.empty()) return nullptr;
    return get(fallback, width, height);
}
