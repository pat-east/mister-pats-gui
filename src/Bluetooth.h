#pragma once

#include <string>
#include <sys/types.h>

// Thin wrapper around the same mechanism stock MiSTer's own "Pair Bluetooth device" OSD
// screen uses — see docs/CONTROLLER.md, "Bluetooth pairing", for why this reuses `btpair` rather
// than driving `bluetoothctl` by hand, and for the one thing in here not yet confirmed
// against real hardware (the sysfs adapter check).
class Bluetooth {
public:
    ~Bluetooth() { cancel(); }

    // Cheap, no new dependency: an adapter's own kernel node, rather than linking libbluetooth
    // just to ask `hci_get_route()` the same question. Not every MiSTer has one at all — this
    // feature must not offer Bluetooth pairing where it does not, see docs/CONTROLLER.md.
    static bool adapterPresent();

    // Starts `/usr/sbin/btpair` as a child process, streaming its stdout. False if it could
    // not be started at all (already running, or popen failed).
    bool startScanning();

    bool running() const { return childPid_ > 0; }

    // One line of btpair's own output, or empty if there is nothing new yet. Call repeatedly
    // while running() to drain it — this project's own modal decides how (or whether) to
    // show each line, rather than trusting btpair's own wording to stay stable across MiSTer
    // versions (see docs/CONTROLLER.md on why a device-list diff, not log parsing, is what
    // actually decides "a new controller connected").
    bool nextLine(std::string &line);

    // Signals the process this object itself started, rather than stock's own `killall` by
    // name — safer, since nothing else on the device happens to share btpair's name, but
    // there is no reason to rely on that when a direct handle is available.
    void cancel();

private:
    pid_t childPid_ = -1;
    int readFd_ = -1;
    std::string pending_;
};
