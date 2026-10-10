#!/bin/sh
# Standalone MiSTer installer. No release data is evaluated as shell code.
set -eu
umask 077
ROOT=/media/fat/mister-pat
LINK=$ROOT/mister-pats-gui.so
API=https://api.github.com/repos/pat-east/mister-pats-gui/releases/latest
BASE=https://github.com/pat-east/mister-pats-gui/releases/download
MODE=${1:-install}
case "$MODE" in install|--migrate-legacy|--rollback) :;; *) echo "unknown option: $MODE" >&2; exit 2;; esac
[ "$#" -le 1 ] || { echo 'too many arguments' >&2; exit 2; }
fail() { echo "mister-pat update: $*" >&2; exit 1; }
for tool in sha256sum od mktemp readlink awk; do command -v "$tool" >/dev/null 2>&1 || fail "missing $tool"; done
[ -w /media/fat ] || fail '/media/fat is not writable'
[ ! -e "$ROOT/.development-build" ] && [ ! -L "$ROOT/.development-build" ] || fail 'development build active; use tools/deploy.sh --restore-release'
mkdir -p "$ROOT" || fail "cannot create $ROOT"
[ -d "$ROOT" ] && [ ! -L "$ROOT" ] || fail "unsafe installation root: $ROOT"
LOCK=$ROOT/.update-lock
OWN_LOCK=0
STAGE=
UNDO=
ROLLBACK_DONE=0
cleanup() {
    status=$?
    keep_stage=0
    if [ "$MODE" = --rollback ] && [ -n "$UNDO" ] && [ "$ROLLBACK_DONE" -eq 0 ]; then
        for name in MiSTer_gui mister-gui; do
            if cp "$UNDO/$name" "$ROOT/$name.undo-$$" &&
               [ "$(sha "$ROOT/$name.undo-$$")" = "$(sha "$UNDO/$name")" ] &&
               mv -f "$ROOT/$name.undo-$$" "$ROOT/$name"; then :
            else echo "manual repair required: $ROOT/$name $UNDO/$name" >&2; keep_stage=1; fi
        done
        sync
    fi
    if [ -n "$STAGE" ] && [ "$keep_stage" -eq 0 ]; then rm -rf "$STAGE"; fi
    if [ "$OWN_LOCK" -eq 1 ]; then rm -f "$LOCK/owner"; rmdir "$LOCK" || :; fi
    exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
boot_id() { cat /proc/sys/kernel/random/boot_id; }
acquire_lock() {
    boot=$(boot_id) || fail 'cannot read boot ID'
    [ -n "$boot" ] || fail 'empty boot ID'
    attempt=0
    while [ "$attempt" -lt 2 ]; do
        if mkdir "$LOCK" 2>/dev/null; then
            OWN_LOCK=1
            printf 'pid=%s\nboot=%s\n' "$$" "$boot" > "$LOCK/owner" || fail 'cannot write lock owner'
            return
        fi
        [ -d "$LOCK" ] && [ ! -L "$LOCK" ] || fail "unsafe update lock: $LOCK"
        [ -r "$LOCK/owner" ] || fail "unreadable update lock; inspect $LOCK"
        owner=$(cat "$LOCK/owner")
        pid=$(printf '%s\n' "$owner" | sed -n '1s/^pid=\([0-9][0-9]*\)$/\1/p')
        oldboot=$(printf '%s\n' "$owner" | sed -n '2s/^boot=\([0-9a-f-][0-9a-f-]*\)$/\1/p')
        [ -n "$pid" ] && [ "$pid" -gt 0 ] && [ "${#oldboot}" -eq 36 ] || fail "malformed update lock; inspect $LOCK"
        if [ "$oldboot" = "$boot" ] && kill -0 "$pid" 2>/dev/null; then fail 'update already running'; fi
        rm -f "$LOCK/owner" && rmdir "$LOCK" || fail "cannot remove stale lock $LOCK"
        attempt=$((attempt+1))
    done
    fail 'cannot acquire update lock'
}
valid_version() {
    printf '%s\n' "$1" | awk -F. 'NF!=3 {exit 1} {for(i=1;i<=3;i++) if($i !~ /^(0|[1-9][0-9]*)$/ || $i+0>65535) exit 1}'
}
active_target() {
    [ -L "$ROOT/mister-pats-gui.so" ] || return 1
    target=$(readlink "$ROOT/mister-pats-gui.so") || return 1
    case "$target" in mister-pats-gui-*.so) :;; *) return 1;; esac
    active_version=${target#mister-pats-gui-}; active_version=${active_version%.so}
    valid_version "$active_version" && [ -f "$ROOT/$target" ] && [ ! -L "$ROOT/$target" ]
}
elf() { [ "$(od -An -tx1 -N4 "$1" | tr -d ' \n')" = 7f454c46 ]; }
sha() { sha256sum "$1" | awk '{print $1}'; }
ini_value() {
    awk -v wanted="$1" '
        function trim(s) {sub(/^[ \t\r]+/, "", s); sub(/[ \t\r]+$/, "", s); return s}
        /^[ \t]*\[/ { section=trim($0); next }
        section=="[MiSTer]" {
            line=$0; sub(/^[ \t]*/, "", line)
            if(line=="" || line ~ /^[#;]/) next
            equals=index(line,"="); if(!equals) next
            if(trim(substr(line,1,equals-1))==wanted)
                value=trim(substr(line,equals+1))
        }
        END {print value}
    ' "$INI" 2>/dev/null
}
copy_checked() {
    from=$1; to=$2; expected=$3
    rm -f "$to" || fail "cannot clear staging path $to"
    [ ! -L "$to" ] || fail "symlink at destination $to"
    cp "$from" "$to" || fail "cannot copy $to"
    chmod 755 "$to" || fail "cannot chmod $to"
    [ "$(sha "$to")" = "$expected" ] || fail "hash differs after copy: $to"
}
check_bundle() {
    dir=$1; wanted=$2
    [ -d "$dir" ] && [ ! -L "$dir" ] || fail "missing rollback bundle: $dir"
    [ "$(wc -l < "$dir/rollback.sha256")" -eq 3 ] || fail "invalid rollback manifest: $dir"
    awk 'BEGIN {n[1]="target.txt";n[2]="MiSTer_gui";n[3]="mister-gui"} {if(NR>3 || length($0)!=66+length(n[NR]) || substr($0,65,2)!="  " || substr($0,67)!=n[NR] || substr($0,1,64) ~ /[^0-9a-f]/) exit 1} END {if(NR!=3) exit 1}' "$dir/rollback.sha256" || fail "invalid rollback manifest: $dir"
    for name in target.txt MiSTer_gui mister-gui; do
        [ -f "$dir/$name" ] && [ ! -L "$dir/$name" ] || fail "unsafe rollback copy: $dir/$name"
        expected=$(awk -v n="$name" '$2==n && length($1)==64 && $1 !~ /[^0-9a-f]/ {print $1}' "$dir/rollback.sha256")
        [ -n "$expected" ] && [ "$(sha "$dir/$name")" = "$expected" ] || fail "invalid rollback copy: $dir/$name"
    done
    [ "$(cat "$dir/target.txt")" = "$wanted" ] || fail "rollback target mismatch: $dir"
}
restore_binary_pair() {
    dir=$1
    for name in MiSTer_gui mister-gui; do
        hash=$(sha "$dir/$name")
        copy_checked "$dir/$name" "$ROOT/$name.new-$$" "$hash"
        mv -f "$ROOT/$name.new-$$" "$ROOT/$name" || fail "cannot restore $name; inspect $dir"
    done
    sync
}
acquire_lock
mkdir -p "$ROOT/icons" "$ROOT/fonts" "$ROOT/gamesdb" "$ROOT/logs" "$ROOT/.update-staging" "$ROOT/.rollback" || fail 'cannot create installation folders'
for dir in "$ROOT/.update-staging" "$ROOT/.rollback"; do
    [ -d "$dir" ] && [ ! -L "$dir" ] || fail "unsafe directory: $dir"
done
ROUTINE=0
newest=
newest_version=
if [ "$MODE" = install ]; then
    for path in "$ROOT"/mister-pats-gui-*.so; do
        [ -f "$path" ] && [ ! -L "$path" ] || continue
        name=${path##*/}
        version=${name#mister-pats-gui-}; version=${version%.so}
        valid_version "$version" || continue
        if [ -z "$newest_version" ] || [ "$(awk -v a="$newest_version" -v b="$version" 'BEGIN {split(a,x,".");split(b,y,".");for(i=1;i<=3;i++) if(x[i]+0<y[i]+0){print -1;exit} else if(x[i]+0>y[i]+0){print 1;exit} print 0}')" -lt 0 ]; then
            newest=$name; newest_version=$version
        fi
    done
    if [ -n "$newest" ] && [ -f "$ROOT/MiSTer_gui" ] && [ -f "$ROOT/mister-gui" ]; then
        legacy_bundle="$ROOT/.rollback/$newest_version"
        if [ -f "$legacy_bundle/target.txt" ] &&
           [ "$(cat "$legacy_bundle/target.txt")" = legacy ] &&
           [ -f "$ROOT/MiSTer_gui" ] && [ -f "$ROOT/mister-gui" ] &&
           [ "$(sha "$ROOT/MiSTer_gui")" = "$(sha "$legacy_bundle/MiSTer_gui")" ] &&
           [ "$(sha "$ROOT/mister-gui")" = "$(sha "$legacy_bundle/mister-gui")" ]; then
            fail 'legacy rollback is active; use --migrate-legacy to reinstall'
        fi
        [ ! -L "$ROOT/MiSTer_gui" ] && [ ! -L "$ROOT/mister-gui" ] ||
            fail 'versioned installation has unsafe bootstrap files'
        if [ -e "$LINK" ] || [ -L "$LINK" ]; then
            active_target || fail "invalid version pin: $LINK"
        fi
        ROUTINE=1
    fi
fi
if [ "$MODE" = --rollback ]; then
    if ! active_target; then
        [ ! -e "$LINK" ] && [ ! -L "$LINK" ] || fail 'invalid version pin'
        newest=
        newest_version=
        for path in "$ROOT"/mister-pats-gui-*.so; do
            [ -f "$path" ] && [ ! -L "$path" ] || continue
            name=${path##*/}
            version=${name#mister-pats-gui-}; version=${version%.so}
            valid_version "$version" || continue
            if [ -z "$newest_version" ] || [ "$(awk -v a="$newest_version" -v b="$version" 'BEGIN {split(a,x,".");split(b,y,".");for(i=1;i<=3;i++) if(x[i]+0<y[i]+0){print -1;exit} else if(x[i]+0>y[i]+0){print 1;exit} print 0}')" -lt 0 ]; then
                newest=$name; newest_version=$version
            fi
        done
        [ -n "$newest" ] || fail 'no versioned GUI to roll back'
        target=$newest; active_version=$newest_version
    fi
    bundle="$ROOT/.rollback/$active_version"
    [ -r "$bundle/target.txt" ] || fail "missing $bundle/target.txt"
    old=$(cat "$bundle/target.txt")
    if [ "$old" != legacy ]; then
        case "$old" in mister-pats-gui-*.so) :;; *) fail 'unsafe rollback target';; esac
        previous=${old#mister-pats-gui-}; previous=${previous%.so}
        valid_version "$previous" && [ -f "$ROOT/$old" ] && [ ! -L "$ROOT/$old" ] || fail "missing previous library: $ROOT/$old"
    fi
    check_bundle "$bundle" "$old"
    STAGE=$(mktemp -d "$ROOT/.update-staging/rollback-XXXXXX") || fail 'cannot stage rollback safety copies'
    for name in MiSTer_gui mister-gui; do
        [ -f "$ROOT/$name" ] && [ ! -L "$ROOT/$name" ] || fail "missing current $name"
        cp "$ROOT/$name" "$STAGE/$name" || fail "cannot preserve current $name"
    done
    UNDO=$STAGE
    restore_binary_pair "$bundle"
    if [ "$old" = legacy ]; then
        [ ! -L "$LINK" ] || rm -f "$LINK" || fail 'cannot remove active link'
    else
        rm -f "$ROOT/mister-pats-gui.so.new-$$"
        ln -s "$old" "$ROOT/mister-pats-gui.so.new-$$" || fail 'cannot create rollback link'
        mv -f "$ROOT/mister-pats-gui.so.new-$$" "$ROOT/mister-pats-gui.so" || fail 'cannot switch rollback link'
    fi
    ROLLBACK_DONE=1
    sync
    echo 'Rollback restored. Restart MiSTer manually.'
    exit 0
fi
# Recover a fully published bundle when a previous run stopped before switching the link.
recovery_target=
if [ "$ROUTINE" -eq 0 ]; then
    if active_target; then recovery_target=$target
    elif [ "$MODE" = --migrate-legacy ] && [ ! -e "$LINK" ] && [ -f "$ROOT/mister-gui" ]; then recovery_target=legacy
    fi
fi
recovery_bundle=
if [ -n "$recovery_target" ]; then
    for dir in "$ROOT"/.rollback/*; do
        [ -d "$dir" ] || continue
        [ -r "$dir/target.txt" ] || fail "incomplete rollback bundle: $dir"
        [ "$(cat "$dir/target.txt")" = "$recovery_target" ] || continue
        [ -z "$recovery_bundle" ] || fail "ambiguous rollback bundles: $recovery_bundle $dir"
        recovery_bundle=$dir
    done
fi
if [ -n "$recovery_bundle" ]; then
    check_bundle "$recovery_bundle" "$recovery_target"
    restore_binary_pair "$recovery_bundle"
fi
command -v curl >/dev/null 2>&1 || fail 'curl is required for verified HTTPS'
command -v df >/dev/null 2>&1 || fail 'df is required'
CA_BUNDLE=
for path in /media/fat/Scripts/.config/downloader/cacert.pem "$ROOT/cacert.pem" /etc/ssl/certs/cacert.pem /etc/ssl/cert.pem; do
    if [ -f "$path" ] && [ -r "$path" ]; then CA_BUNDLE=$path; break; fi
done
curl_https() {
    if [ -n "$CA_BUNDLE" ]; then curl --cacert "$CA_BUNDLE" "$@"
    else curl "$@"; fi
}
STAGE=$(mktemp -d "$ROOT/.update-staging/run-XXXXXX") || fail 'cannot create staging'
fetch() {
    url=$1; out=$2; limit=$3; timeout=$4
    case "$url" in https://*) :;; *) fail "non-HTTPS URL: $url";; esac
    attempt=0
    while [ "$attempt" -lt 2 ]; do
        rm -f "$out"
        if curl_https --proto '=https' --proto-redir '=https' --fail --location --max-redirs 5 \
            --silent --show-error --connect-timeout 12 --max-time "$timeout" \
            --max-filesize "$limit" --output "$out" --url "$url"; then
            size=$(wc -c < "$out")
            [ "$size" -gt 0 ] && [ "$size" -le "$limit" ] || fail "invalid download size: $out"
            return
        else
            code=$?
        fi
        case "$code" in 1|2|22|35|60|63|77) break;; esac
        attempt=$((attempt+1))
    done
    fail "verified HTTPS download failed: $url (check CA bundle and curl options)"
}
# A recursive awk tokenizer reads only top-level release fields and assets[].name.
cat > "$STAGE/release-json.awk" <<'AWK'
function bad() { exit 2 }
function ws() { while (p<=length(s) && substr(s,p,1) ~ /[ \t\r\n]/) p++ }
function string( c,e,h,n,i) {
    ws(); if (substr(s,p,1)!="\"") bad(); p++; val=""
    while(p<=length(s)) {
        c=substr(s,p++,1)
        if(c=="\"") return val
        if(c=="\\") {
            e=substr(s,p++,1)
            if(e=="u") {
                h=substr(s,p,4); if(h !~ /^[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]$/) bad()
                p+=4; n=0; for(i=1;i<=4;i++) { c=index("0123456789abcdef",tolower(substr(h,i,1)))-1; n=n*16+c }
                c=n<128?sprintf("%c",n):"?"
            } else if(e=="\"" || e=="\\" || e=="/") c=e
            else if(e=="n") c="\n"; else if(e=="r") c="\r"; else if(e=="t") c="\t"
            else if(e=="b" || e=="f") c="?"; else bad()
        } else if(c ~ /[[:cntrl:]]/) bad()
        val=val c
    }
    bad()
}
function value(depth,context, c,key,v,t,subcontext,count,start) {
    if(depth>32) bad(); ws(); c=substr(s,p,1)
    if(c=="{") {
        p++; ws(); count=0
        if(substr(s,p,1)!="}") while(1) {
            key=string(); ws(); if(substr(s,p++,1)!=":") bad()
            subcontext=context=="root"?key:(context=="asset"?"asset-field":"ignore")
            value(depth+1,subcontext); v=val; t=typ
            if(context=="root" && (key=="tag_name" || key=="draft" || key=="prerelease" || key=="body" || key=="assets")) {
                if(++seen[key]!=1) bad(); if(key=="tag_name" && t!="string") bad()
                if((key=="draft" || key=="prerelease") && t!="bool") bad()
                if(key=="body" && t!="string") bad(); if(key=="assets" && t!="array") bad()
                if(key=="tag_name") tag=v; if(key=="draft" || key=="prerelease") flags[key]=v
            }
            if(context=="asset" && key=="name") {if(++count!=1 || t!="string" || ++names[v]!=1) bad(); print "asset=" v}
            ws(); c=substr(s,p++,1); if(c=="}") break; if(c!=",") bad()
        }
        else p++
        if(context=="asset" && count!=1) bad(); typ="object"; val=""; return
    }
    if(c=="[") {
        p++; ws(); if(substr(s,p,1)!="]") while(1) {
            value(depth+1,context=="assets"?"asset":"ignore"); ws(); c=substr(s,p++,1)
            if(c=="]") break; if(c!=",") bad()
        }
        else p++
        typ="array"; val=""; return
    }
    if(c=="\"") {val=string(); typ="string"; return}
    if(substr(s,p,4)=="true") {p+=4; val="true"; typ="bool"; return}
    if(substr(s,p,5)=="false") {p+=5; val="false"; typ="bool"; return}
    if(substr(s,p,4)=="null") {p+=4; val=""; typ="null"; return}
    if(c ~ /[-0-9]/) {
        if(c=="-") p++
        c=substr(s,p,1)
        if(c=="0") p++
        else {
            if(c !~ /[1-9]/) bad()
            while(substr(s,p,1) ~ /[0-9]/) p++
        }
        if(substr(s,p,1)==".") {
            p++; start=p; while(substr(s,p,1) ~ /[0-9]/) p++; if(p==start) bad()
        }
        c=substr(s,p,1)
        if(c=="e" || c=="E") {
            p++; c=substr(s,p,1); if(c=="+" || c=="-") p++
            start=p; while(substr(s,p,1) ~ /[0-9]/) p++; if(p==start) bad()
        }
        val=""; typ="number"; return
    }
    bad()
}
{ s=s $0 "\n" }
END {
    p=1; value(0,"root"); ws(); if(p<=length(s)) bad()
    if(seen["tag_name"]!=1 || seen["draft"]!=1 || seen["prerelease"]!=1 || seen["body"]!=1 || seen["assets"]!=1) bad()
    if(flags["draft"]!="false" || flags["prerelease"]!="false") bad()
    print "tag=" tag
}
AWK
fetch "$API" "$STAGE/release.json" 1048576 15
awk -f "$STAGE/release-json.awk" "$STAGE/release.json" > "$STAGE/fields" || fail 'invalid GitHub release JSON'
TAG=$(sed -n 's/^tag=//p' "$STAGE/fields")
case "$TAG" in v*) VERSION=${TAG#v};; *) fail 'release tag must be vX.Y.Z';; esac
valid_version "$VERSION" || fail 'invalid release version'
LIB="mister-pats-gui-$VERSION.so"
if [ "$ROUTINE" -eq 1 ]; then
    selected_version=$newest_version
    if [ -L "$LINK" ]; then
        active_target || fail "invalid version pin: $LINK"
        selected_version=$active_version
    fi
    relation=$(awk -v a="$selected_version" -v b="$VERSION" 'BEGIN {split(a,x,".");split(b,y,".");for(i=1;i<=3;i++) if(x[i]+0<y[i]+0){print -1;exit} else if(x[i]+0>y[i]+0){print 1;exit} print 0}')
    if [ "$relation" -ge 0 ]; then
        echo "No newer release (selected v$selected_version; GitHub v$VERSION)."
        exit 0
    fi
    required_assets="$LIB release-info.txt SHA256SUMS"
else
    required_assets="mister-gui MiSTer_gui $LIB release-info.txt SHA256SUMS"
fi
for name in $required_assets; do
    grep -Fx "asset=$name" "$STAGE/fields" >/dev/null || fail "missing release asset $name"
done
url="$BASE/$TAG"
fetch "$url/release-info.txt" "$STAGE/release-info.txt" 1024 15
fetch "$url/SHA256SUMS" "$STAGE/SHA256SUMS" 4096 15
[ "$(wc -l < "$STAGE/release-info.txt")" -eq 3 ] || fail 'invalid release-info.txt'
[ "$(tail -c 1 "$STAGE/release-info.txt" | od -An -tu1 | tr -d ' \n')" = 10 ] || fail 'release-info.txt must end in LF'
awk -v v="$VERSION" 'NR==1 && $0!="version="v {exit 1} NR==2 && $0!="loader_abi=1" {exit 1} NR==3 && $0 !~ /^gamesdb_format=(0|[1-9][0-9]*)$/ {exit 1} NR>3 {exit 1} END {if(NR!=3) exit 1}' "$STAGE/release-info.txt" || fail 'invalid release-info.txt fields'
format=$(sed -n '3s/^gamesdb_format=//p' "$STAGE/release-info.txt")
case "$format" in ''|*[!0-9]*) fail 'invalid gamesdb format';; esac
[ "$format" = 0 ] || case "$format" in 0*) fail 'leading zero in gamesdb format';; esac
[ "$format" -le 65535 ] || fail 'invalid gamesdb format'
[ "$(wc -l < "$STAGE/SHA256SUMS")" -eq 4 ] || fail 'invalid SHA256SUMS line count'
awk -v lib="$LIB" 'BEGIN {names[1]="mister-gui";names[2]="MiSTer_gui";names[3]=lib;names[4]="release-info.txt"} {if(length($0)!=66+length(names[NR]) || substr($0,65,2)!="  " || substr($0,67)!=names[NR] || substr($0,1,64) ~ /[^0-9a-f]/) exit 1} END {if(NR!=4) exit 1}' "$STAGE/SHA256SUMS" || fail 'invalid SHA256SUMS format'
expected=$(awk '$2=="release-info.txt" {print $1}' "$STAGE/SHA256SUMS")
[ "$(sha "$STAGE/release-info.txt")" = "$expected" ] || fail 'release-info hash mismatch'
if [ "$ROUTINE" -eq 1 ]; then
    fetch "$url/$LIB" "$STAGE/$LIB" 67108864 120
    elf "$STAGE/$LIB" || fail "not an ELF file: $LIB"
    expected=$(awk -v n="$LIB" '$2==n {print $1}' "$STAGE/SHA256SUMS")
    [ "$(sha "$STAGE/$LIB")" = "$expected" ] || fail "hash mismatch: $LIB"
    destination="$ROOT/$LIB"
    if [ -e "$destination" ] || [ -L "$destination" ]; then
        [ -f "$destination" ] && [ ! -L "$destination" ] &&
            [ "$(sha "$destination")" = "$expected" ] || fail "versioned library differs: $destination"
        echo "v$VERSION already installed."
        exit 0
    fi
    free_kb=$(df -Pk "$ROOT" | awk 'NR==2 {print $4}')
    needed_kb=$(du -sk "$STAGE/$LIB" | awk '{print $1}')
    [ "$free_kb" -ge $((needed_kb+1024)) ] || fail 'not enough free space'
    copy_checked "$STAGE/$LIB" "$destination.new-$$" "$expected"
    mv "$destination.new-$$" "$destination" || fail "cannot install $destination"
    sync
    if [ -L "$LINK" ]; then
        echo "v$VERSION installed. Version pin $(readlink "$LINK") remains active."
    else
        echo "v$VERSION installed. Restart MiSTer to load the newest library."
    fi
    exit 0
fi
if active_target; then
    old=$target; current=$active_version
    [ "$MODE" != --migrate-legacy ] || fail 'migration requested but a versioned GUI is already active'
    for name in mister-gui MiSTer_gui; do
        [ -f "$ROOT/$name" ] && [ ! -L "$ROOT/$name" ] || fail "installed binary needs repair: $ROOT/$name"
    done
else
    [ ! -e "$ROOT/mister-pats-gui.so" ] && [ ! -L "$ROOT/mister-pats-gui.so" ] || fail 'broken active link; repair manually'
    old=
    current=
    partial=0
    if [ -e "$ROOT/mister-gui" ] || [ -e "$ROOT/MiSTer_gui" ]; then
        if [ "$MODE" = --migrate-legacy ] && [ -f "$ROOT/mister-gui" ] && [ -f "$ROOT/MiSTer_gui" ]; then
            old=legacy
        elif [ "$MODE" = install ] && [ -f "$ROOT/$LIB" ] && [ ! -L "$ROOT/$LIB" ]; then
            partial=1
        else
            fail 'unknown installation; use --migrate-legacy only for a verified legacy GUI'
        fi
    else
        [ "$MODE" = install ] || fail 'no legacy installation to migrate'
    fi
fi
if [ -n "$current" ]; then
    relation=$(awk -v a="$current" -v b="$VERSION" 'BEGIN {split(a,x,".");split(b,y,".");for(i=1;i<=3;i++) if(x[i]+0<y[i]+0){print -1;exit} else if(x[i]+0>y[i]+0){print 1;exit} print 0}')
    [ "$relation" -le 0 ] || fail "installed v$current is newer than v$VERSION"
fi
printf 'Installed: %s; selected: v%s\n' "${current:-${old:-none}}" "$VERSION"
for name in mister-gui MiSTer_gui "$LIB"; do
    fetch "$url/$name" "$STAGE/$name" 67108864 120
    elf "$STAGE/$name" || fail "not an ELF file: $name"
    expected=$(awk -v n="$name" '$2==n {print $1}' "$STAGE/SHA256SUMS")
    [ "$(sha "$STAGE/$name")" = "$expected" ] || fail "hash mismatch: $name"
done
if [ "${partial:-0}" -eq 1 ]; then
    for name in mister-gui MiSTer_gui "$LIB"; do
        expected=$(awk -v n="$name" '$2==n {print $1}' "$STAGE/SHA256SUMS")
        if [ -e "$ROOT/$name" ] || [ -L "$ROOT/$name" ]; then
            [ -f "$ROOT/$name" ] && [ ! -L "$ROOT/$name" ] && [ "$(sha "$ROOT/$name")" = "$expected" ] || fail "unlinked installation differs from release: $ROOT/$name"
        elif [ "$name" = "$LIB" ]; then
            fail "missing partial-install library: $ROOT/$LIB"
        fi
    done
    for name in MiSTer_gui mister-gui; do
        [ -e "$ROOT/$name" ] && continue
        expected=$(awk -v n="$name" '$2==n {print $1}' "$STAGE/SHA256SUMS")
        copy_checked "$STAGE/$name" "$ROOT/$name.new-$$" "$expected"
        mv -f "$ROOT/$name.new-$$" "$ROOT/$name" || fail "cannot complete interrupted first installation: $name"
    done
    sync
    echo "v$VERSION installed. Restart MiSTer to load the newest library."
    exit 0
fi
if [ "${relation:--1}" -eq 0 ]; then
    for name in mister-gui MiSTer_gui "$LIB"; do
        expected=$(awk -v n="$name" '$2==n {print $1}' "$STAGE/SHA256SUMS")
        [ -f "$ROOT/$name" ] && [ ! -L "$ROOT/$name" ] && [ "$(sha "$ROOT/$name")" = "$expected" ] || fail "installed v$VERSION differs: $name"
    done
    echo 'Already up to date.'
    exit 0
fi
# Leave room for new binaries, same-volume staging copies, and rollback copies.
free_kb=$(df -Pk "$ROOT" | awk 'NR==2 {print $4}')
needed_kb=$(du -sk "$STAGE" | awk '{print $1}')
[ "$free_kb" -ge $((needed_kb*3+1024)) ] || fail 'not enough free space'
newlib="$ROOT/$LIB"
expected=$(awk -v n="$LIB" '$2==n {print $1}' "$STAGE/SHA256SUMS")
if [ -e "$newlib" ] || [ -L "$newlib" ]; then
    [ -f "$newlib" ] && [ ! -L "$newlib" ] && [ "$(sha "$newlib")" = "$expected" ] || fail "versioned library differs: $newlib"
else
    copy_checked "$STAGE/$LIB" "$newlib.new-$$" "$expected"
    mv "$newlib.new-$$" "$newlib" || fail "cannot install $newlib"
fi
bundle="$ROOT/.rollback/$VERSION"
if [ -n "$old" ]; then
    if [ -e "$bundle" ]; then
        check_bundle "$bundle" "$old"
        restore_binary_pair "$bundle"
    else
        pending=$(mktemp -d "$ROOT/.rollback/.${VERSION}.new-XXXXXX") || fail 'cannot stage rollback'
        printf '%s\n' "$old" > "$pending/target.txt"
        cp "$ROOT/MiSTer_gui" "$pending/MiSTer_gui" || fail 'cannot back up main process'
        cp "$ROOT/mister-gui" "$pending/mister-gui" || fail 'cannot back up GUI loader'
        (cd "$pending" && sha256sum target.txt MiSTer_gui mister-gui > rollback.sha256) || fail 'cannot hash rollback'
        check_bundle "$pending" "$old"
        mv "$pending" "$bundle" || fail 'cannot publish rollback bundle'
        sync
    fi
fi
commit_ok=0
for name in MiSTer_gui mister-gui; do
    expected=$(awk -v n="$name" '$2==n {print $1}' "$STAGE/SHA256SUMS")
    if ! cp "$STAGE/$name" "$ROOT/$name.new-$$" || ! chmod 755 "$ROOT/$name.new-$$" || [ "$(sha "$ROOT/$name.new-$$")" != "$expected" ] || ! mv -f "$ROOT/$name.new-$$" "$ROOT/$name"; then
        [ -z "$old" ] || restore_binary_pair "$bundle"
        fail "commit failed at $name; inspect $bundle"
    fi
done
sync
sync
echo "v$VERSION installed. Newest library will load after a manual restart."
# Optional assets run only after the release has committed.
ICON_COMMIT=2fcc05a0695326e25451d1e66c8db08f4d20e928
FONT_VERSION=2.0.0
if curl_https --proto '=https' --proto-redir '=https' --fail --location --silent --show-error --connect-timeout 12 --max-time 15 --max-filesize 1048576 \
    "https://api.github.com/repos/pat-east/mister-pats-gui/contents/assets/icons?ref=$ICON_COMMIT" -o "$STAGE/icons.json"; then
    sed -n 's/^[[:space:]]*"name":[[:space:]]*"\([A-Za-z0-9._-]*\.bmp\)".*/\1/p' "$STAGE/icons.json" > "$STAGE/icons.txt"
    while IFS= read -r name; do
        [ -n "$name" ] || continue
        if [ -s "$ROOT/icons/$name" ] && [ "$(od -An -tx1 -N2 "$ROOT/icons/$name" | tr -d ' \n')" = 424d ]; then continue; fi
        if curl_https --proto '=https' --proto-redir '=https' --fail --location --silent --show-error --connect-timeout 12 --max-time 15 --max-filesize 1048576 \
            "https://raw.githubusercontent.com/pat-east/mister-pats-gui/$ICON_COMMIT/assets/icons/$name" -o "$STAGE/$name" && \
            [ "$(od -An -tx1 -N2 "$STAGE/$name" | tr -d ' \n')" = 424d ]; then
            cp "$STAGE/$name" "$ROOT/icons/$name.new-$$" && mv "$ROOT/icons/$name.new-$$" "$ROOT/icons/$name" || echo "warning: icon $name" >&2
        else echo "warning: icon $name" >&2; fi
    done < "$STAGE/icons.txt"
else echo 'warning: system icons unavailable' >&2; fi
fontbase="https://raw.githubusercontent.com/floriankarsten/space-grotesk/$FONT_VERSION"
for name in SpaceGrotesk-Bold.ttf SpaceGrotesk-Medium.ttf; do
    if curl_https --proto '=https' --proto-redir '=https' --fail --location --silent --show-error --connect-timeout 12 --max-time 15 --max-filesize 10485760 \
        "$fontbase/fonts/ttf/static/$name" -o "$STAGE/$name" && \
        [ "$(od -An -tx1 -N4 "$STAGE/$name" | tr -d ' \n')" = 00010000 ]; then
        cp "$STAGE/$name" "$ROOT/fonts/$name.new-$$" && mv "$ROOT/fonts/$name.new-$$" "$ROOT/fonts/$name" || echo "warning: font $name" >&2
    else echo "warning: font $name" >&2; fi
done
if curl_https --proto '=https' --proto-redir '=https' --fail --location --silent --show-error --connect-timeout 12 --max-time 15 --max-filesize 1048576 \
    "$fontbase/OFL.txt" -o "$STAGE/OFL.txt" && [ -s "$STAGE/OFL.txt" ]; then
    cp "$STAGE/OFL.txt" "$ROOT/fonts/SpaceGrotesk-OFL.txt.new-$$" && mv "$ROOT/fonts/SpaceGrotesk-OFL.txt.new-$$" "$ROOT/fonts/SpaceGrotesk-OFL.txt" || echo 'warning: font license' >&2
else echo 'warning: font license' >&2; fi
INI=/media/fat/MiSTer.ini
if [ -r "$INI" ]; then
    [ "$(ini_value main)" = 'mister-pat/MiSTer_gui' ] || echo "Add to [MiSTer] in $INI: main=mister-pat/MiSTer_gui"
    [ "$(ini_value gui)" = 'mister-pat/mister-gui' ] || echo "Add to [MiSTer] in $INI: gui=mister-pat/mister-gui"
else
    printf 'Add to [MiSTer] in %s:\nmain=mister-pat/MiSTer_gui\ngui=mister-pat/mister-gui\n' "$INI"
fi
