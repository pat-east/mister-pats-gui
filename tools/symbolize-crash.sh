#!/bin/sh
# Resolve a crash address against the loader or the exact versioned GUI library.
set -eu
LOG=${1:-/media/fat/mister-pat/logs/crash.log}
DEBUG_DIR=${2:-build}
MAP=${MAP:-$(dirname "$LOG")/module-map.log}
ADDR2LINE=${ADDR2LINE:-${CROSS:-arm-unknown-linux-gnueabihf}-addr2line}
[ -r "$LOG" ] || { echo "cannot read $LOG" >&2; exit 1; }
[ -r "$MAP" ] || { echo "cannot read $MAP; copy it from the same GUI run" >&2; exit 1; }
command -v "$ADDR2LINE" >/dev/null 2>&1 || { echo "missing $ADDR2LINE" >&2; exit 1; }
python3 - "$LOG" "$MAP" "$DEBUG_DIR" "$ADDR2LINE" <<'PY'
import os
import re
import subprocess
import sys

log, module_map, debug_dir, addr2line = sys.argv[1:]
with open(module_map, encoding='utf-8', errors='replace') as source:
    lines = source.readlines()
if not lines or not lines[0].startswith('pid='):
    raise SystemExit('invalid module-map.log')
pid = lines[0].strip()[4:]
segments = []
for line in lines[1:]:
    fields = line.split(maxsplit=5)
    if len(fields) != 6:
        continue
    match = re.fullmatch(r'([0-9a-f]+)-([0-9a-f]+)', fields[0])
    if not match or not fields[5].startswith('/media/fat/mister-pat/'):
        continue
    segments.append((int(match[1], 16), int(match[2], 16), int(fields[2], 16), fields[5].strip()))

def resolve(label, address):
    absolute = int(address, 16)
    segment = next((s for s in segments if s[0] <= absolute < s[1]), None)
    if segment is None:
        print(f'  {label} {address}: no matching loader or GUI mapping')
        return
    start, end, offset, path = segment
    basename = os.path.basename(path)
    debug = os.path.join(debug_dir, basename + '.debug')
    if not os.path.isfile(debug):
        print(f'  {label} {address}: missing {debug}')
        return
    relative = absolute if basename == 'mister-gui' else absolute - start + offset
    print(f'  {label} {address} in {basename} (+0x{relative:x})')
    result = subprocess.run([addr2line, '-f', '-C', '-i', '-e', debug,
                             f'0x{relative:x}'], text=True, capture_output=True)
    print('    ' + result.stdout.strip().replace('\n', '\n    '))

with open(log, encoding='utf-8', errors='replace') as source:
    for line in source:
        if 'fatal ' not in line and 'std::terminate backtrace:' not in line:
            continue
        if f'pid={pid}' not in line:
            continue
        print(line.rstrip())
        if 'fatal ' in line:
            for label in ('pc', 'lr'):
                match = re.search(rf'\b{label}=(0x[0-9a-fA-F]+)', line)
                if match:
                    resolve(label, match[1])
        else:
            addresses = line.split('std::terminate backtrace:', 1)[1].split(' exception=', 1)[0]
            for address in re.findall(r'0x[0-9a-fA-F]+', addresses):
                resolve('backtrace', address)
PY
