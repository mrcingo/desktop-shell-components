#!/usr/bin/env python3
"""Answers questions about a headless compositor's window tree, for test scripts.

Reads the tree from swaymsg on the socket in $SWAYSOCK and prints one line:

  frame APP_ID       outer frame of the window: x y width height (title bar included)
  state APP_ID       floating|tiled, fullscreen|windowed, focused|unfocused, output name
  workspace OUTPUT   workspace area of the output: x y width height
  focused            application ID of the focused window
  count              number of windows

Usage: query-window-tree.py QUESTION [APP_ID|OUTPUT]
"""

import json
import os
import subprocess
import sys


def read_tree():
    socket_path = os.environ["SWAYSOCK"]
    reply = subprocess.run(["swaymsg", "-s", socket_path, "-t", "get_tree", "-r"], capture_output=True, check=True)

    return json.loads(reply.stdout)


def children_of(node):
    return node.get("nodes", []) + node.get("floating_nodes", [])


def walk(node, output_name=None):
    """Yields every node with the name of the output it is on."""
    if node.get("type") == "output":
        output_name = node.get("name")

    yield node, output_name

    for child in children_of(node):
        yield from walk(child, output_name)


def find_window(tree, application_id):
    for node, output_name in walk(tree):
        if node.get("pid") and node.get("app_id") == application_id:
            return node, output_name

    return None, None


def frame_of(window):
    rectangle = window["rect"]
    title_height = window["deco_rect"]["height"]

    return rectangle["x"], rectangle["y"] - title_height, rectangle["width"], rectangle["height"] + title_height


def describe_state(window, output_name):
    placement = "floating" if window["type"] == "floating_con" else "tiled"
    screen = "fullscreen" if window.get("fullscreen_mode") else "windowed"
    focus = "focused" if window.get("focused") else "unfocused"

    return f"{placement} {screen} {focus} {output_name}"


def answer_frame(tree, application_id):
    window, _ = find_window(tree, application_id)

    return "missing" if window is None else " ".join(str(value) for value in frame_of(window))


def answer_state(tree, application_id):
    window, output_name = find_window(tree, application_id)

    return "missing" if window is None else describe_state(window, output_name)


def answer_workspace(tree, output_name):
    for node, node_output in walk(tree):
        if node.get("type") == "workspace" and node_output == output_name and node.get("visible", True):
            rectangle = node["rect"]
            return f'{rectangle["x"]} {rectangle["y"]} {rectangle["width"]} {rectangle["height"]}'

    return "missing"


def answer_focused(tree, _):
    for node, _ in walk(tree):
        if node.get("pid") and node.get("focused"):
            return node.get("app_id") or "x11"

    return "none"


def answer_count(tree, _):
    return str(sum(1 for node, _ in walk(tree) if node.get("pid")))


ANSWERS = {
    "frame": answer_frame,
    "state": answer_state,
    "workspace": answer_workspace,
    "focused": answer_focused,
    "count": answer_count,
}


def main(arguments):
    question = arguments[0]
    subject = arguments[1] if len(arguments) > 1 else None

    print(ANSWERS[question](read_tree(), subject))


if __name__ == "__main__":
    main(sys.argv[1:])
