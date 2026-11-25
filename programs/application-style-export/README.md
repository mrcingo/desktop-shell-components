# Application style export

Gives other programs the shared look, so they match the bar and the other
components: GTK applications (such as the Thunar file manager), the lock
screen and sway's prompt bar.

On every `swaymsg reload` it reads the colors and font of your sway bar and
your `~/.config/desktop-shell-components/shared-theme.css`, writes these
files in `~/.config`, and exits:

| File                    | Gives the look to                                      |
|-------------------------|--------------------------------------------------------|
| `gtk-3.0/gtk.css`       | GTK 3 applications: every widget, flat, in your colors |
| `gtk-4.0/gtk.css`       | GTK 4 and libadwaita applications: colors              |
| `gtk-3.0/settings.ini`, `gtk-4.0/settings.ini` | dark variant and font (other keys are kept) |
| `swaylock/config`       | the lock screen (Win+L)                                |
| `swaynag/config`        | sway's prompt bar                                      |

It also sets the desktop's color scheme and font in GSettings, which
libadwaita, the portals and Chromium-based programs follow.

Your own files are never overwritten: a file is only written when it does
not exist or starts with this program's marker line. Delete that line to
keep a file as yours. Nothing needs root; the program only writes inside
`~/.config`, never through links, and only when something changed. A run
takes under 10 ms.

The launcher's colors and font, for `wmenu`:

    desktop-shell-application-style-export --print-launcher-arguments

## Settings

Optional. In `~/.config/desktop-shell-components/application-style-export.ini`:

| Setting in `[export]` | Values                          | Default |
|-----------------------|---------------------------------|---------|
| `enabled`             | `true`, `false`                 | `true`  |
| `gtk3`, `gtk4`        | `true`, `false`                 | `true`  |
| `gtk-settings`        | `true`, `false`                 | `true`  |
| `gsettings`           | `true`, `false`                 | `true`  |
| `lock-screen`         | `true`, `false`                 | `true`  |
| `prompts`             | `true`, `false`                 | `true`  |
| `color-scheme`        | `auto`, `dark`, `light`         | `auto`  |

`auto` picks dark when the background is dark. Apply with `swaymsg reload`.

## Drive mounting policy

The separate package `desktop-shell-drive-mount-policy` makes mounting,
unlocking or formatting drives through udisks2 (what file managers use)
ask for an administrator password every time, instead of letting whoever
is logged in do it without one. The Trash keeps working.
