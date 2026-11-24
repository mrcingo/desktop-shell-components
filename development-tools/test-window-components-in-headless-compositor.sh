#!/bin/sh
# End-to-end test of the workspace window bar and the window switcher,
# entirely inside a headless compositor: nothing appears on the desktop.
#
# Opens three terminals (one in a background tab), starts both programs,
# presses Alt+Tab with a virtual keyboard and checks that the previously
# focused window gains focus. Screenshots land in OUTPUT_DIRECTORY.
#
# Usage: test-window-components-in-headless-compositor.sh BUILD_DIRECTORY OUTPUT_DIRECTORY
set -eu

# Never talk to the desktop this runs from: every compositor command below
# goes to the headless compositor's own socket, once it exists.
unset SWAYSOCK WAYLAND_DISPLAY

build_directory=$(realpath "$1")
output_directory=$(realpath "$2")
tools_directory=$(realpath "$(dirname "$0")")
started_process_ids=""
headless_socket=""

required_programs="
    programs/workspace-window-bar/desktop-shell-workspace-window-bar
    programs/window-switcher/desktop-shell-window-switcher
    development-tools/send-virtual-key-presses
"

require_built_programs() {
    for program in $required_programs; do
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

    while ! grep -q '^SWAYSOCK=' "$output_directory/compositor-environment" 2>/dev/null; do
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

program_process_ids=""

start_in_background() {
    "$@" >> "$output_directory/programs.log" 2>&1 &
    started_process_ids="$started_process_ids $!"
    program_process_ids="$program_process_ids $!"
}

# Resident memory, plus CPU ticks and wakeups (context switches) spent while nothing happens.
idle_counters() {
    awk '{ print $14 + $15 }' "/proc/$1/stat"
    awk '/^VmRSS/ { print $2 } /^voluntary_ctxt_switches/ { print $2 }' "/proc/$1/status"
}

report_idle_resource_usage() {
    for process_id in $program_process_ids; do
        set -- $(idle_counters "$process_id")
        sleep 5
        set -- "$@" $(idle_counters "$process_id")
        echo "$(cat "/proc/$process_id/comm"): ${5} KiB resident, $(( $4 - $1 )) CPU ticks and $(( $6 - $3 )) wakeups in 5 idle seconds"
    done
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

# sway starts both programs on every configuration reload; a second copy must leave at once.
check_second_copies_exit() {
    for program in desktop-shell-workspace-window-bar desktop-shell-window-switcher; do
        timeout 5 "$build_directory"/programs/*/"$program"
        echo "second $program exited with status $?"
    done
}

switch_with_alt_tab_and_screenshot() {
    "$build_directory/development-tools/send-virtual-key-presses" alt-down tab wait wait wait wait alt-up &
    sleep 1.4
    grim "$output_directory/window-switcher.png"
    wait $!
    sleep 0.5
}

run_test() {
    require_built_programs
    start_headless_compositor
    open_test_windows

    export DESKTOP_SHELL_COMPONENTS_DATA_DIRECTORY="$tools_directory/../shared-data"
    start_in_background "$build_directory/programs/workspace-window-bar/desktop-shell-workspace-window-bar"
    start_in_background "$build_directory/programs/window-switcher/desktop-shell-window-switcher"
    sleep 1.5

    check_second_copies_exit
    grim -g "0,1178 1920x22" "$output_directory/workspace-window-bar.png"
    title_before=$(focused_window_title)
    switch_with_alt_tab_and_screenshot
    title_after=$(focused_window_title)

    echo "focused before Alt+Tab: $title_before"
    echo "focused after Alt+Tab:  $title_after"
    report_idle_resource_usage
    test "$title_before" != "$title_after"
}

trap stop_everything_started EXIT
mkdir -p "$output_directory"
run_test
