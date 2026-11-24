# desktop-shell-workspace-window-bar

A bar along the bottom of every output that lists the windows of the
workspace shown there, as their application name and process ID:

    foot 4123  google-chrome 2210

The focused window is highlighted. Clicking a window focuses it. The bar is
exactly as tall as sway's top bar.

## Setup

Installing it adds `/etc/sway/config.d/60-desktop-shell-workspace-window-bar.conf`,
which starts the bar with sway. Run `swaymsg reload` once after installing.

To stop using it, uninstall it, or remove that file.

## More

`man desktop-shell-workspace-window-bar` lists the options and style sheet
names.
