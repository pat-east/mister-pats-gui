#!/bin/sh
# Starts the new GUI with the framebuffer to itself, then puts everything back.
#
# ConsoleMode's renderer is *suspended*, not killed. Killing it ends the MiSTer script
# session it runs in, and the main binary then switches the framebuffer overlay back off —
# which leaves our GUI running but invisible. Suspending keeps the overlay up.

renderer_pid() {
    ps -o pid,args 2>/dev/null | grep '[C]onsoleMode_arm' | awk '{print $1}' | head -1
}

RENDERER=$(renderer_pid)

if [ -n "$RENDERER" ]; then
    echo "suspending ConsoleMode renderer (pid $RENDERER)"
    kill -STOP "$RENDERER"
fi

resume() {
    if [ -n "$RENDERER" ] && kill -0 "$RENDERER" 2>/dev/null; then
        echo "resuming ConsoleMode renderer"
        kill -CONT "$RENDERER"
    fi
}
trap resume EXIT INT TERM

/media/fat/mister-pat/mister-gui "$@"
