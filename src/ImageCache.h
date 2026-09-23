#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "Image.h"

// Keeps scaled artwork around and limits how much decoding happens per frame, so scrolling
// stays responsive on a library with thousands of entries.
class ImageCache {
public:
    explicit ImageCache(size_t maxEntries = 120) : maxEntries_(maxEntries) {}

    // Call once per frame. The budget limits the time spent *decoding* in this frame —
    // not the time since the frame began, which everything else would use up first.
    // One boxart costs roughly 47 ms here, so in practice this allows one per frame.
    void beginFrame(int budgetMs = 8);

    // Returns the picture scaled to *fit inside* the given box, keeping its own
    // proportions — so the result is usually smaller than the box in one dimension.
    // Scaling to the box exactly would distort the artwork, and callers could not
    // recover the original proportions afterwards.
    // Returns nullptr while a decode is still pending or if the file cannot be read.
    ImagePtr get(const std::string &path, int width, int height);

    // Tries a second location when the first holds nothing. A miss is cached for a while, so
    // scrolling past the same missing artwork repeatedly costs one failed open rather than
    // one per frame — but it is retried after that, rather than blacklisted forever. A drive
    // that is still settling right after boot can turn a real picture into a miss for the
    // first attempt or two, and that must not become permanent.
    ImagePtr get(const std::string &path, const std::string &fallback, int width, int height);

    size_t size() const { return entries_.size(); }

    // Counts successful decodes. Screens that redraw incrementally watch this:
    // artwork arrives after the tile was last painted, so without it a resting
    // tile would never show its picture.
    uint64_t generation() const { return generation_; }

private:
    struct Entry {
        ImagePtr image;
        bool failed = false;
        uint64_t lastUsed = 0;
        uint64_t failedAtTick = 0;
    };

    void evictIfNeeded();

    std::unordered_map<std::string, Entry> entries_;
    size_t maxEntries_;
    uint64_t tick_ = 0;
    uint64_t generation_ = 0;
    int budgetMs_ = 0;
    int64_t decodedMs_ = 0;
};
