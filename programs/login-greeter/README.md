# desktop-shell-login-greeter

Login screen for greetd.

## Setup

In `/etc/greetd/config.toml`:

    [default_session]
    command = "sway --config /etc/greetd/login-greeter-compositor-configuration"
    user = "greeter"

Set your keyboard layout in `/etc/greetd/login-greeter-compositor-configuration`.

If it fails, log in with Ctrl+Alt+F2.
