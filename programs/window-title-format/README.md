# Window title format

Adds the process ID to every window's title bar, in grey:

    ~/Programming (4123)

## Needs

Title bars and Pango markup in your sway configuration:

    default_border normal
    font pango:Monaspace Neon 10

Without `pango:` the markup shows as text; use a format without markup then.

## Settings

Optional. In `~/.config/desktop-shell-components/window-title-format.ini`:

    [titles]
    format = %title ({pid})

| Setting   | Values                                                    | Default                                    |
|-----------|-----------------------------------------------------------|--------------------------------------------|
| `enabled` | `true`, `false`                                           | `true`                                     |
| `format`  | sway's `%title` `%app_id` ..., plus `{pid}` `{name}` `{workspace}` | `%title <span alpha='60%'>({pid})</span>` |

The format may use `"` or `'`, not both. Apply with `swaymsg reload`.
Turning it off restores the plain titles.
