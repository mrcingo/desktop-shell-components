#include <locale.h>
#include <stdlib.h>

#include <glib.h>

#include "desktop-session-entries.h"
#include "login-greeter-application.h"
#include "program-command-line.h"
#include "shared-style-sheets.h"
#include "software-rendering-preference.h"

/* Without any session there is nothing to log in to. */
static bool
run_greeter_with_sessions(
    const char                                *login_daemon_socket_path,
    GPtrArray                                 *session_entries,
    const struct program_command_line_options *command_line
)
{
    if (session_entries->len == 0) {
        g_printerr("No sessions found in %s\n", SESSION_ENTRIES_DIRECTORY);
        return false;
    }

    const struct login_greeter_options options = {
        .login_daemon_socket_path  = login_daemon_socket_path,
        .data_directory            = shared_style_sheets_resolve_data_directory(INSTALLED_DATA_DIRECTORY),
        .style_sheet_override_path = command_line->style_sheet_override_path,
        .session_entries           = session_entries,
    };

    return login_greeter_application_run(&options);
}

static bool
run_greeter_if_sessions_exist(
    const char                                *login_daemon_socket_path,
    const struct program_command_line_options *command_line
)
{
    GPtrArray *session_entries     = desktop_session_entries_load_directory(SESSION_ENTRIES_DIRECTORY);
    bool       session_was_started = run_greeter_with_sessions(login_daemon_socket_path, session_entries, command_line);

    g_ptr_array_unref(session_entries);

    return session_was_started;
}

static int
run_greeter(
    const struct program_command_line_options *command_line
)
{
    const char *login_daemon_socket_path = g_getenv("GREETD_SOCK");

    if (login_daemon_socket_path == NULL) {
        g_printerr("GREETD_SOCK is not set; the greeter must be started by greetd\n");
        return EXIT_FAILURE;
    }

    if (!run_greeter_if_sessions_exist(login_daemon_socket_path, command_line)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* Names are translated and sorted for the current locale. */
static void
prepare_process_environment(void)
{
    setlocale(LC_ALL, "");
    prefer_software_rendering_for_simple_interface();
}

int
main(
    int   argument_count,
    char *arguments[]
)
{
    struct program_command_line_options command_line = {0};

    prepare_process_environment();

    if (!program_command_line_parse(&argument_count, &arguments, "- login screen for greetd", &command_line)) {
        return EXIT_FAILURE;
    }

    int exit_status = run_greeter(&command_line);

    program_command_line_options_clear(&command_line);

    return exit_status;
}
