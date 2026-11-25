# Window menu

Right-click a window's title bar or border, or its entry in the [window bar](../workspace-window-bar):

- the window's title, application, process ID and memory
- Close, End process (SIGTERM), Force close (SIGKILL), Pause and Resume process
- Copy the process ID, title, application name, command line or working directory
- Screenshot: the window's own content, copied to the clipboard and saved in
  Pictures/Screenshots as `Screenshot 2026-11-25 113045 - Slack.png`
- Floating, Fullscreen, Sticky, Move to workspace, Move to scratchpad

The menu opens at the pointer. Process actions are off for X11 windows, whose process ID belongs to the X
server. Force close reaches the process the menu was opened for, even if
its ID was reused meanwhile.

## Settings

Optional. In `~/.config/desktop-shell-components/window-context-menu.ini`:

| Setting                             | Values          | Default |
|-------------------------------------|-----------------|---------|
| `enabled` in `[menu]`               | `true`, `false` | `true`  |
| `follow-sway-bar` in `[appearance]` | `true`, `false` | `true`  |
| `copy` in `[screenshot]`            | `true`, `false` | `true`  |
| `save` in `[screenshot]`            | `true`, `false` | `true`  |
| `folder` in `[screenshot]`          | a folder inside your home; empty for Pictures/Screenshots | empty |

Apply with `swaymsg reload`. To open the menu with a key for the focused
window, add to your sway configuration:

    bindsym $mod+m mark --add _window-context-menu-target, nop desktop-shell-window-context-menu

## Screenshots

The screenshot is taken after the menu has closed, from the window itself, so
windows covering it and the menu never show, and it keeps every pixel. Files
are never replaced: a second one in the same second gets `-2`. Nothing pops
up; problems go to the log.

To take one of the focused window without the menu, as Alt+Print Screen does
on Windows, add to your sway configuration:

    bindsym Mod1+Print mark --add _window-context-menu-target, nop desktop-shell-window-context-menu screenshot
