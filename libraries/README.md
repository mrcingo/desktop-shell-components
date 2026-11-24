# Libraries

Each library does one job. A library only uses the ones above it.

| Library                           | Job                                                              |
|-----------------------------------|------------------------------------------------------------------|
| `typed-json-member-lookup`        | Read JSON from other processes without trusting its types        |
| `framed-message-stream`           | Length-prefixed messages over a socket                           |
| `compositor-ipc-message-format`   | Encode and decode sway's IPC messages                            |
| `compositor-ipc-client`           | Send IPC requests and receive replies and events                 |
| `compositor-window-tree`          | Outputs, visible windows and focus order from sway's layout tree |
| `window-focus-history`            | Windows ordered by when they last had focus                      |
| `window-label-format`             | Labels such as `{name} {pid}` for windows                        |
| `text-ellipsis-truncation`        | Shorten text to a number of characters, ending in `...`          |
| `compositor-configuration-directives` | Read top-level settings from sway's configuration text       |
| `compositor-title-bar-style`      | The font and padding sway draws title bars with                  |
| `rendered-text-width-measurement` | How wide text is in pixels in a Pango font                       |
| `window-title-format-command`     | The command that sets a window's title bar text                  |
| `window-title-fitting`            | Shorten a title so the title bar text fits                       |
| `compositor-state-tracker`        | Keep the window tree and focus history current                   |
| `compositor-bar-appearance`       | Read the sway bar's colors and font                              |
| `login-daemon-protocol`           | Encode greetd requests and decode its replies                    |
| `login-daemon-conversation`       | Run one login attempt with greetd                                |
| `desktop-session-entries`         | Read the sessions in `/usr/share/wayland-sessions`               |
| `system-keyboard-layout`          | The keyboard layout set with `localectl`                         |
| `shared-memory-buffer`            | Sealed shared memory for pixel buffers                           |
| `pixel-box-downscaling`           | Shrink an image in one pass                                      |
| `wayland-pixel-format-conversion` | Match Wayland pixel formats to GTK texture formats               |
| `foreign-toplevel-registry`       | Find a window's Wayland handle by its identifier                 |
| `toplevel-window-capture`         | Capture a window's contents                                      |
| `shared-style-sheets`             | Load the shared theme, a program's style and user overrides      |
| `compositor-matched-appearance`   | Style sheets that follow the sway bar                            |
| `layer-shell-window`              | Create panels and overlays as layer-shell surfaces               |
| `per-monitor-objects`             | Keep one object per monitor as monitors come and go              |
| `monitor-connector-lookup`        | Find a monitor by its connector name, such as eDP-1              |
| `alt-key-release-watcher`         | Notice when Alt is released                                      |
| `login-form-widget`               | The login form: username, password, session, questions           |
| `program-command-line`            | The `--style` option every program accepts                       |
| `program-configuration-file`      | Read optional INI settings, warning about mistakes               |
| `software-rendering-preference`   | Use GTK's lighter renderer for small interfaces                  |
| `single-instance-lock`            | Let only one copy of a program run per display                   |
| `replaced-executable-restart`     | Restart a program whose file an upgrade replaced                 |

## Adding a library

Copy the layout of an existing library, add it to `meson.build` and to this
table.
