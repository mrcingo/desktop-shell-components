#!/bin/sh
# Takes the README images in a headless compositor, so they always show the
# current code and never anything from the desktop they are made on: the
# windows show only this repository's files and a clock.
#
# Usage: take-documentation-screenshots.sh BUILD_DIRECTORY
set -eu

build_directory=$(realpath "$1")
tools_directory=$(realpath "$(dirname "$0")")
images_directory=$(realpath -m "$tools_directory/../documentation/images")
output_directory=$(mktemp -d)

. "$tools_directory/headless-compositor-session.sh"

clean_up() {
    stop_everything_started
    rm -rf "$output_directory"
}

# Tabbed, so every window has the full screen and its preview shows it whole.
open_documentation_windows() {
    swaymsg -s "$headless_socket" "layout tabbed" > /dev/null
    swaymsg -s "$headless_socket" "exec foot -T theme sh -c 'cat $tools_directory/../shared-data/shared-theme.css; exec sleep 600'" > /dev/null
    sleep 0.5
    swaymsg -s "$headless_socket" "exec foot -T clock sh -c 'while :; do clear; date +%T; sleep 1; done'" > /dev/null
    sleep 0.5
    swaymsg -s "$headless_socket" "exec foot -T settings sh -c 'cat $tools_directory/../shared-data/default-settings/window-switcher.ini; exec sleep 600'" > /dev/null
    sleep 1
}

take_desktop_screenshots() {
    use_source_tree_style_sheets
    start_in_background "$build_directory/programs/workspace-window-bar/desktop-shell-workspace-window-bar"
    start_in_background "$build_directory/programs/window-switcher/desktop-shell-window-switcher"
    sleep 1.5
    grim -g "0,1178 480x22" "$images_directory/workspace-window-bar.png"
    hold_alt_tab_in_background
    sleep 1.4
    grim -g "545,483 831x234" "$images_directory/window-switcher.png"
    grim -s 0.5 "$images_directory/desktop.png"
    wait $!
}

# The login screen with the GTK theme, and with the sway bar's theme saved as a user theme.
take_login_screenshots() {
    sway_theme_home="$output_directory/sway-theme-home"

    mkdir -p "$sway_theme_home/desktop-shell-components"
    "$build_directory/programs/print-sway-theme/desktop-shell-print-sway-theme" > "$sway_theme_home/desktop-shell-components/shared-theme.css"
    take_login_screenshot "$output_directory/no-theme-home" login-greeter-gtk-theme.png
    take_login_screenshot "$sway_theme_home" login-greeter-sway-theme.png
}

take_login_screenshot() {
    XDG_CONFIG_HOME=$1 "$build_directory/libraries/login-form-widget/login-form-widget-preview" "$tools_directory/../shared-data" empty >> "$output_directory/programs.log" 2>&1 &
    sleep 1.5
    grim -g "770,408 380x384" "$images_directory/$2"
    kill $!
}

trap clean_up EXIT
mkdir -p "$images_directory"
require_built_programs \
    programs/workspace-window-bar/desktop-shell-workspace-window-bar \
    programs/window-switcher/desktop-shell-window-switcher \
    programs/print-sway-theme/desktop-shell-print-sway-theme \
    libraries/login-form-widget/login-form-widget-preview \
    development-tools/send-virtual-key-presses
start_headless_compositor
open_documentation_windows
take_desktop_screenshots
take_login_screenshots
