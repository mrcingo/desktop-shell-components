# Notification daemon

Receives every notification, such as Slack's, plays a sound and tells the
status bar, which shows how many are unread and the latest one. Click it
there to jump to the sender's window. Nothing pops up.

Sounds: if the application names its own sound, that one plays. If the
application plays its own sound (it sets `suppress-sound`, as Slack does),
nothing more plays. Otherwise a short chime plays, and a more insistent one
for critical notifications. At most one sound every 1.5 seconds; none during
do-not-disturb, except critical ones.

## Settings

Optional. In `~/.config/desktop-shell-components/notification-daemon.ini`:

    [sound]
    volume = 50

    [app:Spotify]
    ignore = true

| Setting                            | Values                                         | Default                     |
|------------------------------------|------------------------------------------------|-----------------------------|
| `play-sound` in `[sound]`          | `unless-suppressed`, `always`, `never`         | `unless-suppressed`         |
| `volume` in `[sound]`              | 0 to 100                                       | `70`                        |
| `minimum-interval` in `[sound]`    | milliseconds between sounds                    | `1500`                      |
| `player` in `[sound]`              | a command; `{volume}` becomes 0.00 to 1.00     | `pw-play --volume={volume}` |
| `file`, `critical-file` in `[sound]` | sound files                                  | the shipped chimes          |
| `low`, `normal`, `critical` in `[timeouts]` | seconds until a notification expires; 0 never | `5`, `10`, `0`    |
| `size` in `[history]`              | notifications kept                             | `100`                       |
| `on-at-start` in `[do-not-disturb]` | `true`, `false`                               | `false`                     |
| `critical-sounds` in `[do-not-disturb]` | `true`, `false`                           | `true`                      |

Per application, in a `[app:NAME]` section (the name or desktop entry, any
case): `play-sound`, `urgency` (`low`, `normal`, `critical`), `timeout`
(seconds) and `ignore` (`true`: no sound, not counted).

Apply with `swaymsg reload`.

## Status bar interface

Bus name `org.freedesktop.Notifications`, object
`/io/github/mrcingo/DesktopShellComponents/NotificationCenter`, interface
`io.github.mrcingo.DesktopShellComponents.NotificationCenter`:

| Member                | Type | Meaning                                                    |
|-----------------------|------|------------------------------------------------------------|
| `UnreadCount`         | `u`  | unread notifications                                       |
| `LatestText`          | `s`  | `Slack: New message`, at most 120 characters; empty if none |
| `LatestUrgency`       | `y`  | 0 low, 1 normal, 2 critical                                |
| `DoNotDisturb`        | `b`  | writable                                                   |
| `ActivateLatest()`    |      | bring up the newest unread one's window, send its default action, mark it read |
| `MarkAllRead()`       |      |                                                            |
| `ToggleDoNotDisturb()` |     |                                                            |

Properties announce changes with `PropertiesChanged`.

The chimes are made from sine waves at build time and are public domain (CC0).
