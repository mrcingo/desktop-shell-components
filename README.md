# desktop-shell-components

**Completely vibecoded. Use at your own risk.**

Small desktop programs for sway: a login screen, a window bar and an Alt+Tab
window switcher. They share one theme, so they match each other and sway's
own bar.

| Program                              | What it does                                                    |
|--------------------------------------|-----------------------------------------------------------------|
| `desktop-shell-login-greeter`        | Login screen for the greetd login daemon                        |
| `desktop-shell-workspace-window-bar` | Bottom bar listing the windows of the visible workspace         |
| `desktop-shell-window-switcher`      | Alt+Tab between the windows of a workspace, with live previews  |

Each program has its own README in `programs/`.

## Requirements

* sway 1.12 or later (window previews need it; the rest works on 1.11)
* gtk4 4.16 or later
* gtk4-layer-shell 1.3 or later
* glib2, json-glib, wayland
* greetd, for the login screen only
* The Monaspace Neon font, used by the default theme

To build: meson, wayland-protocols 1.40 or later, and scdoc for the manual pages.

## Installing

On Arch Linux, build and install every package with:

    cd distribution-packaging/arch-linux
    makepkg -si

Elsewhere, install with meson. The prefix and configuration directory must be
the system ones, so the setup files land where sway and greetd look:

    meson setup build --prefix=/usr --sysconfdir=/etc
    meson compile -C build
    meson test -C build
    sudo meson install -C build

## Setup

The bar and the switcher need no configuration. They install files into
`/etc/sway/config.d`, which sway's default configuration includes. Start them
with:

    swaymsg reload

If your own sway configuration does not end with
`include /etc/sway/config.d/*`, add that line.

The login screen needs one change in greetd's configuration; see
`programs/login-greeter/README.md`.

## Theme

All colours and the font are set once in `shared-data/shared-theme.css`.
To change them, copy the variables you want into
`~/.config/desktop-shell-components/shared-theme.css`. To change one program's
layout, use `~/.config/desktop-shell-components/PROGRAM.css`, where PROGRAM is
`login-greeter`, `workspace-window-bar` or `window-switcher`.

## Contributing

The code is split into small libraries, described in `libraries/README.md`.
Layout and size rules are checked by the tests; see `development-tools/README.md`.

## License

0BSD: use it for anything, no conditions. See `LICENSE`.
