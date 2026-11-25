#pragma once

#include <cstdint>

class UiSounds {
public:
    // Hand a sound to the player and return at once. A single long-lived worker thread starts `aplay` (posix_spawn: no copy of
    // this process's page tables, unlike fork) and feeds it the WAV; the caller never forks, never waits and never blocks.
    // At most one sound is playing and one waits; a navigation tick that finds both busy is dropped, never queued.
    // If aplay or an audio device is unavailable, the application continues silently.
    static void play();
    static void playTabChange();
    static void playNavigation();
    static void playGameLaunch();

    // What the calls cost the caller since the last take (the debug log shows it): number of calls, total and worst
    // milliseconds spent inside play*(), and how many sounds were dropped. Resets the counters.
    struct Stats { int calls = 0; int dropped = 0; double totalMs = 0; double maxMs = 0; };
    static Stats takeStats();
};
