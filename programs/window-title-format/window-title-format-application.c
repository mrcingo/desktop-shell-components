#include <inttypes.h>

#include <glib.h>

#include "compositor-state-tracker.h"
#include "window-title-format-application.h"
#include "window-title-format-command.h"
#include "window-title-format-settings.h"

struct window_title_format_application {
    const struct window_title_format_options *options;
    GMainLoop                                *main_loop;
    bool                                      started_successfully;

    struct compositor_state_tracker     *compositor_state;
    struct window_title_format_settings *settings;
    /* The command last sent per window, so unchanged windows cost nothing. */
    GHashTable *sent_commands; // int64_t container ID -> char *command
};

static GHashTable *
create_sent_commands_table(void)
{
    return g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, g_free);
}

static void
run_compositor_command(
    struct window_title_format_application *application,
    const char                             *command,
    compositor_ipc_reply_callback           reply_callback
)
{
    struct compositor_ipc_client *client = compositor_state_tracker_get_ipc_client(application->compositor_state);

    compositor_ipc_client_send_request(client, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, command, reply_callback, application);
}

/* Sends the window's command unless it was already sent, and records it in current_commands. */
static void
format_window_title(
    struct window_title_format_application *application,
    const struct compositor_window         *window,
    GHashTable                             *current_commands
)
{
    char       *command          = window_title_format_command_build(application->settings->title_format, window);
    const char *previous_command = g_hash_table_lookup(application->sent_commands, &window->container_id);

    if (g_strcmp0(command, previous_command) != 0) {
        run_compositor_command(application, command, NULL);
    }

    g_hash_table_insert(current_commands, g_memdup2(&window->container_id, sizeof(int64_t)), command);
}

/* Closed windows drop out, because only the windows of this tree are kept. */
static void
format_all_window_titles(
    struct window_title_format_application *application,
    const struct compositor_window_tree    *tree
)
{
    GHashTable *current_commands = create_sent_commands_table();

    for (guint i = 0; i < tree->all_windows->len; i++) {
        format_window_title(application, g_ptr_array_index(tree->all_windows, i), current_commands);
    }

    g_hash_table_unref(application->sent_commands);

    application->sent_commands = current_commands;
}

static void
handle_window_tree_updated(
    void                                *user_data,
    const struct compositor_window_tree *tree
)
{
    format_all_window_titles(user_data, tree);
}

static void
ignore_key_binding_command(
    void       *user_data,
    const char *command
)
{}

static void
handle_compositor_gone(
    void *user_data
)
{
    struct window_title_format_application *application = user_data;

    g_main_loop_quit(application->main_loop);
}

static void
quit_once_titles_are_restored(
    JsonNode *reply_payload,
    void     *user_data
)
{
    handle_compositor_gone(user_data);
}

/* One command for all windows, so a single reply says every title is back to the compositor's default. */
static char *
build_title_restoring_command(
    GHashTable *sent_commands
)
{
    GString       *command = g_string_new(NULL);
    GHashTableIter iterator;
    int64_t       *container_id;

    g_hash_table_iter_init(&iterator, sent_commands);

    while (g_hash_table_iter_next(&iterator, (gpointer *)&container_id, NULL)) {
        g_string_append_printf(command, "[con_id=%" PRId64 "] title_format %%title;", *container_id);
    }

    return g_string_free(command, FALSE);
}

/* Turning the titles off gives the windows their plain titles back, then leaves. */
static void
restore_titles_and_quit(
    struct window_title_format_application *application
)
{
    if (g_hash_table_size(application->sent_commands) == 0) {
        g_main_loop_quit(application->main_loop);
        return;
    }

    g_autofree char *command = build_title_restoring_command(application->sent_commands);

    run_compositor_command(application, command, quit_once_titles_are_restored);
}

/* A new format must reach every window, so the record of sent commands starts over. */
static void
reformat_all_window_titles(
    struct window_title_format_application *application
)
{
    const struct compositor_window_tree *tree = compositor_state_tracker_get_window_tree(application->compositor_state);

    g_hash_table_remove_all(application->sent_commands);

    if (tree != NULL) {
        format_all_window_titles(application, tree);
    }
}

static void
handle_compositor_configuration_reloaded(
    void *user_data
)
{
    struct window_title_format_application *application = user_data;

    window_title_format_settings_destroy(application->settings);

    application->settings = window_title_format_settings_load(application->options->configuration_override_path);

    if (!application->settings->is_enabled) {
        restore_titles_and_quit(application);
        return;
    }

    reformat_all_window_titles(application);
}

static const struct compositor_state_tracker_handler compositor_state_handler = {
    .handle_window_tree_updated               = handle_window_tree_updated,
    .handle_key_binding_command               = ignore_key_binding_command,
    .handle_compositor_gone                   = handle_compositor_gone,
    .handle_compositor_configuration_reloaded = handle_compositor_configuration_reloaded,
};

static bool
connect_to_compositor(
    struct window_title_format_application *application
)
{
    const char *socket_path = application->options->compositor_socket_path;
    GError     *error       = NULL;

    application
        ->compositor_state = compositor_state_tracker_connect(socket_path, &compositor_state_handler, application, &error);

    if (application->compositor_state == NULL) {
        g_printerr("Cannot connect to the compositor: %s\n", error->message);
        g_error_free(error);
    }

    return application->compositor_state != NULL;
}

/* With the titles turned off there is nothing to do, which counts as success. */
static bool
format_titles_until_compositor_exits(
    struct window_title_format_application *application
)
{
    if (!application->settings->is_enabled) {
        return true;
    }

    if (!connect_to_compositor(application)) {
        return false;
    }

    g_main_loop_run(application->main_loop);
    compositor_state_tracker_destroy(application->compositor_state);

    return true;
}

bool
window_title_format_application_run(
    const struct window_title_format_options *options
)
{
    struct window_title_format_application application = {
        .options       = options,
        .main_loop     = g_main_loop_new(NULL, FALSE),
        .settings      = window_title_format_settings_load(options->configuration_override_path),
        .sent_commands = create_sent_commands_table(),
    };

    bool ran_successfully = format_titles_until_compositor_exits(&application);

    g_hash_table_unref(application.sent_commands);
    window_title_format_settings_destroy(application.settings);
    g_main_loop_unref(application.main_loop);

    return ran_successfully;
}
