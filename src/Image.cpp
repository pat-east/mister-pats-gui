#include "Image.h"

#include <cstdio>
#include <cstring>
#include <csetjmp>
#include <png.h>

extern "C" {
#include <jpeglib.h>
}

namespace {

// The first bytes of the file, which is the only reliable statement about its format.
enum class Format { Unknown, Png, Jpeg };

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
    return Format::Unknown;
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
    switch (sniff(path)) {
    case Format::Png:  return loadPng(path);
    case Format::Jpeg: return loadJpeg(path);
    default:           return nullptr;
    }
}

ImagePtr Image::loadJpeg(const std::string &path) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return nullptr;

    jpeg_decompress_struct info;
    JpegError error;
    info.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpegFail;

    if (setjmp(error.escape)) {
        jpeg_destroy_decompress(&info);
        std::fclose(file);
        return nullptr;
    }

    jpeg_create_decompress(&info);
    jpeg_stdio_src(&info, file);
    jpeg_read_header(&info, TRUE);

    // Ask for the byte order our canvas already uses, so no per-pixel shuffle is needed.
    info.out_color_space = JCS_EXT_BGRA;
    jpeg_start_decompress(&info);

    auto image = std::make_shared<Image>(int(info.output_width), int(info.output_height));
    while (info.output_scanline < info.output_height) {
        JSAMPROW scanline = reinterpret_cast<JSAMPROW>(image->row(int(info.output_scanline)));
        jpeg_read_scanlines(&info, &scanline, 1);
    }

    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
    std::fclose(file);

    // JPEG has no alpha; JCS_EXT_BGRA leaves that byte undefined.
    for (int y = 0; y < image->height(); ++y) {
        uint32_t *pixels = image->row(y);
        for (int x = 0; x < image->width(); ++x) pixels[x] |= 0xFF000000u;
    }

    return image;
}

ImagePtr Image::loadPng(const std::string &path) {
    png_image png;
    std::memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_file(&png, path.c_str())) return nullptr;

    // On little-endian ARM, BGRA byte order matches our ARGB32 words.
    png.format = PNG_FORMAT_BGRA;

    auto image = std::make_shared<Image>(int(png.width), int(png.height));
    if (!png_image_finish_read(&png, nullptr, image->row(0), 0, nullptr)) {
        png_image_free(&png);
        return nullptr;
    }

    png_image_free(&png);
    return image;
}

ImagePtr Image::scaledTo(int w, int h) const {
    if (!valid() || w <= 0 || h <= 0) return nullptr;

    auto out = std::make_shared<Image>(w, h);

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
