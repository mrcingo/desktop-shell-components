#!/bin/sh
# End-to-end test of the notification daemon, on a private session bus of its
# own and in a headless compositor, so neither the desktop nor its real
# session bus is touched. Sounds go to a fake player that only writes down
# what it would play. Checks capabilities, sounds and their rules, the status
# bar properties, bringing up the sender's window, expiry, closing,
# do-not-disturb and per-application rules, and reports idle resource use.
#
# Usage: test-notification-daemon-in-headless-compositor.sh BUILD_DIRECTORY OUTPUT_DIRECTORY
set -eu

# A private bus for everything below; the real session bus is never used.
if [ -z "${NOTIFICATION_TEST_ON_PRIVATE_BUS:-}" ]; then
    unset DBUS_SESSION_BUS_ADDRESS
    exec env NOTIFICATION_TEST_ON_PRIVATE_BUS=1 dbus-run-session -- "$0" "$@"
fi

build_directory=$(realpath "$1")
mkdir -p "$2"
output_directory=$(realpath "$2")
tools_directory=$(realpath "$(dirname "$0")")
failures=0

. "$tools_directory/headless-compositor-session.sh"
trap stop_everything_started EXIT

sounds_log="$output_directory/played-sounds.log"
signals_log="$output_directory/notification-signals.log"
center_path=/io/github/mrcingo/DesktopShellComponents/NotificationCenter
center_interface=io.github.mrcingo.DesktopShellComponents.NotificationCenter

# check DESCRIPTION CONDITION: the condition is shell code run here, so it can use the functions below.
check() {
    if eval "$2"; then
        echo "PASS $1"
    else
        echo "FAIL $1"
        failures=$((failures + 1))
    fi
}

write_daemon_settings() {
    cat > "$output_directory/notification-daemon.ini" <<SETTINGS
[sound]
player = sh -c 'echo "\$0 \$1" >> $sounds_log' {volume}
volume = 40
file = /test/chime.wav
critical-file = /test/critical-chime.wav

[timeouts]
normal = 1

[app:Ignored]
ignore = true
SETTINGS
}

notify() {
    python3 "$tools_directory/send-test-notification.py" "$@"
}

notifications_call() {
    method=$1
    shift
    gdbus call --session --dest org.freedesktop.Notifications --object-path /org/freedesktop/Notifications --method "org.freedesktop.Notifications.$method" "$@"
}

center_call() {
    gdbus call --session --dest org.freedesktop.Notifications --object-path $center_path --method "$center_interface.$1" > /dev/null
}

center_property() {
    gdbus call --session --dest org.freedesktop.Notifications --object-path $center_path \
        --method org.freedesktop.DBus.Properties.Get $center_interface "$1"
}

sound_count() {
    if [ -f "$sounds_log" ]; then
        wc -l < "$sounds_log"
    else
        echo 0
    fi
}

# Sounds are at most one per 1.5 seconds; tests that expect a sound wait that long first.
wait_for_sound_interval() {
    sleep 1.7
}

last_sound() {
    tail -n 1 "$sounds_log"
}

open_windows() {
    swaymsg -s "$headless_socket" 'exec foot --app-id=Slack -T slack-window sh -c "exec sleep 600"' > /dev/null
    sleep 0.6
    swaymsg -s "$headless_socket" 'exec foot -T other-window sh -c "exec sleep 600"' > /dev/null
    sleep 0.6
}

start_daemon() {
    write_daemon_settings
    start_in_background "$build_directory/programs/notification-daemon/desktop-shell-notification-daemon" --config "$output_directory/notification-daemon.ini"
    sleep 1
    gdbus monitor --session --dest org.freedesktop.Notifications > "$signals_log" 2>&1 &
    started_process_ids="$started_process_ids $!"
    sleep 0.3
}

focused_title() {
    swaymsg -s "$headless_socket" -t get_tree -r | "$tools_directory/print-focused-window-title.py"
}

signal_seen() {
    grep -q "$1" "$signals_log"
}

test_capabilities_and_sound() {
    check "capabilities offer actions and sound" "notifications_call GetCapabilities | grep -q \"'actions', 'sound'\""
    notify Slack "Hello from Slack" --default-action --stay 30 > "$output_directory/slack-id" &
    sleep 1
    check "a plain notification plays the chime at the set volume" '[ "$(last_sound)" = "0.40 /test/chime.wav" ]'
    check "the status bar sees one unread" "center_property UnreadCount | grep -q 'uint32 1'"
    check "the status bar sees the latest text" "center_property LatestText | grep -q 'Slack: Hello from Slack'"
}

# The sender is a process outside any window, so the application name finds Slack's window.
test_activate_latest_by_name() {
    swaymsg -s "$headless_socket" '[title="other-window"] focus' > /dev/null
    center_call ActivateLatest
    sleep 0.6
    check "ActivateLatest brings up Slack's window" '[ "$(focused_title)" = "slack-window" ]'
    check "the sender hears its default action" "signal_seen \"ActionInvoked (uint32 $(cat "$output_directory/slack-id"), 'default')\""
    check "reading dismisses it for the sender" "signal_seen \"NotificationClosed (uint32 $(cat "$output_directory/slack-id"), uint32 2)\""
    check "nothing is unread any more" "center_property UnreadCount | grep -q 'uint32 0'"
}

# A script running in a terminal sends; the terminal's window comes up.
test_activate_latest_by_process() {
    swaymsg -s "$headless_socket" "exec foot -T notifying-terminal python3 $tools_directory/send-test-notification.py build-finished Done --stay 30" > /dev/null
    sleep 1.2
    swaymsg -s "$headless_socket" '[title="slack-window"] focus' > /dev/null
    center_call ActivateLatest
    sleep 0.6
    check "ActivateLatest brings up the sending terminal" '[ "$(focused_title)" = "notifying-terminal" ]'
}

test_sound_rules() {
    wait_for_sound_interval
    before=$(sound_count)
    notify Slack "Slack plays its own sound" --suppress-sound > /dev/null
    check "suppress-sound plays nothing more" '[ "$(sound_count)" = "$before" ]'
    notify Monitor "Disk almost full" --urgency 2 > /dev/null
    sleep 0.3
    check "critical plays the insistent chime" '[ "$(last_sound)" = "0.40 /test/critical-chime.wav" ]'
    notify Burst first > /dev/null
    notify Burst second > /dev/null
    sleep 0.3
    check "a burst within 1.5 s makes one sound" '[ "$(sound_count)" = "$((before + 1))" ]'
}

test_do_not_disturb_and_rules() {
    wait_for_sound_interval
    center_call ToggleDoNotDisturb
    before=$(sound_count)
    notify Mail "Quiet please" > /dev/null
    sleep 0.3
    check "do-not-disturb is on" "center_property DoNotDisturb | grep -q true"
    check "do-not-disturb silences normal notifications" '[ "$(sound_count)" = "$before" ]'
    center_call ToggleDoNotDisturb
    unread_before=$(center_property UnreadCount)
    notify ignored "Should be ignored" > /dev/null
    sleep 0.3
    check "an ignored application neither sounds nor counts" '[ "$(sound_count)" = "$before" ] && [ "$(center_property UnreadCount)" = "$unread_before" ]'
}

test_expiry_and_closing() {
    expiring_id=$(notify Timer "Expires after a second")
    sleep 1.5
    check "a notification expires for its sender after its timeout" "signal_seen \"NotificationClosed (uint32 $expiring_id, uint32 1)\""
    check "an expired notification stays unread" "center_property LatestText | grep -q 'Timer: Expires'"
    closing_id=$(notify Closer "Withdrawn")
    notifications_call CloseNotification "$closing_id" > /dev/null
    sleep 0.3
    check "CloseNotification reports reason 3" "signal_seen \"NotificationClosed (uint32 $closing_id, uint32 3)\""
    center_call MarkAllRead
    check "MarkAllRead empties the latest text" "center_property LatestText | grep -q \"''\""
}

# A second copy for the same display leaves at once; one for another display finds the name taken.
test_second_copies() {
    "$build_directory/programs/notification-daemon/desktop-shell-notification-daemon" --config "$output_directory/notification-daemon.ini"
    check "a second copy for the same display exits at once" "true"
    if WAYLAND_DISPLAY=other-display timeout 5 "$build_directory/programs/notification-daemon/desktop-shell-notification-daemon" 2> "$output_directory/rival.log"; then
        rival_status=0
    else
        rival_status=$?
    fi
    check "a rival server leaves the name to the running one" '[ "$rival_status" = 1 ] && grep -q "Another program" "$output_directory/rival.log"'
}

# CPU ticks, wakeups and resident memory over ten idle seconds.
report_idle_resource_usage() {
    daemon_process_id=$(echo $program_process_ids | awk '{print $1}')
    set -- $(awk '{ print $14 + $15 }' "/proc/$daemon_process_id/stat") $(awk '/^voluntary_ctxt_switches/ { print $2 }' "/proc/$daemon_process_id/status")
    sleep 10
    set -- "$@" $(awk '{ print $14 + $15 }' "/proc/$daemon_process_id/stat") $(awk '/^voluntary_ctxt_switches/ { print $2 }' "/proc/$daemon_process_id/status")
    echo "idle: $(awk '/^VmRSS/ { print $2 }' "/proc/$daemon_process_id/status") KiB resident, $(($3 - $1)) CPU ticks and $(($4 - $2)) wakeups in 10 s"
}

run_test() {
    require_built_programs programs/notification-daemon/desktop-shell-notification-daemon
    start_headless_compositor
    open_windows
    start_daemon
    test_capabilities_and_sound
    test_activate_latest_by_name
    test_activate_latest_by_process
    test_sound_rules
    test_do_not_disturb_and_rules
    test_expiry_and_closing
    test_second_copies
    report_idle_resource_usage
    echo "$failures failed"
    [ "$failures" = 0 ]
}

run_test
