#ifndef PROGRAM_COMMAND_LINE_H
#define PROGRAM_COMMAND_LINE_H

#include <stdbool.h>

/*
 * The command line every desktop component accepts: --help and
 * --style FILE, which replaces the component's shipped style sheet.
 */

struct program_command_line_options {
    char *style_sheet_override_path; // NULL unless --style was given
};

/* Prints the problem and returns false for an invalid command line. */
bool program_command_line_parse(
    int                                 *argument_count,
    char                              ***arguments,
    const char                          *program_summary,
    struct program_command_line_options *options
);

void program_command_line_options_clear(struct program_command_line_options *options);

#endif
