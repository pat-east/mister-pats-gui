#!/bin/sh
# Install or update MiSTer Pat's GUI on the MiSTer itself.
# MiSTer.ini is inspected but never edited; required boot-path lines are printed at the end.

set -eu

REPOSITORY="pat-east/mister-pats-gui"
RELEASE_API="https://api.github.com/repos/$REPOSITORY/releases/latest"
ICON_COMMIT="2fcc05a0695326e25451d1e66c8db08f4d20e928"
ICONS_API="https://api.github.com/repos/$REPOSITORY/contents/assets/icons?ref=$ICON_COMMIT"
ICONS_RAW="https://raw.githubusercontent.com/$REPOSITORY/$ICON_COMMIT/assets/icons"
INSTALL_ROOT="/media/fat/mister-pat"
INI_FILE="/media/fat/MiSTer.ini"

TEMP_ROOT="/tmp/mister-pat-setup-$$"
mkdir -p "$TEMP_ROOT"
cleanup() {
    rm -rf "$TEMP_ROOT"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

fail() {
    printf 'mister-pat setup: %s\n' "$*" >&2
    exit 1
}

fetch() {
    url=$1
    output=$2

    if command -v curl >/dev/null 2>&1; then
        if curl -4 -fsSL --connect-timeout 12 --max-time 60 -o "$output" "$url" \
                2>/dev/null; then
            return 0
        fi
        if curl -4 -fsSLk --connect-timeout 12 --max-time 60 -o "$output" "$url" \
                2>/dev/null; then
            return 0
        fi
    fi

    if command -v wget >/dev/null 2>&1; then
        if wget -4 --no-check-certificate --connect-timeout=12 -q -T 60 \
                -O "$output" "$url"; then
            return 0
        fi
    fi

    rm -f "$output"
    return 1
}

install_if_changed() {
    source_file=$1
    destination=$2
    mode=$3

    if [ -f "$destination" ] && cmp -s "$source_file" "$destination"; then
        chmod "$mode" "$destination" || fail "cannot set permissions on $destination"
        INSTALL_RESULT="unchanged"
        printf '  current: %s\n' "$destination"
        return 0
    fi

    staged="$destination.new"
    rm -f "$staged"
    cp "$source_file" "$staged" || fail "cannot stage $destination"
    chmod "$mode" "$staged" || fail "cannot set permissions on $destination"
    mv -f "$staged" "$destination" || fail "cannot install $destination"
    INSTALL_RESULT="changed"
    printf '  installed: %s\n' "$destination"
}

is_elf() {
    signature=$(od -An -tx1 -N4 "$1" | tr -d ' \n')
    [ "$signature" = "7f454c46" ]
}

is_bmp() {
    signature=$(od -An -tx1 -N2 "$1" | tr -d ' \n')
    [ "$signature" = "424d" ]
}

ini_value() {
    key=$1
    awk -v wanted="$key" '
        function trim(s) {
            sub(/^[ \t\r]+/, "", s)
            sub(/[ \t\r]+$/, "", s)
            return s
        }
        /^[ \t]*\[/ {
            section = trim($0)
            next
        }
        section == "[MiSTer]" {
            line = $0
            sub(/^[ \t]*/, "", line)
            if (line == "" || line ~ /^[#;]/) next
            equals = index(line, "=")
            if (equals == 0) next
            name = trim(substr(line, 1, equals - 1))
            if (name == wanted) value = trim(substr(line, equals + 1))
        }
        END { print value }
    ' "$INI_FILE" 2>/dev/null
}

printf 'Creating installation directories under %s\n' "$INSTALL_ROOT"
mkdir -p "$INSTALL_ROOT/icons" "$INSTALL_ROOT/fonts" "$INSTALL_ROOT/gamesdb" \
         "$INSTALL_ROOT/logs" || fail "cannot create installation directories"

printf 'Checking the latest GitHub release\n'
fetch "$RELEASE_API" "$TEMP_ROOT/release.json" || fail "cannot reach the GitHub release API"
TAG=$(sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
      "$TEMP_ROOT/release.json" | head -n 1)
case "$TAG" in
    ""|*[!A-Za-z0-9._-]*) fail "could not read a safe release tag" ;;
esac
printf 'Latest release: %s\n' "$TAG"

printf 'Downloading release binaries\n'
fetch "https://github.com/$REPOSITORY/releases/download/$TAG/mister-gui" \
      "$TEMP_ROOT/mister-gui" || fail "release $TAG has no downloadable mister-gui asset"
fetch "https://github.com/$REPOSITORY/releases/download/$TAG/MiSTer_gui" \
      "$TEMP_ROOT/MiSTer_gui" || fail "release $TAG has no downloadable MiSTer_gui asset"
[ -s "$TEMP_ROOT/mister-gui" ] || fail "downloaded mister-gui is empty"
[ -s "$TEMP_ROOT/MiSTer_gui" ] || fail "downloaded MiSTer_gui is empty"
is_elf "$TEMP_ROOT/mister-gui" || fail "mister-gui is not an ELF binary"
is_elf "$TEMP_ROOT/MiSTer_gui" || fail "MiSTer_gui is not an ELF binary"

MISTER_GUI_CHANGED=0
install_if_changed "$TEMP_ROOT/mister-gui" "$INSTALL_ROOT/mister-gui" 755
GUI_RESULT=$INSTALL_RESULT
install_if_changed "$TEMP_ROOT/MiSTer_gui" "$INSTALL_ROOT/MiSTer_gui" 755
MAIN_RESULT=$INSTALL_RESULT
if [ "$MAIN_RESULT" = "changed" ]; then
    MISTER_GUI_CHANGED=1
fi

printf 'Downloading the system icon assets\n'
fetch "$ICONS_API" "$TEMP_ROOT/icons.json" || fail "cannot list the system icon assets"
sed -n 's/^[[:space:]]*"name":[[:space:]]*"\([^"]*\.bmp\)".*/\1/p' \
    "$TEMP_ROOT/icons.json" > "$TEMP_ROOT/icon-names.txt"
icon_count=$(wc -l < "$TEMP_ROOT/icon-names.txt" | tr -d ' ')
[ "$icon_count" -gt 0 ] || fail "GitHub listed no BMP icon assets"

while IFS= read -r name || [ -n "$name" ]; do
    case "$name" in
        ""|*[!A-Za-z0-9._-]*) fail "unsafe icon filename received from GitHub" ;;
    esac
    destination="$INSTALL_ROOT/icons/$name"
    if [ -s "$destination" ] && is_bmp "$destination"; then
        printf '  current: %s\n' "$destination"
        continue
    fi
    temporary="$TEMP_ROOT/$name"
    fetch "$ICONS_RAW/$name" "$temporary" || fail "cannot download icon $name"
    is_bmp "$temporary" || fail "downloaded icon is not a BMP: $name"
    install_if_changed "$temporary" "$destination" 644
    rm -f "$temporary"
done < "$TEMP_ROOT/icon-names.txt"

printf 'System icon assets available: %s\n' "$icon_count"

printf '\nChecking %s\n' "$INI_FILE"
if [ ! -r "$INI_FILE" ]; then
    printf 'MiSTer.ini is missing or unreadable. Add these lines manually; the file was not changed:\n'
    printf '\n[MiSTer]\nmain=mister-pat/MiSTer_gui\ngui=mister-pat/mister-gui\n'
else
    main_value=$(ini_value main)
    gui_value=$(ini_value gui)
    need_main=0
    need_gui=0
    [ "$main_value" = "mister-pat/MiSTer_gui" ] || need_main=1
    [ "$gui_value" = "mister-pat/mister-gui" ] || need_gui=1

    if [ "$need_main" -eq 0 ] && [ "$need_gui" -eq 0 ]; then
        printf 'Boot paths already point to this installation. MiSTer.ini was not changed.\n'
    else
        printf 'Manual MiSTer.ini changes are needed. This script did not edit the file.\n'
        if ! grep -q '^[[:space:]]*\[MiSTer\][[:space:]]*$' "$INI_FILE"; then
            printf '\nAdd this section:\n[MiSTer]\n'
        fi
        [ "$need_main" -eq 0 ] || printf 'Add or set: main=mister-pat/MiSTer_gui\n'
        [ "$need_gui" -eq 0 ] || printf 'Add or set: gui=mister-pat/mister-gui\n'
    fi
fi

printf '\nSetup files are in %s. No reboot was started.\n' "$INSTALL_ROOT"
[ "$MISTER_GUI_CHANGED" -eq 0 ] || \
    printf 'MiSTer_gui changed; reboot when ready to activate the new main binary.\n'
[ "$GUI_RESULT" = "unchanged" ] || \
    printf 'The running GUI keeps using its current code until it is restarted.\n'
