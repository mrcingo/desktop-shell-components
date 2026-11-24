# Shell functions shared by the headless test and screenshot scripts. Source
# this file after setting build_directory, output_directory and
# tools_directory.
#
# Everything runs in a sway of its own with no screen and no input devices.
# Commands only ever go to that sway's socket, and cleanup only stops what
# was started here, so the desktop the scripts run from is never touched.

# Never talk to the desktop this runs from.
unset SWAYSOCK WAYLAND_DISPLAY

started_process_ids=""
program_process_ids=""
headless_socket=""

require_built_programs() {
    for program in "$@"; do
        if [ ! -x "$build_directory/$program" ]; then
            echo "Missing $program; build it with: meson compile -C $build_directory $(basename "$program")" >&2
            exit 1
        fi
    done
}

start_headless_compositor() {
    "$tools_directory/run-headless-compositor.sh" \
        "$tools_directory/headless-compositor-test-configuration" \
        > "$output_directory/compositor-environment" 2> "$output_directory/compositor.log" &

    started_process_ids="$started_process_ids $!"

    while ! grep -q '^SWAYSOCK=' "$output_directory/compositor-environment" 2> /dev/null; do
        sleep 0.1
    done

    export $(cat "$output_directory/compositor-environment")
    headless_socket=$SWAYSOCK
}

stop_everything_started() {
    if [ -n "$headless_socket" ]; then
        swaymsg -s "$headless_socket" exit > /dev/null 2>&1 || true
    fi

    if [ -n "$started_process_ids" ]; then
        kill $started_process_ids 2> /dev/null || true
    fi
}

start_in_background() {
    "$@" >> "$output_directory/programs.log" 2>&1 &
    started_process_ids="$started_process_ids $!"
    program_process_ids="$program_process_ids $!"
}

# The shipped style sheets of the source tree, not the installed ones.
use_source_tree_style_sheets() {
    export DESKTOP_SHELL_COMPONENTS_DATA_DIRECTORY="$tools_directory/../shared-data"
}

open_test_windows() {
    swaymsg -s "$headless_socket" 'exec foot -T first-terminal sh -c "echo first; exec sleep 600"' > /dev/null
    sleep 0.5
    swaymsg -s "$headless_socket" 'exec foot -T second-terminal sh -c "while :; do date +%T.%N; sleep 0.2; done"' > /dev/null
    sleep 0.5
    swaymsg -s "$headless_socket" 'layout tabbed; exec foot -T third-terminal top' > /dev/null
    sleep 1
}

focused_window_title() {
    swaymsg -s "$headless_socket" -t get_tree -r | "$tools_directory/print-focused-window-title.py"
}

# Holds Alt, presses Tab and keeps Alt down for about two seconds, long enough for screenshots.
hold_alt_tab_in_background() {
    "$build_directory/development-tools/send-virtual-key-presses" alt-down tab wait wait wait wait wait wait alt-up &
}
