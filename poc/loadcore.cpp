// Loads an .rbf into the FPGA and optionally hands over to the stock MiSTer binary.
//   loadcore <rbf-path> [mgl-path] [--handover]
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include "fpga.h"

static void handover(const char *stock, const char *rbf, const char *mgl) {
    std::printf("handing over to %s\n  argv[1]=%s\n  argv[2]=%s\n", stock, rbf, mgl ? mgl : "(none)");
    std::fflush(stdout);

    // Detached from our session, otherwise it dies with the ssh connection.
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }
    if (pid == 0) {
        setsid();
        if (chdir("/") != 0) perror("chdir /");
        if (mgl) execl(stock, stock, rbf, mgl, (char *)nullptr);
        else     execl(stock, stock, rbf, (char *)nullptr);
        perror("execl");
        _exit(1);
    }
    std::printf("started pid %d\n", (int)pid);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: loadcore <rbf-path> [mgl-path] [--handover]\n");
        return 1;
    }

    const char *rbf = argv[1];
    const char *mgl = nullptr;
    bool do_handover = false;
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--handover")) do_handover = true;
        else mgl = argv[i];
    }

    Fpga fpga;
    if (!fpga.open_mem()) return 1;

    std::printf("before: mode=%d ready=%d\n", fpga.mode(), fpga.ready() ? 1 : 0);
    if (!fpga.ready()) {
        std::printf("FPGA not in user mode - refusing to program\n");
        return 1;
    }

    int ret = fpga.load_rbf(rbf);
    std::printf("load_rbf returned %d\n", ret);
    std::printf("after:  mode=%d ready=%d\n", fpga.mode(), fpga.ready() ? 1 : 0);

    if (ret) return 1;

    if (do_handover) handover("/media/fat/MiSTer", rbf, mgl);
    else std::printf("no handover requested - the new core has no software driving it yet\n");

    return 0;
}
