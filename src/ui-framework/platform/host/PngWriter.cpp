#include "ui-framework/platform/host/PngWriter.h"

#include <cstdio>
#include <vector>

#include <png.h>

namespace ui {

bool writePng(const Canvas &canvas, const std::string &path) {
    FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        if (png) png_destroy_write_struct(&png, info ? &info : nullptr);
        std::fclose(file);
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, canvas.width(), canvas.height(), 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    std::vector<unsigned char> line(size_t(canvas.width()) * 3);
    for (int y = 0; y < canvas.height(); ++y) {
        const uint32_t *row = canvas.row(y);
        for (int x = 0; x < canvas.width(); ++x) {
            line[size_t(x) * 3 + 0] = (row[x] >> 16) & 0xFF;
            line[size_t(x) * 3 + 1] = (row[x] >> 8) & 0xFF;
            line[size_t(x) * 3 + 2] = row[x] & 0xFF;
        }
        png_write_row(png, line.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

}  // namespace ui
