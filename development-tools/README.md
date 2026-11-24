# Development tools

Tools for working on this repository. None of them are installed.

## Code layout

Every C file follows `.clang-format`, plus blank lines between the steps of
each function. Apply both with:

    development-tools/format-all-source-files.sh

It needs clang-format and python3. `meson test` then checks the rules that
formatting cannot fix:

* every function has at most 7 statements, and every if or loop body at
  most 3 (`check-function-size.py`, for C and Python),
* every file is plain ASCII (`check-plain-ascii-text.py`).

## Testing without your desktop

These scripts start their own headless sway, so nothing appears on your
screen and your session is never touched. They need sway, grim and foot.

    meson compile -C build send-virtual-key-presses login-form-widget-preview
    development-tools/test-window-components-in-headless-compositor.sh build OUTPUT_DIRECTORY
    development-tools/preview-login-form-in-headless-compositor.sh build SCREENSHOT.png [empty|error|long|question|open]

The first opens test windows, runs the bar and the switcher, presses Alt+Tab
and checks that the focus moved. It saves screenshots and prints each
program's idle memory, CPU time and wakeups.

## Other tools

| Tool                          | Use                                                        |
|-------------------------------|------------------------------------------------------------|
| `run-headless-compositor.sh`  | Start a headless sway with a given configuration           |
| `send-virtual-key-presses`    | Type keys into a headless sway: `alt-down tab wait alt-up` |
| `list-wayland-globals`        | List the Wayland protocols a compositor offers             |
| `print-focused-window-title.py` | Read sway's window tree and print the focused title      |
