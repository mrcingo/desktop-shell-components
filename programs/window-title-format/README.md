# Window title format

Adds the process ID to every window's title bar, in grey. Long titles end
in `...` so the process ID stays in view:

    ~/Programming/desktop-shell-compo... (4123)

## Needs

Title bars and Pango markup in your sway configuration:

    default_border normal
    font pango:Monaspace Neon 10

Without `pango:` the markup shows as text; use a format without markup then.

## Settings

Optional. In `~/.config/desktop-shell-components/window-title-format.ini`:

    [titles]
    format = {title} ({pid})

| Setting         | Values                                                        | Default                                    |
|-----------------|---------------------------------------------------------------|--------------------------------------------|
| `enabled`       | `true`, `false`                                               | `true`                                     |
| `format`        | `{title}` `{pid}` `{name}` `{workspace}`, sway's `%app_id` ... | `{title} <span alpha='60%'>({pid})</span>` |
| `title-length`  | most characters of `{title}`; `0` for no limit                | `60`                                       |
| `fit-title-bar` | `true` (shorten to fit the title bar), `false`                | `true`                                     |

The format may use `"` or `'`, not both. Fitting uses the font and
`titlebar_padding` from your main sway configuration file. Apply with `swaymsg reload`.
Turning it off restores the plain titles.
