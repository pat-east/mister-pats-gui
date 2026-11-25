#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace ui {

// A pixel buffer in ARGB32. The framework only needs buffers; decoding files is the application's
// concern (it owns the image cache). A renderer that uploads textures keys them by `id()`.
class Image {
public:
    Image() = default;
    Image(int w, int h) : width_(w), height_(h), pixels_(size_t(w) * h, 0), id_(nextId()) {}

    bool valid() const { return width_ > 0 && height_ > 0; }
    int width() const { return width_; }
    int height() const { return height_; }

    // True when every pixel is fully opaque (lets a renderer copy instead of blend).
    bool opaque() const { return opaque_; }
    void setOpaque(bool opaque) { opaque_ = opaque; }

    // Stable identity for renderer-side caches (textures); never reused during a run.
    uint64_t id() const { return id_; }

    const uint32_t *row(int y) const { return &pixels_[size_t(y) * width_]; }
    uint32_t *row(int y) { return &pixels_[size_t(y) * width_]; }
    uint32_t at(int x, int y) const { return pixels_[size_t(y) * width_ + x]; }

private:
    static uint64_t nextId() {
        static uint64_t counter = 0;
        return ++counter;
    }

    int width_ = 0;
    int height_ = 0;
    bool opaque_ = false;
    std::vector<uint32_t> pixels_;
    uint64_t id_ = 0;
};

using ImagePtr = std::shared_ptr<Image>;

}  // namespace ui
