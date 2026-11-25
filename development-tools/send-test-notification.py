#!/usr/bin/env python3
"""Sends one notification on the session bus, prints its ID and stays connected for a while.

Staying connected lets the notification daemon find the sending process, as
it does for real applications such as Slack.

Usage: send-test-notification.py APPLICATION SUMMARY [--urgency N] [--suppress-sound]
                                 [--default-action] [--stay SECONDS]
"""

import argparse
import time

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib  # noqa: E402


# (flag, keyword arguments for argparse)
OPTIONS = [
    ("--urgency", {"type": int, "default": 1}),
    ("--suppress-sound", {"action": "store_true"}),
    ("--default-action", {"action": "store_true"}),
    ("--stay", {"type": float, "default": 0}),
]


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("application")
    parser.add_argument("summary")

    for flag, settings in OPTIONS:
        parser.add_argument(flag, **settings)

    return parser.parse_args()


def build_parameters(arguments):
    """The (susssasa{sv}i) parameters of Notify."""
    hints = {"urgency": GLib.Variant("y", arguments.urgency), "suppress-sound": GLib.Variant("b", arguments.suppress_sound)}
    actions = ["default", "Open"] if arguments.default_action else []

    return GLib.Variant("(susssasa{sv}i)", (arguments.application, 0, "", arguments.summary, "body", actions, hints, -1))


def send(arguments):
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    reply = bus.call_sync(
        "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications",
        "Notify", build_parameters(arguments), GLib.VariantType("(u)"), Gio.DBusCallFlags.NONE, 5000, None,
    )

    return bus, reply.unpack()[0]


def main():
    arguments = parse_arguments()
    _bus, notification_id = send(arguments)

    print(notification_id, flush=True)
    time.sleep(arguments.stay)


if __name__ == "__main__":
    main()
