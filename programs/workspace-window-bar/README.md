# Window bar

![Window bar](../../documentation/images/workspace-window-bar.png)

Lists the windows of the current workspace at the bottom of each output.
Click a window to focus it; right-click it for the [window menu](../window-context-menu).

## Settings

Optional. In `~/.config/desktop-shell-components/workspace-window-bar.ini`:

    [bar]
    position = top

| Setting                       | Values                                      | Default        |
|-------------------------------|---------------------------------------------|----------------|
| `enabled`                     | `true`, `false`                             | `true`         |
| `position`                    | `bottom`, `top`                             | `bottom`       |
| `height`                      | `auto` (same as sway's bar), 10 to 200      | `auto`         |
| `label`                       | text with `{name}` `{pid}` `{title}` `{workspace}` | `{name} {pid}` |
| `left-click`                  | `focus`, `close`, `menu`, `nothing`         | `focus`        |
| `middle-click`                | `focus`, `close`, `menu`, `nothing`         | `close`        |
| `right-click`                 | `focus`, `close`, `menu`, `nothing`         | `menu`         |
| `follow-sway-bar` in `[appearance]` | `true`, `false` (use the GTK theme)   | `true`         |

Apply with `swaymsg reload`.

## Turn it off

Set `enabled = false` under `[bar]` and run `swaymsg reload`.
