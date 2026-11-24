# desktop-shell-login-greeter

A login screen for greetd. It asks for a username and password, lets you pick
a session from `/usr/share/wayland-sessions`, and shows any further question
PAM asks, such as a one-time code.

The form appears on the monitor the pointer last entered. Escape cancels a
question or a running login attempt.

## Setup

1. Point greetd at the installed configuration in `/etc/greetd/config.toml`:

       [default_session]
       command = "sway --config /etc/greetd/login-greeter-compositor-configuration"
       user = "greeter"

2. Set your keyboard layout in
   `/etc/greetd/login-greeter-compositor-configuration`; the file has an
   example. Upgrades keep your changes.

3. Log out. The new login screen appears.

That configuration runs a separate sway for the login screen, starts the
greeter in it and makes sway exit when the greeter does.

There is no graphical fallback on purpose, as a terminal on the login screen
could give out a shell. If the greeter fails, log in on another virtual
terminal with Ctrl+Alt+F2.

## More

`man desktop-shell-login-greeter` lists the options and style sheet names.
