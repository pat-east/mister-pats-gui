#pragma once

class UiSounds {
public:
    // Start playback in the background. If aplay or an audio device is unavailable, the
    // application continues silently.
    static void play();
    static void playTabChange();
    static void playNavigation();
    static void playGameLaunch();
};
