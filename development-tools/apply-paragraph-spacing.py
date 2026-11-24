#!/usr/bin/env python3
"""Separate the steps of every C function body with blank lines.

clang-format aligns code but never adds blank lines, so this pass gives
function bodies their paragraph structure:

  * a blank line before every if, for, while, switch, do, return and goto,
    unless it opens its block or is a return ending a two-line block,
  * a blank line after every block that closes before more statements,
  * a blank line wherever the kind of statement changes between
    declarations, assignments and calls, so each kind forms its own
    aligned paragraph.

Only function bodies are touched: a function body opens with a line that
holds just "{", as .clang-format lays them out, so struct and enum bodies
keep their members together. Run it after clang-format, then run
clang-format again so alignment follows the new paragraphs.

Usage: apply-paragraph-spacing.py FILE...
"""

import re
import sys

CONTROL_STATEMENT = re.compile(r"^(if|for|while|switch|do|return|goto)\b")
CASE_LABEL        = re.compile(r"^(case\b.*|default):$")
ASSIGNMENT        = re.compile(r"^[\w.\->\[\]*()]+\s*([-+*/|&^]|<<|>>)?=[^=]")
DECLARATION       = re.compile(
    r"^(static\s+)?(const\s+)?"
    r"(struct\s+\w+|enum\s+\w+|unsigned\s+\w+|[A-Za-z_]\w*)"
    r"[\s*]+(const\s+)?\**\w+(\[[^\]]*\])?\s*(=.*)?[;,]?$"
)
NOT_A_DECLARATION = re.compile(r"^(return|goto|case|default|else|break|continue)\b")
CLOSING_FOLLOWERS = ("}", "else", "case ", "default:", "while", "#")


COMMENT_LINE      = re.compile(r"^(//|/\*|\*/|\*(\s|$))")


def is_comment(text):
    """A comment line, but not a statement that dereferences a pointer, such as "*value = 0;"."""
    return COMMENT_LINE.match(text) is not None


def ends_statement(text):
    return text.endswith(";")


def is_declaration(text):
    if NOT_A_DECLARATION.match(text):
        return False

    if "(" in text.split("=")[0]:
        return False  # a call or a function pointer

    return DECLARATION.match(text) is not None


def statement_kind(text):
    if is_declaration(text):
        return "declaration"

    if ASSIGNMENT.match(text):
        return "assignment"

    return "call"


def is_plain_statement(text):
    if not text or is_comment(text):
        return False

    if text.startswith(("#", "}", "{")):
        return False

    if CONTROL_STATEMENT.match(text) or CASE_LABEL.match(text):
        return False

    return not text.startswith(("else", "break", "continue"))


class FunctionBodySpacer:
    """Walks a file line by line and inserts blank lines inside function bodies."""

    def __init__(self):
        self.output_lines         = []
        self.brace_depth          = 0
        self.inside_function      = False
        self.previous             = ""
        self.before_previous      = ""
        self.statement_start      = None
        self.previous_statement   = None

    def starts_new_statement(self):
        if self.previous == "" or is_comment(self.previous):
            return True

        return self.previous.endswith((";", "{", "}"))

    def previous_line_completes_statement(self):
        if ends_statement(self.previous):
            return True

        return self.previous == "}"

    def ends_two_line_block(self, current):
        if not current.startswith(("return", "goto")):
            return False

        return self.before_previous.endswith("{")

    def wants_blank_before_control(self, current):
        if not CONTROL_STATEMENT.match(current):
            return False

        if not self.previous_line_completes_statement():
            return False  # opening a block, after a comment or a label, or continuing a statement

        return not self.ends_two_line_block(current)

    def wants_blank_after_block(self, current):
        if self.previous != "}" or current == "":
            return False

        return not current.startswith(CLOSING_FOLLOWERS)

    def wants_blank_between_kinds(self, current):
        if not ends_statement(self.previous):
            return False

        if not is_plain_statement(self.previous_statement):
            return False

        if not is_plain_statement(current):
            return False

        return statement_kind(self.previous_statement) != statement_kind(current)

    def already_separated(self):
        if not self.output_lines:
            return True

        return self.output_lines[-1].strip() == ""

    def wants_blank_line(self, current):
        if not self.inside_function or self.already_separated():
            return False

        if self.wants_blank_before_control(current):
            return True

        if self.wants_blank_after_block(current):
            return True

        return self.wants_blank_between_kinds(current)

    def track_braces(self, current):
        if current == "{" and self.brace_depth == 0:
            self.inside_function = True

        self.brace_depth += current.count("{") - current.count("}")

        if self.brace_depth == 0:
            self.inside_function = False

    def track_statements(self, current):
        if self.starts_new_statement() and current != "":
            self.statement_start = current

        if current.endswith(";"):
            self.previous_statement = self.statement_start
        elif current.endswith(("{", "}")):
            self.previous_statement = None

    def add_line(self, line):
        current = line.strip()

        if self.wants_blank_line(current):
            self.output_lines.append("\n")

        self.track_statements(current)
        self.output_lines.append(line)
        self.track_braces(current)
        self.before_previous, self.previous = self.previous, current


def space_file(path):
    spacer = FunctionBodySpacer()

    with open(path) as source:
        for line in source:
            spacer.add_line(line)

    with open(path, "w") as target:
        target.writelines(spacer.output_lines)


if __name__ == "__main__":
    for file_path in sys.argv[1:]:
        space_file(file_path)
