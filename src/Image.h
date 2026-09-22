#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// An ARGB32 bitmap. Loading is separate from drawing so decoding can move off the render path.
class Image {
public:
    Image() = default;
    Image(int w, int h) : width_(w), height_(h), pixels_(size_t(w) * h, 0) {}

    // Decides by content, not by file name. Artwork on a MiSTer is routinely a JPEG called
    // ".png" — Console Mode's own artwork optimiser writes exactly that — and trusting the
    // extension means a silently blank tile.
    static std::shared_ptr<Image> load(const std::string &path);

    static std::shared_ptr<Image> loadPng(const std::string &path);
    static std::shared_ptr<Image> loadJpeg(const std::string &path);

    // Writes the picture as JPEG. Used by the scraper, which shrinks and re-encodes artwork
    // before it ever reaches the drive — a 512-pixel cover is a tenth the size this way, and
    // that is the difference between writing a gigabyte to a marginal drive and writing two
    // hundred megabytes.
    bool saveJpeg(const std::string &path, int quality = 85) const;

    bool valid() const { return width_ > 0 && height_ > 0; }
    int width() const { return width_; }
    int height() const { return height_; }

    const uint32_t *row(int y) const { return &pixels_[size_t(y) * width_]; }
    uint32_t *row(int y) { return &pixels_[size_t(y) * width_]; }

    uint32_t at(int x, int y) const { return pixels_[size_t(y) * width_ + x]; }

    // Area-averaged when shrinking, bilinear when enlarging.
    std::shared_ptr<Image> scaledTo(int w, int h) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint32_t> pixels_;
};

using ImagePtr = std::shared_ptr<Image>;
