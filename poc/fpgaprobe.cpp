// Read-only probe of the Cyclone V FPGA manager registers.
// Validates the /dev/mem mapping and register layout before anything is written.
//
// Register layout and address constants follow MiSTer-devel/Main_MiSTer
// (fpga_io.cpp, fpga_manager.h, fpga_base_addr_ac5.h), which derive from U-Boot. GPL.
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define FPGA_REG_BASE 0xFF000000
#define FPGA_REG_SIZE 0x01000000

#define SOCFPGA_FPGAMGRREGS_ADDRESS 0xff706000
#define SOCFPGA_FPGAMGRDATA_ADDRESS 0xffb90000
#define SOCFPGA_L3REGS_ADDRESS      0xff800000

#define FPGAMGRREGS_STAT_MODE_MASK 0x7
#define FPGAMGRREGS_STAT_MSEL_MASK 0xf8
#define FPGAMGRREGS_STAT_MSEL_LSB  3

static volatile uint32_t *map_base = nullptr;

static inline volatile uint32_t *reg(uint32_t addr) {
    return &map_base[(addr & 0xFFFFFF) >> 2];
}

static const char *mode_name(uint32_t mode) {
    switch (mode) {
    case 0x0: return "FPGA_OFF";
    case 0x1: return "RESET_PHASE";
    case 0x2: return "CFG_PHASE";
    case 0x3: return "INIT_PHASE";
    case 0x4: return "USER_MODE";
    default:  return "UNKNOWN";
    }
}

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    void *m = mmap(nullptr, FPGA_REG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FPGA_REG_BASE);
    if (m == MAP_FAILED) { perror("mmap peripheral region"); close(fd); return 1; }
    map_base = (volatile uint32_t *)m;

    std::printf("mapped 0x%08X len 0x%08X\n\n", FPGA_REG_BASE, FPGA_REG_SIZE);

    const uint32_t stat  = *reg(SOCFPGA_FPGAMGRREGS_ADDRESS + 0x00);
    const uint32_t ctrl  = *reg(SOCFPGA_FPGAMGRREGS_ADDRESS + 0x04);
    const uint32_t gpo   = *reg(SOCFPGA_FPGAMGRREGS_ADDRESS + 0x10);
    const uint32_t gpi   = *reg(SOCFPGA_FPGAMGRREGS_ADDRESS + 0x14);
    const uint32_t porta = *reg(SOCFPGA_FPGAMGRREGS_ADDRESS + 0x850);

    const uint32_t mode = stat & FPGAMGRREGS_STAT_MODE_MASK;
    const uint32_t msel = (stat & FPGAMGRREGS_STAT_MSEL_MASK) >> FPGAMGRREGS_STAT_MSEL_LSB;

    std::printf("stat           = 0x%08X\n", stat);
    std::printf("  mode         = %u (%s)\n", mode, mode_name(mode));
    std::printf("  msel         = 0x%02X\n", msel);
    std::printf("ctrl           = 0x%08X\n", ctrl);
    std::printf("gpo            = 0x%08X\n", gpo);
    std::printf("gpi            = 0x%08X\n", gpi);
    std::printf("  gpi[31]      = %u   (0 = FPGA ready, per is_fpga_ready)\n", (gpi >> 31) & 1);
    std::printf("gpio_ext_porta = 0x%08X\n", porta);
    std::printf("  initdone bit = %u\n", (porta >> 2) & 1);

    std::printf("\nexpectation: mode 4 (USER_MODE) with a core running, gpi[31] = 0\n");
    std::printf("verdict: %s\n",
                (mode == 0x4 && ((gpi >> 31) & 1) == 0)
                    ? "PLAUSIBLE - mapping and layout look correct"
                    : "UNEXPECTED - do not write anything, re-check the layout");

    munmap(m, FPGA_REG_SIZE);
    close(fd);
    return 0;
}
