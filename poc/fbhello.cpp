// PoC: draw text directly into the MiSTer framebuffer (/dev/fb0) so it appears on the TV.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

// Glyphs are looked up per character range rather than from one positional table,
// so a miscounted filler entry cannot shift the whole alphabet.
static const unsigned char GLYPH_UPPER[26][8] = {
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, // A
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // B
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // C
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // D
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, // E
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, // F
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // G
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // J
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // K
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // N
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // O
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // P
    {0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00}, // Q
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // R
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // S
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // U
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // Y
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // Z
};

static const unsigned char GLYPH_DIGIT[10][8] = {
    {0x3C,0x66,0x6E,0x7E,0x76,0x66,0x3C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x3C,0x66,0x06,0x0C,0x30,0x60,0x7E,0x00}, // 2
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 3
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}, // 4
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 5
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 6
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, // 7
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // 9
};

static const unsigned char GLYPH_BLANK[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static const unsigned char GLYPH_DASH[8]  = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00};
static const unsigned char GLYPH_DOT[8]   = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00};
static const unsigned char GLYPH_COLON[8] = {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00};
static const unsigned char GLYPH_BANG[8]  = {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00};

static const unsigned char *glyph(char c) {
    if (c >= 'a' && c <= 'z') c = char(c - 32);
    if (c >= 'A' && c <= 'Z') return GLYPH_UPPER[c - 'A'];
    if (c >= '0' && c <= '9') return GLYPH_DIGIT[c - '0'];
    switch (c) {
    case '-': return GLYPH_DASH;
    case '.': return GLYPH_DOT;
    case ':': return GLYPH_COLON;
    case '!': return GLYPH_BANG;
    default:  return GLYPH_BLANK;
    }
}

struct Canvas {
    uint8_t *mem;
    int w, h, bpp;
    size_t stride;
    fb_var_screeninfo var;
};

static uint32_t pack(const Canvas &c, uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(r) >> (8 - c.var.red.length)) << c.var.red.offset |
           (uint32_t(g) >> (8 - c.var.green.length)) << c.var.green.offset |
           (uint32_t(b) >> (8 - c.var.blue.length)) << c.var.blue.offset;
}

static void rect(const Canvas &c, int x, int y, int w, int h, uint32_t col) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > c.w) w = c.w - x;
    if (y + h > c.h) h = c.h - y;
    if (w <= 0 || h <= 0) return;

    for (int j = 0; j < h; ++j) {
        uint8_t *row = c.mem + size_t(y + j) * c.stride + size_t(x) * (c.bpp / 8);
        if (c.bpp == 32) {
            uint32_t *p = reinterpret_cast<uint32_t *>(row);
            for (int i = 0; i < w; ++i) p[i] = col;
        } else if (c.bpp == 16) {
            uint16_t *p = reinterpret_cast<uint16_t *>(row);
            for (int i = 0; i < w; ++i) p[i] = uint16_t(col);
        }
    }
}

static int text_width(const char *s, int scale) {
    return int(std::strlen(s)) * 9 * scale;
}

static void text(const Canvas &c, int x, int y, const char *s, int scale, uint32_t col) {
    for (int n = 0; s[n]; ++n) {
        const unsigned char *g = glyph(s[n]);
        for (int row = 0; row < 8; ++row)
            for (int bit = 0; bit < 8; ++bit)
                if (g[row] & (0x80 >> bit))
                    rect(c, x + (n * 9 + bit) * scale, y + row * scale, scale, scale, col);
    }
}

int main(int argc, char **argv) {
    int seconds = (argc > 1) ? atoi(argv[1]) : 15;

    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { perror("open /dev/fb0"); return 1; }

    fb_var_screeninfo var{};
    fb_fix_screeninfo fix{};
    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) { perror("FBIOGET_VSCREENINFO"); return 1; }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) { perror("FBIOGET_FSCREENINFO"); return 1; }

    std::printf("fb0: %ux%u @ %u bpp, line_length=%u, smem_len=%u, id=%s\n",
                var.xres, var.yres, var.bits_per_pixel, fix.line_length, fix.smem_len, fix.id);

    size_t size = size_t(fix.line_length) * var.yres;
    uint8_t *front = (uint8_t *)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (front == MAP_FAILED) { perror("mmap"); return 1; }

    // Render off-screen and blit once per frame, otherwise the visible buffer tears badly
    // while the running frontend paints into it as well.
    uint8_t *back = (uint8_t *)std::malloc(size);
    if (!back) { std::fprintf(stderr, "malloc failed\n"); return 1; }

    Canvas c{};
    c.mem = back;
    c.w = var.xres;
    c.h = var.yres;
    c.bpp = var.bits_per_pixel;
    c.stride = fix.line_length;
    c.var = var;

    const uint32_t bg     = pack(c, 0x10, 0x14, 0x2c);
    const uint32_t accent = pack(c, 0x4c, 0x8d, 0xff);
    const uint32_t white  = pack(c, 0xff, 0xff, 0xff);
    const uint32_t dim    = pack(c, 0x9a, 0xa4, 0xc0);

    int s1 = c.w / 220;  if (s1 < 1) s1 = 1;
    int s2 = c.w / 640;  if (s2 < 1) s2 = 1;

    const char *l1 = "HELLO FROM OUR OWN GUI";
    const char *l2 = "BUILT ON AN M1 MAC - RUNNING ON THE MISTER";

    const int fps = 30;
    const int frames = seconds * fps;

    for (int f = 0; f < frames; ++f) {
        rect(c, 0, 0, c.w, c.h, bg);
        rect(c, 0, 0, c.w, 10, accent);
        rect(c, 0, c.h - 10, c.w, 10, accent);

        text(c, (c.w - text_width(l1, s1)) / 2, c.h / 2 - 9 * s1, l1, s1, white);
        text(c, (c.w - text_width(l2, s2)) / 2, c.h / 2 + 6 * s1, l2, s2, dim);

        char l3[64];
        std::snprintf(l3, sizeof(l3), "FRAME %d OF %d", f + 1, frames);
        text(c, (c.w - text_width(l3, s2)) / 2, c.h / 2 + 6 * s1 + 14 * s2, l3, s2, accent);

        int bw = c.w / 6;
        int bx = int((double(f % fps) / fps) * (c.w + bw)) - bw;
        rect(c, bx, c.h - 10 - c.h / 12, bw, c.h / 12, accent);

        std::memcpy(front, back, size);
        usleep(1000000 / fps);
    }

    std::printf("painted %dx%d for %ds (%d frames)\n", c.w, c.h, seconds, frames);

    std::free(back);
    munmap(front, size);
    close(fd);
    return 0;
}
