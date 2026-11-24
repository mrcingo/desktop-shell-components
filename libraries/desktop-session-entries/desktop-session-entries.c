#include <stdbool.h>
#include <string.h>

#include "desktop-session-entries.h"

static const char desktop_entry_group[] = G_KEY_FILE_DESKTOP_GROUP;

/* Not among GLib's G_KEY_FILE_DESKTOP_KEY_* constants. */
static const char desktop_names_key[] = "DesktopNames";

static const char desktop_entry_suffix[] = ".desktop";

static void
destroy_session_entry(
    void *data
)
{
    struct desktop_session_entry *entry = data;

    g_free(entry->display_name);
    g_strfreev(entry->command_arguments);
    g_strfreev(entry->environment_variables);
    g_free(entry);
}

static bool
read_boolean_key(
    GKeyFile   *key_file,
    const char *key
)
{
    return g_key_file_get_boolean(key_file, desktop_entry_group, key, NULL);
}

static bool
program_is_in_path(
    const char *program_name
)
{
    char *program_path = g_find_program_in_path(program_name);
    bool  is_found     = program_path != NULL;

    g_free(program_path);

    return is_found;
}

/* A missing TryExec program means the session is not installed. */
static bool
is_installed(
    GKeyFile *key_file
)
{
    char *try_exec_program = g_key_file_get_string(key_file, desktop_entry_group, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, NULL);
    bool  is_found         = true;

    if (try_exec_program != NULL) {
        is_found = program_is_in_path(try_exec_program);
    }

    g_free(try_exec_program);

    return is_found;
}

static bool
is_shown_to_users(
    GKeyFile *key_file
)
{
    if (read_boolean_key(key_file, G_KEY_FILE_DESKTOP_KEY_HIDDEN)) {
        return false;
    }

    if (read_boolean_key(key_file, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY)) {
        return false;
    }

    return is_installed(key_file);
}

/* A field code is a % followed by exactly one character, such as %U. */
static bool
is_field_code(
    const char *argument
)
{
    if (argument[0] != '%') {
        return false;
    }

    if (argument[1] == '\0') {
        return false;
    }

    return argument[2] == '\0';
}

/* %% is the escape for a literal %, so it is kept as a single %. */
static bool
keep_argument(
    char *argument
)
{
    if (!is_field_code(argument)) {
        return true;
    }

    if (argument[1] != '%') {
        return false;
    }

    argument[1] = '\0';

    return true;
}

/* Moves the argument at index down to kept_count, or frees it; returns the new kept count. */
static size_t
keep_or_free_argument(
    char **arguments,
    size_t kept_count,
    size_t index
)
{
    if (!keep_argument(arguments[index])) {
        g_free(arguments[index]);
        return kept_count;
    }

    arguments[kept_count] = arguments[index];

    return kept_count + 1;
}

/* Field codes have no meaning for a session, so arguments made of one are dropped. */
static void
remove_field_codes(
    char **arguments
)
{
    size_t kept_count = 0;

    for (size_t i = 0; arguments[i] != NULL; i++) {
        kept_count = keep_or_free_argument(arguments, kept_count, i);
    }

    arguments[kept_count] = NULL;
}

static bool
has_desktop_names(
    char **desktop_names
)
{
    return desktop_names != NULL && desktop_names[0] != NULL;
}

/* Display managers export DesktopNames as XDG_CURRENT_DESKTOP, falling back to the entry's file name. */
static char *
describe_current_desktop(
    char      **desktop_names,
    const char *entry_base_name
)
{
    if (has_desktop_names(desktop_names)) {
        return g_strjoinv(":", desktop_names);
    }

    return g_strdup(entry_base_name);
}

static const char *
describe_session_desktop(
    char      **desktop_names,
    const char *entry_base_name
)
{
    if (has_desktop_names(desktop_names)) {
        return desktop_names[0];
    }

    return entry_base_name;
}

static char **
format_environment(
    const char *current_desktop,
    const char *session_desktop
)
{
    GStrvBuilder *builder = g_strv_builder_new();

    g_strv_builder_add(builder, "XDG_SESSION_TYPE=wayland");
    g_strv_builder_take(builder, g_strconcat("XDG_CURRENT_DESKTOP=", current_desktop, NULL));
    g_strv_builder_take(builder, g_strconcat("XDG_SESSION_DESKTOP=", session_desktop, NULL));

    return g_strv_builder_unref_to_strv(builder);
}

static char **
build_session_environment(
    GKeyFile   *key_file,
    const char *entry_base_name
)
{
    char **desktop_names   = g_key_file_get_string_list(key_file, desktop_entry_group, desktop_names_key, NULL, NULL);
    char  *current_desktop = describe_current_desktop(desktop_names, entry_base_name);
    char **environment     = format_environment(current_desktop, describe_session_desktop(desktop_names, entry_base_name));

    g_free(current_desktop);
    g_strfreev(desktop_names);

    return environment;
}

static char **
split_exec_line(
    GKeyFile *key_file,
    GError  **error
)
{
    char  *exec_line = g_key_file_get_string(key_file, desktop_entry_group, G_KEY_FILE_DESKTOP_KEY_EXEC, error);
    char **arguments = NULL;

    if (exec_line != NULL) {
        g_shell_parse_argv(exec_line, NULL, &arguments, error);
    }

    g_free(exec_line);

    return arguments;
}

static char **
reject_empty_command(
    char      **arguments,
    const char *entry_path
)
{
    if (arguments[0] != NULL) {
        return arguments;
    }

    g_warning("Skipping %s: empty Exec", entry_path);
    g_strfreev(arguments);

    return NULL;
}

/* Returns the parsed command, or NULL with error set or a warning logged. */
static char **
parse_exec_command(
    GKeyFile   *key_file,
    const char *entry_path,
    GError    **error
)
{
    char **arguments = split_exec_line(key_file, error);

    if (arguments == NULL) {
        return NULL;
    }

    remove_field_codes(arguments);

    return reject_empty_command(arguments, entry_path);
}

static struct desktop_session_entry *
create_session_entry(
    char  *display_name,
    char **command_arguments,
    char **environment_variables
)
{
    struct desktop_session_entry *entry = g_new0(struct desktop_session_entry, 1);

    entry->display_name          = display_name;
    entry->command_arguments     = command_arguments;
    entry->environment_variables = environment_variables;

    return entry;
}

static char *
read_display_name(
    GKeyFile *key_file,
    GError  **error
)
{
    return g_key_file_get_locale_string(key_file, desktop_entry_group, G_KEY_FILE_DESKTOP_KEY_NAME, NULL, error);
}

/* Both the name and the command must be readable; whichever was read is freed otherwise. */
static struct desktop_session_entry *
combine_entry_parts(
    char  *display_name,
    char **command_arguments,
    char **environment_variables
)
{
    if (display_name != NULL && command_arguments != NULL) {
        return create_session_entry(display_name, command_arguments, environment_variables);
    }

    g_free(display_name);
    g_strfreev(command_arguments);
    g_strfreev(environment_variables);

    return NULL;
}

static struct desktop_session_entry *
parse_session_entry(
    GKeyFile   *key_file,
    const char *entry_path,
    const char *entry_base_name,
    GError    **error
)
{
    if (!is_shown_to_users(key_file)) {
        return NULL;
    }

    char *display_name = read_display_name(key_file, error);

    if (display_name == NULL) {
        return NULL;
    }

    return combine_entry_parts(
        display_name,
        parse_exec_command(key_file, entry_path, error),
        build_session_environment(key_file, entry_base_name)
    );
}

static struct desktop_session_entry *
parse_session_file(
    GKeyFile   *key_file,
    const char *entry_path,
    const char *entry_base_name,
    GError    **error
)
{
    if (!g_key_file_load_from_file(key_file, entry_path, G_KEY_FILE_NONE, error)) {
        return NULL;
    }

    return parse_session_entry(key_file, entry_path, entry_base_name, error);
}

/* Consumes error, if any. */
static void
warn_about_skipped_entry(
    const char *entry_path,
    GError     *error
)
{
    if (error == NULL) {
        return;
    }

    g_warning("Skipping %s: %s", entry_path, error->message);
    g_error_free(error);
}

static struct desktop_session_entry *
load_session_entry(
    const char *entry_path,
    const char *entry_base_name
)
{
    GKeyFile                     *key_file = g_key_file_new();
    GError                       *error    = NULL;
    struct desktop_session_entry *entry    = parse_session_file(key_file, entry_path, entry_base_name, &error);

    warn_about_skipped_entry(entry_path, error);
    g_key_file_unref(key_file);

    return entry;
}

static int
compare_display_names(
    const void *first,
    const void *second
)
{
    const struct desktop_session_entry *first_entry  = *(struct desktop_session_entry *const *)first;
    const struct desktop_session_entry *second_entry = *(struct desktop_session_entry *const *)second;

    return g_utf8_collate(first_entry->display_name, second_entry->display_name);
}

static void
add_entry_if_loaded(
    GPtrArray                    *entries,
    struct desktop_session_entry *entry
)
{
    if (entry != NULL) {
        g_ptr_array_add(entries, entry);
    }
}

static void
load_named_entry(
    GPtrArray  *entries,
    const char *directory_path,
    const char *file_name
)
{
    size_t base_name_length = strlen(file_name) - strlen(desktop_entry_suffix);
    char  *entry_base_name  = g_strndup(file_name, base_name_length);
    char  *entry_path       = g_build_filename(directory_path, file_name, NULL);

    add_entry_if_loaded(entries, load_session_entry(entry_path, entry_base_name));
    g_free(entry_path);
    g_free(entry_base_name);
}

static void
load_directory_entry(
    GPtrArray  *entries,
    const char *directory_path,
    const char *file_name
)
{
    if (!g_str_has_suffix(file_name, desktop_entry_suffix)) {
        return;
    }

    load_named_entry(entries, directory_path, file_name);
}

static void
read_and_close_directory(
    GPtrArray  *entries,
    GDir       *directory,
    const char *directory_path
)
{
    const char *file_name;

    while ((file_name = g_dir_read_name(directory)) != NULL) {
        load_directory_entry(entries, directory_path, file_name);
    }

    g_dir_close(directory);
}

static GDir *
open_session_directory(
    const char *directory_path
)
{
    GError *error     = NULL;
    GDir   *directory = g_dir_open(directory_path, 0, &error);

    if (directory == NULL) {
        g_warning("Cannot read sessions: %s", error->message);
        g_error_free(error);
    }

    return directory;
}

GPtrArray *
desktop_session_entries_load_directory(
    const char *directory_path
)
{
    GPtrArray *entries   = g_ptr_array_new_with_free_func(destroy_session_entry);
    GDir      *directory = open_session_directory(directory_path);

    if (directory == NULL) {
        return entries;
    }

    read_and_close_directory(entries, directory, directory_path);
    g_ptr_array_sort(entries, compare_display_names);

    return entries;
}
