#!/bin/sh
# Edge cases of window management in a headless compositor with two
# monitors: keyboard snapping, monitors, new windows, windows off screen,
# the window menu and layout tidying. Prints PASS or FAIL per scenario and
# exits with the number of failures. Runs itself on a private session bus.
#
# The compositor configuration is built from SWAY_CONFIGURATION (a sway
# configuration such as your own; default: the repository's test
# configuration) without its exec, include, output and status_command
# lines, plus the window snap and window menu drop-ins pointing at the
# build, and the Win+arrow bindings.
#
# SCENARIOS="name ..." runs only those scenarios.
#
# Usage: test-window-management-edge-cases-in-headless-compositor.sh BUILD_DIRECTORY OUTPUT_DIRECTORY [SWAY_CONFIGURATION]
set -u

if [ -z "${DBUS_SESSION_BUS_ADDRESS_IS_PRIVATE:-}" ]; then
    export DBUS_SESSION_BUS_ADDRESS_IS_PRIVATE=1
    exec dbus-run-session -- "$0" "$@"
fi

unset SWAYSOCK WAYLAND_DISPLAY

build_directory=$(realpath "$1")
output_directory=$(realpath "$2")
tools_directory=$(realpath "$(dirname "$0")")
source_directory=$(realpath "$tools_directory/..")
sway_configuration=${3:-$tools_directory/headless-compositor-test-configuration}
failures=0

pointer="$build_directory/development-tools/send-virtual-pointer-events"
keys="$build_directory/development-tools/send-virtual-key-presses"
snap_program="$build_directory/programs/window-snap/desktop-shell-window-snap"
menu_program="$build_directory/programs/window-context-menu/desktop-shell-window-context-menu"

# The input tools are not built by default; without them no key or click arrives.
for tool in "$pointer" "$keys"; do
    if [ ! -x "$tool" ]; then
        echo "missing $tool: build it with ninja -C $build_directory development-tools/$(basename "$tool")" >&2
        exit 1
    fi
done

mkdir -p "$output_directory/configuration-home/desktop-shell-components"

write_test_configuration() {
    echo 'output HEADLESS-1 resolution 1920x1200 position 0 0'
    echo 'output HEADLESS-2 resolution 1280x1024 position 1920 0'
    echo 'output * bg #6699cc solid_color'
    # Keys bound again below are left out: sway warns about bindings it overwrites.
    grep -vE '^\s*(exec|exec_always|include|output|status_command)\b' "$sway_configuration" |
        grep -vE '^\s*bindsym\s+(\$mod|Mod4)\+(Shift\+)?(Left|Right|Up|Down|Tab)\s'
    echo "exec_always $snap_program"
    echo "exec $menu_program"
    echo 'bindsym --border button3 mark --add _window-context-menu-target, nop desktop-shell-window-context-menu'
    echo 'mode "desktop-shell-window-snap" {'
    echo '    bindsym Escape nop desktop-shell-window-snap cancel'
    echo '}'
    echo 'bindsym Mod4+Left  nop desktop-shell-window-snap left'
    echo 'bindsym Mod4+Right nop desktop-shell-window-snap right'
    echo 'bindsym Mod4+Up    nop desktop-shell-window-snap up'
    echo 'bindsym Mod4+Down  nop desktop-shell-window-snap down'
    echo 'bindsym Mod4+Shift+Left  nop desktop-shell-window-snap monitor-left'
    echo 'bindsym Mod4+Shift+Right nop desktop-shell-window-snap monitor-right'
    echo 'bindsym Mod4+Tab layout toggle split tabbed stacking'
}

start_session() {
    write_test_configuration > "$output_directory/sway.conf"
    printf '[snap]\nfloating-border-width = 2\ntiled-border-width = 2\n' > "$output_directory/configuration-home/desktop-shell-components/window-snap.ini"
    mkdir -p "$output_directory/configuration-home/foot"
    printf '[main]\nresize-by-cells=no\n' > "$output_directory/configuration-home/foot/foot.ini"

    export XDG_CONFIG_HOME="$output_directory/configuration-home"
    export DESKTOP_SHELL_COMPONENTS_DATA_DIRECTORY="$source_directory/shared-data"
    export G_MESSAGES_DEBUG="window-snap window-context-menu window-context-menu-popover"
    export WLR_HEADLESS_OUTPUTS=2
    export VIRTUAL_POINTER_LAYOUT_SIZE=3200x1200

    # GLib writes debug messages to standard output, next to the session's variables.
    "$tools_directory/run-headless-compositor.sh" "$output_directory/sway.conf" > "$output_directory/programs.log" 2> "$output_directory/errors.log" &
    sleep 3
    grep -E '^(WAYLAND_DISPLAY|SWAYSOCK)=' "$output_directory/programs.log" > "$output_directory/environment"
    . "$output_directory/environment"
    export WAYLAND_DISPLAY SWAYSOCK
    compositor 'focus output HEADLESS-1'

    # The first keys of a new virtual keyboard can arrive before the compositor has set it up.
    "$keys" shift-down shift-up > /dev/null 2>&1
}

compositor() {
    swaymsg -s "$SWAYSOCK" "$@" > /dev/null
}

query() {
    "$tools_directory/query-window-tree.py" "$@"
}

check() {
    if [ "$2" = "$3" ]; then
        echo "PASS  $1"
        return
    fi

    echo "FAIL  $1: expected [$3], got [$2]"
    failures=$((failures + 1))
}

check_true() {
    if eval "$2"; then
        echo "PASS  $1"
        return
    fi

    echo "FAIL  $1 ($2)"
    failures=$((failures + 1))
}

# Opens a terminal whose application ID is $1 and waits for it.
open_window() {
    compositor exec "foot -a $1 ${2:-}"
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        [ "$(query frame "$1")" != missing ] && break
        sleep 0.3
    done
    sleep 0.4
}

close_all_windows() {
    compositor '[all] kill'
    sleep 0.8
    compositor 'focus output HEADLESS-1'
}

press() {
    "$keys" "$@" > /dev/null 2>&1
    sleep 0.5
}

fast_press() {
    VIRTUAL_KEY_STEP_MILLISECONDS=40 "$keys" "$@" > /dev/null 2>&1
    sleep 1
}

# The workspace area of an output split into halves and quarters: x y width height.
area() {
    set -- $(query workspace "$1") "$2"
    x=$1; y=$2; w=$3; h=$4; half_w=$((w / 2)); half_h=$((h / 2))
    case $5 in
        left)        echo "$x $y $half_w $h" ;;
        right)       echo "$((x + half_w)) $y $((w - half_w)) $h" ;;
        top-left)    echo "$x $y $half_w $half_h" ;;
        bottom-left) echo "$x $((y + half_h)) $half_w $((h - half_h))" ;;
        whole)       echo "$x $y $w $h" ;;
    esac
}

commands_since() {
    tail -n +"$1" "$output_directory/programs.log" | grep -c 'window-snap-DEBUG.*command:'
}

log_lines() {
    wc -l < "$output_directory/programs.log"
}

scenario_keyboard_snapping() {
    close_all_windows
    press super-down left super-up
    check_true "Win+Left on an empty workspace keeps the program running" 'kill -0 $(pgrep -n -f "^$snap_program")'

    open_window one
    original=$(query frame one)
    press super-down left super-up
    check "Win+Left: left half" "$(query frame one)" "$(area HEADLESS-1 left)"
    press super-down up super-up
    check "then Win+Up: top-left quarter" "$(query frame one)" "$(area HEADLESS-1 top-left)"
    press super-down down super-up
    check "then Win+Down: left half again" "$(query frame one)" "$(area HEADLESS-1 left)"
    press super-down right super-up
    check "then Win+Right: back where it was" "$(query frame one)" "$original"
    press super-down up super-up
    check "Win+Up on a free window: maximized" "$(query frame one)" "$(area HEADLESS-1 whole)"
    press super-down down super-up
    check "then Win+Down: restored" "$(query frame one)" "$original"
}

scenario_fast_presses() {
    close_all_windows
    open_window one
    original=$(query frame one)
    start=$(log_lines)
    fast_press super-down left left left left left left left left left left super-up
    check "10 fast Win+Left: left half" "$(query frame one)" "$(area HEADLESS-1 left)"
    check_true "10 fast Win+Left send at most 2 commands" "[ $(commands_since "$start") -le 2 ]"
    fast_press super-down right left right left right left right left right super-up
    check "fast alternating Left/Right ends deterministically back where it was" "$(query frame one)" "$original"
}

scenario_moved_after_snapping() {
    close_all_windows
    open_window one
    press super-down left super-up
    set -- $(query frame one)
    "$pointer" move:$(($1 + 100)),$(($2 + 10)) left-down move:$(($1 + 200)),$(($2 + 110)) move:$(($1 + 300)),$(($2 + 210)) left-up wait > /dev/null 2>&1
    press super-down up super-up
    check "dragged away after Win+Left, then Win+Up maximizes (not a quarter)" "$(query frame one)" "$(area HEADLESS-1 whole)"
}

scenario_fullscreen_and_tiled() {
    close_all_windows
    open_window one
    compositor '[app_id=one] fullscreen enable'
    sleep 0.4
    press super-down left super-up
    check "Win+Left on a fullscreen window: windowed left half" "$(query state one | cut -d' ' -f2) $(query frame one)" "windowed $(area HEADLESS-1 left)"

    close_all_windows
    open_window one
    compositor '[app_id=one] floating disable'
    open_window two
    compositor '[app_id=two] floating disable'
    compositor '[app_id=two] focus'
    press super-down left super-up
    check "Win+Left on a tiled window: floating left half" "$(query state two | cut -d' ' -f1) $(query frame two)" "floating $(area HEADLESS-1 left)"
    press super-down right super-up
    check "then Win+Right: back in the tiling" "$(query state two | cut -d' ' -f1)" "tiled"
}

scenario_two_monitors() {
    close_all_windows
    open_window one
    compositor '[app_id=one] move container to output HEADLESS-2'
    compositor '[app_id=one] focus'
    sleep 0.4
    press super-down left super-up
    check "Win+Left on the right monitor: its left half" "$(query frame one)" "$(area HEADLESS-2 left)"
    press super-down left super-up
    check "Win+Left again: right half of the monitor to the left" "$(query state one | cut -d' ' -f4) $(query frame one)" "HEADLESS-1 $(area HEADLESS-1 right)"
    press super-down left super-up
    set -- $(query frame one) $(query workspace HEADLESS-1)
    check_true "then Win+Left restores it, as on Windows, fully on that monitor" "[ $1 -ge $5 ] && [ $2 -ge $6 ] && [ $(($1 + $3)) -le $(($5 + $7)) ] && [ $(($2 + $4)) -le $(($6 + $8)) ]"
    press super-down left super-up
    check "then Win+Left: its left half" "$(query frame one)" "$(area HEADLESS-1 left)"
    press super-down left super-up
    check "and with no monitor further left it stays" "$(query frame one)" "$(area HEADLESS-1 left)"

    close_all_windows
    open_window one
    press super-down shift-down right shift-up super-up
    check "Win+Shift+Right: moved to the right monitor" "$(query state one | cut -d' ' -f4)" "HEADLESS-2"
    set -- $(query frame one) $(query workspace HEADLESS-2)
    check_true "and fully inside its workspace area" "[ $1 -ge $5 ] && [ $2 -ge $6 ] && [ $(($1 + $3)) -le $(($5 + $7)) ] && [ $(($2 + $4)) -le $(($6 + $8)) ]"
}

scenario_monitor_removed() {
    close_all_windows
    open_window one
    compositor '[app_id=one] move container to output HEADLESS-2'
    compositor '[app_id=one] focus'
    sleep 0.4
    press super-down up super-up
    compositor 'output HEADLESS-2 disable'
    sleep 1
    press super-down down super-up
    set -- $(query frame one) $(query workspace HEADLESS-1)
    check_true "maximized on a monitor that went away, restored: fully on the remaining monitor" "[ $1 -ge $5 ] && [ $2 -ge $6 ] && [ $(($1 + $3)) -le $(($5 + $7)) ] && [ $(($2 + $4)) -le $(($6 + $8)) ]"
    compositor 'output HEADLESS-2 enable'
    sleep 1
}

scenario_new_windows() {
    close_all_windows
    open_window big "--window-size-pixels=3000x2500"
    set -- $(query frame big) $(query workspace HEADLESS-1)
    check_true "a new window bigger than the screen fits inside the workspace area" "[ $1 -ge $5 ] && [ $2 -ge $6 ] && [ $(($1 + $3)) -le $(($5 + $7)) ] && [ $(($2 + $4)) -le $(($6 + $8)) ]"

    close_all_windows
    open_window one
    open_window two
    open_window three
    corners=$(for name in one two three; do query frame $name | cut -d' ' -f1,2; done | sort -u | wc -l)
    check "three new windows of the same size cascade (three different corners)" "$corners" "3"
}

scenario_off_screen() {
    close_all_windows
    open_window one
    compositor '[app_id=one] move container to output HEADLESS-2'
    compositor '[app_id=one] move absolute position 2700 700'
    sleep 0.5
    compositor 'output HEADLESS-2 disable'
    sleep 1.5
    set -- $(query frame one) $(query workspace HEADLESS-1)
    check_true "a window left beyond a monitor that went away is brought fully onto the remaining one" "[ $1 -ge $5 ] && [ $2 -ge $6 ] && [ $(($1 + $3)) -le $(($5 + $7)) ] && [ $(($2 + $4)) -le $(($6 + $8)) ]"
    compositor 'output HEADLESS-2 enable'
    sleep 1
}

# Right-clicks at x, y, then Escape from a keyboard that exists before the
# menu opens, as a real keyboard does (a keyboard added later gets no focus).
right_click_then_escape() {
    VIRTUAL_KEY_STEP_MILLISECONDS=400 "$keys" wait wait wait wait escape > /dev/null 2>&1 &
    keyboard_process=$!
    sleep 0.3
    "$pointer" move:$1,$2 right-click > /dev/null 2>&1
    wait "$keyboard_process"
    sleep 0.3
}

menus_opened_since() {
    tail -n +"$1" "$output_directory/programs.log" | grep -c 'menu opened'
}

scenario_window_menu() {
    close_all_windows
    open_window one
    open_window two
    compositor '[app_id=one] move absolute position 100 200'
    compositor '[app_id=two] move absolute position 1000 200'
    sleep 0.5
    start=$(log_lines)
    for round in 1 2 3 4 5 6 7 8 9 10; do
        for name in one two; do
            set -- $(query frame $name)
            right_click_then_escape $(($1 + $3 / 2)) $(($2 + 8))
            right_click_then_escape $(($1 + $3 - 1)) $(($2 + $4 / 2))
        done
        set -- $(query frame one)
        "$pointer" move:$(($1 + 40)),$(($2 + 8)) left-down move:$(($1 + 60)),$(($2 + 20)) left-up > /dev/null 2>&1
    done
    check "40 right-clicks on title bars and borders, with Escape and drags between, open 40 menus" "$(menus_opened_since "$start")" "40"

    set -- $(query frame one)
    start=$(log_lines)
    "$pointer" move:$(($1 + $3 / 2)),$(($2 + 8)) right-click > /dev/null 2>&1
    set -- $(query frame two)
    "$pointer" move:$(($1 + $3 / 2)),$(($2 + 8)) right-click > /dev/null 2>&1
    sleep 0.5
    check "a right-click on another title bar while a menu is open opens that window's menu" "$(menus_opened_since "$start")" "2"
    "$pointer" move:5,600 left-click > /dev/null 2>&1
    sleep 0.5

    compositor '[app_id=two] move container to output HEADLESS-2'
    compositor '[app_id=two] move absolute position 2500 600'
    sleep 0.5
    set -- $(query frame two)
    start=$(log_lines)
    "$pointer" move:$(($1 + $3 - 20)),$(($2 + 8)) right-click wait > /dev/null 2>&1
    sleep 0.5
    grim "$output_directory/menu-at-right-edge-of-second-monitor.png" 2> /dev/null
    check "the menu opens on the second monitor, at its right edge" "$(menus_opened_since "$start")" "1"
    # 1024 pixels high: the menu fits neither below nor above the title bar.
    moved=$(tail -n +"$start" "$output_directory/programs.log" | grep -c 'menu moved up to fit: 39[0-9] pixels')
    check "a menu too tall for either side of the title bar is moved up until all of it shows" "$moved" "1"
    "$pointer" move:2000,100 left-click > /dev/null 2>&1
    sleep 0.5

    set -- $(query frame two)
    "$pointer" move:$(($1 + $3 / 2)),$(($2 + 8)) right-click wait > /dev/null 2>&1
    compositor '[app_id=two] kill'
    sleep 1
    check_true "a menu whose window closes, closes" "tail -n 5 '$output_directory/programs.log' | grep -q 'menu closed: its window is gone'"
    check_true "the menu program keeps running" 'kill -0 $(pgrep -n -f "^$menu_program")'
}

scenario_tidying() {
    close_all_windows
    open_window one
    compositor '[app_id=one] floating disable'
    open_window two
    compositor '[app_id=two] floating disable'
    compositor '[app_id=two] focus; layout tabbed; focus parent; split v; layout tabbed; focus child'
    start=$(log_lines)
    press super-down tab super-up
    sleep 1
    tidies=$(tail -n +"$start" "$output_directory/programs.log" | grep -c 'split none')
    check "a nested tab group is tidied with one command" "$tidies" "1"
}

scenario_border_resizing() {
    close_all_windows
    open_window one
    for inset in 0; do
        set -- $(query frame one)
        before="$3 $4"
        x=$(($1 + $3 - 1 - inset)); y=$(($2 + $4 / 2))
        "$pointer" move:$x,$y left-down move:$((x + 40)),$y move:$((x + 80)),$y left-up wait > /dev/null 2>&1
        set -- $(query frame one)
        check_true "dragging the right border $inset px inside the edge resizes" "[ '$3 $4' != '$before' ]"
    done
    check "the compositor is back in its default mode" "$(swaymsg -s "$SWAYSOCK" -t get_binding_state -r | python3 -c 'import json, sys; print(json.load(sys.stdin)["name"])')" "default"
}

start_session

# SCENARIOS, such as "window_menu off_screen", runs only those.
for scenario in ${SCENARIOS:-keyboard_snapping fast_presses moved_after_snapping fullscreen_and_tiled two_monitors monitor_removed new_windows off_screen window_menu tidying border_resizing}; do
    "scenario_$scenario"
done

grim "$output_directory/final.png" 2> /dev/null
compositor exit
echo "$failures failure(s)"
exit "$failures"
