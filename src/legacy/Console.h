#pragma once

// Puts the virtual console into graphics mode for as long as the application runs.
//
// Without this the kernel keeps drawing the text console — cursor, boot messages, and in
// MiSTer's script context the "Press any key to continue" prompt — straight into the same
// framebuffer we are painting, which looks like a crash or like foreign output bleeding in.
class ConsoleGuard {
public:
    ~ConsoleGuard();

    // Safe to call when no console is available; the guard then simply does nothing.
    void acquire();
    void release();

private:
    int fd_ = -1;
    int previousMode_ = 0;
    bool changed_ = false;
};
