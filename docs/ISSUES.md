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

### Planned measure (not implemented)

Make the kernel's own signal the trigger: read `res_count` every few seconds (a sysfs read of
kernel memory, no storage access) and request one full redraw only when it changes. A redraw
does not change the counter, so this cannot loop or flicker. It would also cover later mode
changes, such as returning from a core.

### Diagnostic logging (implemented, logging only)

The GUI never redraws because of it. Lines in `/media/fat/mister-pat/logs/debug.log`:

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
