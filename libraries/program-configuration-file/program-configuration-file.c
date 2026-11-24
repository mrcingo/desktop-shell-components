#include <glib.h>

#include "program-configuration-file.h"

static const char configuration_directory_name[] = "desktop-shell-components";

struct program_configuration_file {
    GKeyFile *key_file;
    char     *path; // NULL when the defaults are in use
};

static char *
configuration_path_in(
    const char *base_directory,
    const char *program_name
)
{
    char *file_name = g_strconcat(program_name, ".ini", NULL);
    char *path      = g_build_filename(base_directory, configuration_directory_name, file_name, NULL);

    g_free(file_name);

    return path;
}

/* The program's file in base_directory, or NULL when there is none. */
static char *
existing_path_in(
    const char *base_directory,
    const char *program_name
)
{
    char *path = configuration_path_in(base_directory, program_name);

    if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        return path;
    }

    g_free(path);

    return NULL;
}

static char *
first_existing_path(
    const char *const *base_directories,
    const char        *program_name
)
{
    for (size_t i = 0; base_directories[i] != NULL; i++) {
        char *path = existing_path_in(base_directories[i], program_name);

        if (path != NULL) {
            return path;
        }
    }

    return NULL;
}

/* The user's file comes before the system-wide ones, in XDG order. */
static char *
find_configuration_path(
    const char *program_name
)
{
    const char *const user_directory[] = {g_get_user_config_dir(), NULL};
    char             *user_path        = first_existing_path(user_directory, program_name);

    if (user_path != NULL) {
        return user_path;
    }

    return first_existing_path(g_get_system_config_dirs(), program_name);
}

static bool
load_key_file(
    GKeyFile   *key_file,
    const char *path
)
{
    GError *error     = NULL;
    bool    is_loaded = g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, &error);

    if (!is_loaded) {
        g_warning("%s: %s; using the defaults", path, error->message);
        g_error_free(error);
    }

    return is_loaded;
}

static void
read_file_if_found(
    struct program_configuration_file *configuration,
    const char                        *path
)
{
    if (path == NULL) {
        return; // no file anywhere: every setting keeps its default
    }

    if (load_key_file(configuration->key_file, path)) {
        configuration->path = g_strdup(path);
    }
}

/* The --config file when given, else the first one found in the XDG directories. */
static char *
choose_configuration_path(
    const char *program_name,
    const char *override_path
)
{
    if (override_path != NULL) {
        return g_strdup(override_path);
    }

    return find_configuration_path(program_name);
}

struct program_configuration_file *
program_configuration_file_load(
    const char *program_name,
    const char *override_path
)
{
    struct program_configuration_file *configuration = g_new0(struct program_configuration_file, 1);

    g_autofree char *path = choose_configuration_path(program_name, override_path);

    configuration->key_file = g_key_file_new();

    read_file_if_found(configuration, path);

    return configuration;
}

void
program_configuration_file_destroy(
    struct program_configuration_file *configuration
)
{
    if (configuration == NULL) {
        return;
    }

    g_key_file_unref(configuration->key_file);
    g_free(configuration->path);
    g_free(configuration);
}

const char *
program_configuration_file_get_path(
    struct program_configuration_file *configuration
)
{
    return configuration->path;
}

static bool
key_matches(
    const struct program_configuration_key *known_key,
    const char                             *group_name,
    const char                             *key_name
)
{
    if (g_strcmp0(known_key->group_name, group_name) != 0) {
        return false;
    }

    return g_strcmp0(known_key->key_name, key_name) == 0;
}

static bool
is_known_key(
    const struct program_configuration_key *known_keys,
    const char                             *group_name,
    const char                             *key_name
)
{
    for (size_t i = 0; known_keys[i].group_name != NULL; i++) {
        if (key_matches(&known_keys[i], group_name, key_name)) {
            return true;
        }
    }

    return false;
}

static void
report_unknown_keys_in_group(
    struct program_configuration_file      *configuration,
    const struct program_configuration_key *known_keys,
    const char                             *group_name
)
{
    char **key_names = g_key_file_get_keys(configuration->key_file, group_name, NULL, NULL);

    for (size_t i = 0; key_names[i] != NULL; i++) {
        if (!is_known_key(known_keys, group_name, key_names[i])) {
            g_warning("%s: [%s] has no setting \"%s\"; it is ignored", configuration->path, group_name, key_names[i]);
        }
    }

    g_strfreev(key_names);
}

void
program_configuration_file_report_unknown_keys(
    struct program_configuration_file      *configuration,
    const struct program_configuration_key *known_keys
)
{
    char **group_names = g_key_file_get_groups(configuration->key_file, NULL);

    for (size_t i = 0; group_names[i] != NULL; i++) {
        report_unknown_keys_in_group(configuration, known_keys, group_names[i]);
    }

    g_strfreev(group_names);
}

static bool
has_key(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name
)
{
    return g_key_file_has_key(configuration->key_file, group_name, key_name, NULL);
}

/* Where a setting lives, for reading it and for naming it in warnings. */
struct setting_location {
    struct program_configuration_file *configuration;
    const char                        *group_name;
    const char                        *key_name;
};

static void
report_invalid_value(
    const struct setting_location *location,
    const char                    *expected
)
{
    g_warning(
        "%s: [%s] %s must be %s; using the default",
        location->configuration->path,
        location->group_name,
        location->key_name,
        expected
    );
}

static bool
location_has_key(
    const struct setting_location *location
)
{
    return has_key(location->configuration, location->group_name, location->key_name);
}

char *
program_configuration_file_read_text(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    const char                        *default_value
)
{
    if (!has_key(configuration, group_name, key_name)) {
        return g_strdup(default_value);
    }

    return g_key_file_get_string(configuration->key_file, group_name, key_name, NULL);
}

static bool
parse_boolean(
    const struct setting_location *location,
    bool                          *value
)
{
    GError *error = NULL;

    *value = g_key_file_get_boolean(location->configuration->key_file, location->group_name, location->key_name, &error);

    bool is_valid = error == NULL;

    g_clear_error(&error);

    return is_valid;
}

/* False, with a warning for a value that is neither true nor false, when the default applies. */
static bool
read_valid_boolean(
    const struct setting_location *location,
    bool                          *value
)
{
    if (!location_has_key(location)) {
        return false;
    }

    if (parse_boolean(location, value)) {
        return true;
    }

    report_invalid_value(location, "true or false");

    return false;
}

bool
program_configuration_file_read_boolean(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    bool                               default_value
)
{
    const struct setting_location location = {configuration, group_name, key_name};
    bool                          value    = default_value;

    if (read_valid_boolean(&location, &value)) {
        return value;
    }

    return default_value;
}

static bool
is_in_range(
    int value,
    int minimum,
    int maximum
)
{
    if (value < minimum) {
        return false;
    }

    return value <= maximum;
}

static bool
parse_integer(
    const struct setting_location *location,
    int                           *value
)
{
    GError *error = NULL;

    *value = g_key_file_get_integer(location->configuration->key_file, location->group_name, location->key_name, &error);

    bool is_valid = error == NULL;

    g_clear_error(&error);

    return is_valid;
}

static void
report_value_out_of_range(
    const struct setting_location *location,
    int                            minimum,
    int                            maximum
)
{
    g_autofree char *expected = g_strdup_printf("a whole number from %d to %d", minimum, maximum);

    report_invalid_value(location, expected);
}

static bool
parse_integer_in_range(
    const struct setting_location *location,
    int                            minimum,
    int                            maximum,
    int                           *value
)
{
    if (!parse_integer(location, value)) {
        return false;
    }

    return is_in_range(*value, minimum, maximum);
}

/* False, with a warning for a value that is not a whole number in range, when the default applies. */
static bool
read_valid_integer(
    const struct setting_location *location,
    int                            minimum,
    int                            maximum,
    int                           *value
)
{
    if (!location_has_key(location)) {
        return false;
    }

    if (parse_integer_in_range(location, minimum, maximum, value)) {
        return true;
    }

    report_value_out_of_range(location, minimum, maximum);

    return false;
}

int
program_configuration_file_read_integer(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    int                                default_value,
    int                                minimum,
    int                                maximum
)
{
    const struct setting_location location = {configuration, group_name, key_name};
    int                           value    = default_value;

    if (read_valid_integer(&location, minimum, maximum, &value)) {
        return value;
    }

    return default_value;
}

static int
find_choice(
    const char *const *choices,
    const char        *value
)
{
    for (int i = 0; choices[i] != NULL; i++) {
        if (g_strcmp0(choices[i], value) == 0) {
            return i;
        }
    }

    return -1;
}

static int
choice_or_default(
    const struct setting_location *location,
    const char *const             *choices,
    const char                    *value,
    int                            default_index
)
{
    int choice_index = find_choice(choices, value);

    if (choice_index >= 0) {
        return choice_index;
    }

    g_autofree char *expected = g_strjoinv(" or ", (char **)choices);

    report_invalid_value(location, expected);

    return default_index;
}

int
program_configuration_file_read_choice(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name,
    const char *const                 *choices,
    int                                default_index
)
{
    const struct setting_location location = {configuration, group_name, key_name};

    g_autofree char *value = program_configuration_file_read_text(configuration, group_name, key_name, NULL);

    if (value == NULL) {
        return default_index;
    }

    return choice_or_default(&location, choices, value, default_index);
}
