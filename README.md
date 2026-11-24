# desktop-shell-components

**Completely vibecoded. Use at your own risk.**

Login screen, window bar and Alt+Tab switcher for sway.

## Install

Arch Linux:

    cd distribution-packaging/arch-linux
    makepkg -si
    swaymsg reload

Other systems:

    meson setup build --prefix=/usr --sysconfdir=/etc
    meson compile -C build
    sudo meson install -C build

Needs sway 1.12, gtk4 4.16, gtk4-layer-shell and json-glib.

## Use

The bar and the switcher start with sway. The login screen needs one change;
see `programs/login-greeter`.

## Theme

Override colors in `~/.config/desktop-shell-components/shared-theme.css`.

## License

0BSD.
