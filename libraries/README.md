# Libraries

Each library does one job. A library only uses the ones above it.

| Library                           | Job                                                              |
|-----------------------------------|------------------------------------------------------------------|
| `typed-json-member-lookup`        | Read JSON from other processes without trusting its types        |
| `framed-message-stream`           | Length-prefixed messages over a socket                           |
| `compositor-ipc-message-format`   | Encode and decode sway's IPC messages                            |
| `compositor-ipc-client`           | Send IPC requests and receive replies and events                 |
| `redundant-layout-wrapper-finder` | Containers whose only child is another container, such as T[foot thunar] |
| `compositor-window-tree`          | Outputs, visible windows and focus order from sway's layout tree |
| `window-focus-history`            | Windows ordered by when they last had focus                      |
| `window-label-format`             | Labels such as `{name} {pid}` for windows                        |
| `text-ellipsis-truncation`        | Shorten text to a number of characters, ending in `...`          |
| `compositor-configuration-directives` | Read top-level settings from sway's configuration text       |
| `compositor-title-bar-style`      | The font and padding sway draws title bars with                  |
| `rendered-text-width-measurement` | How wide text is in pixels in a Pango font                       |
| `window-title-format-command`     | The command that sets a window's title bar text                  |
| `window-title-fitting`            | Shorten a title so the title bar text fits                       |
| `window-snap-placement`           | Where a dragged window lands, and the command that puts it there |
| `window-snap-keyboard-steps`      | Windows-like snapping with the arrow keys, step by step          |
| `floating-window-arrangement`     | Fit new windows, cascade them, bring back windows off screen     |
| `notification-request`            | Check and clean a notification another program sent             |
| `notification-store`              | Recent notifications: IDs, replacement, unread                   |
| `notification-sound-player`       | Play a notification sound without blocking, at most so often     |
| `process-window-lookup`           | The window of a process, its parent or its child                 |
| `process-window-focusing`         | Bring up the window of a process                                 |
| `status-notifier-activation-watcher` | Notice clicks on tray icons, with the icon's process ID       |
| `notification-specification-server` | The notification server on D-Bus, and its status bar interface |
| `process-information-snapshot`    | A process's command line, directory, memory and state from /proc |
| `process-signal-handle`           | Signal exactly the process a menu was opened for (pidfd)         |
| `window-context-menu-target`      | The window and process a window menu acts on                     |
| `compositor-state-tracker`        | Keep the window tree and focus history current                   |
| `compositor-bar-appearance`       | Read the sway bar's colors and font                              |
| `theme-palette`                   | The shared look as plain colors and a font                       |
| `toolkit-style-sheet-writer`      | GTK 3 and GTK 4 user style sheets in the shared look             |
| `utility-program-style-writer`    | Lock screen, prompt bar and launcher settings in the shared look |
| `managed-file-writing`            | Keep files in ~/.config current without overwriting the user's   |
| `recolored-symbolic-icons`        | Single-color icons recolored to the text color, in the user's icon overlay |
| `established-theme-reading`       | The shared look from defaults, the sway bar and shared-theme.css |
| `desktop-interface-settings`      | The desktop color scheme and font in GSettings                   |
| `window-context-menu-actions`     | Close, kill, pause, copy and move actions of the window menu     |
| `window-context-menu-popover`     | The window menu shown on right-click                             |
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
| `notification-rules`              | Per-application notification settings and sound decisions       |
| `software-rendering-preference`   | Use GTK's lighter renderer for small interfaces                  |
| `single-instance-lock`            | Let only one copy of a program run per display                   |
| `window-screenshot-file-naming`   | Safe names and folders for window screenshots                    |
| `exclusive-file-writing`          | Save a new file all at once, never replacing an existing one     |
| `window-screenshot-capturing`     | Copy and save a window's own content, as Alt+Print Screen does   |
| `replaced-executable-restart`     | Restart a program whose file an upgrade replaced                 |
| `text-placeholder-expansion`      | Fill `{name}` placeholders in user-written formats               |
| `human-readable-quantities`       | Short sizes and durations: `1.2M`, `2h 15m`                      |
| `processor-usage-sampling`        | Processor use from /proc/stat                                    |
| `hardware-temperature-sensor`     | Temperatures from hwmon devices, found by driver name            |
| `memory-usage-reading`            | Memory and swap use from /proc/meminfo                           |
| `filesystem-free-space`           | Size and free space of a filesystem                              |
| `network-interface-traffic`       | Bytes through a network interface and their rate                 |
| `display-backlight-level`         | Read and set the screen backlight                                |
| `polled-system-readings`          | The values above on one timer, each at its own interval          |
| `system-bus-properties`           | Typed D-Bus properties of system services, and their signals     |
| `network-manager-status`          | Connection, Wi-Fi signal and internet reachability               |
| `bluetooth-adapter-status`        | Bluetooth adapter power and connected devices                    |
| `power-profile-status`            | The power profile, and switching to the next one                 |
| `battery-charge-status`           | Battery charge, state and time left                              |
| `system-service-readings`         | The four services above, read again only when they change        |
| `notification-center-status`      | Unread notifications and do-not-disturb, from the notification daemon |
| `audio-volume-control`            | Volume and mute of the default output, as they change            |
| `keyboard-layout-short-names`     | "es" for "Spanish", from the XKB rules                           |
| `compositor-keyboard-layout`      | The active keyboard layout in sway's replies and events          |
| `compositor-keyboard-layout-follower` | Follow the keyboard layout through sway's IPC                |
| `compositor-attention-requests`   | Windows asking for attention, and focusing the latest            |
| `status-bar-block-encoding`       | Write swaybar's status line protocol                             |
| `status-bar-click-decoding`       | Read swaybar's click events                                      |
| `clock-boundary-timing`           | Wake a clock exactly when the shown time changes                 |

## Adding a library

Copy the layout of an existing library, add it to `meson.build` and to this
table.
