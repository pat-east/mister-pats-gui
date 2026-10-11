#include "Framebuffer.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "Canvas.h"

Framebuffer::~Framebuffer() { close(); }

bool Framebuffer::open(const std::string &device) {
    fd_ = ::open(device.c_str(), O_RDWR);
    if (fd_ < 0) { perror(device.c_str()); return false; }

    if (ioctl(fd_, FBIOGET_VSCREENINFO, &var_) < 0) { perror("FBIOGET_VSCREENINFO"); return false; }
    if (ioctl(fd_, FBIOGET_FSCREENINFO, &fix_) < 0) { perror("FBIOGET_FSCREENINFO"); return false; }

    size_ = size_t(fix_.line_length) * var_.yres;
    memory_ = static_cast<uint8_t *>(
        mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    if (memory_ == MAP_FAILED) { perror("mmap framebuffer"); memory_ = nullptr; return false; }

    // Our canvas is ARGB32; when the framebuffer agrees, presenting is a straight copy.
    directCopy_ = var_.bits_per_pixel == 32 && var_.red.offset == 16 &&
                  var_.green.offset == 8 && var_.blue.offset == 0 &&
                  var_.red.length == 8 && var_.green.length == 8 && var_.blue.length == 8;

    return true;
}

void Framebuffer::close() {
    if (memory_) { munmap(memory_, size_); memory_ = nullptr; }
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

std::string Framebuffer::describe() const {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%ux%u @ %u bpp, stride %u, id=%s, direct=%d",
                  var_.xres, var_.yres, var_.bits_per_pixel, fix_.line_length,
                  fix_.id, directCopy_ ? 1 : 0);
    return buf;
}

void Framebuffer::present(const Canvas &canvas, const std::vector<Rect> &regions) {
    if (!memory_) return;

    const Rect screen{0, 0, width(), height()};
    for (const Rect &region : regions) {
        const Rect r = region.intersect(screen).intersect(canvas.bounds());
        if (r.empty()) continue;

        for (int y = r.y; y < r.bottom(); ++y) {
            const uint32_t *src = canvas.row(y) + r.x;
            uint8_t *dst = memory_ + size_t(y) * fix_.line_length + size_t(r.x) * (var_.bits_per_pixel / 8);

            if (directCopy_) {
                std::memcpy(dst, src, size_t(r.w) * 4);
                continue;
            }

            if (var_.bits_per_pixel == 32) {
                uint32_t *out = reinterpret_cast<uint32_t *>(dst);
                for (int i = 0; i < r.w; ++i) {
                    const uint32_t p = src[i];
                    out[i] = (((p >> 16) & 0xFF) >> (8 - var_.red.length)) << var_.red.offset |
                             (((p >> 8) & 0xFF) >> (8 - var_.green.length)) << var_.green.offset |
                             ((p & 0xFF) >> (8 - var_.blue.length)) << var_.blue.offset;
                }
            } else if (var_.bits_per_pixel == 16) {
                uint16_t *out = reinterpret_cast<uint16_t *>(dst);
                for (int i = 0; i < r.w; ++i) {
                    const uint32_t p = src[i];
                    out[i] = uint16_t(
                        (((p >> 16) & 0xFF) >> (8 - var_.red.length)) << var_.red.offset |
                        (((p >> 8) & 0xFF) >> (8 - var_.green.length)) << var_.green.offset |
                        ((p & 0xFF) >> (8 - var_.blue.length)) << var_.blue.offset);
                }
            }
        }
    }
}

void Framebuffer::present(const Canvas &canvas) {
    if (!memory_) return;

    const int rows = std::min(height(), canvas.height());
    const int cols = std::min(width(), canvas.width());

    for (int y = 0; y < rows; ++y) {
        const uint32_t *src = canvas.row(y);
        uint8_t *dst = memory_ + size_t(y) * fix_.line_length;

        if (directCopy_) {
            std::memcpy(dst, src, size_t(cols) * 4);
            continue;
        }

        if (var_.bits_per_pixel == 32) {
            uint32_t *out = reinterpret_cast<uint32_t *>(dst);
            for (int x = 0; x < cols; ++x) {
                const uint32_t p = src[x];
                out[x] = (((p >> 16) & 0xFF) >> (8 - var_.red.length)) << var_.red.offset |
                         (((p >> 8) & 0xFF) >> (8 - var_.green.length)) << var_.green.offset |
                         ((p & 0xFF) >> (8 - var_.blue.length)) << var_.blue.offset;
            }
        } else if (var_.bits_per_pixel == 16) {
            uint16_t *out = reinterpret_cast<uint16_t *>(dst);
            for (int x = 0; x < cols; ++x) {
                const uint32_t p = src[x];
                out[x] = uint16_t(
                    (((p >> 16) & 0xFF) >> (8 - var_.red.length)) << var_.red.offset |
                    (((p >> 8) & 0xFF) >> (8 - var_.green.length)) << var_.green.offset |
                    ((p & 0xFF) >> (8 - var_.blue.length)) << var_.blue.offset);
            }
        }
    }
}
