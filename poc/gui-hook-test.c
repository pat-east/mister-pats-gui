// Tiny framebuffer frontend for manually checking Main_MiSTer's optional gui= hook.
// It exits cleanly when /tmp/CORENAME changes, so the menu frontend can restart it on return.
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define CORE_FILE "/tmp/CORENAME"
#define FAIL_FILE "/tmp/mister-gui-poc-fail"

static volatile sig_atomic_t running = 1;

static void stop(int signal_number) {
    (void)signal_number;
    running = 0;
}

static uint32_t rgb(const struct fb_var_screeninfo *v, unsigned r, unsigned g, unsigned b) {
    return ((r >> (8 - v->red.length)) << v->red.offset) |
           ((g >> (8 - v->green.length)) << v->green.offset) |
           ((b >> (8 - v->blue.length)) << v->blue.offset);
}

static void pixel(uint8_t *fb, const struct fb_fix_screeninfo *f,
                  const struct fb_var_screeninfo *v, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= (int)v->xres || y >= (int)v->yres) return;
    uint32_t *row = (uint32_t *)(fb + (size_t)(y + v->yoffset) * f->line_length);
    row[x + v->xoffset] = color;
}

static void rect(uint8_t *fb, const struct fb_fix_screeninfo *f,
                 const struct fb_var_screeninfo *v, int x, int y, int w, int h,
                 uint32_t color) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            pixel(fb, f, v, x + i, y + j, color);
}

// Five-by-seven uppercase glyph rows; set bits are drawn left to right.
static uint8_t glyph_row(char c, int row) {
    static const uint8_t G[7] = {14, 17, 16, 23, 17, 17, 14};
    static const uint8_t U[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t I[7] = {31, 4, 4, 4, 4, 4, 31};
    static const uint8_t H[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t O[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t K[7] = {17, 18, 20, 24, 20, 18, 17};
    switch (c) {
    case 'G': return G[row]; case 'U': return U[row]; case 'I': return I[row];
    case 'H': return H[row]; case 'O': return O[row]; case 'K': return K[row];
    default: return 0;
    }
}

static void label(uint8_t *fb, const struct fb_fix_screeninfo *f,
                  const struct fb_var_screeninfo *v, int x, int y, int scale,
                  uint32_t color) {
    static const char text[] = "GUI HOOK OK";
    for (int n = 0; text[n]; ++n) {
        for (int row = 0; row < 7; ++row) {
            uint8_t bits = glyph_row(text[n], row);
            for (int col = 0; col < 5; ++col) {
                if (bits & (1u << (4 - col)))
                    rect(fb, f, v, x + (n * 6 + col) * scale, y + row * scale,
                         scale, scale, color);
            }
        }
    }
}

static int read_core(char *name, size_t size) {
    FILE *file = fopen(CORE_FILE, "r");
    if (!file) return 0;
    if (!fgets(name, (int)size, file)) name[0] = '\0';
    fclose(file);
    name[strcspn(name, "\r\n")] = '\0';
    return name[0] != '\0';
}

int main(void) {
    if (access(FAIL_FILE, F_OK) == 0) {
        fprintf(stderr, "GUI hook PoC: intentional failure requested by %s\n", FAIL_FILE);
        return 42;
    }

    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { perror("GUI hook PoC: open /dev/fb0"); return 1; }

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 ||
        ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
        perror("GUI hook PoC: framebuffer info"); close(fd); return 1;
    }
    if (var.bits_per_pixel != 32 || fix.line_length == 0 || fix.smem_len == 0) {
        fprintf(stderr, "GUI hook PoC: expected a 32-bit framebuffer, got %u bpp\n",
                var.bits_per_pixel);
        close(fd); return 1;
    }

    uint8_t *fb = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("GUI hook PoC: mmap"); close(fd); return 1; }

    char initial_core[128] = "";
    const int watch_core = read_core(initial_core, sizeof(initial_core));
    printf("GUI hook PoC running on %ux%u framebuffer; core=%s; press and hold MENU to return.\n",
           var.xres, var.yres, watch_core ? initial_core : "unknown");
    fflush(stdout);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = stop;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    int scale_x = (int)var.xres / 240;
    int scale_y = (int)var.yres / 160;
    const int scale = scale_x < scale_y ? (scale_x > 0 ? scale_x : 1)
                                        : (scale_y > 0 ? scale_y : 1);
    const int panel_w = (int)var.xres * 3 / 5;
    const int panel_h = (int)var.yres / 4;
    const int left = ((int)var.xres - panel_w) / 2;
    const int top = ((int)var.yres - panel_h) / 2;
    const uint32_t navy = rgb(&var, 18, 26, 48);
    const uint32_t blue = rgb(&var, 55, 140, 255);
    const uint32_t white = rgb(&var, 245, 248, 255);
    const uint32_t dark = rgb(&var, 35, 48, 78);
    const int bar_w = panel_w - 40 * scale;
    const int bar_x = left + 20 * scale;
    const int bar_y = top + panel_h - 12 * scale;
    const int label_w = 11 * 6 * scale - scale;
    const int label_x = ((int)var.xres - label_w) / 2;
    const int label_y = top + (panel_h - 7 * scale) / 2 - 8 * scale;

    int frame = 0;
    while (running) {
        if (watch_core) {
            char core[128];
            if (read_core(core, sizeof(core)) && strcmp(core, initial_core) != 0) {
                printf("GUI hook PoC: core changed from %s to %s; exiting cleanly.\n",
                       initial_core, core);
                break;
            }
        }
        // Redraw the static panel as well as the bar: MiSTer and fbcon share this buffer.
        rect(fb, &fix, &var, left, top, panel_w, panel_h, navy);
        rect(fb, &fix, &var, left, top, panel_w, 5 * scale, blue);
        label(fb, &fix, &var, label_x, label_y, scale, white);
        const int filled = bar_w * (frame % 40) / 39;
        rect(fb, &fix, &var, bar_x, bar_y, bar_w, 8 * scale, dark);
        rect(fb, &fix, &var, bar_x, bar_y, filled, 8 * scale, blue);
        ++frame;
        struct timespec delay = {0, 100000000};
        nanosleep(&delay, NULL);
    }

    munmap(fb, fix.smem_len);
    close(fd);
    return 0;
}
