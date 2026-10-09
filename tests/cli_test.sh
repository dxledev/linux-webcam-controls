#!/usr/bin/env bash
set -euo pipefail

binary="$1"
test_directory="$(/usr/bin/mktemp -d /tmp/webcam-controls-cli.XXXXXX)"
config_directory="$test_directory/config"
fixture_directory="$(cd -- "${BASH_SOURCE[0]%/*}/fixtures" && pwd)"
export WEBCAM_CONTROLS_V4L2_CTL="$fixture_directory/v4l2-ctl"
export WEBCAM_CONTROLS_TEST_LOG="$test_directory/commands.log"
export QT_QPA_PLATFORM=invalid

"$binary" --help >/dev/null
"$binary" --version | /usr/bin/grep -qx 'Webcam Controls 1.0.0'
"$binary" --config-dir "$config_directory" theme preset paper --dry-run > "$test_directory/preview.json"
[[ ! -e "$config_directory" ]]
"$binary" --config-dir "$config_directory" theme preset slate >/dev/null
"$binary" --config-dir "$config_directory" theme set 'accent=#abcdef' >/dev/null
/usr/bin/jq -e '.colors.accent == "#abcdef" and .name == "custom"' "$config_directory/theme.json" >/dev/null
before="$(/usr/bin/sha256sum "$config_directory/theme.json")"
if "$binary" --config-dir "$config_directory" theme set 'accent=red' 2>/dev/null; then exit 1; fi
[[ "$before" == "$(/usr/bin/sha256sum "$config_directory/theme.json")" ]]
"$binary" --config-dir "$config_directory" theme export "$test_directory/export.json"
"$binary" --config-dir "$config_directory" theme import "$test_directory/export.json" >/dev/null
"$binary" --config-dir "$config_directory" theme follow "$test_directory/export.json" >/dev/null
/usr/bin/jq -e --arg path "$test_directory/export.json" '.source == $path' "$config_directory/theme.json" >/dev/null
"$binary" --config-dir "$config_directory" theme unfollow >/dev/null
/usr/bin/jq -e 'has("source") | not' "$config_directory/theme.json" >/dev/null
if "$binary" --config-dir "$config_directory" theme follow "$config_directory/theme.json" 2>/dev/null; then exit 1; fi
"$binary" devices | /usr/bin/jq -e '.devices | length == 2' >/dev/null
"$binary" controls --device /dev/video2 | /usr/bin/jq -e '.device == "/dev/video2" and (.controls | length == 16)' >/dev/null
"$binary" set brightness 20 --device /dev/video0 --dry-run | /usr/bin/jq -e '.arguments[1] == "--set-ctrl=brightness=20"' >/dev/null
if /usr/bin/grep -q -- '--set-ctrl=brightness=20' "$WEBCAM_CONTROLS_TEST_LOG"; then exit 1; fi
"$binary" set brightness 20 --device /dev/video0 | /usr/bin/jq -e '.value == "20"' >/dev/null
if "$binary" set brightness '20,contrast=0' --device /dev/video0 2>/dev/null; then exit 1; fi
if "$binary" set zoom_absolute 106 --device /dev/video0 2>/dev/null; then exit 1; fi
if "$binary" controls --device /dev/video1 2>/dev/null; then exit 1; fi
if "$binary" set contrast 99 --device /dev/video0 2>/dev/null; then exit 1; fi
if "$binary" --device /tmp/video0 devices 2>/dev/null; then exit 1; fi
WEBCAM_CONTROLS_TEST_EMPTY=1 "$binary" controls --device /dev/video0 | /usr/bin/jq -e '.controls == []' >/dev/null
printf '{invalid\n' > "$config_directory/theme.json"
"$binary" --config-dir "$config_directory" theme preset paper >/dev/null
/usr/bin/jq -e '.name == "paper"' "$config_directory/theme.json" >/dev/null
printf 'CLI checks passed.\n'
