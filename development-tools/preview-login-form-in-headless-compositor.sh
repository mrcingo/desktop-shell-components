#!/bin/sh
# Screenshots the login form in one state inside a headless compositor, to
# check the login screen's style without greetd and without the desktop.
#
# Usage: preview-login-form-in-headless-compositor.sh BUILD_DIRECTORY SCREENSHOT [empty|error|long|question|open]
set -eu

build_directory=$(realpath "$1")
screenshot_path=$(realpath "$2")
form_state=${3:-empty}
tools_directory=$(realpath "$(dirname "$0")")
output_directory=$(mktemp -d)

. "$tools_directory/headless-compositor-session.sh"

clean_up() {
    stop_everything_started
    rm -rf "$output_directory"
}

take_screenshot() {
    require_built_programs libraries/login-form-widget/login-form-widget-preview
    start_headless_compositor
    start_in_background "$build_directory/libraries/login-form-widget/login-form-widget-preview" "$tools_directory/../shared-data" "$form_state"
    sleep 1.5
    grim "$screenshot_path"
}

trap clean_up EXIT
take_screenshot
