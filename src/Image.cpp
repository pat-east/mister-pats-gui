#include "Image.h"

#include <cstdio>
#include <cstring>
#include <csetjmp>
#include <new>
#include <png.h>

extern "C" {
#include <jpeglib.h>
}

namespace {

// The UI only needs small covers and 1080p backgrounds. Reject implausibly large decoded
// images before allocating pixel buffers, including files that claim enormous dimensions.
constexpr uint64_t kMaxDecodedPixels = 4u * 1024u * 1024u;

bool dimensionsAreSafe(uint32_t width, uint32_t height) {
    return width > 0 && height > 0 && width <= 8192 && height <= 8192 &&
           uint64_t(width) * height <= kMaxDecodedPixels;
}

// The first bytes of the file, which is the only reliable statement about its format.
enum class Format { Unknown, Png, Jpeg, Bmp };

Format sniff(const std::string &path) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return Format::Unknown;

    unsigned char header[8] = {0};
    const size_t got = std::fread(header, 1, sizeof(header), file);
    std::fclose(file);
    if (got < 3) return Format::Unknown;

    static const unsigned char kPng[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (got >= 8 && std::memcmp(header, kPng, 8) == 0) return Format::Png;
    if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) return Format::Jpeg;
    if (header[0] == 'B' && header[1] == 'M') return Format::Bmp;
    return Format::Unknown;
}

uint16_t read16(const unsigned char *p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

uint32_t read32(const unsigned char *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
           (uint32_t(p[3]) << 24);
}

uint8_t maskedChannel(uint32_t pixel, uint32_t mask, uint8_t fallback) {
    if (!mask) return fallback;
    unsigned shift = 0;
    while (((mask >> shift) & 1u) == 0u) ++shift;
    const uint32_t maximum = mask >> shift;
    const uint32_t value = (pixel & mask) >> shift;
    return uint8_t((uint64_t(value) * 255u + maximum / 2u) / maximum);
}

// libjpeg's default error handler calls exit(). For a frontend that would mean one corrupt
// box art takes the whole interface down, so it longjmps back to the caller instead.
struct JpegError {
    jpeg_error_mgr base;
    jmp_buf escape;
};

void jpegFail(j_common_ptr info) {
    longjmp(reinterpret_cast<JpegError *>(info->err)->escape, 1);
}

} // namespace

ImagePtr Image::load(const std::string &path) {
    try {
        switch (sniff(path)) {
        case Format::Png:  return loadPng(path);
        case Format::Jpeg: return loadJpeg(path);
        case Format::Bmp:  return loadBmp(path);
        default:           return nullptr;
        }
    } catch (...) {
        // Decoding untrusted or interrupted artwork must remain a per-image failure, including
        // allocation failures in the BMP reader, rather than escaping into the GUI loop.
        return nullptr;
    }
}

std::shared_ptr<Image> Image::loadBmp(const std::string &path) {
    std::unique_ptr<std::FILE, decltype(&std::fclose)> handle(
        std::fopen(path.c_str(), "rb"), &std::fclose);
    std::FILE *file = handle.get();
    if (!file) return nullptr;

    unsigned char fileHeader[14];
    if (std::fread(fileHeader, 1, sizeof(fileHeader), file) != sizeof(fileHeader) ||
        fileHeader[0] != 'B' || fileHeader[1] != 'M') {
        return nullptr;
    }

    const uint32_t pixelOffset = read32(fileHeader + 10);
    unsigned char sizeBytes[4];
    if (std::fread(sizeBytes, 1, sizeof(sizeBytes), file) != sizeof(sizeBytes)) {
        return nullptr;
    }
    const uint32_t dibSize = read32(sizeBytes);
    if (dibSize < 40 || dibSize > 124) {
        return nullptr;
    }

    std::vector<unsigned char> dib(dibSize);
    std::memcpy(dib.data(), sizeBytes, sizeof(sizeBytes));
    if (std::fread(dib.data() + 4, 1, dibSize - 4, file) != dibSize - 4) {
        return nullptr;
    }

    const int32_t width = int32_t(read32(dib.data() + 4));
    const int32_t signedHeight = int32_t(read32(dib.data() + 8));
    const uint16_t planes = read16(dib.data() + 12);
    const uint16_t bitsPerPixel = read16(dib.data() + 14);
    const uint32_t compression = read32(dib.data() + 16);
    if (width <= 0 || signedHeight == 0 || signedHeight == INT32_MIN || planes != 1 ||
        (bitsPerPixel != 24 && bitsPerPixel != 32) ||
        (compression != 0 && compression != 3 && compression != 6)) {
        return nullptr;
    }

    const int height = signedHeight < 0 ? -signedHeight : signedHeight;
    if (width > 16384 || height > 16384 ||
        !dimensionsAreSafe(uint32_t(width), uint32_t(height))) {
        // `handle` owns the stream and closes it on every return path.
        return nullptr;
    }

    uint32_t redMask = bitsPerPixel == 24 ? 0x00FF0000u : 0x00FF0000u;
    uint32_t greenMask = 0x0000FF00u;
    uint32_t blueMask = 0x000000FFu;
    uint32_t alphaMask = 0;
    if (compression == 3 || compression == 6) {
        unsigned char masks[16] = {};
        if (dibSize >= 52) {
            std::memcpy(masks, dib.data() + 40, dibSize >= 56 ? 16 : 12);
        } else {
            const size_t maskBytes = compression == 6 ? 16 : 12;
            if (std::fseek(file, long(14 + dibSize), SEEK_SET) != 0 ||
                std::fread(masks, 1, maskBytes, file) != maskBytes) {
                return nullptr;
            }
        }
        redMask = read32(masks);
        greenMask = read32(masks + 4);
        blueMask = read32(masks + 8);
        if (dibSize >= 56 || compression == 6) alphaMask = read32(masks + 12);
    }

    const uint64_t rowBytes64 = ((uint64_t(width) * bitsPerPixel + 31u) / 32u) * 4u;
    if (rowBytes64 > size_t(-1) || pixelOffset < 14 + dibSize ||
        std::fseek(file, long(pixelOffset), SEEK_SET) != 0) {
        return nullptr;
    }

    const size_t rowBytes = size_t(rowBytes64);
    std::vector<unsigned char> rowBytesBuffer(rowBytes);
    auto image = std::make_shared<Image>(width, height);
    image->opaque_ = bitsPerPixel == 24 || alphaMask == 0;
    const bool topDown = signedHeight < 0;
    for (int fileY = 0; fileY < height; ++fileY) {
        if (std::fread(rowBytesBuffer.data(), 1, rowBytes, file) != rowBytes) {
            return nullptr;
        }
        const int y = topDown ? fileY : height - 1 - fileY;
        uint32_t *dst = image->row(y);
        for (int x = 0; x < width; ++x) {
            const unsigned char *p = rowBytesBuffer.data() + size_t(x) * (bitsPerPixel / 8);
            uint32_t pixel = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16);
            if (bitsPerPixel == 32) pixel |= uint32_t(p[3]) << 24;

            if (compression == 3 || compression == 6) {
                const uint8_t r = maskedChannel(pixel, redMask, 0);
                const uint8_t g = maskedChannel(pixel, greenMask, 0);
                const uint8_t b = maskedChannel(pixel, blueMask, 0);
                const uint8_t a = maskedChannel(pixel, alphaMask, 255);
                if (a != 255) image->opaque_ = false;
                dst[x] = (uint32_t(a) << 24) | (uint32_t(r) << 16) |
                         (uint32_t(g) << 8) | b;
            } else {
                // BI_RGB's fourth byte is reserved by the BMP spec, not reliable alpha.
                dst[x] = 0xFF000000u | (uint32_t(p[2]) << 16) |
                         (uint32_t(p[1]) << 8) | p[0];
            }
        }
    }

    return image;
}

ImagePtr Image::loadJpeg(const std::string &path) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return nullptr;

    // Keep libjpeg's mutable state and the decoded image on the heap. libjpeg reports corrupt
    // input with longjmp; jumping over stack objects such as shared_ptr would skip destructors
    // and is undefined behavior in C++ (particularly dangerous now that decoding is a worker).
    struct LoadState {
        jpeg_decompress_struct info{};
        JpegError error{};
        ImagePtr image;
        bool created = false;
    };
    std::unique_ptr<LoadState> state(new (std::nothrow) LoadState);
    if (!state) {
        std::fclose(file);
        return nullptr;
    }

    state->info.err = jpeg_std_error(&state->error.base);
    state->error.base.error_exit = jpegFail;

    if (setjmp(state->error.escape)) {
        if (state->created) jpeg_destroy_decompress(&state->info);
        std::fclose(file);
        return nullptr;
    }

    state->created = true;
    jpeg_create_decompress(&state->info);
    jpeg_stdio_src(&state->info, file);
    jpeg_read_header(&state->info, TRUE);

    if (!dimensionsAreSafe(state->info.image_width, state->info.image_height)) {
        jpeg_destroy_decompress(&state->info);
        state->created = false;
        std::fclose(file);
        return nullptr;
    }

    // Ask for the byte order our canvas already uses, so no per-pixel shuffle is needed.
    state->info.out_color_space = JCS_EXT_BGRA;
    jpeg_start_decompress(&state->info);

    if (!dimensionsAreSafe(state->info.output_width, state->info.output_height)) {
        jpeg_destroy_decompress(&state->info);
        state->created = false;
        std::fclose(file);
        return nullptr;
    }

    try {
        state->image = std::make_shared<Image>(int(state->info.output_width),
                                               int(state->info.output_height));
        state->image->opaque_ = true;
        while (state->info.output_scanline < state->info.output_height) {
            JSAMPROW scanline = reinterpret_cast<JSAMPROW>(
                state->image->row(int(state->info.output_scanline)));
            jpeg_read_scanlines(&state->info, &scanline, 1);
        }
        jpeg_finish_decompress(&state->info);
    } catch (...) {
        if (state->created) jpeg_destroy_decompress(&state->info);
        std::fclose(file);
        return nullptr;
    }

    jpeg_destroy_decompress(&state->info);
    state->created = false;
    std::fclose(file);

    // JPEG has no alpha; JCS_EXT_BGRA leaves that byte undefined.
    for (int y = 0; y < state->image->height(); ++y) {
        uint32_t *pixels = state->image->row(y);
        for (int x = 0; x < state->image->width(); ++x) pixels[x] |= 0xFF000000u;
    }

    return std::move(state->image);
}

ImagePtr Image::loadPng(const std::string &path) {
    png_image png;
    std::memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_file(&png, path.c_str())) return nullptr;

    if (!dimensionsAreSafe(png.width, png.height)) {
        png_image_free(&png);
        return nullptr;
    }

    // On little-endian ARM, BGRA byte order matches our ARGB32 words.
    png.format = PNG_FORMAT_BGRA;

    ImagePtr image;
    try {
        image = std::make_shared<Image>(int(png.width), int(png.height));
    } catch (...) {
        png_image_free(&png);
        return nullptr;
    }
    if (!png_image_finish_read(&png, nullptr, image->row(0), 0, nullptr)) {
        png_image_free(&png);
        return nullptr;
    }

    png_image_free(&png);
    return image;
}

ImagePtr Image::scaledTo(int w, int h) const {
    if (!valid() || w <= 0 || h <= 0) return nullptr;

    // Already the size asked for — measured on the device at 34 ms for the bilinear path
    // below even at a 1:1 ratio, since nothing there special-cases "nothing to do". A plain
    // copy of the pixels costs a fraction of that.
    if (w == width_ && h == height_) {
        auto same = std::make_shared<Image>(w, h);
        same->pixels_ = pixels_;
        same->opaque_ = opaque_;
        return same;
    }

    auto out = std::make_shared<Image>(w, h);
    out->opaque_ = opaque_;

    const bool shrinking = (w < width_ || h < height_);

    for (int dy = 0; dy < h; ++dy) {
        uint32_t *dst = out->row(dy);

        if (shrinking) {
            const int sy0 = int(long(dy) * height_ / h);
            int sy1 = int(long(dy + 1) * height_ / h);
            if (sy1 <= sy0) sy1 = sy0 + 1;

            for (int dx = 0; dx < w; ++dx) {
                const int sx0 = int(long(dx) * width_ / w);
                int sx1 = int(long(dx + 1) * width_ / w);
                if (sx1 <= sx0) sx1 = sx0 + 1;

                uint32_t sa = 0, sr = 0, sg = 0, sb = 0, n = 0;
                for (int sy = sy0; sy < sy1 && sy < height_; ++sy) {
                    const uint32_t *src = row(sy);
                    for (int sx = sx0; sx < sx1 && sx < width_; ++sx) {
                        const uint32_t p = src[sx];
                        sa += (p >> 24) & 0xFF;
                        sr += (p >> 16) & 0xFF;
                        sg += (p >> 8) & 0xFF;
                        sb += p & 0xFF;
                        ++n;
                    }
                }
                if (!n) n = 1;
                dst[dx] = ((sa / n) << 24) | ((sr / n) << 16) | ((sg / n) << 8) | (sb / n);
            }
        } else {
            const float fy = (h > 1) ? float(dy) * (height_ - 1) / (h - 1) : 0.0f;
            const int y0 = int(fy);
            const int y1 = (y0 + 1 < height_) ? y0 + 1 : y0;
            const float wy = fy - y0;

            for (int dx = 0; dx < w; ++dx) {
                const float fx = (w > 1) ? float(dx) * (width_ - 1) / (w - 1) : 0.0f;
                const int x0 = int(fx);
                const int x1 = (x0 + 1 < width_) ? x0 + 1 : x0;
                const float wx = fx - x0;

                const uint32_t p00 = at(x0, y0), p10 = at(x1, y0);
                const uint32_t p01 = at(x0, y1), p11 = at(x1, y1);

                uint32_t result = 0;
                for (int shift = 0; shift <= 24; shift += 8) {
                    const float c0 = ((p00 >> shift) & 0xFF) * (1 - wx) + ((p10 >> shift) & 0xFF) * wx;
                    const float c1 = ((p01 >> shift) & 0xFF) * (1 - wx) + ((p11 >> shift) & 0xFF) * wx;
                    const float c = c0 * (1 - wy) + c1 * wy;
                    result |= uint32_t(c + 0.5f) << shift;
                }
                dst[dx] = result;
            }
        }
    }

    return out;
}

bool Image::saveJpeg(const std::string &path, int quality) const {
    if (!valid()) return false;

    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) return false;

    jpeg_compress_struct info;
    JpegError error;
    info.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpegFail;

    if (setjmp(error.escape)) {
        jpeg_destroy_compress(&info);
        std::fclose(file);
        return false;
    }

    jpeg_create_compress(&info);
    jpeg_stdio_dest(&info, file);

    info.image_width = JDIMENSION(width_);
    info.image_height = JDIMENSION(height_);
    info.input_components = 4;
    info.in_color_space = JCS_EXT_BGRA;   // our own word order, no shuffling needed

    jpeg_set_defaults(&info);
    jpeg_set_quality(&info, quality, TRUE);
    jpeg_start_compress(&info, TRUE);

    while (info.next_scanline < info.image_height) {
        JSAMPROW scanline = reinterpret_cast<JSAMPROW>(
            const_cast<uint32_t *>(row(int(info.next_scanline))));
        jpeg_write_scanlines(&info, &scanline, 1);
    }

    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    std::fclose(file);
    return true;
}

bool Image::saveBmp(const std::string &path) const {
    if (!valid()) return false;

    const uint64_t stride64 = (uint64_t(width_) * 3u + 3u) & ~uint64_t(3u);
    const uint64_t imageBytes = stride64 * uint64_t(height_);
    const uint64_t fileBytes = 54u + imageBytes;
    if (stride64 > size_t(-1) || fileBytes > UINT32_MAX) return false;

    unsigned char header[54] = {};
    header[0] = 'B';
    header[1] = 'M';
    auto put32 = [&](size_t offset, uint32_t value) {
        header[offset] = uint8_t(value);
        header[offset + 1] = uint8_t(value >> 8);
        header[offset + 2] = uint8_t(value >> 16);
        header[offset + 3] = uint8_t(value >> 24);
    };
    header[2] = uint8_t(fileBytes);
    header[3] = uint8_t(fileBytes >> 8);
    header[4] = uint8_t(fileBytes >> 16);
    header[5] = uint8_t(fileBytes >> 24);
    put32(10, 54);
    put32(14, 40);
    put32(18, uint32_t(width_));
    put32(22, uint32_t(height_));
    header[26] = 1;
    header[28] = 24;
    put32(34, uint32_t(imageBytes));

    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    bool ok = std::fwrite(header, 1, sizeof(header), file) == sizeof(header);
    std::vector<unsigned char> output(size_t(stride64), 0);
    for (int y = height_ - 1; ok && y >= 0; --y) {
        const uint32_t *pixels = row(y);
        for (int x = 0; x < width_; ++x) {
            const uint32_t pixel = pixels[x];
            const uint32_t alpha = pixel >> 24;
            output[size_t(x) * 3] = uint8_t((pixel & 0xFFu) * alpha / 255u);
            output[size_t(x) * 3 + 1] = uint8_t(((pixel >> 8) & 0xFFu) * alpha / 255u);
            output[size_t(x) * 3 + 2] = uint8_t(((pixel >> 16) & 0xFFu) * alpha / 255u);
        }
        ok = std::fwrite(output.data(), 1, output.size(), file) == output.size();
    }
    if (std::fclose(file) != 0) ok = false;
    return ok;
}
