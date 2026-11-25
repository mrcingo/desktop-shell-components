#ifndef PROGRAM_COMMAND_LINE_H
#define PROGRAM_COMMAND_LINE_H

#include <stdbool.h>

/*
 * The command line every desktop component accepts: --help,
 * --config FILE, which reads settings from FILE instead of the usual
 * places, and --style FILE, which replaces the shipped style sheet.
 */

struct program_command_line_options {
    char *configuration_override_path; // NULL unless --config was given
    char *style_sheet_override_path;   // NULL unless --style was given
};

/* Prints the problem and returns false for an invalid command line. */
bool program_command_line_parse(
    int                                 *argument_count,
    char                              ***arguments,
    const char                          *program_summary,
    struct program_command_line_options *options
);

/* A program's own on/off option, such as --print-launcher-arguments. */
struct program_command_line_flag {
    const char *long_name; // without the leading "--"
    const char *description;
    bool       *is_given; // set to true when the option is on the command line
};

/* As program_command_line_parse, plus the program's own flags, a list ending with {NULL}. */
bool program_command_line_parse_with_flags(
    int                                    *argument_count,
    char                                 ***arguments,
    const char                             *program_summary,
    struct program_command_line_options    *options,
    const struct program_command_line_flag *flags
);

void program_command_line_options_clear(struct program_command_line_options *options);

#endif
