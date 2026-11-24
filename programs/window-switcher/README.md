# desktop-shell-window-switcher

Alt+Tab between the windows of the focused workspace. It shows a live
preview of each window, most recently used first, and focuses the selected
one when you release Alt.

Previews need sway 1.12 or later. Windows that cannot be captured show an
empty preview.

## Setup

Installing it adds `/etc/sway/config.d/60-desktop-shell-window-switcher.conf`,
which starts the switcher with sway and binds Alt+Tab and Alt+Shift+Tab. Run
`swaymsg reload` once after installing.

To use other keys, edit that file; upgrades keep your changes. The program
waits in the background and the key bindings tell it what to do, so no new
process starts on each key press.

## Keys

| Key                       | Action                                    |
|---------------------------|-------------------------------------------|
| Alt+Tab, Alt+Shift+Tab    | Open the switcher, then move the selection |
| Release Alt               | Focus the selected window                 |
| Left, Right               | Move the selection                        |
| Return or a click         | Focus the selected or clicked window      |
| Escape                    | Close without switching                   |

## More

`man desktop-shell-window-switcher` lists the options and style sheet names.
