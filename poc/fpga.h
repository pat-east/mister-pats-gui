// Cyclone V FPGA configuration from Linux userspace: writes an .rbf bitstream through the
// FPGA manager, with the HPS<->FPGA bridges disabled around the transfer.
//
// Ported from MiSTer-devel/Main_MiSTer (fpga_io.cpp, fpga_manager.h, fpga_base_addr_ac5.h),
// which in turn derives from U-Boot's socfpga FPGA manager driver. GPL-2.0+.
#pragma once

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

class Fpga {
public:
    ~Fpga() {
        if (base_ && base_ != MAP_FAILED) munmap((void *)base_, REG_SIZE);
        if (fd_ >= 0) close(fd_);
    }

    bool open_mem() {
        fd_ = ::open("/dev/mem", O_RDWR | O_SYNC);
        if (fd_ < 0) { perror("open /dev/mem"); return false; }
        void *m = mmap(nullptr, REG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, REG_BASE);
        if (m == MAP_FAILED) { perror("mmap peripherals"); return false; }
        base_ = (volatile uint32_t *)m;
        return true;
    }

    int mode() const { return int(rd(FPGAMGR + STAT) & STAT_MODE_MASK); }
    bool ready() const { return mode() == MODE_USERMODE && (rd(FPGAMGR + PORTA) & PORTA_ID); }

    // Mirrors fpga_core_reset(1): clear bits 30/31 of GPO, then assert bit 30.
    // Main_MiSTer tracks GPO in a cached copy because the register is not considered
    // reliable to read back; as a separate process we have no such history and must read it.
    void core_reset() {
        uint32_t gpo = rd(FPGAMGR + GPO) & ~0xC0000000u;
        wr(FPGAMGR + GPO, gpo | 0x40000000u);
    }

    // Loads the bitstream at `path`. Returns 0 on success, negative on error.
    int load_rbf(const char *path) {
        size_t size = 0;
        uint8_t *buf = read_file(path, &size);
        if (!buf) return -100;

        const uint8_t *data = buf;
        size_t len = size;

        // MiSTer wraps some bitstreams in a 16-byte header: magic, then the payload
        // length as a uint32 at offset 12.
        if (size > 16 && !std::memcmp(buf, "MiSTer", 6)) {
            len = *(const uint32_t *)(buf + 12);
            data = buf + 16;
            std::printf("MiSTer header found, payload %zu bytes\n", len);
            if (len > size - 16) { std::printf("header length out of range\n"); std::free(buf); return -101; }
        }

        std::printf("bitstream: %zu bytes\n", len);

        // Order follows fpga_load_rbf(): hold the core before touching the bridges.
        // Skipping this step on the first attempt coincided with the USB bus dropping out.
        core_reset();
        bridges(false);
        int ret = program(data, len);
        if (ret) std::printf("programming failed: %d\n", ret);
        else bridges(true);

        std::free(buf);
        return ret;
    }

private:
    static const uint32_t REG_BASE = 0xFF000000;
    static const uint32_t REG_SIZE = 0x01000000;

    static const uint32_t FPGAMGR  = 0xFF706000;
    static const uint32_t MGRDATA  = 0xFFB90000;
    static const uint32_t NIC301   = 0xFF800000;   // remap at +0x00
    static const uint32_t RSTMGR   = 0xFFD05000;   // brg_mod_reset at +0x1C
    static const uint32_t SYSMGR   = 0xFFD08000;   // fpgaintfgrp_module at +0x28
    static const uint32_t SDR      = 0xFFC20000;   // fpgaportrst at +0x5080

    static const uint32_t STAT = 0x000, CTRL = 0x004, DCLKCNT = 0x008, DCLKSTAT = 0x00C;
    static const uint32_t GPO = 0x010, GPI = 0x014;
    static const uint32_t PORTA_EOI = 0x84C, PORTA = 0x850;

    static const uint32_t STAT_MODE_MASK = 0x7, STAT_MSEL_MASK = 0xF8, STAT_MSEL_LSB = 3;
    static const uint32_t CTRL_CFGWDTH = 0x200, CTRL_AXICFGEN = 0x100;
    static const uint32_t CTRL_NCONFIGPULL = 0x4, CTRL_NCE = 0x2, CTRL_EN = 0x1;
    static const uint32_t CTRL_CDRATIO_LSB = 6;

    static const uint32_t PORTA_CRC = 0x8, PORTA_ID = 0x4, PORTA_CD = 0x2, PORTA_NS = 0x1;

    static const int MODE_RESETPHASE = 1, MODE_CFGPHASE = 2, MODE_INITPHASE = 3, MODE_USERMODE = 4;
    static const uint32_t TIMEOUT = 0x1000000;

    volatile uint32_t *base_ = nullptr;
    int fd_ = -1;

    volatile uint32_t *r(uint32_t addr) const { return &base_[(addr & 0xFFFFFF) >> 2]; }
    uint32_t rd(uint32_t addr) const { return *r(addr); }
    void wr(uint32_t addr, uint32_t v) { *r(addr) = v; }
    void set(uint32_t addr, uint32_t bits) { wr(addr, rd(addr) | bits); }
    void clr(uint32_t addr, uint32_t bits) { wr(addr, rd(addr) & ~bits); }

    static uint8_t *read_file(const char *path, size_t *out_size) {
        int f = ::open(path, O_RDONLY);
        if (f < 0) { perror(path); return nullptr; }
        struct stat st{};
        if (fstat(f, &st) < 0) { perror("fstat"); close(f); return nullptr; }

        // Rounded up so the word-wise transfer never reads past the buffer.
        size_t padded = (size_t(st.st_size) + 3) & ~size_t(3);
        uint8_t *buf = (uint8_t *)std::calloc(1, padded + 16);
        if (!buf) { std::printf("out of memory for %lld bytes\n", (long long)st.st_size); close(f); return nullptr; }

        ssize_t got = read(f, buf, st.st_size);
        close(f);
        if (got != st.st_size) { std::printf("short read on %s\n", path); std::free(buf); return nullptr; }

        *out_size = size_t(st.st_size);
        return buf;
    }

    void set_cd_ratio(uint32_t ratio) {
        wr(FPGAMGR + CTRL, (rd(FPGAMGR + CTRL) & ~(0x3u << CTRL_CDRATIO_LSB)) |
                               ((ratio & 0x3) << CTRL_CDRATIO_LSB));
    }

    int dclkcnt_set(uint32_t cnt) {
        if (rd(FPGAMGR + DCLKSTAT)) wr(FPGAMGR + DCLKSTAT, 0x1);
        wr(FPGAMGR + DCLKCNT, cnt);
        for (uint32_t i = 0; i < TIMEOUT; ++i) {
            if (!rd(FPGAMGR + DCLKSTAT)) continue;
            wr(FPGAMGR + DCLKSTAT, 0x1);
            return 0;
        }
        return -1;
    }

    void bridges(bool enable) {
        if (enable) {
            wr(SDR + 0x5080, 0x00003FFF);
            wr(RSTMGR + 0x1C, 0x00000000);
            wr(NIC301 + 0x00, 0x00000019);
        } else {
            wr(SYSMGR + 0x28, 0);
            wr(SDR + 0x5080, 0);
            wr(RSTMGR + 0x1C, 7);
            wr(NIC301 + 0x00, 1);
        }
    }

    int program_init() {
        uint32_t msel = (rd(FPGAMGR + STAT) & STAT_MSEL_MASK) >> STAT_MSEL_LSB;

        if (msel & 0x8) {
            set(FPGAMGR + CTRL, CTRL_CFGWDTH);
            if ((msel & 0x3) == 0x0)      set_cd_ratio(0); // x1
            else if ((msel & 0x3) == 0x1) set_cd_ratio(2); // x4
            else if ((msel & 0x3) == 0x2) set_cd_ratio(3); // x8
        } else {
            clr(FPGAMGR + CTRL, CTRL_CFGWDTH);
            if ((msel & 0x3) == 0x0)      set_cd_ratio(0); // x1
            else if ((msel & 0x3) == 0x1) set_cd_ratio(1); // x2
            else if ((msel & 0x3) == 0x2) set_cd_ratio(2); // x4
        }

        clr(FPGAMGR + CTRL, CTRL_NCE);
        set(FPGAMGR + CTRL, CTRL_EN);
        set(FPGAMGR + CTRL, CTRL_NCONFIGPULL);

        for (uint32_t i = 0; i < TIMEOUT && mode() != MODE_RESETPHASE; ++i) {}
        if (mode() != MODE_RESETPHASE) return -1;

        clr(FPGAMGR + CTRL, CTRL_NCONFIGPULL);

        for (uint32_t i = 0; i < TIMEOUT && mode() != MODE_CFGPHASE; ++i) {}
        if (mode() != MODE_CFGPHASE) return -2;

        wr(FPGAMGR + PORTA_EOI, 0xFFF);
        set(FPGAMGR + CTRL, CTRL_AXICFGEN);
        return 0;
    }

    void program_write(const uint8_t *data, size_t size) {
        volatile uint32_t *dst = r(MGRDATA);
        const uint32_t *src = (const uint32_t *)data;
        size_t words = (size + 3) / 4;
        for (size_t i = 0; i < words; ++i) *dst = src[i];
    }

    int program_poll_cd() {
        const uint32_t mask = PORTA_NS | PORTA_CD;
        uint32_t reg = rd(FPGAMGR + PORTA);
        if (!(reg & mask)) { std::printf("configuration error (porta=0x%08X)\n", reg); return -3; }
        clr(FPGAMGR + CTRL, CTRL_AXICFGEN);
        return 0;
    }

    int program_poll_initphase() {
        if (dclkcnt_set(0x4)) return -5;
        for (uint32_t i = 0; i < TIMEOUT; ++i) {
            int m = mode();
            if (m == MODE_INITPHASE || m == MODE_USERMODE) return 0;
        }
        return -6;
    }

    int program_poll_usermode() {
        if (dclkcnt_set(0x5000)) return -7;
        for (uint32_t i = 0; i < TIMEOUT && mode() != MODE_USERMODE; ++i) {}
        if (mode() != MODE_USERMODE) return -8;
        clr(FPGAMGR + CTRL, CTRL_EN);
        return 0;
    }

    int program(const uint8_t *data, size_t size) {
        if ((uintptr_t)data & 0x3) { std::printf("bitstream not 32-bit aligned\n"); return -9; }
        int ret = program_init();
        if (ret) return ret;
        program_write(data, size);
        if ((ret = program_poll_cd())) return ret;
        if ((ret = program_poll_initphase())) return ret;
        return program_poll_usermode();
    }
};
