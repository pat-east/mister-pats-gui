#include "ImageCache.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <time.h>
#include <utility>
#include <unistd.h>

namespace {

// How long a miss sticks before it is worth trying again. Long enough that scrolling past a
// game with genuinely no artwork does not retry it every frame; short enough that a drive
// still settling right after boot recovers within a few seconds rather than for the rest of
// the session. ~3 s at the app's ~30 fps frame pace.
constexpr uint64_t kFailureRetryTicks = 90;
constexpr size_t kMaxQueuedArtwork = 32;

int64_t monotonicMs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return int64_t(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

bool isBmp(const std::string &path) {
    return path.size() >= 4 && path.compare(path.size() - 4, 4, ".bmp") == 0;
}

} // namespace

ImageCache::ImageCache(size_t maxEntries, size_t maxBytes)
    : maxEntries_(maxEntries), maxBytes_(maxBytes) {
    semaphoreReady_ = sem_init(&workReady_, 0, 0) == 0;
    if (!semaphoreReady_) workerUnavailable_ = true;
}

ImageCache::~ImageCache() {
    {
        std::lock_guard<std::mutex> lock(workMutex_);
        stopping_ = true;
        workQueue_.clear();
    }
    if (semaphoreReady_) sem_post(&workReady_);
    if (worker_.joinable()) worker_.join();
    if (semaphoreReady_) sem_destroy(&workReady_);
}

void ImageCache::beginFrame(int budgetMs) {
    ++tick_;
    budgetMs_ = budgetMs;
    decodedMs_ = 0;

    std::vector<Completed> ready;
    {
        std::lock_guard<std::mutex> lock(workMutex_);
        ready.swap(completed_);
        for (const Completed &item : ready) pending_.erase(item.key);

        // Keep requests from the previous frame only if the worker already picked them up.
        // A tab switch or letter jump can otherwise leave a trail of obsolete prefetches in
        // front of the newly visible artwork. In-flight file I/O remains owned by the worker
        // and is never cancelled underneath the decoder.
        for (const Work &work : workQueue_) pending_.erase(work.key);
        workQueue_.clear();
    }
    for (Completed &item : ready) {
        Entry entry;
        entry.lastUsed = tick_;
        entry.failedAtTick = tick_;
        if (item.image && item.image->valid()) {
            entry.image = std::move(item.image);
            entry.retryPreferred = !item.primaryLoaded;
        } else {
            entry.failed = true;
        }
        const bool loaded = bool(entry.image);
        try {
            if (storeEntry(item.key, std::move(entry)) && loaded) ++generation_;
        } catch (...) {
            // Under memory pressure, discard this completed image instead of letting an
            // allocation exception escape into the frame loop.
        }
    }
    evictIfNeeded();
}

void ImageCache::evictIfNeeded() {
    while (entries_.size() > maxEntries_ || cachedBytes_ > maxBytes_) {
        auto oldest = entries_.begin();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
            if (it->second.lastUsed < oldest->second.lastUsed) oldest = it;
        cachedBytes_ -= oldest->second.bytes;
        entries_.erase(oldest);
    }
}

bool ImageCache::storeEntry(const std::string &key, Entry entry) {
    if (entry.image)
        entry.bytes = size_t(entry.image->width()) * size_t(entry.image->height()) *
                      sizeof(uint32_t);

    auto current = entries_.find(key);
    if (current != entries_.end()) {
        cachedBytes_ -= current->second.bytes;
        current->second = std::move(entry);
        cachedBytes_ += current->second.bytes;
    } else {
        auto inserted = entries_.emplace(key, std::move(entry));
        if (!inserted.second) return false;
        cachedBytes_ += inserted.first->second.bytes;
    }
    evictIfNeeded();
    const auto stored = entries_.find(key);
    return stored != entries_.end() && bool(stored->second.image);
}

ImagePtr ImageCache::get(const std::string &path, int width, int height, bool resize) {
    if (path.empty() || width <= 0 || height <= 0) return nullptr;

    try {
    const std::string key = resize
                                ? path + "@" + std::to_string(width) + "x" + std::to_string(height)
                                : path + "@native";

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
    const int64_t startedAt = monotonicMs();

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

        entry.image = resize ? source->scaledTo(targetW, targetH) : source;
        if (!entry.image) { entry.failed = true; entry.failedAtTick = tick_; }
    }

    decodedMs_ += monotonicMs() - startedAt;

    const bool failed = entry.failed;
    ImagePtr result = entry.image;
    if (!failed) ++generation_;
    storeEntry(key, std::move(entry));  // replaces an expired miss and enforces the RAM budget

    return failed ? nullptr : result;
    } catch (...) {
        // Cache bookkeeping or a resize can fail under memory pressure. Treat that like an
        // artwork miss so a cover can never take down the render loop or splash screen.
        return nullptr;
    }
}

ImagePtr ImageCache::get(const std::string &path, const std::string &fallback, int width,
                         int height) {
    if (ImagePtr image = get(path, width, height, !isBmp(path))) return image;
    if (fallback.empty()) return nullptr;
    return get(fallback, width, height, !isBmp(fallback));
}

ImagePtr ImageCache::requestAsync(const std::string &path, const std::string &fallback,
                                  int width, int height, bool priority) {
    std::vector<std::string> candidates;
    if (!path.empty()) candidates.push_back(path);
    if (!fallback.empty() && fallback != path) candidates.push_back(fallback);
    return requestAsyncCandidates(candidates, width, height, priority);
}

ImagePtr ImageCache::requestAsyncCandidates(const std::vector<std::string> &candidates,
                                           int width, int height, bool priority) {
    try {
    if (candidates.empty() || width <= 0 || height <= 0) return nullptr;
    std::string key;
    for (const std::string &candidate : candidates) {
        key += std::to_string(candidate.size());
        key += ':';
        key += candidate;
        key += '\x1e';
    }
    if (isBmp(candidates.front())) key += "@native";
    else key += "@" + std::to_string(width) + "x" + std::to_string(height);
    auto it = entries_.find(key);
    ImagePtr cachedFallback;
    if (it != entries_.end()) {
        const bool shouldRetry =
            (it->second.failed || it->second.retryPreferred) &&
            tick_ - it->second.failedAtTick >= kFailureRetryTicks;
        if (!shouldRetry) {
            it->second.lastUsed = tick_;
            return it->second.failed ? nullptr : it->second.image;
        }
        if (it->second.retryPreferred) {
            cachedFallback = it->second.image;
            it->second.lastUsed = tick_;
        }
    }

    // Do not make thread creation part of application startup. If the system cannot create a
    // worker, preserve the old loading path rather than leaving covers permanently blank.
    if (workerUnavailable_ || workerFailed_.load() || !semaphoreReady_) {
        workerUnavailable_ = true;
        for (const std::string &candidate : candidates) {
            if (ImagePtr image = get(candidate, width, height, !isBmp(candidate))) return image;
        }
        return nullptr;
    }
    if (!workerStarted_) {
        try {
            worker_ = std::thread([this]() noexcept {
                try {
                    workerLoop();
                } catch (...) {
                    // A worker infrastructure exception must never escape the thread entry
                    // point. The render thread notices and falls back to synchronous loading.
                    workerFailed_.store(true);
                }
            });
            workerStarted_ = true;
        } catch (...) {
            workerUnavailable_ = true;
            for (const std::string &candidate : candidates) {
                if (ImagePtr image = get(candidate, width, height, !isBmp(candidate))) return image;
            }
            return nullptr;
        }
    }

    Work work{key, candidates, width, height};
    {
        std::lock_guard<std::mutex> lock(workMutex_);
        if (pending_.insert(key).second) {
            if (workQueue_.size() >= kMaxQueuedArtwork) {
                pending_.erase(workQueue_.back().key);
                workQueue_.pop_back();
            }
            try {
                if (priority) workQueue_.push_front(std::move(work));
                else workQueue_.push_back(std::move(work));
            } catch (...) {
                pending_.erase(key);
                throw;
            }
            if (sem_post(&workReady_) != 0) {
                pending_.erase(key);
                for (auto it = workQueue_.begin(); it != workQueue_.end(); ++it) {
                    if (it->key == key) {
                        workQueue_.erase(it);
                        break;
                    }
                }
            }
        }
    }
    return cachedFallback;
    } catch (...) {
        // Requests are opportunistic. If queuing runs out of memory, leave this frame with a
        // placeholder and let a later frame retry instead of unwinding through the GUI loop.
        return nullptr;
    }
}

void ImageCache::workerLoop() {
    for (;;) {
        while (sem_wait(&workReady_) != 0) {
            if (errno == EINTR) continue;
            workerFailed_.store(true);
            return;
        }

        Work work;
        {
            std::lock_guard<std::mutex> lock(workMutex_);
            if (stopping_) return;
            if (workQueue_.empty()) continue;
            work = std::move(workQueue_.front());
            workQueue_.pop_front();
        }

        auto loadAndFit = [&](const std::string &candidate) -> ImagePtr {
            if (candidate.empty()) return nullptr;
            ImagePtr source = Image::load(candidate);
            if (!source || !source->valid()) {
                // Match the synchronous cache's recovery for drives that briefly fail an early read.
                usleep(5000);
                source = Image::load(candidate);
            }
            if (!source || !source->valid()) return nullptr;
            if (isBmp(candidate)) {
                // Prepared BMP variants must fit their view bounds. Reject a wrong or hostile
                // file here instead of retaining an unexpectedly large bitmap in the cache.
                if (source->width() > work.width || source->height() > work.height)
                    return nullptr;
                return source;
            }

            int targetW = work.width;
            int targetH = work.height;
            if (long(source->width()) * work.height > long(source->height()) * work.width)
                targetH = std::max(1, int(long(source->height()) * work.width / source->width()));
            else
                targetW = std::max(1, int(long(source->width()) * work.height / source->height()));
            return source->scaledTo(targetW, targetH);
        };

        ImagePtr image;
        bool primaryLoaded = false;
        try {
            for (size_t i = 0; i < work.candidates.size() && !image; ++i) {
                image = loadAndFit(work.candidates[i]);
                if (i == 0) primaryLoaded = bool(image);
            }
        } catch (...) {
            // Allocation and decoder exceptions must not escape a std::thread entry point:
            // an uncaught exception there calls std::terminate and exits the whole GUI.
            image.reset();
        }

        {
            std::lock_guard<std::mutex> lock(workMutex_);
            try {
                completed_.push_back({work.key, std::move(image), primaryLoaded});
            } catch (...) {
                // If even publishing the result runs out of memory, forget the request so a
                // later visible frame can try again. Never let allocation failure kill the app.
                pending_.erase(work.key);
            }
        }
    }
}
