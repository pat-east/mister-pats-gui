#pragma once

#include <cstdint>
#include <atomic>
#include <deque>
#include <mutex>
#include <semaphore.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "Image.h"

// Keeps scaled artwork around within both an entry and byte limit, while decoding misses off
// the render path so scrolling stays responsive on a library with thousands of entries.
class ImageCache {
public:
    explicit ImageCache(size_t maxEntries = 120,
                        size_t maxBytes = 32u * 1024u * 1024u);
    ~ImageCache();

    ImageCache(const ImageCache &) = delete;
    ImageCache &operator=(const ImageCache &) = delete;

    // Call once per frame. The budget limits the time spent *decoding* in this frame —
    // not the time since the frame began, which everything else would use up first.
    // One boxart costs roughly 47 ms here, so in practice this allows one per frame.
    void beginFrame(int budgetMs = 8);

    // Returns the picture scaled to fit inside the given box, keeping its proportions.
    // Set resize=false when the source is already authored at its display size; width and
    // height still validate the request, but the decoded pixels are cached unchanged.
    // Returns nullptr while a decode is still pending or if the file cannot be read.
    ImagePtr get(const std::string &path, int width, int height, bool resize = true);

    // Tries a second location when the first holds nothing. A miss is cached for a while, so
    // scrolling past the same missing artwork repeatedly costs one failed open rather than
    // one per frame — but it is retried after that, rather than blacklisted forever. A drive
    // that is still settling right after boot can turn a real picture into a miss for the
    // first attempt or two, and that must not become permanent.
    ImagePtr get(const std::string &path, const std::string &fallback, int width, int height);

    // Queues a cover decode/resize on the image worker. Completed images are adopted by the
    // render thread at beginFrame(), so entries_ and generation_ remain single-threaded.
    ImagePtr requestAsync(const std::string &path, const std::string &fallback, int width,
                          int height, bool priority = false);

    size_t size() const { return entries_.size(); }
    size_t memoryBytes() const { return cachedBytes_; }

    // Counts successful decodes. Screens that redraw incrementally watch this:
    // artwork arrives after the tile was last painted, so without it a resting
    // tile would never show its picture.
    uint64_t generation() const { return generation_; }

private:
    struct Entry {
        ImagePtr image;
        bool failed = false;
        bool retryPreferred = false;
        uint64_t lastUsed = 0;
        uint64_t failedAtTick = 0;
        size_t bytes = 0;
    };

    void evictIfNeeded();
    bool storeEntry(const std::string &key, Entry entry);
    void workerLoop();

    struct Work {
        std::string key;
        std::string path;
        std::string fallback;
        int width;
        int height;
    };
    struct Completed {
        std::string key;
        ImagePtr image;
        bool primaryLoaded;
    };

    std::unordered_map<std::string, Entry> entries_;
    size_t maxEntries_;
    size_t maxBytes_;
    size_t cachedBytes_ = 0;
    uint64_t tick_ = 0;
    uint64_t generation_ = 0;
    int budgetMs_ = 0;
    int64_t decodedMs_ = 0;

    std::mutex workMutex_;
    sem_t workReady_{};
    bool semaphoreReady_ = false;
    std::deque<Work> workQueue_;
    std::vector<Completed> completed_;
    std::unordered_set<std::string> pending_;
    bool stopping_ = false;
    bool workerStarted_ = false;
    bool workerUnavailable_ = false;
    std::atomic<bool> workerFailed_{false};
    std::thread worker_;
};
