#include "Font.h"

#include <algorithm>
#include <cmath>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "BuiltinFont.h"
#include "Canvas.h"

namespace {

constexpr int kBuiltinCell = 8;

// Preserve the bitmap face's pixel edges while taking the harshness out of its old
// whole-cell scaling. At typical UI sizes this reduces the glyph height by about a quarter;
// at the smallest sizes it keeps the existing 8-pixel glyphs from getting any larger.
int builtinHeight(int size) {
    const int stepped = std::max(1, size / kBuiltinCell) * kBuiltinCell;
    const int compact = std::max(kBuiltinCell, size * 3 / 4);
    return std::min(stepped, compact);
}

int builtinAdvance(int size) {
    const int height = builtinHeight(size);
    return (height * (kBuiltinCell + 1) + kBuiltinCell - 1) / kBuiltinCell;
}

bool isPlayStationShape(unsigned int cp) {
    return cp == 0x2715 || cp == 0x25CB || cp == 0x25A1 || cp == 0x25B3;
}

bool isDirectionArrow(unsigned int cp) {
    return cp == 0x2191 || cp == 0x2193 || cp == 0x2190 || cp == 0x2192;
}

const unsigned char *directionArrowRows(unsigned int cp) {
    static constexpr unsigned char up[8] =
        {0x18, 0x3C, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18};
    static constexpr unsigned char down[8] =
        {0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x3C, 0x18};
    static constexpr unsigned char left[8] =
        {0x18, 0x0C, 0x06, 0x7F, 0x7F, 0x06, 0x0C, 0x18};
    static constexpr unsigned char right[8] =
        {0x18, 0x30, 0x60, 0x7F, 0x7F, 0x60, 0x30, 0x18};
    if (cp == 0x2191) return up;
    if (cp == 0x2193) return down;
    if (cp == 0x2190) return right;
    return left;
}

void drawDirectionArrow(Canvas &canvas, int x, int y, int size, unsigned int cp, Color color) {
    const unsigned char *rows = directionArrowRows(cp);
    for (int gy = 0; gy < kBuiltinCell; ++gy) {
        const int top = y + gy * size / kBuiltinCell;
        const int bottom = y + (gy + 1) * size / kBuiltinCell;
        for (int gx = 0; gx < kBuiltinCell; ++gx) {
            if (!(rows[gy] & (0x80 >> gx))) continue;
            const int left = x + gx * size / kBuiltinCell;
            const int right = x + (gx + 1) * size / kBuiltinCell;
            canvas.fillRect({left, top, right - left, bottom - top}, color);
        }
    }
}

float segmentDistance(float x, float y, float x1, float y1, float x2, float y2) {
    const float dx = x2 - x1, dy = y2 - y1;
    const float length2 = dx * dx + dy * dy;
    const float t = length2 > 0 ? std::max(0.0f, std::min(1.0f,
        ((x - x1) * dx + (y - y1) * dy) / length2)) : 0.0f;
    const float px = x1 + t * dx, py = y1 + t * dy;
    return std::sqrt((x - px) * (x - px) + (y - py) * (y - py));
}

bool insidePlayStationStroke(unsigned int cp, float x, float y, float width, float height,
                             float stroke) {
    const float cx = width * 0.5f, cy = height * 0.5f;
    if (cp == 0x2715) {
        const float inset = width * 0.22f;
        return segmentDistance(x, y, inset, inset, width - inset, height - inset) <= stroke ||
               segmentDistance(x, y, width - inset, inset, inset, height - inset) <= stroke;
    }
    if (cp == 0x25CB) {
        const float radius = std::min(width, height) * 0.34f;
        const float circleY = cy + 1.5f;
        return std::abs(std::sqrt((x - cx) * (x - cx) + (y - circleY) * (y - circleY)) -
                        radius) <= stroke;
    }
    if (cp == 0x25A1) {
        const float half = std::min(width, height) * 0.32f;
        const float dx = std::abs(x - cx), dy = std::abs(y - cy);
        return dx <= half + stroke && dy <= half + stroke &&
               (std::abs(dx - half) <= stroke || std::abs(dy - half) <= stroke);
    }
    if (cp == 0x25B3) {
        const float top = height * 0.16f, bottom = height * 0.83f;
        return segmentDistance(x, y, cx, top, width * 0.16f, bottom) <= stroke ||
               segmentDistance(x, y, width * 0.16f, bottom, width * 0.84f, bottom) <= stroke ||
               segmentDistance(x, y, width * 0.84f, bottom, cx, top) <= stroke;
    }
    return false;
}

void drawPlayStationShape(Canvas &canvas, int x, int y, int width, int height,
                          unsigned int cp, Color color) {
    constexpr int kSamples = 4;
    const float stroke = std::max(1.0f, std::min(width, height) * 0.075f);
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            int covered = 0;
            for (int sy = 0; sy < kSamples; ++sy)
                for (int sx = 0; sx < kSamples; ++sx)
                    if (insidePlayStationStroke(cp, px + (sx + 0.5f) / kSamples,
                                                py + (sy + 0.5f) / kSamples,
                                                float(width), float(height), stroke))
                        ++covered;
            if (covered)
                canvas.blendPixel(x + px, y + py, color,
                                  uint8_t((covered * 255 + kSamples * kSamples / 2) /
                                          (kSamples * kSamples)));
        }
    }
}

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
    reset();
}

void Font::reset() {
    if (face_) FT_Done_Face(asFace(face_));
    if (library_) FT_Done_FreeType(asLibrary(library_));
    face_ = nullptr;
    library_ = nullptr;
    path_.clear();
    activeSize_ = 0;
    cache_.clear();
    ascenders_.clear();
    lineHeights_.clear();
}

int Font::renderSize(int size) const {
    return std::max(1, int((long(size) * scalePercent_ + 50) / 100));
}

bool Font::load(const std::vector<std::string> &candidates) {
    reset();
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
    if (!face_) return builtinHeight(size);

    auto it = ascenders_.find(size);
    if (it != ascenders_.end()) return it->second;

    FT_Face face = asFace(face_);
    const int actualSize = renderSize(size);
    FT_Set_Pixel_Sizes(face, 0, FT_UInt(actualSize));
    activeSize_ = actualSize;

    const int value = int(face->size->metrics.ascender >> 6);
    ascenders_[size] = value;
    lineHeights_[size] = int(face->size->metrics.height >> 6);
    return value;
}

int Font::lineHeight(int size) {
    if (!face_) return builtinHeight(size);
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
    if (FT_Get_Char_Index(face, codepoint) == 0) return nullptr;
    const int actualSize = renderSize(size);
    if (activeSize_ != actualSize) {
        FT_Set_Pixel_Sizes(face, 0, FT_UInt(actualSize));
        activeSize_ = actualSize;
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
        return int(codepoints.size()) * builtinAdvance(size);
    }

    int width = 0;
    for (unsigned int cp : codepoints) {
        const Glyph *g = glyphFor(cp, size);
        if (g) {
            width += g->advance;
        } else {
            const bool standaloneShape = isPlayStationShape(cp) || isDirectionArrow(cp);
            width += standaloneShape ? std::max(builtinAdvance(size), renderSize(size) + 2)
                                     : builtinAdvance(size);
        }
    }
    return width;
}

void Font::draw(Canvas &canvas, int x, int y, const std::string &text, int size, Color color) {
    const std::vector<unsigned int> codepoints = decodeUtf8(text);

    if (!face_) {
        const int height = builtinHeight(size);
        const int advance = builtinAdvance(size);
        canvas.markDamage({x, y, measure(text, size), height});
        int pen = x;
        for (unsigned int cp : codepoints) {
            if (isDirectionArrow(cp)) {
                drawDirectionArrow(canvas, pen, y, height, cp, color);
                pen += advance;
                continue;
            }
            const unsigned char *rows = builtin_font::glyph(cp);
            for (int gy = 0; gy < kBuiltinCell; ++gy) {
                const int top = y + gy * height / kBuiltinCell;
                const int bottom = y + (gy + 1) * height / kBuiltinCell;
                for (int gx = 0; gx < kBuiltinCell; ++gx) {
                    if (!(rows[gy] & (0x80 >> gx))) continue;
                    const int left = pen + gx * height / kBuiltinCell;
                    const int right = pen + (gx + 1) * height / kBuiltinCell;
                    canvas.fillRect({left, top, right - left, bottom - top}, color);
                }
            }
            pen += advance;
        }
        return;
    }

    const int baseline = y + ascender(size);
    const int actualSize = renderSize(size);
    canvas.markDamage({x, y, measure(text, size), lineHeight(size) + actualSize / 4});
    int pen = x;

    for (unsigned int cp : codepoints) {
        const Glyph *g = glyphFor(cp, size);
        if (!g) {
            const unsigned char *rows = builtin_font::glyph(cp);
            const bool playStationShape = isPlayStationShape(cp);
            const int height = builtinHeight(size);
            const bool directionArrow = isDirectionArrow(cp);
            const int glyphHeight = (playStationShape || directionArrow)
                                        ? std::max(height, actualSize)
                                        : height;
            const int glyphWidth = glyphHeight;
            const int advance = (playStationShape || directionArrow)
                                    ? std::max(builtinAdvance(size), glyphWidth + 2)
                                    : builtinAdvance(size);
            // The bitmap fallback uses its own compact pixel face, whose top starts lower
            // relative to the baseline than the TrueType face's line box does. Anchor its
            // bottom to the same baseline so missing symbols (notably the PS button shapes)
            // sit alongside the surrounding text instead of riding high.
            const int fallbackY = baseline - glyphHeight + (playStationShape ? 3 : 0);
            const int fallbackX = pen + (advance - glyphWidth) / 2;
            if (directionArrow) {
                drawDirectionArrow(canvas, fallbackX, fallbackY, glyphHeight, cp, color);
                pen += advance;
                continue;
            }
            if (playStationShape) {
                drawPlayStationShape(canvas, fallbackX, fallbackY, glyphWidth, glyphHeight, cp,
                                     color);
                pen += advance;
                continue;
            }
            for (int gy = 0; gy < kBuiltinCell; ++gy) {
                const int top = fallbackY + gy * glyphHeight / kBuiltinCell;
                const int bottom = fallbackY + (gy + 1) * glyphHeight / kBuiltinCell;
                for (int gx = 0; gx < kBuiltinCell; ++gx) {
                    if (!(rows[gy] & (0x80 >> gx))) continue;
                    const int left = fallbackX + gx * glyphWidth / kBuiltinCell;
                    const int right = fallbackX + (gx + 1) * glyphWidth / kBuiltinCell;
                    canvas.fillRect({left, top, right - left, bottom - top}, color);
                }
            }
            pen += advance;
            continue;
        }

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
