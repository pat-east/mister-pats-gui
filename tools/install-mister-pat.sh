#!/bin/sh
# First install from a fresh MiSTer Fusion card. Installs the GUI and enables it in MiSTer.ini.
# Further setup, including core downloads, is handled by the GUI onboarding.
set -eu
umask 077

ROOT=/media/fat
APP=$ROOT/mister-pat
INI=$ROOT/MiSTer.ini
INI_SOURCE=$INI
BACKUP=$ROOT/MiSTer.ini.bak.mister-pat
RAW=https://raw.githubusercontent.com/pat-east/mister-pats-gui/main/tools/install-update.sh
STEP=0
if [ -t 1 ]; then
    BOLD=$(printf '\033[1m')
    ACCENT=$(printf '\033[1;36m')
    ERROR=$(printf '\033[1;31m')
    RESET=$(printf '\033[0m')
else
    BOLD=; ACCENT=; ERROR=; RESET=
fi
step() {
    STEP=$((STEP+1))
    printf '\n%s[%s/4] %s%s\n' "$ACCENT" "$STEP" "$*" "$RESET"
}
note() { printf '  %s\n' "$*"; }
done_msg() { printf '%s  [OK]%s %s\n' "$ACCENT" "$RESET" "$*"; }
fail() {
    printf '\n%s[STOP]%s %s\n' "$ERROR" "$RESET" "$*" >&2
    exit 1
}
progress_run() {
    progress_hint=$1; shift
    if [ ! -t 1 ]; then
        note "$progress_hint"
        "$@"
        return $?
    fi
    "$@" &
    progress_pid=$!
    progress_frame=0
    while kill -0 "$progress_pid" 2>/dev/null; do
        case $((progress_frame % 4)) in
            0) progress_spinner='|' ;;
            1) progress_spinner='/' ;;
            2) progress_spinner='-' ;;
            3) progress_spinner='\' ;;
        esac
        printf '\r\033[2K  %s %s' "$progress_spinner" "$progress_hint"
        progress_frame=$((progress_frame+1))
        sleep 0.4
    done
    if wait "$progress_pid"; then
        printf '\r\033[2K  [OK] %s\n' "$progress_hint"
        return 0
    else
        progress_status=$?
        printf '\r\033[2K'
        return "$progress_status"
    fi
}

printf '\n%s' "$ACCENT"
cat <<'MISTER_PAT_LOGO'
                                         ..:@@@@@@@@@@@@    .
                                         @@@@@@@@@@@@@@@@@@@=
                                    .%@@@@@@....@@@@@@@@@@@@@@*
                                  .:.%@@@@@@   .@@@@@@@@@@@@@@*:..
                                  .@@@@:.           @@@@@@@@@@@@+
                                 @@@@=  .          ..  .@@@@@@@@@@@
                             ..@@@@...                 .  @@@@@@@@@@@...
                             @@@@                         @@@@@@@@@@@@@.
                             @@@@.                        @@@@@@@@@@@@@::.
                           ..@@                             %@@@@@@@@@@@@
                          .@@@@                              .*@@@@@@@@@@
                          .@@@@                              ...#@@@@@@@@
                          .@@@@                                 . .@@@@@@
                          .@@@@                                 . .@@@@@@
                          .@@@@.......         .     ...........#@@@@@@@@
                             @@@@@@@@@@@@@@@@@@@......@@@@@@@@@@@@@@@@@@@
                             @@@@             @@@@@@@@               @@@@
                             @@@@.           ...@@@@@@               @@...
                             @@@@...         ...@@..@@. ..        .  @@..
                             @@@@@@           @@    @@@@..         @@@@@@.
                             @@++++###########++.  .##@@+++++++++++@@@@@@
                            .@@   .@@@@@@@@@@@.       @@@@@@@@@@@@@@@@@@@.
                                                      @@..           @@.
                                                    @@@@             @@.
                            .@@. .           :@@@@ .@@.           .@@@@.
                            .@@@@                 @@               @@@@.
                            ...@@                 ...              @@...
                              .==                               *%%@@
                                 ::.                         ..:#@@@@
                                 @@                          .*@@@@@@.
                                 @@::.                     .::*@@@@
                                  .@@=   .                ..%@@@@@@
                                     #@@@...              @@@@@@+.
                                     #@@@@@@              @@@@*
                                     ..@@@@@@@        @@@@@@@@*
                                       ..@@@@@@@@@@@@@@@@@@@=..
                                         ..:@@@@@@@@@@@@   ..
MISTER_PAT_LOGO
printf '%s%sMiSTer Pat | First-time setup%s\n' "$BOLD" "$ACCENT" "$RESET"
printf '%s============================================================%s\n' "$ACCENT" "$RESET"
note 'Installs the GUI, configures startup, then restarts MiSTer.'
note 'Continue with core setup in the GUI onboarding after restart.'
note 'Your ROMs and BIOS files are not downloaded or changed.'
step 'Check storage and MiSTer configuration'
[ -d "$ROOT" ] && [ -w "$ROOT" ] || fail 'MiSTer storage is not available for writing.'
if [ -e "$INI" ] || [ -L "$INI" ]; then
    [ -f "$INI" ] && [ ! -L "$INI" ] || fail 'MiSTer.ini is unsafe; nothing was changed.'
    note 'Using MiSTer.ini. A backup will be saved before startup settings are added.'
else
    INI_SOURCE=
    note 'MiSTer.ini is missing; creating one with only the GUI startup entries.'
    note 'MiSTer_example.ini will be left unchanged.'
fi
if [ -e "$APP" ] || [ -L "$APP" ]; then
    [ -d "$APP" ] && [ ! -L "$APP" ] || fail 'the GUI installation directory is unsafe.'
fi
for tool in curl mktemp awk cp chmod mv rm stat sleep; do
    command -v "$tool" >/dev/null 2>&1 || fail "required system tool is missing: $tool"
done
if [ -e "$BACKUP" ] || [ -L "$BACKUP" ]; then
    [ -f "$BACKUP" ] && [ ! -L "$BACKUP" ] || fail 'the MiSTer.ini backup path is not a regular file.'
fi
check_ini() {
awk '
    function trim(s) { sub(/^[ \t\r]+/, "", s); sub(/[ \t\r]+$/, "", s); return s }
    /^[ \t]*\[/ {
        section=trim($0)
        if (section=="[MiSTer]" && ++sections > 1) bad=1
        next
    }
    section=="[MiSTer]" {
        line=$0; sub(/^[ \t]*/, "", line)
        if (line=="" || line ~ /^[#;]/) next
        pos=index(line,"="); if (!pos) next
        key=trim(substr(line,1,pos-1)); value=trim(substr(line,pos+1))
        if (key=="main" || key=="gui") {
            if (++count[key] > 1) bad=1
            expected=(key=="main" ? "mister-pat/MiSTer_gui" : "mister-pat/mister-gui")
            if (value!="" && value!=expected) bad=1
        }
    }
    END { if (bad) exit 1 }
' "$INI_SOURCE" || fail 'MiSTer.ini has duplicate or conflicting startup entries; no configuration was changed.'
}
if [ -n "$INI_SOURCE" ]; then check_ini; fi
done_msg 'Storage and configuration are ready.'
mkdir -p "$APP/.update-staging" || fail 'cannot prepare temporary installation storage.'
[ -d "$APP/.update-staging" ] && [ ! -L "$APP/.update-staging" ] || fail 'temporary installation storage is unsafe.'
STAGE=$(mktemp -d "$APP/.update-staging/first-install-XXXXXX") || fail 'cannot create temporary storage.'
OUT=
cleanup() {
    [ -z "${STAGE:-}" ] || rm -rf "$STAGE"
    [ -z "${OUT:-}" ] || rm -f "$OUT"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

CA=
for candidate in "$ROOT/Scripts/.config/downloader/cacert.pem" "$APP/cacert.pem" \
                 /etc/ssl/certs/cacert.pem /etc/ssl/cert.pem; do
    if [ -r "$candidate" ]; then CA=$candidate; break; fi
done
step 'Download and install the GUI'
if [ -n "$CA" ]; then
    progress_run 'Downloading the GUI updater over verified HTTPS' curl --cacert "$CA" --proto '=https' --proto-redir '=https' --fail --location \
        --silent --show-error --connect-timeout 12 --max-time 30 --max-filesize 131072 \
        --output "$STAGE/install-update.sh" "$RAW" || fail 'the GUI installer could not be downloaded.'
else
    progress_run 'Downloading the GUI updater over verified HTTPS' curl --proto '=https' --proto-redir '=https' --fail --location --silent --show-error \
        --connect-timeout 12 --max-time 30 --max-filesize 131072 \
        --output "$STAGE/install-update.sh" "$RAW" || fail 'the GUI installer could not be downloaded.'
fi
[ -s "$STAGE/install-update.sh" ] || fail 'the downloaded GUI installer is empty.'
note 'Installing the latest verified GUI release and interface assets...'
note 'The spinner shows the current download or checksum check.'
if ! MISTER_PAT_FIRST_INSTALL=1 sh "$STAGE/install-update.sh"; then
    fail 'the GUI installation failed; MiSTer.ini was left unchanged.'
fi
[ -x "$APP/mister-gui" ] || fail 'the GUI launcher is missing after installation.'
done_msg 'GUI files and optional assets are ready. Continuing setup.'

step 'Configure MiSTer startup'
note 'Preparing MiSTer.ini with the MiSTer Pat startup entries...'
OUT=$(mktemp "$ROOT/.MiSTer.ini.first-install-XXXXXX") || fail 'cannot stage MiSTer.ini safely.'
if [ -n "$INI_SOURCE" ]; then
    awk '
    function trim(s) { sub(/^[ \t\r]+/, "", s); sub(/[ \t\r]+$/, "", s); return s }
    function add_missing() {
        if (section=="[MiSTer]") {
            if (!seen_main) print "main=mister-pat/MiSTer_gui"
            if (!seen_gui) print "gui=mister-pat/mister-gui"
        }
    }
    { sub(/\r$/, "") }
    /^[ \t]*\[/ {
        add_missing(); section=trim($0); if (section=="[MiSTer]") found_section=1; print; next
    }
    section=="[MiSTer]" {
        line=$0; probe=line; sub(/^[ \t]*/, "", probe)
        if (probe!="" && probe !~ /^[#;]/) {
            pos=index(probe,"=")
            if (pos) {
                key=trim(substr(probe,1,pos-1))
                if (key=="main") { print "main=mister-pat/MiSTer_gui"; seen_main=1; next }
                if (key=="gui") { print "gui=mister-pat/mister-gui"; seen_gui=1; next }
            }
        }
    }
    { print }
    END {
        add_missing()
        if (!found_section) {
            if (NR) print ""
            print "[MiSTer]"
            print "main=mister-pat/MiSTer_gui"
            print "gui=mister-pat/mister-gui"
        }
    }
' "$INI_SOURCE" > "$OUT" || { rm -f "$OUT"; fail 'cannot prepare MiSTer.ini.'; }
    chmod "$(stat -c '%a' "$INI_SOURCE")" "$OUT" 2>/dev/null || chmod 644 "$OUT"
    if [ ! -e "$BACKUP" ]; then
        note 'Saving the original configuration to MiSTer.ini.bak.mister-pat.'
        cp -p "$INI_SOURCE" "$BACKUP" || { rm -f "$OUT"; fail 'could not back up the MiSTer configuration; no configuration was changed.'; }
    fi
else
    printf '%s\n' '[MiSTer]' 'main=mister-pat/MiSTer_gui' 'gui=mister-pat/mister-gui' > "$OUT" || fail 'cannot write the new MiSTer.ini.'
    chmod 644 "$OUT" || fail 'cannot set permissions on the new MiSTer.ini.'
fi
note 'Activating /media/fat/MiSTer.ini...'
mv -f "$OUT" "$INI" || fail 'could not activate the startup settings; any existing MiSTer.ini was left unchanged.'
OUT=
sync
rm -rf "$STAGE"
STAGE=
if command -v reboot >/dev/null 2>&1; then
    step 'Restart MiSTer'
    note 'Restarting MiSTer now. It should start directly in MiSTer Pat GUI.'
    reboot || fail 'setup is complete, but MiSTer did not restart. Restart it manually to start the GUI.'
else
    step 'Restart MiSTer'
    note 'Setup is complete. Restart MiSTer manually to start the GUI.'
fi
