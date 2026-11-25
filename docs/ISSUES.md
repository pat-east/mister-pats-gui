# Known issues and investigations

Open problems that are not solved yet, with the evidence collected so far. When one of them
shows up again, start from the entry here instead of from scratch.

## Home is blank after a cold start

**Status:** open. Cause strongly suspected, not proven. Diagnostic logging is in place; no fix
yet.

### Symptom

After the MiSTer has been off for a while and is started cold, the Home tab shows the top bar,
the "Home" title, the search field and the button hints, but no tiles and no row titles. The
GUI is running and responsive. Any controller input makes it repaint, and everything appears.
Restarting the MiSTer over SSH (a warm start) does not show it. The Home rows are not empty:
the log reports the library and 204 favourites as loaded.

### Evidence (2026-10-13, v0.8.0)

- A read of `/dev/fb0` (no signal sent to the GUI, nothing touched) shows the content area as
  exactly `0x00000000` (alpha 0). The GUI never writes alpha 0, so the area was cleared from
  outside after our last full present. It was not drawn wrongly. The top bar, title, search
  field and hints are intact because the frame loop redraws them every frame, while the content
  area is only redrawn when something changes or a full redraw is requested.
- `dmesg` shows the `MiSTer_fb` kernel module applying a mode (three lines each time) at kernel
  uptime 3.2 s, 3.4 s, 10.2 s and **35.8 s**. The GUI starts at about 15 s, enters its frame loop
  at about 23 s and does its last full redraws at about 23 s and 25 s (the delayed Home repaint
  two seconds after the loop starts). The mode was therefore applied again about ten seconds
  after our last repaint. (Kernel uptime is the GUI log's wall-clock time minus the boot time,
  21:56:40 here.)
- `/sys/module/MiSTer_fb/parameters/res_count` was 8 afterwards (2 + 3 + 3), matching the
  dmesg lines: it counts each applied mode. The `mode` parameter file also had a later write
  time (about 47 s) without a matching count change; that is unexplained.
- Main_MiSTer writes that parameter in `fb_write_module_params()` (`video.cpp`), called from
  `video_fb_enable()` and `video_fb_config()`. Those run on a video mode change and again
  whenever the OSD overlay is switched back on.
- The GUI runs on tty2 (`stdin`/`stdout` are `/dev/tty2`, the console guard holds `/dev/tty0`),
  started by the patched Main_MiSTer through agetty.

### Reproduced (2026-10-25, v0.8.4)

The same blank Home screen was captured on a live MiSTer before restarting it:

- `debug.log` records the GUI entering its frame loop at kernel uptime 13.9 s, a full redraw at
  13.9 s, and the delayed full redraw at 16.0 s.
- At kernel uptime 165.5 s, the framebuffer diagnostic records `res_count 2 -> 5` while Home
  (tab 1) is active. `dmesg` has three matching `MiSTer_fb` mode-application lines at uptime
  164.820–164.835 s.
- The system log in `/tmp/messages` has those same three kernel lines and no recorded userspace
  event at that time. Its next event is an Xbox 360 receiver input-device registration at 178.9 s;
  the available logs do not identify what requested the mode application.
- A read-only `/dev/fb0` capture at uptime 224.7 s showed alpha 0 across the entire main content
  area (y=260–1007); the top bar, Home title, search field and footer remained visible.

This confirms that a `MiSTer_fb` mode reapplication happened after the last full redraw and that
the main content area was subsequently cleared. It confirms the observed failure mechanism; the
exact cause of the late mode reapplication is still unknown.

### Hypothesis

Main_MiSTer re-applies the framebuffer mode on a cold start well after the GUI has painted
(for example after the display handshake finishes, or when the OSD overlay is re-enabled), and
the kernel module clears the framebuffer memory when it does. The GUI's only safeguard is one
extra full repaint two seconds after the frame loop starts, which is far too early on a cold
boot. After the clear, nothing asks the Home screen to repaint its body until the next input.

Not established: that the module really clears the memory on a mode write (its source is not
in this repository), what triggers the write at 35.8 s, and why a warm start avoids it.

### Rejected approach

Comparing sampled framebuffer pixels with the canvas about once a second and redrawing on a
mismatch. It was tried before and caused constant redraws and flicker.

### Recovery measure (implemented)

The GUI reads `res_count` every two seconds (a sysfs read of kernel memory, with no storage
access). When a valid previously observed count changes, it requests one full redraw and saves
the new count. Further checks with the same count do not request another redraw. This also covers
later mode changes, such as returning from a core.

### Diagnostic logging (implemented)

Counter changes are logged in `/media/fat/mister-pat/logs/debug.log`:

- `framebuffer: MiSTer_fb res_count A -> B (kernel uptime T s, tab=N)` every time the counter
  differs from the last read (checked every 2 s, at most 20 lines per run). The first line shows
  the value at startup. `tab` is the numeric `Tab` of the screen being shown.
- `run: full redraw begins (..., kernel uptime T s)` now carries the same uptime clock.

`T` is the clock of the `[ ... ]` stamps in `dmesg`, so a counter change can be lined up with
the last redraw without guessing at wall-clock offsets.

### What to collect when it happens again

Do not restart the MiSTer first.

1. Read the framebuffer: `ssh root@<device> 'cat /dev/fb0' > fb.raw` (convert with
   `tools/fbshot.py fb.raw fb.png 1920 1080`). Check that the content area is all zero.
2. `ssh root@<device> 'dmesg | grep MiSTer_fb; cat /sys/module/MiSTer_fb/parameters/res_count; cat /proc/uptime'`
3. The tail of `/media/fat/mister-pat/logs/debug.log` (the `res_count` lines and the redraw
   lines with their uptimes).

What decides it: a `res_count` change after the last `full redraw begins` confirms the
hypothesis and the planned measure applies. No change after the last redraw means something
else cleared the memory, and the hypothesis is wrong.

## Menu sounds are switched off (the tick comes much too late)

**Status:** switched off for 0.9.0 (owner decision, 2026-11-24). To be redone in a later release. The Start sound is separate and
still works.

### Symptom

With "Menu sounds" on, the navigation tick (and the tab-change and game-launch sounds under the same switch) arrived long after
the key press. Earlier it also made the input feel sluggish, most clearly on the on-screen keyboard and in the Input test.

### Evidence

- **Sluggish input (fixed in code, kept).** The owner switched Menu sounds off and the GUI was snappy again, so the sounds were the
  main cause. `UiSounds::playWavInBackground` did a double `fork()` of the whole GUI process for every direction key and the
  main thread waited for the first child. `fork()` copies the page tables; the new GUI holds more memory than Classic (second
  canvas, painter caches, image cache), so each key press cost milliseconds on the Cortex-A9, and a held key stacked up `aplay`
  processes. It is now one long-lived worker thread that starts `aplay` with `posix_spawn`, plays at most one sound with one
  waiting, and drops or coalesces further navigation ticks (`tests/ui_sounds_test.cpp`; the cost of the call is about 0.05 ms).
  With that fix the owner found the GUI snappy again.
- **Late tick (not fixed, not diagnosed).** After the fix the tick still came much too late. Suspected, not proven: starting
  `aplay` per sound has a latency of its own (process start, opening the ALSA device, the buffer), far above the
  ~16 ms of a frame; the one-playing-one-waiting rule can delay a tick behind a sound that is still playing; and the key repeat
  of a held direction produces ticks faster than they can be played. The measured line `pats-gui input:` in `debug.log` reports
  the cost of the call (`sounds N calls T ms`), not the time until the sound is audible.

### What is in the code now

- `Preferences::menuSoundsEnabled()` returns `false` (the saved value and the setter stay). Because of that, `App::dispatch`
  (navigation), `App::selectTab` (tab change) and the game-launch sound do not play.
- The "Menu sounds" row is removed from the new GUI's Settings (Interface). Classic's row still exists, without effect.
- `UiSounds` (worker thread, `posix_spawn`, coalescing) and the sound data are unchanged and ready.

### To consider when sounds come back

1. **No process per sound.** Keep one audio output open (the project already has `PcmOutput` for the video sound) and mix or
   write pre-decoded sound buffers into it; do not start `aplay` for a tick.
2. **Measure the latency from key to audible sound**, not the cost of the call (a loopback or a log of the time the buffer is
   handed to the device plus the buffer length). Aim well under 50 ms.
3. **Never block or slow the frame loop**; no `fork()` of the GUI process, no waiting on a child.
4. **Rate limit and coalesce:** a held direction must not queue ticks (one tick per N ms), and a tick must not wait behind a
   longer sound (cut the older one or skip the new one).
5. **One switch, three sounds today:** navigation, tab change and game launch share "Menu sounds"; decide whether they should
   get separate switches. Bring the row back in Settings (Interface) and update the golden pictures.
6. The Start sound, the sounds of the first-run Jump Start and the wizard flows (none exist yet) should use the same output.
