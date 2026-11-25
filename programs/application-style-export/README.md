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
| `~/.local/share/icons/Adwaita/symbolic/...` | single-color icons in your text color (see below) |

It also sets the desktop's color scheme and font in GSettings, which
libadwaita, the portals and Chromium-based programs follow.

Your own files are never overwritten: a file is only written when it does
not exist or starts with this program's marker line. Delete that line to
keep a file as yours. Nothing needs root; the program only writes inside
`~/.config` and `~/.local/share`, never through links, and only when
something changed. A run takes about 10 to 15 ms.

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
| `recolor-icons`       | `true`, `false`                 | `true`  |
| `color-scheme`        | `auto`, `dark`, `light`         | `auto`  |

`auto` picks dark when the background is dark. Apply with `swaymsg reload`.

## Icons

Some programs, such as Thunar's side pane (Home, Recent), show Adwaita's
single-color icons as they are, in a dark gray that disappears on a dark
background. The program copies those icons, recolored to your text color,
into `~/.local/share/icons/Adwaita/symbolic`, which the toolkit reads before
the system's copy. Only single-color icons change; full-color icons, such
as the folders in a file list, keep their color, and the icon theme stays
Adwaita. Only the foreground gray is replaced, so green, orange and red
status marks stay.

Which icons: `recolored-icons` under `[icons]`, names separated by `;`
without the `-symbolic` ending. Copies no longer listed are removed; icons
you put there yourself are never touched.

## Drive mounting policy

The separate package `desktop-shell-drive-mount-policy` makes mounting,
unlocking or formatting drives through udisks2 (what file managers use)
ask for an administrator password every time, instead of letting whoever
is logged in do it without one. The Trash keeps working.
