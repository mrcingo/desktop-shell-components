#ifndef UTILITY_PROGRAM_STYLE_WRITER_H
#define UTILITY_PROGRAM_STYLE_WRITER_H

#include "theme-palette.h"

/*
 * The established look for the small programs around sway, in their own
 * formats: the lock screen's configuration (swaylock), the prompt bar's
 * configuration (swaynag) and the launcher's command line options (wmenu).
 * Each output starts with no marker; the caller adds its own first line.
 */

/* Free the results with g_free(). */
char *utility_program_style_write_lock_screen(const struct theme_palette *palette);
char *utility_program_style_write_prompt_bar(const struct theme_palette *palette);

/* "-f" "Monaspace Neon 10" "-N" "222222" ...; free with g_strfreev(). */
char **utility_program_style_launcher_arguments(const struct theme_palette *palette);

#endif
