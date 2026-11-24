#!/usr/bin/env python3
"""Fail if any text file in the repository holds a character outside plain ASCII.

Every file is plain text: no em dashes, middle dots, arrows, smart quotes
or ellipsis characters.

Usage: check-plain-ascii-text.py DIRECTORY
"""

import os
import sys

CHECKED_SUFFIXES = (
    ".c", ".h", ".css", ".py", ".sh", ".build", ".options", ".scd", ".in",
    ".md", ".json", ".xml", ".clang-format", "PKGBUILD", "configuration",
    ".install", ".conf", ".editorconfig", ".ini",
)

SKIPPED_DIRECTORIES = {".git", "__pycache__"}


def is_checked_directory(parent_directory, name):
    """Skips version control, caches and meson build directories, which hold generated upstream code."""
    if name in SKIPPED_DIRECTORIES:
        return False

    return not os.path.isdir(os.path.join(parent_directory, name, "meson-private"))


def checked_files_in(root_directory):
    for directory, subdirectories, file_names in os.walk(root_directory):
        subdirectories[:] = [name for name in subdirectories if is_checked_directory(directory, name)]

        yield from (os.path.join(directory, name) for name in file_names if name.endswith(CHECKED_SUFFIXES))


def non_ascii_lines(path):
    with open(path, encoding="utf-8", errors="replace") as text:
        lines = list(text)

    return [(number, line.rstrip("\n")) for number, line in enumerate(lines, start=1) if not line.isascii()]


def main(root_directory):
    problems = [f"{path}:{number}: {line}" for path in checked_files_in(root_directory) for number, line in non_ascii_lines(path)]

    for problem in problems:
        print(problem)

    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
