#include "ui-framework/core/Svg.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace ui {

namespace {

constexpr float kPi = 3.14159265358979f;

// The value of an attribute inside one tag, or an empty string. The name must start after a space.
std::string attribute(const std::string &tag, const std::string &name) {
    const std::string needle = " " + name + "=\"";
    const size_t at = tag.find(needle);
    if (at == std::string::npos) return std::string();
    const size_t start = at + needle.size();
    const size_t end = tag.find('"', start);
    if (end == std::string::npos) return std::string();
    return tag.substr(start, end - start);
}

float number(const std::string &text, float fallback = 0.0f) {
    return text.empty() ? fallback : float(std::atof(text.c_str()));
}

// Reads path data one token at a time. Numbers may touch each other ("1.5.5", "-1-1"), and the arc's two
// flags are single digits that may touch the next number.
struct PathReader {
    const char *p;
    const char *end;

    void skip() { while (p < end && (std::isspace((unsigned char)*p) || *p == ',')) ++p; }
    bool more() { skip(); return p < end; }
    bool atNumber() {
        skip();
        return p < end && (std::isdigit((unsigned char)*p) || *p == '-' || *p == '+' || *p == '.');
    }
    bool readNumber(float &out) {
        skip();
        if (p >= end) return false;
        const char *start = p;
        if (*p == '+' || *p == '-') ++p;
        while (p < end && std::isdigit((unsigned char)*p)) ++p;
        if (p < end && *p == '.') {
            ++p;
            while (p < end && std::isdigit((unsigned char)*p)) ++p;
        }
        if (p < end && (*p == 'e' || *p == 'E')) {
            const char *save = p;
            ++p;
            if (p < end && (*p == '+' || *p == '-')) ++p;
            if (p < end && std::isdigit((unsigned char)*p)) while (p < end && std::isdigit((unsigned char)*p)) ++p;
            else p = save;
        }
        if (p == start) return false;
        out = float(std::atof(std::string(start, p).c_str()));
        return true;
    }
    bool readFlag(int &out) {
        skip();
        if (p >= end || (*p != '0' && *p != '1')) return false;
        out = *p - '0';
        ++p;
        return true;
    }
};

struct Builder {
    std::vector<std::vector<std::pair<float, float>>> contours;
    std::vector<std::pair<float, float>> current;

    void begin(float x, float y) {
        finish();
        current.push_back({x, y});
    }
    void line(float x, float y) { current.push_back({x, y}); }
    void finish() {
        if (current.size() >= 2) contours.push_back(current);
        current.clear();
    }
};

void cubic(Builder &b, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
    const int steps = 14;
    for (int i = 1; i <= steps; ++i) {
        const float t = float(i) / steps, u = 1 - t;
        b.line(u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3,
               u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3);
    }
}

void quad(Builder &b, float x0, float y0, float x1, float y1, float x2, float y2) {
    const int steps = 10;
    for (int i = 1; i <= steps; ++i) {
        const float t = float(i) / steps, u = 1 - t;
        b.line(u * u * x0 + 2 * u * t * x1 + t * t * x2, u * u * y0 + 2 * u * t * y1 + t * t * y2);
    }
}

// An elliptical arc from (x0,y0) to (x,y), converted from the endpoint form (SVG implementation notes).
void arc(Builder &b, float x0, float y0, float rx, float ry, float phiDeg, int large, int sweep, float x, float y) {
    if (rx == 0 || ry == 0 || (x0 == x && y0 == y)) { b.line(x, y); return; }
    rx = std::fabs(rx);
    ry = std::fabs(ry);
    const float phi = phiDeg * kPi / 180.0f, cp = std::cos(phi), sp = std::sin(phi);
    const float dx = (x0 - x) / 2, dy = (y0 - y) / 2;
    const float x1p = cp * dx + sp * dy, y1p = -sp * dx + cp * dy;
    const float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1) { const float s = std::sqrt(lambda); rx *= s; ry *= s; }
    const float num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    const float den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    float coef = den == 0 ? 0 : std::sqrt(std::max(0.0f, num / den));
    if (large == sweep) coef = -coef;
    const float cxp = coef * (rx * y1p / ry), cyp = coef * -(ry * x1p / rx);
    const float cx = cp * cxp - sp * cyp + (x0 + x) / 2, cy = sp * cxp + cp * cyp + (y0 + y) / 2;
    auto angle = [](float ux, float uy, float vx, float vy) {
        const float a = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
        return a;
    };
    const float theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (!sweep && dtheta > 0) dtheta -= 2 * kPi;
    if (sweep && dtheta < 0) dtheta += 2 * kPi;
    const int steps = std::max(6, int(std::fabs(dtheta) / (kPi / 12)));
    for (int i = 1; i <= steps; ++i) {
        const float t = theta1 + dtheta * float(i) / steps;
        const float ex = rx * std::cos(t), ey = ry * std::sin(t);
        b.line(cp * ex - sp * ey + cx, sp * ex + cp * ey + cy);
    }
}

// Turns path data into closed polylines.
std::vector<std::vector<std::pair<float, float>>> flattenPath(const std::string &d) {
    Builder b;
    PathReader r{d.data(), d.data() + d.size()};
    float cx = 0, cy = 0, sx = 0, sy = 0, lastCx = 0, lastCy = 0;   // current point, subpath start, last control
    char cmd = 0, prev = 0;
    while (r.more()) {
        if (std::isalpha((unsigned char)*r.p)) { cmd = *r.p++; }
        else if (cmd == 0) break;
        else if (cmd == 'M') cmd = 'L';
        else if (cmd == 'm') cmd = 'l';
        const bool rel = std::islower((unsigned char)cmd) != 0;
        const char c = char(std::toupper((unsigned char)cmd));
        float a[7];
        auto get = [&](int n) { for (int i = 0; i < n; ++i) if (!r.readNumber(a[i])) return false; return true; };
        if (c == 'Z') {
            b.finish();
            cx = sx; cy = sy;
            prev = 'Z';
            // After a close, the next command starts a new subpath at the start point.
            cmd = 0;
            if (!r.more()) break;
            continue;
        }
        if (c == 'M') {
            if (!get(2)) break;
            cx = rel ? cx + a[0] : a[0]; cy = rel ? cy + a[1] : a[1];
            sx = cx; sy = cy;
            b.begin(cx, cy);
        } else if (c == 'L') {
            if (!get(2)) break;
            cx = rel ? cx + a[0] : a[0]; cy = rel ? cy + a[1] : a[1];
            b.line(cx, cy);
        } else if (c == 'H') {
            if (!get(1)) break;
            cx = rel ? cx + a[0] : a[0];
            b.line(cx, cy);
        } else if (c == 'V') {
            if (!get(1)) break;
            cy = rel ? cy + a[0] : a[0];
            b.line(cx, cy);
        } else if (c == 'C') {
            if (!get(6)) break;
            const float x1 = rel ? cx + a[0] : a[0], y1 = rel ? cy + a[1] : a[1];
            const float x2 = rel ? cx + a[2] : a[2], y2 = rel ? cy + a[3] : a[3];
            const float x3 = rel ? cx + a[4] : a[4], y3 = rel ? cy + a[5] : a[5];
            cubic(b, cx, cy, x1, y1, x2, y2, x3, y3);
            lastCx = x2; lastCy = y2; cx = x3; cy = y3;
        } else if (c == 'S') {
            if (!get(4)) break;
            const bool smooth = prev == 'C' || prev == 'S';
            const float x1 = smooth ? 2 * cx - lastCx : cx, y1 = smooth ? 2 * cy - lastCy : cy;
            const float x2 = rel ? cx + a[0] : a[0], y2 = rel ? cy + a[1] : a[1];
            const float x3 = rel ? cx + a[2] : a[2], y3 = rel ? cy + a[3] : a[3];
            cubic(b, cx, cy, x1, y1, x2, y2, x3, y3);
            lastCx = x2; lastCy = y2; cx = x3; cy = y3;
        } else if (c == 'Q') {
            if (!get(4)) break;
            const float x1 = rel ? cx + a[0] : a[0], y1 = rel ? cy + a[1] : a[1];
            const float x2 = rel ? cx + a[2] : a[2], y2 = rel ? cy + a[3] : a[3];
            quad(b, cx, cy, x1, y1, x2, y2);
            lastCx = x1; lastCy = y1; cx = x2; cy = y2;
        } else if (c == 'T') {
            if (!get(2)) break;
            const bool smooth = prev == 'Q' || prev == 'T';
            const float x1 = smooth ? 2 * cx - lastCx : cx, y1 = smooth ? 2 * cy - lastCy : cy;
            const float x2 = rel ? cx + a[0] : a[0], y2 = rel ? cy + a[1] : a[1];
            quad(b, cx, cy, x1, y1, x2, y2);
            lastCx = x1; lastCy = y1; cx = x2; cy = y2;
        } else if (c == 'A') {
            float rx, ry, rot, x, y;
            int large, sweep;
            if (!r.readNumber(rx) || !r.readNumber(ry) || !r.readNumber(rot) || !r.readFlag(large) ||
                !r.readFlag(sweep) || !r.readNumber(x) || !r.readNumber(y)) break;
            if (rel) { x += cx; y += cy; }
            arc(b, cx, cy, rx, ry, rot, large, sweep, x, y);
            cx = x; cy = y;
        } else {
            break;   // unknown command: stop, keep what was read
        }
        prev = c;
    }
    b.finish();
    return b.contours;
}

}  // namespace

bool SvgIcon::parse(const std::string &svg) {
    shapes_.clear();
    viewW_ = viewH_ = 16.0f;
    // The view box, from the <svg> tag (falls back to width and height).
    const size_t svgAt = svg.find("<svg");
    if (svgAt != std::string::npos) {
        const std::string tag = svg.substr(svgAt, svg.find('>', svgAt) - svgAt);
        const std::string box = attribute(tag, "viewBox");
        float v[4] = {0, 0, 16, 16};
        if (!box.empty()) {
            PathReader r{box.data(), box.data() + box.size()};
            for (int i = 0; i < 4; ++i) if (!r.readNumber(v[i])) break;
            viewW_ = v[2];
            viewH_ = v[3];
        } else {
            viewW_ = number(attribute(tag, "width"), 16.0f);
            viewH_ = number(attribute(tag, "height"), 16.0f);
        }
    }

    size_t at = 0;
    while ((at = svg.find('<', at)) != std::string::npos) {
        const size_t close = svg.find('>', at);
        if (close == std::string::npos) break;
        const std::string tag = svg.substr(at, close - at + 1);
        at = close + 1;
        if (attribute(tag, "fill") == "none") continue;
        Shape shape;
        shape.evenOdd = attribute(tag, "fill-rule") == "evenodd";
        auto addContour = [&](const std::vector<std::pair<float, float>> &poly) {
            std::vector<Point> pts;
            for (const auto &q : poly) pts.push_back({q.first, q.second});
            shape.contours.push_back(pts);
        };
        if (tag.compare(0, 5, "<path") == 0) {
            for (const auto &poly : flattenPath(attribute(tag, "d"))) addContour(poly);
        } else if (tag.compare(0, 7, "<circle") == 0) {
            const float cx = number(attribute(tag, "cx")), cy = number(attribute(tag, "cy")), r = number(attribute(tag, "r"));
            std::vector<std::pair<float, float>> poly;
            for (int i = 0; i < 48; ++i) poly.push_back({cx + r * std::cos(2 * kPi * i / 48), cy + r * std::sin(2 * kPi * i / 48)});
            addContour(poly);
        } else if (tag.compare(0, 5, "<rect") == 0) {
            const float x = number(attribute(tag, "x")), y = number(attribute(tag, "y"));
            const float w = number(attribute(tag, "width")), h = number(attribute(tag, "height"));
            addContour({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}});
        } else {
            continue;
        }
        if (!shape.contours.empty()) shapes_.push_back(std::move(shape));
    }
    return !shapes_.empty();
}

ImagePtr SvgIcon::rasterize(int w, int h) const {
    ImagePtr img(new Image(std::max(1, w), std::max(1, h)));
    if (shapes_.empty()) return img;
    const float scale = std::min(float(w) / viewW_, float(h) / viewH_);
    const float ox = (float(w) - viewW_ * scale) / 2, oy = (float(h) - viewH_ * scale) / 2;
    constexpr int kSub = 8;   // sub-scanlines per pixel row

    std::vector<float> total(size_t(w) * h, 0.0f);
    std::vector<float> cover(size_t(w) * h, 0.0f);
    std::vector<float> row(size_t(w), 0.0f);
    struct Crossing { float x; int dir; };
    std::vector<Crossing> crossings;

    for (const Shape &shape : shapes_) {
        std::fill(cover.begin(), cover.end(), 0.0f);
        for (int y = 0; y < h; ++y) {
            std::fill(row.begin(), row.end(), 0.0f);
            for (int s = 0; s < kSub; ++s) {
                const float ys = float(y) + (float(s) + 0.5f) / kSub;
                crossings.clear();
                for (const auto &contour : shape.contours) {
                    const size_t n = contour.size();
                    for (size_t i = 0; i < n; ++i) {
                        const Point &a = contour[i], &b = contour[(i + 1) % n];
                        const float ay = a.y * scale + oy, by = b.y * scale + oy;
                        if ((ay <= ys && by > ys) || (by <= ys && ay > ys)) {
                            const float ax = a.x * scale + ox, bx = b.x * scale + ox;
                            crossings.push_back({ax + (ys - ay) * (bx - ax) / (by - ay), by > ay ? 1 : -1});
                        }
                    }
                }
                std::sort(crossings.begin(), crossings.end(), [](const Crossing &p, const Crossing &q) { return p.x < q.x; });
                int winding = 0;
                for (size_t i = 0; i + 1 < crossings.size(); ++i) {
                    winding += crossings[i].dir;
                    const bool inside = shape.evenOdd ? ((i + 1) % 2 == 1) : winding != 0;
                    if (!inside) continue;
                    float xa = std::max(0.0f, crossings[i].x), xb = std::min(float(w), crossings[i + 1].x);
                    if (xb <= xa) continue;
                    const int ia = int(xa), ib = std::min(w - 1, int(xb));
                    if (ia == ib) { row[size_t(ia)] += xb - xa; continue; }
                    row[size_t(ia)] += float(ia + 1) - xa;
                    for (int i2 = ia + 1; i2 < ib; ++i2) row[size_t(i2)] += 1.0f;
                    row[size_t(ib)] += xb - float(ib);
                }
            }
            for (int x = 0; x < w; ++x) cover[size_t(y) * w + x] = std::min(1.0f, row[size_t(x)] / kSub);
        }
        // Several shapes of one icon are laid over each other.
        for (size_t i = 0; i < total.size(); ++i) total[i] = 1.0f - (1.0f - total[i]) * (1.0f - cover[i]);
    }

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img->row(y)[x] = (uint32_t(total[size_t(y) * w + x] * 255.0f + 0.5f) << 24) | 0x00FFFFFFu;
    return img;
}

}  // namespace ui
