#!/bin/sh
# Resolve the program counters in crash.log using the exact matching GUI binary.
set -eu

LOG=${1:-/media/fat/mister-pat/logs/crash.log}
BINARY=${2:-build/mister-gui.debug}
ADDR2LINE=${ADDR2LINE:-${CROSS:-arm-unknown-linux-gnueabihf}-addr2line}

if [ ! -r "$LOG" ]; then
    echo "cannot read crash log: $LOG" >&2
    exit 1
fi
if [ ! -r "$BINARY" ]; then
    echo "cannot read symbolized GUI binary: $BINARY" >&2
    exit 1
fi
if ! command -v "$ADDR2LINE" >/dev/null 2>&1; then
    echo "cannot find $ADDR2LINE; set ADDR2LINE to the target addr2line tool" >&2
    exit 1
fi

symbolize() {
    label=$1
    address=$2
    printf '  %s %s\n' "$label" "$address"
    "$ADDR2LINE" -f -C -i -e "$BINARY" "$address" | sed 's/^/    /'
}

while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        *"fatal "*)
            printf '%s\n' "$line"
            for label in pc lr; do
                value=$(printf '%s\n' "$line" | awk -v key="$label" '{ for (i = 1; i <= NF; ++i) if (index($i, key "=") == 1) { print substr($i, length(key) + 2); exit } }')
                [ -z "$value" ] || symbolize "$label" "$value"
            done
            ;;
        *"std::terminate backtrace:"*)
            printf '%s\n' "$line"
            addresses=${line#*std::terminate backtrace:}
            addresses=${addresses%% exception=*}
            for address in $addresses; do
                case "$address" in
                    0x*) symbolize backtrace "$address" ;;
                esac
            done
            ;;
    esac
done < "$LOG"
