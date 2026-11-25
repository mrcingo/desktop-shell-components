# Window snap

Title bars that work as on Windows 11, with no extra key:

- **Click** a title bar: the window gets focus (on a tab: that tab shows).
- **Double click**: the window fills the workspace; double click again to
  restore it.
- **Drag**: a preview shows where the window will land.
  - Screen side: that half. Corner: that quarter. Top edge: the whole
    workspace.
  - Another window: next to it, on the side nearest the pointer, back in
    the tiling. Its title bar: a tab with it.
  - Anywhere else: a floating window just moves there.
  - A snapped or maximized window dragged away gets its old size back.
  - Escape or the right button cancels.

Right-click and middle-click on title bars keep working as before. The
keyboard stays with your applications; Escape cancels a drag through a
sway binding mode that is only active while dragging.

## Keyboard

Add to your sway configuration for Windows-style snapping with Super and the
arrow keys (Super+Left then Super+Up gives the top left quarter):

    bindsym Mod4+Left  nop desktop-shell-window-snap left
    bindsym Mod4+Right nop desktop-shell-window-snap right
    bindsym Mod4+Up    nop desktop-shell-window-snap up
    bindsym Mod4+Down  nop desktop-shell-window-snap down

`left-half`, `right-half`, `maximize` and `restore` jump straight there.

## Settings

Optional. In `~/.config/desktop-shell-components/window-snap.ini`:

| Setting                           | Values             | Default |
|-----------------------------------|--------------------|---------|
| `enabled` in `[snap]`               | `true`, `false`    | `true`  |
| `edge-zone-size` in `[snap]`        | pixels, 2 to 400   | `24`    |
| `drag-threshold` in `[snap]`        | pixels, 1 to 100   | `8`     |
| `floating-border-width` in `[snap]` | 0 to 50            | `6`     |
| `tiled-border-width` in `[snap]`    | 0 to 50            | `2`     |
| `opacity` in `[preview]`            | percent            | `30`    |
| `show-labels` in `[preview]`        | `true`, `false`    | `true`  |
| `follow-sway-bar` in `[appearance]` | `true`, `false`    | `true`  |

Apply with `swaymsg reload`. With `enabled = false` title bars behave as
sway's own again.

## Tidy layouts

Layout changes can leave a tab group holding nothing but another group,
which sway shows as an extra title bar such as `T[foot thunar]`. The
program removes such wrappers as soon as they appear; the windows and their
tabs stay. Turn it off with `remove-redundant-groups = false` under
`[layout]`.
