#include "Font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include "BuiltinFont.h"
#include "Canvas.h"

namespace {

constexpr int kBuiltinCell = 8;

FT_Library asLibrary(void *p) { return static_cast<FT_Library>(p); }
FT_Face asFace(void *p) { return static_cast<FT_Face>(p); }

} // namespace

std::vector<unsigned int> decodeUtf8(const std::string &text) {
    std::vector<unsigned int> out;
    out.reserve(text.size());

    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        unsigned int cp = c;
        int extra = 0;

        if (c >= 0xF0) { cp = c & 0x07; extra = 3; }
        else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
        else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }

        if (extra && i + extra < text.size()) {
            bool ok = true;
            for (int k = 1; k <= extra; ++k) {
                const unsigned char cc = static_cast<unsigned char>(text[i + k]);
                if ((cc & 0xC0) != 0x80) { ok = false; break; }
                cp = (cp << 6) | (cc & 0x3F);
            }
            if (ok) { out.push_back(cp); i += extra + 1; continue; }
        }

        out.push_back(c);
        ++i;
    }

    return out;
}

Font::~Font() {
    if (face_) FT_Done_Face(asFace(face_));
    if (library_) FT_Done_FreeType(asLibrary(library_));
}

bool Font::load(const std::vector<std::string> &candidates) {
    FT_Library lib = nullptr;
    if (FT_Init_FreeType(&lib) != 0) return false;
    library_ = lib;

    for (const std::string &candidate : candidates) {
        FT_Face face = nullptr;
        if (FT_New_Face(lib, candidate.c_str(), 0, &face) == 0) {
            face_ = face;
            path_ = candidate;
            return true;
        }
    }

    return false;
}

int Font::ascender(int size) {
    if (!face_) return kBuiltinCell * size / kBuiltinCell;

    auto it = ascenders_.find(size);
    if (it != ascenders_.end()) return it->second;

    FT_Face face = asFace(face_);
    FT_Set_Pixel_Sizes(face, 0, FT_UInt(size));
    activeSize_ = size;

    const int value = int(face->size->metrics.ascender >> 6);
    ascenders_[size] = value;
    lineHeights_[size] = int(face->size->metrics.height >> 6);
    return value;
}

int Font::lineHeight(int size) {
    if (!face_) return size;
    ascender(size);
    auto it = lineHeights_.find(size);
    return (it != lineHeights_.end()) ? it->second : size;
}

const Font::Glyph *Font::glyphFor(unsigned int codepoint, int size) {
    const uint64_t key = (uint64_t(size) << 32) | codepoint;
    auto it = cache_.find(key);
    if (it != cache_.end()) return &it->second;

    if (!face_) return nullptr;

    FT_Face face = asFace(face_);
    if (activeSize_ != size) {
        FT_Set_Pixel_Sizes(face, 0, FT_UInt(size));
        activeSize_ = size;
    }

    if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER) != 0) return nullptr;

    Glyph glyph;
    const FT_Bitmap &bitmap = face->glyph->bitmap;
    glyph.width = int(bitmap.width);
    glyph.height = int(bitmap.rows);
    glyph.bearingX = face->glyph->bitmap_left;
    glyph.bearingY = face->glyph->bitmap_top;
    glyph.advance = int(face->glyph->advance.x >> 6);
    glyph.coverage.resize(size_t(glyph.width) * glyph.height);

    for (int y = 0; y < glyph.height; ++y)
        for (int x = 0; x < glyph.width; ++x)
            glyph.coverage[size_t(y) * glyph.width + x] = bitmap.buffer[y * bitmap.pitch + x];

    return &cache_.emplace(key, std::move(glyph)).first->second;
}

int Font::measure(const std::string &text, int size) {
    const std::vector<unsigned int> codepoints = decodeUtf8(text);

    if (!face_) {
        const int cell = std::max(1, size / kBuiltinCell);
        return int(codepoints.size()) * (kBuiltinCell + 1) * cell;
    }

    int width = 0;
    for (unsigned int cp : codepoints) {
        const Glyph *g = glyphFor(cp, size);
        width += g ? g->advance : size / 2;
    }
    return width;
}

void Font::draw(Canvas &canvas, int x, int y, const std::string &text, int size, Color color) {
    const std::vector<unsigned int> codepoints = decodeUtf8(text);

    if (!face_) {
        const int cell = std::max(1, size / kBuiltinCell);
        canvas.markDamage({x, y, measure(text, size), kBuiltinCell * cell});
        int pen = x;
        for (unsigned int cp : codepoints) {
            const unsigned char *rows = builtin_font::glyph(cp);
            for (int gy = 0; gy < kBuiltinCell; ++gy)
                for (int gx = 0; gx < kBuiltinCell; ++gx)
                    if (rows[gy] & (0x80 >> gx))
                        canvas.fillRect({pen + gx * cell, y + gy * cell, cell, cell}, color);
            pen += (kBuiltinCell + 1) * cell;
        }
        return;
    }

    const int baseline = y + ascender(size);
    canvas.markDamage({x, y, measure(text, size), lineHeight(size) + size / 4});
    int pen = x;

    for (unsigned int cp : codepoints) {
        const Glyph *g = glyphFor(cp, size);
        if (!g) { pen += size / 2; continue; }

        for (int gy = 0; gy < g->height; ++gy) {
            for (int gx = 0; gx < g->width; ++gx) {
                const uint8_t cov = g->coverage[size_t(gy) * g->width + gx];
                if (cov) canvas.blendPixel(pen + g->bearingX + gx, baseline - g->bearingY + gy,
                                           color, cov);
            }
        }
        pen += g->advance;
    }
}

void Font::drawCentered(Canvas &canvas, const Rect &area, const std::string &text, int size,
                        Color color) {
    const int w = measure(text, size);
    const int h = lineHeight(size);
    draw(canvas, area.x + (area.w - w) / 2, area.y + (area.h - h) / 2, text, size, color);
}

std::string Font::elide(const std::string &text, int size, int maxWidth) {
    if (measure(text, size) <= maxWidth) return text;

    // Trim whole code points so multi-byte characters are never split.
    std::string result = text;
    while (!result.empty()) {
        do {
            result.pop_back();
        } while (!result.empty() && (static_cast<unsigned char>(result.back()) & 0xC0) == 0x80);

        if (measure(result + "...", size) <= maxWidth) return result + "...";
    }

    return std::string();
}
