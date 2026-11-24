#!/bin/sh
# End-to-end test of the workspace window bar and the window switcher in a
# headless compositor: opens three terminals (one in a background tab),
# starts both programs, presses Alt+Tab with a virtual keyboard and checks
# that the previously focused window gains focus. Also checks that a second
# copy exits at once and reports idle resource use.
#
# Usage: test-window-components-in-headless-compositor.sh BUILD_DIRECTORY OUTPUT_DIRECTORY
set -eu

build_directory=$(realpath "$1")
output_directory=$(realpath "$2")
tools_directory=$(realpath "$(dirname "$0")")

. "$tools_directory/headless-compositor-session.sh"

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

# sway starts both programs on every configuration reload; a second copy must leave at once.
check_second_copies_exit() {
    for program in desktop-shell-workspace-window-bar desktop-shell-window-switcher; do
        timeout 5 "$build_directory"/programs/*/"$program"
        echo "second $program exited with status $?"
    done
}

# The workspace's top edge in pixels: the space bars reserve above it.
workspace_top_edge() {
    swaymsg -s "$headless_socket" -t get_workspaces -r | python3 -c 'import json, sys; print(next(w for w in json.load(sys.stdin) if w["focused"])["rect"]["y"])'
}

# Settings apply on "swaymsg reload": moving the bar to the top must move the workspace down.
check_configuration_reload() {
    top_edge_before=$(workspace_top_edge)
    mkdir -p "$XDG_CONFIG_HOME/desktop-shell-components"
    printf '[bar]\nposition = top\n' > "$XDG_CONFIG_HOME/desktop-shell-components/workspace-window-bar.ini"
    swaymsg -s "$headless_socket" reload > /dev/null
    sleep 1.5
    top_edge_after=$(workspace_top_edge)

    echo "workspace top edge before and after moving the bar to the top: $top_edge_before, $top_edge_after"
    test "$top_edge_after" -gt "$top_edge_before"
}

switch_with_alt_tab_and_screenshot() {
    hold_alt_tab_in_background
    sleep 1.4
    grim "$output_directory/window-switcher.png"
    wait $!
    sleep 0.5
}

run_test() {
    require_built_programs \
        programs/workspace-window-bar/desktop-shell-workspace-window-bar \
        programs/window-switcher/desktop-shell-window-switcher \
        development-tools/send-virtual-key-presses
    start_headless_compositor
    open_test_windows
    use_source_tree_style_sheets
    export XDG_CONFIG_HOME="$output_directory/configuration-home"
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
    check_configuration_reload
}

trap stop_everything_started EXIT
mkdir -p "$output_directory"
run_test
