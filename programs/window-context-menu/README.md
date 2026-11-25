# Window menu

Right-click a window's title bar or border, or its entry in the [window bar](../workspace-window-bar):

- the window's title, application, process ID and memory
- Close, End process (SIGTERM), Force close (SIGKILL), Pause and Resume process
- Copy the process ID, title, application name, command line or working directory
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

Apply with `swaymsg reload`. To open the menu with a key for the focused
window, add to your sway configuration:

    bindsym $mod+m mark --add _window-context-menu-target, nop desktop-shell-window-context-menu
