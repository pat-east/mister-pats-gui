#!/bin/sh
# Convert only the icon sources listed in assets/tab-icons/manifest.tsv into alpha BMPs.
set -eu
cd "$(dirname "$0")/.."
command -v magick >/dev/null 2>&1 || { echo 'ImageMagick (magick) is required' >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo 'Python 3 is required' >&2; exit 1; }

while IFS="$(printf '\t')" read -r output source; do
    case "$output" in ''|'#'*) continue ;; esac
    case "$output:$source" in *[!A-Za-z0-9._:-]*|*/*) echo "invalid manifest row: $output $source" >&2; exit 1 ;; esac
    case "$output" in *.bmp) ;; *) echo "invalid output filename: $output" >&2; exit 1 ;; esac
    case "$source" in *.svg) ;; *) echo "invalid source filename: $source" >&2; exit 1 ;; esac
    [ -s "assets/tab-icons/source/$source" ] || { echo "missing SVG: $source" >&2; exit 1; }

    temp_base="build/tab-icons/${output%.bmp}"
    mkdir -p build/tab-icons
    sed 's/fill="currentColor"/fill="#ffffff"/' \
        "assets/tab-icons/source/$source" > "$temp_base.svg"
    magick -background none -density 384 "$temp_base.svg" -resize 26x26 \
        -gravity center -extent 26x26 -depth 8 "RGBA:$temp_base.rgba"
    python3 tools/rgba-to-bmp.py "$temp_base.rgba" "assets/tab-icons/$output" 26 26
done < assets/tab-icons/manifest.tsv

rm -rf build/tab-icons
