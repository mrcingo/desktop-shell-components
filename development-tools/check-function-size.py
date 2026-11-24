#!/usr/bin/env python3
"""Fail if a function is too long or holds a dense block.

Every function should read as a few obvious steps, with complexity pushed
down into helpers and libraries:

  * a function holds at most MAXIMUM_FUNCTION_STATEMENTS statements,
  * the body of an if, else, for, while or case holds at most
    MAXIMUM_BLOCK_STATEMENTS statements.

In C, a statement is anything ending in ";" plus each if, else, for, while
and switch header; a call spread over several lines counts once. A switch
body is not a block of its own: each case is checked as one. Function
bodies are recognised by structure, not layout: a top-level "{" right after
the ")" of a parameter list.

Python files are measured the same way from their syntax tree.

Usage: check-function-size.py FILE_OR_DIRECTORY...
A directory is searched for .c and .py files, skipping .git and __pycache__.
"""

import ast
import os
import re
import sys

MAXIMUM_FUNCTION_STATEMENTS = 7
MAXIMUM_BLOCK_STATEMENTS    = 3

C_CONTROL_HEADER    = re.compile(r"^(\} )?(if|else|for|while|switch|do)\b")
C_CASE_LABEL        = re.compile(r"^(case\b.*|default):$")
C_SWITCH_HEADER     = re.compile(r"^switch\b")
SKIPPED_DIRECTORIES = {".git", "__pycache__"}
PYTHON_BLOCKS       = (ast.If, ast.For, ast.While, ast.With, ast.Try)


# C source


def strip_comments_and_strings(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(\\.|[^"\\])*"', '""', text)

    return re.sub(r"'(\\.|[^'\\])*'", "''", text)


def count_top_level_semicolons(text, parenthesis_depth):
    """Semicolons outside parentheses, so a for header counts as one statement."""
    count = 0

    for character in text:
        parenthesis_depth += {"(": 1, ")": -1}.get(character, 0)

        if character == ";" and parenthesis_depth == 0:
            count += 1

    return count, parenthesis_depth


def count_line_statements(stripped, parenthesis_depth):
    headers                   = int(C_CONTROL_HEADER.match(stripped) is not None)
    semicolons, depth_after   = count_top_level_semicolons(stripped, parenthesis_depth)

    return headers + semicolons, depth_after


def count_c_statements(lines):
    count             = 0
    parenthesis_depth = 0

    for line in lines:
        found, parenthesis_depth = count_line_statements(line.strip(), parenthesis_depth)
        count                   += found

    return count


def previous_non_space_index(text, index):
    while index > 0 and text[index - 1].isspace():
        index -= 1

    return index - 1


def matching_open_parenthesis(text, closing_index):
    depth = 0

    for index in range(closing_index, -1, -1):
        depth += {")": 1, "(": -1}.get(text[index], 0)

        if depth == 0:
            return index

    return 0


def function_name_before(text, brace_index):
    """The identifier in front of the parameter list that ends just before the brace."""
    parameters_start = matching_open_parenthesis(text, previous_non_space_index(text, brace_index))
    match            = re.search(r"(\w+)\s*$", text[:parameters_start])

    return match.group(1) if match else "?"


def opens_function_body(text, brace_index):
    """A top-level "{" right after ")" opens a function; struct, enum and initializer braces never follow ")"."""
    before = previous_non_space_index(text, brace_index)

    return before >= 0 and text[before] == ")"


def top_level_brace_event(character, depth):
    if character == "{" and depth == 0:
        return "open"

    if character == "}" and depth == 1:
        return "close"

    return None


def brace_depths(text):
    """The brace depth in front of every character."""
    depth = 0

    for character in text:
        yield depth
        depth += {"{": 1, "}": -1}.get(character, 0)


def top_level_braces(text):
    events = map(top_level_brace_event, text, brace_depths(text))

    return [index for index, event in enumerate(events) if event]


def line_of(text, index):
    return text.count("\n", 0, index)


def find_c_function_bodies(text):
    """Yields (function name, opening line index, closing line index); top-level braces alternate open and close."""
    braces = list(top_level_braces(text))
    pairs  = zip(braces[0::2], braces[1::2])

    return [(function_name_before(text, opening), line_of(text, opening), line_of(text, closing)) for opening, closing in pairs if opens_function_body(text, opening)]


class CBlockScanner:
    """Tracks open blocks in one function body and measures each one as it closes."""

    def __init__(self, lines):
        self.lines        = lines
        self.open_blocks  = []  # (opening line index, kind) with kind "block", "switch" or "case"
        self.dense_blocks = []

    def close_block(self, closing):
        opening, kind = self.open_blocks.pop()
        count         = count_c_statements(self.lines[opening + 1:closing])

        if kind != "switch" and count > MAXIMUM_BLOCK_STATEMENTS:
            self.dense_blocks.append((opening, count))

    def close_case_if_open(self, closing):
        if self.open_blocks and self.open_blocks[-1][1] == "case":
            self.close_block(closing)

    def close_block_if_open(self, closing):
        if self.open_blocks:
            self.close_block(closing)

    def open_block_if_header(self, index, stripped):
        if not stripped.endswith("{"):
            return

        if not C_CONTROL_HEADER.match(stripped):
            return

        kind = "switch" if C_SWITCH_HEADER.match(stripped) else "block"
        self.open_blocks.append((index, kind))

    def start_case(self, index):
        self.close_case_if_open(index)
        self.open_blocks.append((index, "case"))

    def end_block(self, index):
        self.close_case_if_open(index)
        self.close_block_if_open(index)

    def scan_line(self, index):
        stripped = self.lines[index].strip()

        if C_CASE_LABEL.match(stripped):
            self.start_case(index)
            return

        if stripped.startswith("}"):
            self.end_block(index)

        self.open_block_if_header(index, stripped)

    def scan(self, first, last):
        for index in range(first + 1, last):
            self.scan_line(index)

        while self.open_blocks:
            self.close_block(last)

        return self.dense_blocks


def check_c_function(path, lines, name, first, last):
    total = count_c_statements(lines[first + 1:last])

    if total > MAXIMUM_FUNCTION_STATEMENTS:
        yield f"{path}:{first + 1}: {name} has {total} statements"

    for opening, count in CBlockScanner(lines).scan(first, last):
        yield f"{path}:{opening + 1}: block in {name} has {count} statements"


def check_c_file(path):
    with open(path) as source:
        text = strip_comments_and_strings(source.read())

    lines  = text.split("\n")
    bodies = find_c_function_bodies(text)

    return [problem for name, first, last in bodies for problem in check_c_function(path, lines, name, first, last)]


# Python source


def nested_python_statements(statement):
    if isinstance(statement, (ast.FunctionDef, ast.ClassDef)):
        return []

    bodies = [getattr(statement, name, []) for name in ("body", "orelse", "finalbody")]

    return [child for body in bodies for child in body]


def count_python_statements(statements):
    """Every statement counts once, including those nested in blocks."""
    return sum(1 + count_python_statements(nested_python_statements(statement)) for statement in statements)


def python_block_bodies(function):
    blocks = [node for node in ast.walk(function) if isinstance(node, PYTHON_BLOCKS)]

    return [(node.lineno, body) for node in blocks for body in (node.body, getattr(node, "orelse", [])) if body]


def check_python_function(function, path):
    total = count_python_statements(function.body)

    if total > MAXIMUM_FUNCTION_STATEMENTS:
        yield f"{path}:{function.lineno}: {function.name} has {total} statements"

    for line_number, body in python_block_bodies(function):
        count = count_python_statements(body)

        if count > MAXIMUM_BLOCK_STATEMENTS:
            yield f"{path}:{line_number}: block in {function.name} has {count} statements"


def check_python_file(path):
    with open(path) as source:
        tree = ast.parse(source.read(), path)

    functions = [node for node in ast.walk(tree) if isinstance(node, ast.FunctionDef)]

    return [problem for function in functions for problem in check_python_function(function, path)]


# Command line


def checked_files_in(directory):
    for current_directory, subdirectories, file_names in os.walk(directory):
        subdirectories[:] = [name for name in subdirectories if name not in SKIPPED_DIRECTORIES]

        yield from (os.path.join(current_directory, name) for name in sorted(file_names) if name.endswith((".c", ".py")))


def expand_directories(paths):
    for path in paths:
        if os.path.isdir(path):
            yield from checked_files_in(path)
        else:
            yield path


def check_any_file(path):
    if path.endswith(".py"):
        return check_python_file(path)

    return check_c_file(path)


def main(paths):
    problems = [problem for path in expand_directories(paths) for problem in check_any_file(path)]

    for problem in problems:
        print(problem)

    print(f"{len(problems)} problem(s)", file=sys.stderr)

    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
