#!/bin/bash
# Golden images and layout rules of the new GUI's Settings pages (and the main tabs).
#
#   tests/golden.sh              render everything, compare with tests/golden/, run the layout rules; exit 1 on a changed or missing
#                                picture, 4 on a NEW layout finding (known ones live in tests/layout-known.txt)
#   tests/golden.sh --update     accept the fresh renders as the new golden set (and the current layout findings as known)
#   tests/golden.sh --determinism   render twice and compare byte-wise
#   tests/golden.sh --layout     only the layout rules, over the golden cases plus pseudo-language and text 130 % of every page
#   tests/golden.sh --only TEXT  only the cases whose name contains TEXT
#
# Needs build/app-render (make -C src/app). Diff percentages and diff images need ImageMagick (`magick`), which is optional;
# without it changed pictures are reported without a percentage. Diff images go to $GOLDEN_TMP (default: a fresh temp folder).
# See src/app/tests/README.md.

set -u
here="$(cd "$(dirname "$0")" && pwd)"
app="$here/.."
bin="$app/build/app-render"
gold="$here/golden"
known="$here/layout-known.txt"
mode=check; only=""
while [ $# -gt 0 ]; do
    case "$1" in
        --update) mode=update ;;
        --determinism) mode=determinism ;;
        --layout) mode=layout ;;
        --only) shift; only="${1:-}" ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
    shift
done
[ -x "$bin" ] || { echo "no $bin: run make -C src/app first" >&2; exit 2; }

work="${GOLDEN_TMP:-$(mktemp -d "${TMPDIR:-/tmp}/golden.XXXXXX")}"
mkdir -p "$work/new" "$work/diff" "$work/out"

# ---- the cases: "name|arguments of app-render" ----------------------------------------------------------------------------------
# Every Settings page id (each is a sample state: flows in running, failed, done), the main tabs, and a matrix of the worst
# looks for a representative subset. Names are file names.
pages="$("$bin" --tab settings --list-pages | tr '\n' ' ')"
matrix_pages="system/info standard/choice standard/result.error standard/facts standard/progress library/scan.working library/cores.list system/wifi.networks controllers/wizard updates/confirm system/discinstall.failed standard/viewer"
looks="pseudo text130 light contrast"
lookargs() {
    case "$1" in
        pseudo) echo "--pseudo" ;;
        text130) echo "--look text=130,hint=130" ;;   # Largest text and hint step
        light) echo "--look dark=0" ;;
        contrast) echo "--look contrast=2" ;;
    esac
}

cases="$work/cases.txt"; : > "$cases"
fname() { echo "$1" | tr '/' '_'; }
for t in disc home favorites systems arcade games settings; do echo "tab-$t|--tab $t" >> "$cases"; done
for c in library controllers interface updates system accessibility developer; do echo "menu-$c|--tab settings --category $c" >> "$cases"; done
# the first start (src/app/FirstRun.h): every page of --list-first-run, each with the sample state that makes its page true
first_sets() {
    case "$1" in
        failed) echo "--set library.scan=failed" ;;
        stopped) echo "--set library.scan=cancelled" ;;
        overview|overview.nogames|overview.noassets) echo "--set library.scan=done" ;;
        assets) echo "--set accessibility.font=downloading --set accessibility.auto=0" ;;
        assets.icons) echo "--set system.icons=running --set system.auto=0" ;;
        overview.nocovers) echo "--set library.scan=done --set library.artwork=failed" ;;
    esac
}
first_pages="$("$bin" --list-first-run | tr '\n' ' ')"
for p in $first_pages; do echo "first-$(fname "$p")|--first-run $p $(first_sets "$p")" >> "$cases"; done
for p in $pages; do echo "page-$(fname "$p")|--tab settings --page $p" >> "$cases"; done
# long lists (60 items)
echo "long-library_cores.list-60|--tab settings --page library/cores.list --set library.cores.count=60" >> "$cases"
echo "long-interface_consoles-60|--tab settings --page interface/consoles --set interface.systems=60" >> "$cases"
echo "long-interface_consoles-60-hidden|--tab settings --page interface/consoles --set interface.systems=60 --set interface.hidden=20 --keys down,down,down,down,down,down,down,down,down,down,down,down,down,down" >> "$cases"
for p in $matrix_pages; do
    for l in $looks; do echo "look-$l-$(fname "$p")|--tab settings --page $p $(lookargs "$l")" >> "$cases"; done
done
# layout-only cases (not stored): every page in pseudo-language and at the Largest text step
layoutonly="$work/layoutonly.txt"; : > "$layoutonly"
for p in $pages; do
    echo "lo-pseudo-$(fname "$p")|--tab settings --page $p --pseudo" >> "$layoutonly"
    echo "lo-text130-$(fname "$p")|--tab settings --page $p --look text=130,hint=130" >> "$layoutonly"
done
for p in $first_pages; do
    echo "lo-pseudo-first-$(fname "$p")|--first-run $p $(first_sets "$p") --pseudo" >> "$layoutonly"
    echo "lo-text130-first-$(fname "$p")|--first-run $p $(first_sets "$p") --look text=130,hint=130" >> "$layoutonly"
done
for c in library controllers interface updates system accessibility developer; do
    echo "lo-pseudo-menu-$c|--tab settings --category $c --pseudo" >> "$layoutonly"
    echo "lo-text130-menu-$c|--tab settings --category $c --look text=130,hint=130" >> "$layoutonly"
done
if [ -n "$only" ]; then
    grep -F -- "$only" "$cases" > "$cases.f"; mv "$cases.f" "$cases"
    grep -F -- "$only" "$layoutonly" > "$layoutonly.f"; mv "$layoutonly.f" "$layoutonly"
fi

# ---- render --------------------------------------------------------------------------------------------------------------------
# $1 = case file, $2 = output dir. One app-render per case (they are fast); the layout findings go to $2/<name>.layout.
render_one() {   # name|args  outdir
    local name="${1%%|*}" args="${1#*|}" outdir="$2" log
    log="$outdir/$name.log"
    # shellcheck disable=SC2086
    "$bin" $args --check-layout --suffix "" --out "$outdir/tmp-$name" > "$log" 2>&1; local rc=$?
    local png; png="$(ls "$outdir/tmp-$name"/*.png 2>/dev/null | head -1)"
    if [ -n "$png" ]; then mv "$png" "$outdir/$name.png"; else echo "render failed (rc $rc): $name" >&2; cat "$log" >&2; fi
    rm -rf "$outdir/tmp-$name"
    grep '^layout: ' "$log" | grep -v '^layout: [0-9]* finding' | sed "s|^layout: [^:]*: |layout: $name: |" > "$outdir/$name.layout"
    rm -f "$log"
}
render_all() {   # case file, output dir
    local line
    while IFS= read -r line; do
        render_one "$line" "$2" &
        while [ "$(jobs -r | wc -l)" -ge 8 ]; do sleep 0.05; done
    done < "$1"
    wait
}

if [ "$mode" = determinism ]; then
    mkdir -p "$work/a" "$work/b"
    render_all "$cases" "$work/a"; render_all "$cases" "$work/b"
    bad=0
    for f in "$work/a/"*.png; do
        n="$(basename "$f")"
        if ! cmp -s "$f" "$work/b/$n"; then echo "NOT DETERMINISTIC: $n"; bad=$((bad + 1)); fi
    done
    echo "determinism: $(ls "$work/a/"*.png | wc -l | tr -d ' ') pictures rendered twice, $bad differ"
    [ -z "${GOLDEN_TMP:-}" ] && rm -rf "$work"
    [ "$bad" -eq 0 ]; exit $?
fi

start=$(date +%s)
mkdir -p "$work/new"
render_all "$cases" "$work/new"
if [ "$mode" != update ] || true; then
    mkdir -p "$work/lo"; render_all "$layoutonly" "$work/lo"
fi

# ---- layout findings -----------------------------------------------------------------------------------------------------------
cat "$work"/new/*.layout "$work"/lo/*.layout 2>/dev/null | sort > "$work/layout-all.txt"
# known findings are matched by their text without the case-specific prefix numbers, so they stay stable
sort -u "$known" 2>/dev/null > "$work/known.txt" || : > "$work/known.txt"
comm -23 <(sort -u "$work/layout-all.txt") "$work/known.txt" > "$work/layout-new.txt"
nfind=$(wc -l < "$work/layout-all.txt" | tr -d ' '); nnew=$(wc -l < "$work/layout-new.txt" | tr -d ' ')

if [ "$mode" = layout ]; then
    cat "$work/layout-all.txt"
    echo "layout: $nfind finding(s), $nnew not in layout-known.txt"
    [ "$nnew" -eq 0 ]; rc=$?
    [ -z "${GOLDEN_TMP:-}" ] && rm -rf "$work"; exit $rc
fi

if [ "$mode" = update ]; then
    mkdir -p "$gold"; rm -f "$gold"/*.png
    cp "$work"/new/*.png "$gold"/
    cp "$work/layout-all.txt" "$known"
    echo "golden: updated $(ls "$gold"/*.png | wc -l | tr -d ' ') pictures ($(du -sk "$gold" | cut -f1) kB), $nfind layout finding(s) recorded as known"
    [ -z "${GOLDEN_TMP:-}" ] && rm -rf "$work"; exit 0
fi

# ---- compare -------------------------------------------------------------------------------------------------------------------
changed=0; missing=0; same=0
for f in "$work"/new/*.png; do
    n="$(basename "$f")"
    if [ ! -f "$gold/$n" ]; then echo "NEW (no golden): ${n%.png}"; missing=$((missing + 1)); continue; fi
    if cmp -s "$f" "$gold/$n"; then same=$((same + 1)); continue; fi
    changed=$((changed + 1))
    if command -v magick >/dev/null 2>&1; then
        px=$(magick identify -format '%[fx:w*h]' "$f" 2>/dev/null)
        ae=$(magick compare -metric AE "$gold/$n" "$f" "$work/diff/$n" 2>&1 | awk '{print $1}')
        pct=$(awk -v a="${ae:-0}" -v p="${px:-1}" 'BEGIN{printf "%.3f", 100*a/p}')
        echo "CHANGED: ${n%.png}  $pct % of the pixels differ"
    else
        echo "CHANGED: ${n%.png}  (no ImageMagick: no percentage, no diff image)"
    fi
done
for f in "$gold"/*.png; do
    n="$(basename "$f")"
    if [ -n "$only" ] && ! echo "$n" | grep -qF -- "$only"; then continue; fi
    [ -f "$work/new/$n" ] || { echo "GONE (golden without a case): ${n%.png}"; missing=$((missing + 1)); }
done
echo "golden: $same same, $changed changed, $missing new or gone ($(( $(date +%s) - start )) s)"
[ "$changed" -gt 0 ] && echo "diff images and fresh renders: $work/diff and $work/new (accept with tests/golden.sh --update)"
if [ "$nnew" -gt 0 ]; then
    echo "layout: $nnew NEW finding(s) (of $nfind; known ones are in tests/layout-known.txt):"; cat "$work/layout-new.txt"
else
    echo "layout: no new findings ($nfind known)"
fi
rc=0
[ "$changed" -gt 0 ] || [ "$missing" -gt 0 ] && rc=1
[ "$rc" -eq 0 ] && [ "$nnew" -gt 0 ] && rc=4
[ "$rc" -eq 0 ] && [ -z "${GOLDEN_TMP:-}" ] && rm -rf "$work"
exit $rc
