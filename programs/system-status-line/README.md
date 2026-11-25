# System status line

The right side of the sway bar: network and internet, traffic, Bluetooth,
processor, temperature, memory, disk, volume, brightness, power profile,
battery, keyboard layout and clock.

    WIFI Home 72%  RX 1.2K TX 30K  BT on  CPU 3%  TEMP 47C  MEM 4.4G/31G  DISK 463G  VOL 75%  BRI 100%  PWR balanced  BAT 37% 2h 15m  es  Wed 25 Nov 01:20

Click and scroll on blocks: left click mutes the volume, switches the power
profile or Bluetooth and the clock's format; scrolling changes volume and
brightness; right click on the volume opens pavucontrol.

## Use

In the `bar { }` block of `~/.config/sway/config`:

    status_command desktop-shell-system-status-line

Then run `swaymsg reload`.

## Settings

Optional. In `~/.config/desktop-shell-components/system-status-line.ini`,
copied from `/etc/xdg/desktop-shell-components/`, which lists every setting
with its default. For example, fewer blocks and a different memory format:

    [status]
    blocks = network volume battery clock

    [memory]
    format = RAM {percent}%

Each block has its own group with formats using `{placeholders}`, warning
and urgent thresholds, mouse actions and, for values read on a timer, an
`interval` in seconds.

The settings model (an ordered block list, per-state formats, an alternate
format on click, per-block intervals and thresholds) follows widely used
status bars such as waybar and i3status-rust.

## Resources

Network, Bluetooth, power profile, battery, volume and keyboard layout are
reported by their services as they change. Processor, temperature, memory,
disk, traffic and brightness are read on a timer. A line is only written
when it differs from the last one.
