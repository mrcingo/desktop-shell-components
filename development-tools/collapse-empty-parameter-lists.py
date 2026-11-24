#!/usr/bin/env python3
"""Write "(void)" on one line in function definitions without parameters.

clang-format breaks every definition's parameter list onto lines of its
own, which turns a function without parameters into three lines holding
"(", "void" and ")". This pass, run after clang-format, joins them again.

Usage: collapse-empty-parameter-lists.py FILE...
"""

import re
import sys

EMPTY_PARAMETER_LIST = re.compile(r"\(\n[ \t]+void\n\)")


def collapse_file(path):
    with open(path) as source:
        text = source.read()

    collapsed = EMPTY_PARAMETER_LIST.sub("(void)", text)

    if collapsed != text:
        with open(path, "w") as target:
            target.write(collapsed)


if __name__ == "__main__":
    for file_path in sys.argv[1:]:
        collapse_file(file_path)
