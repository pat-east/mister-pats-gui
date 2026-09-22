#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Color.h"
#include "Geometry.h"

class Canvas;

// TrueType text rendering with a per-size glyph cache. Falls back to a built-in 8x8 face
// when no font file can be opened, so the interface stays usable either way.
class Font {
public:
    ~Font();

    // Tries each candidate in order; the first one that opens wins.
    bool load(const std::vector<std::string> &candidates);

    bool usingTrueType() const { return face_ != nullptr; }
    const std::string &path() const { return path_; }

    int lineHeight(int size);
    int measure(const std::string &text, int size);

    // `y` is the top of the line, not the baseline.
    void draw(Canvas &canvas, int x, int y, const std::string &text, int size, Color color);
    void drawCentered(Canvas &canvas, const Rect &area, const std::string &text,
                      int size, Color color);

    // Shortens with a trailing ellipsis until it fits.
    std::string elide(const std::string &text, int size, int maxWidth);

private:
    struct Glyph {
        std::vector<uint8_t> coverage;
        int width = 0, height = 0;
        int bearingX = 0, bearingY = 0;
        int advance = 0;
    };

    const Glyph *glyphFor(unsigned int codepoint, int size);
    int ascender(int size);

    void *library_ = nullptr;   // FT_Library
    void *face_ = nullptr;      // FT_Face
    std::string path_;
    int activeSize_ = 0;
    std::map<uint64_t, Glyph> cache_;
    std::map<int, int> ascenders_;
    std::map<int, int> lineHeights_;
};

// Minimal UTF-8 walk; invalid bytes are surfaced as their raw value.
std::vector<unsigned int> decodeUtf8(const std::string &text);
