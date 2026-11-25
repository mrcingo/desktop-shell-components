#ifndef PROGRAM_CONFIGURATION_FILE_H
#define PROGRAM_CONFIGURATION_FILE_H

#include <stdbool.h>

/*
 * A program's optional INI configuration file, found like other XDG
 * programs find theirs: the path given with --config, else the first of
 *
 *   $XDG_CONFIG_HOME/desktop-shell-components/PROGRAM.ini
 *   $XDG_CONFIG_DIRS/desktop-shell-components/PROGRAM.ini   (/etc/xdg/...)
 *
 * that exists. Without any file every setting keeps its default. Invalid
 * values and unknown keys are reported with a warning naming the file and
 * key, and the default is used instead, so a mistake never stops a program.
 */

struct program_configuration_file;

/*
 * One setting a group accepts, for reporting misspelt keys. A group name
 * ending in "*", such as "app:*", stands for every group starting with
 * what comes before it, as in [app:Slack].
 */
struct program_configuration_key {
    const char *group_name;
    const char *key_name;
};

/* override_path may be NULL. Never returns NULL. */
struct program_configuration_file *program_configuration_file_load(const char *program_name, const char *override_path);

void program_configuration_file_destroy(struct program_configuration_file *configuration);

/* The file that was read, or NULL when the defaults are in use. */
const char *program_configuration_file_get_path(struct program_configuration_file *configuration);

/* Warns about every key that is not in known_keys, which ends with a {NULL, NULL} entry. */
void program_configuration_file_report_unknown_keys(
    struct program_configuration_file      *configuration,
    const struct program_configuration_key *known_keys
);

/* The names of the groups starting with prefix, in file order. Free with g_strfreev(). */
char **program_configuration_file_list_groups_with_prefix(struct program_configuration_file *configuration, const char *prefix);

/* Free the result with g_free(). */
char *program_configuration_file_read_text(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    const char                        *default_value
);

bool program_configuration_file_read_boolean(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    bool                               default_value
);

/* Values outside [minimum, maximum] are reported and replaced by the default. */
int program_configuration_file_read_integer(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    int                                default_value,
    int                                minimum,
    int                                maximum
);

/* Returns the index of the value in choices, a NULL-terminated list, or default_index. */
int program_configuration_file_read_choice(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    const char *const                 *choices,
    int                                default_index
);

#endif
