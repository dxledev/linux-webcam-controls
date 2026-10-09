#!/usr/bin/env bash
set -euo pipefail

project_directory="$1"
build_directory="$2"
test_directory="$(/usr/bin/mktemp -d /tmp/webcam-controls-install.XXXXXX)"
install_prefix="$test_directory/prefix"
config_directory="$test_directory/config/webcam-controls"
apps_list="$test_directory/config/apps.list"
/usr/bin/mkdir -p "$test_directory/config" "$test_directory/palette"
printf 'existing.desktop|existing-icon|Existing App\n' > "$apps_list"
printf '{"background":"#111111","foreground":"#eeeeee","primary":"#abcabc"}\n' > "$test_directory/palette/colors.json"
arguments=(--build-dir "$build_directory" --prefix "$install_prefix" --config-dir "$config_directory" --apps-list "$apps_list" --selector "$test_directory/palette")
/usr/bin/bash "$project_directory/scripts/install-desktop.sh" "${arguments[@]}" --dry-run >/dev/null
[[ ! -e "$install_prefix" && ! -e "$config_directory" ]]
/usr/bin/bash "$project_directory/scripts/install-desktop.sh" "${arguments[@]}" >/dev/null
/usr/bin/cmp "$build_directory/webcam-controls" "$install_prefix/bin/webcam-controls"
/usr/bin/grep -qx 'Name=Webcam Controls' "$install_prefix/bin/applications/io.github.webcam_controls.desktop"
[[ -L "$install_prefix/share/applications/io.github.webcam_controls.desktop" ]]
[[ "$(/usr/bin/readlink "$install_prefix/share/applications/io.github.webcam_controls.desktop")" == "$install_prefix/bin/applications/io.github.webcam_controls.desktop" ]]
/usr/bin/grep -qx 'existing.desktop|existing-icon|Existing App' "$apps_list"
[[ "$(/usr/bin/grep -c '^io.github.webcam_controls.desktop|' "$apps_list")" == 1 ]]
/usr/bin/jq -e '.colors.accent == "#abcabc"' "$config_directory/theme.json" >/dev/null
/usr/bin/bash "$project_directory/scripts/install-desktop.sh" "${arguments[@]}" >/dev/null
[[ "$(/usr/bin/grep -c '^io.github.webcam_controls.desktop|' "$apps_list")" == 1 ]]
if [[ -x /usr/bin/desktop-file-validate ]]; then
    /usr/bin/desktop-file-validate "$install_prefix/bin/applications/io.github.webcam_controls.desktop"
fi
printf 'Installation checks passed.\n'
