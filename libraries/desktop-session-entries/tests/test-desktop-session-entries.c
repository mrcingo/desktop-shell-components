#include <glib.h>
#include <glib/gstdio.h>

#include "desktop-session-entries.h"

struct test_entry_file {
    const char *file_name;
    const char *contents;
};

struct test_registration {
    const char *test_path;
    GTestFunc   test_function;
};

static const struct test_entry_file entry_with_desktop_names[] = {
    {"sway.desktop", "[Desktop Entry]\nName=Sway\nExec=sway --unsupported-gpu\nType=Application\nDesktopNames=sway;wlroots\n"},
    {NULL,           NULL                                                                                                    },
};

/* Three of these are skipped with a warning: noexec, codes and garbage. */
static const struct test_entry_file valid_and_invalid_entries[] = {
    {"b.desktop",         "[Desktop Entry]\nName=B\nExec=b %U\n"                                              },
    {"a.desktop",         "[Desktop Entry]\nName=A\nExec=a\n"                                                 },
    {"hidden.desktop",    "[Desktop Entry]\nName=H\nExec=h\nHidden=true\n"                                    },
    {"nodisplay.desktop", "[Desktop Entry]\nName=N\nExec=n\nNoDisplay=true\n"                                 },
    {"noexec.desktop",    "[Desktop Entry]\nName=X\n"                                                         },
    {"codes.desktop",     "[Desktop Entry]\nName=C\nExec=%f\n"                                                },
    {"garbage.desktop",   "not a key file\n"                                                                  },
    {"README",            "[Desktop Entry]\nName=R\nExec=r\n"                                                 },
    {"missing.desktop",   "[Desktop Entry]\nName=M\nExec=m\nTryExec=desktop-session-entries-missing-program\n"},
    {"percent.desktop",   "[Desktop Entry]\nName=P\nExec=p %% %f\nTryExec=sh\n"                               },
    {NULL,                NULL                                                                                },
};

/* Named, as g_assert_cmpstrv() uses its arguments' text as a format. */
static const char *const desktop_names_command[]     = {"sway", "--unsupported-gpu", NULL};
static const char *const desktop_names_environment[] = {
    "XDG_SESSION_TYPE=wayland",
    "XDG_CURRENT_DESKTOP=sway:wlroots",
    "XDG_SESSION_DESKTOP=sway",
    NULL,
};
static const char *const field_code_command[] = {"b", NULL};
static const char *const percent_command[]    = {"p", "%", NULL};

static void
write_entry_file(
    const char                   *directory,
    const struct test_entry_file *file
)
{
    char *path = g_build_filename(directory, file->file_name, NULL);

    g_assert_true(g_file_set_contents(path, file->contents, -1, NULL));
    g_free(path);
}

static void
write_entry_files(
    const char                   *directory,
    const struct test_entry_file *files
)
{
    for (size_t i = 0; files[i].file_name != NULL; i++) {
        write_entry_file(directory, &files[i]);
    }
}

static void
remove_directory_file(
    const char *directory,
    const char *file_name
)
{
    char *path = g_build_filename(directory, file_name, NULL);

    g_unlink(path);
    g_free(path);
}

static void
remove_directory(
    char *directory
)
{
    GDir       *listing = g_dir_open(directory, 0, NULL);
    const char *file_name;

    while ((file_name = g_dir_read_name(listing)) != NULL) {
        remove_directory_file(directory, file_name);
    }

    g_dir_close(listing);
    g_rmdir(directory);
    g_free(directory);
}

/* Loads the entries of a fresh directory holding files. */
static GPtrArray *
load_entries_from_files(
    const struct test_entry_file *files
)
{
    char *directory = g_dir_make_tmp("desktop-session-entries-test-XXXXXX", NULL);

    g_assert_nonnull(directory);
    write_entry_files(directory, files);

    GPtrArray *entries = desktop_session_entries_load_directory(directory);

    remove_directory(directory);

    return entries;
}

static const struct desktop_session_entry *
entry_at(
    GPtrArray *entries,
    guint      index
)
{
    g_assert_cmpuint(index, <, entries->len);

    return g_ptr_array_index(entries, index);
}

static void
test_desktop_names(void)
{
    GPtrArray                          *entries = load_entries_from_files(entry_with_desktop_names);
    const struct desktop_session_entry *entry   = entry_at(entries, 0);

    g_assert_cmpuint(entries->len, ==, 1);
    g_assert_cmpstr(entry->display_name, ==, "Sway");
    g_assert_cmpstrv(entry->command_arguments, desktop_names_command);
    g_assert_cmpstrv(entry->environment_variables, desktop_names_environment);
    g_ptr_array_unref(entries);
}

static GPtrArray *
load_entries_expecting_skip_warnings(
    const struct test_entry_file *files,
    int                           warning_count
)
{
    for (int i = 0; i < warning_count; i++) {
        g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Skipping *");
    }

    GPtrArray *entries = load_entries_from_files(files);

    g_test_assert_expected_messages();

    return entries;
}

static void
test_skips_invalid(void)
{
    GPtrArray *entries = load_entries_expecting_skip_warnings(valid_and_invalid_entries, 3);

    g_assert_cmpuint(entries->len, ==, 3);
    g_assert_cmpstr(entry_at(entries, 0)->display_name, ==, "A");
    g_assert_cmpstr(entry_at(entries, 1)->display_name, ==, "B");
    g_assert_cmpstr(entry_at(entries, 2)->display_name, ==, "P");
    g_ptr_array_unref(entries);
}

static void
test_field_codes_are_removed(void)
{
    GPtrArray *entries = load_entries_expecting_skip_warnings(valid_and_invalid_entries, 3);

    g_assert_cmpstrv(entry_at(entries, 1)->command_arguments, field_code_command);
    g_assert_cmpstr(entry_at(entries, 1)->environment_variables[1], ==, "XDG_CURRENT_DESKTOP=b");
    g_assert_cmpstrv(entry_at(entries, 2)->command_arguments, percent_command);
    g_ptr_array_unref(entries);
}

static void
test_missing_dir(void)
{
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Cannot read sessions*");

    GPtrArray *entries = desktop_session_entries_load_directory("/nonexistent/wayland-sessions");

    g_test_assert_expected_messages();
    g_assert_cmpuint(entries->len, ==, 0);
    g_ptr_array_unref(entries);
}

static const struct test_registration test_registrations[] = {
    {"/desktop-session-entries/desktop-names",       test_desktop_names          },
    {"/desktop-session-entries/skips-invalid",       test_skips_invalid          },
    {"/desktop-session-entries/field-codes-removed", test_field_codes_are_removed},
    {"/desktop-session-entries/missing-dir",         test_missing_dir            },
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(test_registrations); i++) {
        g_test_add_func(test_registrations[i].test_path, test_registrations[i].test_function);
    }

    return g_test_run();
}
