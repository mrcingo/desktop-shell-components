# Tray click window focus

Click an application's tray icon, such as Slack's, and its window comes up:
sway switches to its workspace and tab, or shows it from the scratchpad.

sway's bar asks the application to show itself without an activation
token, so sway only marks the window red as wanting attention. This
program notices the click on the session bus and focuses the window
itself. It reads only those clicks and sends nothing on the bus.

## Settings

Optional. In `~/.config/desktop-shell-components/tray-click-window-focus.ini`:

| Setting                     | Values                    | Default |
|-----------------------------|---------------------------|---------|
| `enabled` in `[focus]`        | `true`, `false`           | `true`  |
| `second-attempt` in `[focus]` | milliseconds, 0 to 5000   | `400`   |

The second attempt catches applications that only open their window after
the click. Apply with `swaymsg reload`.
