# Webcam Controls

A lightweight, standalone Linux webcam client written in C++20 and Qt 6. Preview your camera, adjust its V4L2 controls, and change every interface color from the command line.

The resizable native Wayland window uses flat colors and recolorable vector icons. Device choices, control menus, and reset confirmations stay inside the window with a blurred backdrop.

## Requirements

- Linux with a V4L2 capture device
- Qt 6.6 or newer: Core, Gui, Widgets, Multimedia, and the Wayland platform plugin
- A Qt Multimedia capture backend
- `v4l2-ctl`, provided by `v4l-utils`
- CMake 3.24 or newer, a C++20 compiler, and Ninja or Make

Camera settings use `v4l2-ctl`; preview uses Qt Multimedia's camera capture API. These are the same camera tools used by the [reference implementation](https://github.com/kristoferlund/omarchy-webcam). No separate video-player executable is required.

## Build and install

From a source checkout:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel 4
bash scripts/install-desktop.sh --dry-run
bash scripts/install-desktop.sh
```

The installer puts the executable in `~/.local/bin`, writes a **Webcam Controls** desktop entry to `~/.local/bin/applications`, registers it with the standard application directory, and appends one entry to `~/.config/apps.list`. Existing application entries keep their order. Repeated installation does not duplicate the entry. Replaced configuration and desktop files receive timestamped backups.

An existing active palette directory is detected during initial installation. To select any other palette explicitly:

```bash
bash scripts/install-desktop.sh --selector /absolute/path/to/active-palette
```

Run `bash scripts/install-desktop.sh --help` for installation prefix, build directory, application list, and configuration directory options. Existing custom theme settings survive reinstallation unless `--selector` is supplied.

Standard CMake installation is also supported:

```bash
cmake --install build --prefix "$HOME/.local"
```

This installs the executable, icon, and standard desktop entry. The additional application list registration is performed by `install-desktop.sh`.

## Use

```bash
webcam-controls
webcam-controls --device /dev/video2
webcam-controls --no-preview
webcam-controls --preview-width 640 --preview-fps 15
```

Choose a camera from the header. Capture nodes are checked individually; metadata-only nodes are excluded. Multiple capture nodes belonging to the same camera remain available. The selected device is saved in `settings.json`.

The settings panel comes from the controls reported by the selected camera:

| Camera control | Interface |
| --- | --- |
| Boolean or integer range 0–1 | Toggle |
| Integer range | Slider |
| Menu or integer menu | In-window picker |
| Button or write-only operation | Action button |
| Integer64, bitmask, or string | Value field and Apply button |
| Read-only or unsupported compound payload | Value display |

Use the reset icon beside a setting to restore its reported default. **Reset defaults** asks for confirmation and resets only currently writable controls with defaults. Inactive, grabbed, disabled, and unsupported controls cannot be changed. Settings and flags are read again after writes, including automatic exposure and focus dependencies.

Press Escape or click outside a modal card to dismiss it. Refresh updates both devices and controls. Search filters settings by name or group.

Preview requests a modest 640-pixel capture width and 30 fps by default, choosing from the camera's supported formats. `--preview-width` and `--preview-fps` change those preferences; the driver may only offer larger or faster formats. Only the latest frame is retained, with display conversion limited to the requested frame rate. Pausing preview, minimizing, hiding, or closing the window releases its capture stream. Camera controls remain usable with preview paused.

## CLI camera controls

```bash
webcam-controls devices
webcam-controls controls --device /dev/video2
webcam-controls set brightness 32 --device /dev/video2 --dry-run
webcam-controls set brightness 32 --device /dev/video2
```

These commands return JSON and work without a graphical session. Decimal and hexadecimal integer values are accepted. Values are checked against the reported range, step, menu entries, and flags before being passed directly to `v4l2-ctl`. String values are passed as a single argument; commas, tabs, and line breaks are rejected.

`--dry-run` reads the device to validate the request and prints the proposed write without changing camera settings. Without `--device`, camera commands select the first available capture node.

## CLI themes

Colors are stored in `$XDG_CONFIG_HOME/webcam-controls/theme.json`, defaulting to `~/.config/webcam-controls/theme.json`. Theme commands write this JSON file atomically. Open windows watch the file and reload colors without restarting or changing camera settings.

```bash
webcam-controls theme list
webcam-controls theme preset slate
webcam-controls theme set 'accent=#fab387' 'accent_text=#202020'
webcam-controls theme show
webcam-controls theme path
webcam-controls theme export ./my-theme.json
webcam-controls theme import ./my-theme.json
```

Built-in palettes are `slate`, `paper`, `forest`, and `rose`. Add `--dry-run` to a theme write to validate and print its result without changing any files.

A complete theme looks like this:

```json
{
  "version": 1,
  "name": "slate",
  "colors": {
    "background": "#14171e",
    "surface": "#1b2029",
    "surface_alt": "#252c38",
    "text": "#edf0f7",
    "muted": "#a1acbf",
    "accent": "#a8c3f0",
    "accent_text": "#18283e",
    "border": "#343e4d",
    "danger": "#f0a6ae"
  }
}
```

All colors use `#RRGGBB`. Unknown color keys and incomplete themes are rejected. Invalid live edits leave the last accepted colors on screen.

### Follow an external palette

```bash
webcam-controls theme follow ~/.config/palettes/active
webcam-controls theme follow /absolute/path/to/colors.json
webcam-controls theme unfollow
```

Following saves both the resolved colors and the source path in `theme.json`. Edits, atomic file replacements, and palette-directory symlink changes update the saved colors and the running interface. When the app is closed, the last accepted colors remain saved; the source is refreshed when it opens again. No background service is needed.

Supported sources include complete application themes, flat JSON palettes, JSON `colors` or `colours` objects, `dark`/`light` objects selected by `mode`, and simple `Colors.qml` files containing literal color properties. Files are parsed as data; QML code is never executed. Palette directories can contain `webcam-controls.json`, `Colors.qml`, `colors.json`, or `palette.json`.

For example, this minimal external palette is sufficient:

```json
{
  "background": "#181825",
  "foreground": "#cdd6f4",
  "primary": "#cba6f7"
}
```

Remaining interface colors are derived from the palette. `theme import` copies a palette once; `theme follow` keeps tracking it. `theme preset`, `theme set`, and `theme import` detach an existing source binding. `theme unfollow` keeps the last saved colors.

Use `--config-dir /absolute/path` or `WEBCAM_CONTROLS_CONFIG_DIR` for a separate configuration. `WEBCAM_CONTROLS_V4L2_CTL` can select a particular `v4l2-ctl` executable.

## Validation

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Qt Test is required for the test targets. Tests cover driver output parsing, safe command arguments, control constraints and failures, theme transitions and persistence, embedded modals, headless CLI commands, and staged installation. Camera fixtures test command behavior without changing hardware settings. Actual capture and driver behavior should also be checked with a real camera.

## License

MIT. See [LICENSE](LICENSE).
