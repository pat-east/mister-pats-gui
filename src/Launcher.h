#pragma once

#include <string>

struct Game;
struct GameSystem;

// Starts cores and games by handing an MGL to the running MiSTer binary through its command
// FIFO. Delegating keeps core loading, ROM mounting and the in-game OSD in proven code.
class Launcher {
public:
    // Builds and prints the MGL but keeps it to itself. Which menu slot a core expects can
    // only be read off its Verilog source, so being able to check the result without seizing
    // the television is what makes that mapping testable at all.
    void setDryRun(bool dryRun) { dryRun_ = dryRun; }

    bool launchGame(const GameSystem &system, const Game &game);
    bool launchCore(const GameSystem &system);

    const std::string &lastError() const { return error_; }
    const std::string &lastMgl() const { return mgl_; }

    static constexpr const char *kMglPath = "/tmp/mister-gui.mgl";
    static constexpr const char *kCommandFifo = "/dev/MiSTer_cmd";

private:
    // CD games live as a folder of tracks. The library points at the folder; the core has to
    // be handed the disc file inside it. Returns `path` unchanged for ordinary ROMs.
    static std::string resolveDisc(const std::string &path);

    bool writeMgl(const std::string &core, const std::string &romPath, int fileIndex,
                  char fileType);
    bool sendCommand(const std::string &command);

    std::string error_;
    std::string mgl_;
    bool dryRun_ = false;
};
