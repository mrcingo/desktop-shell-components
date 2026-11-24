#!/bin/sh
# Starts sway without any screen, input device or connection to the
# desktop it runs from, for previewing and testing components safely.
# Prints the WAYLAND_DISPLAY and SWAYSOCK to use, then keeps running until
# killed. Take screenshots with: grim FILE (with those variables set).
#
# Usage: run-headless-compositor.sh CONFIGURATION_FILE
set -eu

configuration_file=$1
environment_file=$(mktemp)

trap 'rm -f "$environment_file"' EXIT

printf 'exec env > %s\n' "$environment_file" > "$environment_file.config"
cat "$configuration_file" >> "$environment_file.config"

env -u WAYLAND_DISPLAY -u SWAYSOCK -u DISPLAY \
    WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 \
    sway --config "$environment_file.config" &
compositor_process=$!

while ! grep -q '^SWAYSOCK=' "$environment_file" 2>/dev/null; do
    sleep 0.1
done

grep -E '^(WAYLAND_DISPLAY|SWAYSOCK)=' "$environment_file"
rm -f "$environment_file.config"
wait "$compositor_process"
