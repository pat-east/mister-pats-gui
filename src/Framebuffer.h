#pragma once

#include <cstdint>
#include <linux/fb.h>
#include <string>
#include <vector>

#include "Geometry.h"

class Canvas;

// Owns the mapped framebuffer and pushes a finished canvas to it in one pass.
class Framebuffer {
public:
    ~Framebuffer();

    bool open(const std::string &device = "/dev/fb0");
    void close();

    int width() const { return int(var_.xres); }
    int height() const { return int(var_.yres); }
    int bitsPerPixel() const { return int(var_.bits_per_pixel); }
    std::string describe() const;

    void present(const Canvas &canvas);

    // Pushes only the listed regions. A full 1080p frame costs ~14 ms of pure
    // memory traffic, so transferring just what changed is the largest single win.
    void present(const Canvas &canvas, const std::vector<Rect> &regions);

private:
    int fd_ = -1;
    uint8_t *memory_ = nullptr;
    size_t size_ = 0;
    bool directCopy_ = false;
    fb_var_screeninfo var_{};
    fb_fix_screeninfo fix_{};
};
