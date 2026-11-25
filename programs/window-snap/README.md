# Window snap

Windows-style snapping from the keyboard, and tidy layouts. Title bars stay
exactly as sway makes them unless you opt in below: dragging one moves the
window natively, and the translucent border resizes it.

## Keyboard

Add to your sway configuration (Super+Left then Super+Up gives the top left
quarter, as on Windows):

    bindsym Mod4+Left  nop desktop-shell-window-snap left
    bindsym Mod4+Right nop desktop-shell-window-snap right
    bindsym Mod4+Up    nop desktop-shell-window-snap up
    bindsym Mod4+Down  nop desktop-shell-window-snap down
    bindsym Mod4+Shift+Left  nop desktop-shell-window-snap monitor-left
    bindsym Mod4+Shift+Right nop desktop-shell-window-snap monitor-right

As on Windows:

- Win+Left/Right snap to a half; Win+Up then makes a quarter, Win+Down goes
  back to the half; Win+Up on a free window maximizes, Win+Down restores.
- On a window already against an edge, Win+Left/Right carries it to the
  monitor on that side (left half becomes that monitor's right half). On
  the last monitor it stays.
- Win+Right on a left half (or Left on a right half) restores the window.
- Win+Down on a free window does nothing: sway has no minimizing.
- Win+Shift+Left/Right moves the window to the monitor on that side,
  keeping its snapped shape, or its size and relative place.
- A fullscreen window leaves fullscreen first.
- A snapped window moved by hand is free again: the next Win+Up maximizes.
- Restoring puts the window back where it was; if that monitor is gone, on
  the monitor it is on now.

`left-half`, `right-half`, `maximize` and `restore` jump straight there.
Keys typed quickly are handled one after the other; a key that would
change nothing sends nothing to sway.

## Optional: snapping title bars

These two lines in your sway configuration make title bars work like
Windows 11, at the cost of taking over left presses on them (the program
follows the drag by moving the pointer through sway's seat commands, which
can feel less direct than sway's own move):

    bindsym button1 mark --add _window-snap-target, nop desktop-shell-window-snap press
    bindsym --release button1 nop desktop-shell-window-snap release

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

## Settings

Optional. In `~/.config/desktop-shell-components/window-snap.ini`:

| Setting                           | Values             | Default |
|-----------------------------------|--------------------|---------|
| `enabled` in `[snap]`               | `true`, `false`    | `true`  |
| `edge-zone-size` in `[snap]`        | pixels, 2 to 400   | `24`    |
| `drag-threshold` in `[snap]`        | pixels, 1 to 100   | `8`     |
| `floating-border-width` in `[snap]` | 0 to 50            | `6`     |
| `tiled-border-width` in `[snap]`    | 0 to 50            | `2`     |
| `fit-new-windows` in `[snap]`       | `true`, `false`    | `true`  |
| `cascade-new-windows` in `[snap]`   | `true`, `false`    | `true`  |
| `bring-back-off-screen-windows` in `[snap]` | `true`, `false` | `true` |
| `opacity` in `[preview]`            | percent            | `30`    |
| `show-labels` in `[preview]`        | `true`, `false`    | `true`  |
| `follow-sway-bar` in `[appearance]` | `true`, `false`    | `true`  |

Apply with `swaymsg reload`. With `enabled = false` title bars behave as
sway's own again.

## New windows and windows out of reach

As on Windows, a floating window that opens bigger than its monitor is
shrunk to fit, and one that opens exactly over another (every terminal
opens centred at the same size) moves 32 pixels down and right, starting
again at the top left when it would leave the screen. This happens once,
when the window opens; windows moved since are never touched.

When a monitor goes away or changes, every floating window not fully on a
monitor is brought fully onto the nearest one, also on hidden workspaces.
After any other key binding, a window whose title bar cannot be reached is
brought back; a window parked half off screen stays. sway reports no
drags, so nothing is ever pulled from under a dragging pointer.

## Tidy layouts

Layout changes can leave a tab group holding nothing but another group,
which sway shows as an extra title bar such as `T[foot thunar]`. The
program removes such wrappers as soon as they appear; the windows and their
tabs stay. Turn it off with `remove-redundant-groups = false` under
`[layout]`.
