#!/usr/bin/env bash
set -euo pipefail

project_directory="$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd)"
build_directory="$project_directory/build"
install_prefix="$HOME/.local"
config_directory="${XDG_CONFIG_HOME:-$HOME/.config}/webcam-controls"
apps_list="${XDG_CONFIG_HOME:-$HOME/.config}/apps.list"
selector="${APPLICATION_THEME_LINK:-${XDG_CONFIG_HOME:-$HOME/.config}/themes/.caelestia-use}"
desktop_id=io.github.webcam_controls
dry_run=0
selector_explicit=0

usage() {
    printf '%s\n' 'Usage: install-desktop.sh [--dry-run] [--build-dir DIR] [--prefix DIR]' \
        '       [--config-dir DIR] [--apps-list FILE] [--selector FILE_OR_DIR]'
}

fail() {
    printf 'install-desktop: %s\n' "$*" >&2
    exit 1
}

parse_options() {
    while (( $# )); do
        case "$1" in
            --dry-run) dry_run=1; shift ;;
            --help|-h) usage; exit 0 ;;
            --build-dir|--prefix|--config-dir|--apps-list|--selector)
                (( $# >= 2 )) || fail "Missing value for $1"
                [[ "$2" == /* && "$2" != / && "$2" != *$'\n'* && "$2" != *'|'* && "$2" != *'"'* && "$2" != *'%'* && "$2" != *\\* && "$2" != *'$'* && "$2" != *'`'* ]] || fail "Expected a safe absolute path for $1"
                case "$1" in
                    --build-dir) build_directory="${2%/}" ;;
                    --prefix) install_prefix="${2%/}" ;;
                    --config-dir) config_directory="${2%/}" ;;
                    --apps-list) apps_list="$2" ;;
                    --selector) selector="${2%/}"; selector_explicit=1 ;;
                esac
                shift 2 ;;
            *) fail "Unknown option: $1" ;;
        esac
    done
}

validate_installation() {
    [[ -x "$build_directory/webcam-controls" ]] || fail "Build the app first: $build_directory/webcam-controls"
    [[ ! -e "$apps_list" || -f "$apps_list" ]] || fail "App list is not a regular file: $apps_list"
    [[ ! -L "$install_prefix/bin/webcam-controls" ]] || fail 'Refusing to replace a symlinked executable'
    [[ ! -L "$install_prefix/bin/applications/$desktop_id.desktop" ]] || fail 'Refusing to replace a symlinked desktop entry'
    local registered="$install_prefix/share/applications/$desktop_id.desktop"
    local desktop="$install_prefix/bin/applications/$desktop_id.desktop"
    [[ ! -e "$registered" && ! -L "$registered" || -L "$registered" && "$(/usr/bin/readlink -- "$registered")" == "$desktop" ]] ||
        fail "Existing desktop registration is unmanaged: $registered"
    if [[ ! -e "$selector" && "$selector_explicit" == 0 ]]; then
        selector="${XDG_CONFIG_HOME:-$HOME/.config}/themes/current"
        [[ -e "$selector" ]] || selector=""
    fi
    if [[ -n "$selector" ]]; then
        "$build_directory/webcam-controls" --config-dir "$config_directory" theme follow "$selector" --dry-run >/dev/null
    fi
}

backup_file() {
    [[ -f "$1" ]] || return 0
    /usr/bin/cp -p -- "$1" "$1.webcam-controls-backup.$(/usr/bin/date +%Y%m%dT%H%M%S).$$"
}

install_binary() {
    /usr/bin/mkdir -p -- "$install_prefix/bin"
    local temporary
    temporary="$(/usr/bin/mktemp "$install_prefix/bin/.webcam-controls.XXXXXX")"
    /usr/bin/install -m755 -- "$build_directory/webcam-controls" "$temporary"
    /usr/bin/mv -T -- "$temporary" "$install_prefix/bin/webcam-controls"
}

install_desktop() {
    local desktop="$install_prefix/bin/applications/$desktop_id.desktop"
    local registered="$install_prefix/share/applications/$desktop_id.desktop"
    /usr/bin/install -Dm644 -- "$project_directory/assets/$desktop_id.svg" \
        "$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg"
    /usr/bin/mkdir -p -- "${desktop%/*}" "${registered%/*}"
    backup_file "$desktop"
    local temporary
    temporary="$(/usr/bin/mktemp "${desktop%/*}/.webcam-controls-desktop.XXXXXX")"
    /usr/bin/awk -v executable="$install_prefix/bin/webcam-controls" -v config="$config_directory" \
        '/^Exec=/ { print "Exec=\"" executable "\" --config-dir \"" config "\""; next } { print }' \
        "$project_directory/assets/$desktop_id.desktop" > "$temporary"
    /usr/bin/chmod 644 -- "$temporary"
    /usr/bin/mv -T -- "$temporary" "$desktop"
    [[ -L "$registered" ]] || /usr/bin/ln -s -- "$desktop" "$registered"
    if [[ -x /usr/bin/update-desktop-database ]]; then
        /usr/bin/update-desktop-database "$install_prefix/share/applications"
    fi
}

register_app() {
    local entry="$desktop_id.desktop|$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg|Webcam Controls"
    if [[ -f "$apps_list" ]] && /usr/bin/awk -v entry="$entry" \
        '$0 == entry { found = 1 } END { exit !found }' "$apps_list"; then
        return
    fi
    local destination="$apps_list" temporary
    [[ ! -L "$apps_list" ]] || destination="$(/usr/bin/readlink -f -- "$apps_list")"
    [[ -n "$destination" ]] || fail 'App list symlink has no target'
    /usr/bin/mkdir -p -- "${destination%/*}"
    backup_file "$destination"
    temporary="$(/usr/bin/mktemp "${destination%/*}/.webcam-controls-apps.XXXXXX")"
    if [[ -f "$destination" ]]; then
        /usr/bin/awk -F '|' -v id="$desktop_id.desktop" -v entry="$entry" \
            '$1 == id { if (!found++) print entry; next } { print } END { if (!found) print entry }' "$destination" > "$temporary"
        /usr/bin/chmod --reference="$destination" "$temporary"
    else
        /usr/bin/chmod 644 -- "$temporary"
        printf '%s\n' "$entry" >> "$temporary"
    fi
    /usr/bin/mv -T -- "$temporary" "$destination"
}

configure_theme() {
    if [[ -n "$selector" && ( "$selector_explicit" == 1 || ! -f "$config_directory/theme.json" ) ]]; then
        backup_file "$config_directory/theme.json"
        "$install_prefix/bin/webcam-controls" --config-dir "$config_directory" theme follow "$selector" >/dev/null
    elif [[ ! -f "$config_directory/theme.json" ]]; then
        "$install_prefix/bin/webcam-controls" --config-dir "$config_directory" theme preset slate >/dev/null
    fi
}

main() {
    parse_options "$@"
    validate_installation
    if (( dry_run )); then
        printf 'Install executable: %s/bin/webcam-controls\nDesktop entry: %s/bin/applications/%s.desktop\nRegister desktop: %s/share/applications/%s.desktop\nApp list: %s\nSave theme: %s/theme.json\n' \
            "$install_prefix" "$install_prefix" "$desktop_id" "$install_prefix" "$desktop_id" "$apps_list" "$config_directory"
        [[ -z "$selector" ]] || printf 'Follow palette: %s\n' "$selector"
        return
    fi
    install_binary
    install_desktop
    register_app
    configure_theme
    printf 'Installed Webcam Controls.\n'
}

main "$@"
