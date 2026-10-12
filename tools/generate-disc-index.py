#!/usr/bin/env python3
import re
import sys
from pathlib import Path

NAME = re.compile(r'^\s+name "((?:[^"\\]|\\.)*)"\s*$')
SERIAL = re.compile(r'^\s+serial "((?:[^"\\]|\\.)*)"\s*$')

def entries(path):
    title = serial = None
    with open(path, encoding='utf-8', errors='replace') as source:
        for line in source:
            if line.startswith('game ('):
                title = serial = None
            elif line.startswith(')'):
                if title and serial:
                    yield title, serial
                title = serial = None
            else:
                match = NAME.match(line)
                if match and title is None:
                    title = match.group(1).replace('\\"', '"').replace('\\\\', '\\')
                match = SERIAL.match(line)
                if match and serial is None:
                    serial = match.group(1)

def main():
    psx, saturn, out = map(Path, sys.argv[1:4])
    rows = set()
    for title, serial in entries(psx):
        serial = serial.upper().replace('_', '-')
        rows.add(('PSX', serial, title))
    for title, serial in entries(saturn):
        rows.add(('SATURN', serial.upper(), title))
    with out.open('w', encoding='utf-8', newline='\n') as target:
        target.write('# Adapted from libretro/libretro-database metadat/redump/Sony - PlayStation.dat and Sega - Saturn.dat.\n')
        target.write('# Source data: Redump > TOSEC; Copyright remains with source contributors; CC BY-SA 4.0.\n')
        target.write('# https://github.com/libretro/libretro-database | https://creativecommons.org/licenses/by-sa/4.0/\n')
        for system, serial, title in sorted(rows, key=lambda row: (row[0], row[1], row[2])):
            target.write(f'{system}\t{serial}\t{title}\n')
    print(f'{len(rows)} entries -> {out} ({out.stat().st_size} bytes)')

if __name__ == '__main__':
    main()
