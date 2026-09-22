// PoC 3: "Press Start to Play" screen that picks a random game and launches it.
//   startscreen [NES|SNES] [--launch] [--timeout N]
// Without --launch it only writes the MGL, so the MGL format can be checked without
// reconfiguring the FPGA.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <string>
#include <vector>

#include "gfx.h"
#include "zip.h"
#include "fpga.h"

struct System {
    const char *name;
    const char *rbf;                      // as it appears in <rbf>, directory plus prefix
    std::vector<std::string> dirs;
    std::vector<std::string> rom_exts;    // also used to find the ROM inside an archive
    int index;                            // file slot in the core menu
};

static const System SYSTEMS[] = {
    {"NES",  "_Console/NES",  {"/media/usb0/games/NES",  "/media/fat/games/NES"},  {".nes"},          1},
    {"SNES", "_Console/SNES", {"/media/usb0/games/SNES", "/media/fat/games/SNES"}, {".sfc", ".smc"},  1},
};

static const char *MGL_PATH = "/tmp/poc_random.mgl";

static bool ends_with(const std::string &s, const std::string &suffix) {
    return s.size() > suffix.size() &&
           !strcasecmp(s.c_str() + s.size() - suffix.size(), suffix.c_str());
}

static std::vector<std::string> scan_games(const System &sys) {
    std::vector<std::string> out;
    for (const std::string &dir : sys.dirs) {
        DIR *d = opendir(dir.c_str());
        if (!d) continue;
        while (dirent *e = readdir(d)) {
            if (e->d_name[0] == '.') continue;
            std::string name = e->d_name;

            bool match = ends_with(name, ".zip");
            for (const std::string &ext : sys.rom_exts)
                if (ends_with(name, ext)) match = true;

            if (match) out.push_back(dir + "/" + name);
        }
        closedir(d);
    }
    return out;
}

static std::string display_name(const std::string &path) {
    size_t slash = path.find_last_of('/');
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    return (dot == std::string::npos) ? base : base.substr(0, dot);
}

// ConsoleMode's own MGLs address ROMs relative to the core's home directory, climbing to the
// root rather than using an absolute path. Mirrored here because that form is known to work.
static std::string to_mgl_path(const std::string &abs) {
    return "../../../../.." + abs;
}

static bool write_mgl(const System &sys, const std::string &game, std::string *resolved) {
    std::string rom = game;

    if (ends_with(game, ".zip")) {
        std::string inner = zip::find_rom(game.c_str(), sys.rom_exts);
        if (inner.empty()) {
            std::printf("no ROM with a known extension inside %s\n", game.c_str());
            return false;
        }
        rom = game + "/" + inner;
    }

    FILE *f = fopen(MGL_PATH, "w");
    if (!f) { perror(MGL_PATH); return false; }
    std::fprintf(f,
                 "<mistergamedescription>\n"
                 "\t<rbf>%s</rbf>\n"
                 "\t<file delay=\"2\" type=\"f\" index=\"%d\" path=\"%s\"/>\n"
                 "</mistergamedescription>\n",
                 sys.rbf, sys.index, to_mgl_path(rom).c_str());
    fclose(f);

    *resolved = rom;
    return true;
}

struct Input {
    std::vector<int> fds;
    bool opened[32] = {};

    // Wireless pads sleep and reappear as fresh nodes, so scanning once is not enough.
    void rescan() {
        for (int i = 0; i < 32; ++i) {
            if (opened[i]) continue;
            char path[64];
            std::snprintf(path, sizeof(path), "/dev/input/event%d", i);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;
            ioctl(fd, EVIOCGRAB, 1);
            opened[i] = true;
            fds.push_back(fd);
        }
    }

    int poll_event(int timeout_ms) {
        std::vector<pollfd> pfds;
        for (int fd : fds) pfds.push_back({fd, POLLIN, 0});
        if (pfds.empty()) return 0;
        if (poll(pfds.data(), pfds.size(), timeout_ms) <= 0) return 0;

        int result = 0;
        for (size_t i = 0; i < pfds.size(); ++i) {
            if (!(pfds[i].revents & POLLIN)) continue;
            input_event ev{};
            while (read(pfds[i].fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type != EV_KEY || ev.value != 1) continue;
                if (ev.code == BTN_START || ev.code == BTN_A ||
                    ev.code == KEY_ENTER || ev.code == KEY_SPACE) result = 1;
                if (ev.code == KEY_ESC || ev.code == BTN_SELECT) result = 2;
            }
        }
        return result;
    }

    void release() {
        for (int fd : fds) { ioctl(fd, EVIOCGRAB, 0); close(fd); }
        fds.clear();
    }
};

static void launch(const System &sys, const std::string &rbf_dir_prefix) {
    // Resolve the concrete .rbf: the MGL form is "dir/prefix", the file carries a datecode.
    std::string dir = "/media/fat/" + rbf_dir_prefix.substr(0, rbf_dir_prefix.find('/'));
    std::string prefix = rbf_dir_prefix.substr(rbf_dir_prefix.find('/') + 1);

    std::string best;
    DIR *d = opendir(dir.c_str());
    if (d) {
        while (dirent *e = readdir(d)) {
            std::string name = e->d_name;
            if (!ends_with(name, ".rbf")) continue;
            if (name.compare(0, prefix.size(), prefix) != 0) continue;
            char sep = name[prefix.size()];
            if (sep != '_' && sep != '.') continue;
            if (name > best) best = name;
        }
        closedir(d);
    }
    if (best.empty()) { std::printf("no .rbf matching %s in %s\n", prefix.c_str(), dir.c_str()); return; }

    std::string rbf = dir + "/" + best;
    std::printf("core: %s\n", rbf.c_str());

    Fpga fpga;
    if (!fpga.open_mem()) return;
    if (!fpga.ready()) { std::printf("FPGA not in user mode, aborting\n"); return; }

    int ret = fpga.load_rbf(rbf.c_str());
    std::printf("load_rbf returned %d\n", ret);
    if (ret) return;

    std::fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        if (chdir("/") != 0) perror("chdir /");
        execl("/media/fat/MiSTer", "MiSTer", rbf.c_str(), MGL_PATH, (char *)nullptr);
        perror("execl");
        _exit(1);
    }
    std::printf("handed over, pid %d\n", (int)pid);
    (void)sys;
}

int main(int argc, char **argv) {
    const System *sys = &SYSTEMS[0];
    bool do_launch = false;
    bool auto_pick = false;
    int timeout_s = 60;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--launch")) do_launch = true;
        else if (!std::strcmp(argv[i], "--auto")) auto_pick = true;
        else if (!std::strcmp(argv[i], "--timeout") && i + 1 < argc) timeout_s = atoi(argv[++i]);
        else {
            for (const System &s : SYSTEMS)
                if (!strcasecmp(argv[i], s.name)) sys = &s;
        }
    }

    std::srand((unsigned)time(nullptr));
    std::printf("system: %s  launch=%d\n", sys->name, do_launch ? 1 : 0);

    std::vector<std::string> games = scan_games(*sys);
    std::printf("found %zu games\n", games.size());

    if (auto_pick) {
        if (games.empty()) { std::printf("nothing to pick\n"); return 1; }
        std::string pick = games[std::rand() % games.size()], resolved;
        std::printf("chosen: %s\n", pick.c_str());
        if (!write_mgl(*sys, pick, &resolved)) return 1;
        std::printf("rom in mgl: %s\n", resolved.c_str());
        if (do_launch) launch(*sys, sys->rbf);
        return 0;
    }

    Gfx gfx;
    if (!gfx.open("/dev/fb0")) return 1;

    Input input;
    input.rescan();

    const uint32_t bg     = gfx.rgb(0x0b, 0x0d, 0x14);
    const uint32_t accent = gfx.rgb(0x4c, 0x8d, 0xff);
    const uint32_t white  = gfx.rgb(0xff, 0xff, 0xff);
    const uint32_t dim    = gfx.rgb(0x9a, 0xa4, 0xc0);

    const int w = gfx.width(), h = gfx.height();
    const int s_title = w / 320;
    const int s_body  = w / 640;

    std::string chosen, resolved;
    const int max_frames = timeout_s * 30;

    for (int frame = 0; frame < max_frames; ++frame) {
        if (frame % 30 == 0) input.rescan();

        int action = input.poll_event(33);
        if (action == 2) break;

        if (action == 1 && !games.empty() && chosen.empty()) {
            chosen = games[std::rand() % games.size()];
            std::printf("chosen: %s\n", chosen.c_str());
            if (write_mgl(*sys, chosen, &resolved)) {
                std::printf("rom in mgl: %s\n", resolved.c_str());
                if (do_launch) {
                    input.release();
                    gfx.clear(bg);
                    gfx.text_centered(h / 2, "LOADING CORE", s_title, white);
                    gfx.present();
                    launch(*sys, sys->rbf);
                    return 0;
                }
            } else {
                chosen.clear();
            }
        }

        gfx.clear(bg);
        gfx.rect(0, 0, w, 10, accent);
        gfx.rect(0, h - 10, w, 10, accent);

        if (chosen.empty()) {
            uint32_t col = (frame % 60 < 40) ? white : dim;
            gfx.text_centered(h / 2 - 8 * s_title, "PRESS START TO PLAY", s_title, col);

            char sub[128];
            std::snprintf(sub, sizeof(sub), "%s - %zu GAMES", sys->name, games.size());
            gfx.text_centered(h / 2 + 6 * s_title, sub, s_body, dim);
        } else {
            gfx.text_centered(h / 2 - 12 * s_title, "SELECTED", s_title, white);
            std::string name = display_name(chosen);
            if (name.size() > 46) name = name.substr(0, 46);
            gfx.text_centered(h / 2 + 2 * s_title, name.c_str(), s_body, accent);
            gfx.text_centered(h / 2 + 2 * s_title + 16 * s_body, "MGL WRITTEN", s_body, dim);
        }

        gfx.present();
    }

    std::printf("done\n");
    return 0;
}
