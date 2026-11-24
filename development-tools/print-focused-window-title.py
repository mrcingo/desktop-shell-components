#!/usr/bin/env python3
"""Print the title of the focused window, read from a compositor window tree.

Reads the JSON that "swaymsg -t get_tree -r" prints on standard input.
Prints "none" when no window has focus, such as on an empty workspace.

Usage: swaymsg -t get_tree -r | print-focused-window-title.py
"""

import json
import sys


def child_nodes(node):
    return node.get("nodes", []) + node.get("floating_nodes", [])


def is_focused_window(node):
    """Only windows have a process; a focused workspace or container is not a window."""
    return node.get("focused", False) and "pid" in node


def find_focused_window(node):
    if is_focused_window(node):
        return node

    found = (find_focused_window(child) for child in child_nodes(node))

    return next((window for window in found if window is not None), None)


def main():
    window = find_focused_window(json.load(sys.stdin))

    print(window["name"] if window else "none")


if __name__ == "__main__":
    main()
