#!/bin/sh
# Screenshots the login form in one state inside a headless compositor, to
# check the login screen's style without greetd and without the desktop.
#
# Usage: preview-login-form-in-headless-compositor.sh BUILD_DIRECTORY SCREENSHOT [empty|error|long|question|open]
set -eu

# Never talk to the desktop this runs from.
unset SWAYSOCK WAYLAND_DISPLAY

build_directory=$(realpath "$1")
screenshot_path=$2
form_state=${3:-empty}
tools_directory=$(realpath "$(dirname "$0")")
environment_file=$(mktemp)
headless_socket=""
compositor_process_id=""

stop_compositor() {
    if [ -n "$headless_socket" ]; then
        swaymsg -s "$headless_socket" exit > /dev/null 2>&1 || true
    fi

    if [ -n "$compositor_process_id" ]; then
        kill "$compositor_process_id" 2> /dev/null || true
    fi

    rm -f "$environment_file"
}

start_compositor() {
    "$tools_directory/run-headless-compositor.sh" "$tools_directory/headless-compositor-test-configuration" > "$environment_file" 2> /dev/null &
    compositor_process_id=$!

    while ! grep -q '^SWAYSOCK=' "$environment_file" 2> /dev/null; do
        sleep 0.1
    done

    export $(cat "$environment_file")
    headless_socket=$SWAYSOCK
}

take_screenshot() {
    "$build_directory/libraries/login-form-widget/login-form-widget-preview" "$tools_directory/../shared-data" "$form_state" &
    sleep 1.5
    grim "$screenshot_path"
    kill $!
}

trap stop_compositor EXIT
start_compositor
take_screenshot
