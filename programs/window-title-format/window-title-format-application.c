#include <inttypes.h>

#include <glib.h>

#include "compositor-state-tracker.h"
#include "compositor-title-bar-style.h"
#include "rendered-text-width-measurement.h"
#include "replaced-executable-restart.h"
#include "typed-json-member-lookup.h"
#include "window-title-fitting.h"
#include "window-title-format-application.h"
#include "window-title-format-command.h"
#include "window-title-format-settings.h"

struct window_title_format_application {
    const struct window_title_format_options *options;
    GMainLoop                                *main_loop;
    bool                                      started_successfully;

    struct compositor_state_tracker     *compositor_state;
    struct window_title_format_settings *settings;
    /* Both NULL until the compositor's configuration has arrived. */
    struct compositor_title_bar_style      *title_bar_style;
    struct rendered_text_width_measurement *text_measurement;
    /* The command last sent per window, so unchanged windows cost nothing. */
    GHashTable *sent_commands; // int64_t container ID -> char *command
};

static GHashTable *
create_sent_commands_table(void)
{
    return g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, g_free);
}

static void
run_compositor_request(
    struct window_title_format_application *application,
    enum compositor_ipc_request_type        request_type,
    const char                             *payload,
    compositor_ipc_reply_callback           reply_callback
)
{
    struct compositor_ipc_client *client = compositor_state_tracker_get_ipc_client(application->compositor_state);

    compositor_ipc_client_send_request(client, request_type, payload, reply_callback, application);
}

static void
run_compositor_command(
    struct window_title_format_application *application,
    const char                             *command,
    compositor_ipc_reply_callback           reply_callback
)
{
    run_compositor_request(application, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, command, reply_callback);
}

/* NULL when the titles are only limited by their number of characters. */
static const struct rendered_text_width_measurement *
choose_text_measurement(
    struct window_title_format_application *application
)
{
    if (!application->settings->fits_title_bar) {
        return NULL;
    }

    return application->text_measurement;
}

static char *
build_window_title_command(
    struct window_title_format_application *application,
    const struct compositor_window         *window
)
{
    const struct window_title_fitting fitting = {
        .format                   = application->settings->title_format,
        .maximum_title_characters = application->settings->maximum_title_characters,
        .measurement              = choose_text_measurement(application),
        .text_space_width = compositor_title_bar_style_text_space(application->title_bar_style, window->title_bar_width),
    };

    g_autofree char *expanded_text = window_title_fitting_expand(&fitting, window);

    return window_title_format_command_build(fitting.format, window->container_id, expanded_text);
}

/* Sends the window's command unless it was already sent, and records it in current_commands. */
static void
format_window_title(
    struct window_title_format_application *application,
    const struct compositor_window         *window,
    GHashTable                             *current_commands
)
{
    char       *command          = build_window_title_command(application, window);
    const char *previous_command = g_hash_table_lookup(application->sent_commands, &window->container_id);

    if (g_strcmp0(command, previous_command) != 0) {
        run_compositor_command(application, command, NULL);
    }

    g_hash_table_insert(current_commands, g_memdup2(&window->container_id, sizeof(int64_t)), command);
}

/* Closed windows drop out, because only the windows of this tree are kept. Titles wait for the font. */
static void
format_all_window_titles(
    struct window_title_format_application *application,
    const struct compositor_window_tree    *tree
)
{
    if (application->title_bar_style == NULL) {
        return;
    }

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

/*
 * Key bindings resize windows and change layouts, which sway reports with
 * no event, so title bars may have a new width. "nop" bindings change nothing.
 */
static void
handle_key_binding_command(
    void       *user_data,
    const char *command
)
{
    struct window_title_format_application *application = user_data;

    if (g_str_has_prefix(command, "nop")) {
        return;
    }

    compositor_state_tracker_refresh_window_tree(application->compositor_state);
}

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
replace_title_bar_style(
    struct window_title_format_application *application,
    const char                             *configuration_text
)
{
    compositor_title_bar_style_destroy(application->title_bar_style);
    rendered_text_width_measurement_destroy(application->text_measurement);

    application->title_bar_style  = compositor_title_bar_style_parse(configuration_text);
    application->text_measurement = rendered_text_width_measurement_create(application->title_bar_style->font_description);
}

static JsonObject *
reply_object(
    JsonNode *reply_payload
)
{
    if (reply_payload == NULL) {
        return NULL;
    }

    if (!JSON_NODE_HOLDS_OBJECT(reply_payload)) {
        return NULL;
    }

    return json_node_get_object(reply_payload);
}

/* Without a usable reply the text is empty and sway's defaults apply, so titles still appear. */
static const char *
read_configuration_text(
    JsonNode *reply_payload
)
{
    const char *configuration_text = typed_json_lookup_string_member(reply_object(reply_payload), "config");

    if (configuration_text == NULL) {
        return "";
    }

    return configuration_text;
}

static void
handle_configuration_text(
    JsonNode *reply_payload,
    void     *user_data
)
{
    replace_title_bar_style(user_data, read_configuration_text(reply_payload));
    reformat_all_window_titles(user_data);
}

/* The font and padding come from the compositor's configuration, read again on every reload. */
static void
request_title_bar_style(
    struct window_title_format_application *application
)
{
    run_compositor_request(application, COMPOSITOR_IPC_REQUEST_GET_CONFIGURATION, NULL, handle_configuration_text);
}

static void
reload_settings(
    struct window_title_format_application *application
)
{
    window_title_format_settings_destroy(application->settings);

    application->settings = window_title_format_settings_load(application->options->configuration_override_path);
}

static void
handle_compositor_configuration_reloaded(
    void *user_data
)
{
    struct window_title_format_application *application = user_data;

    replaced_executable_restart_if_replaced(application->options->executable_restart);
    reload_settings(application);

    if (!application->settings->is_enabled) {
        restore_titles_and_quit(application);
        return;
    }

    request_title_bar_style(application);
}

static const struct compositor_state_tracker_handler compositor_state_handler = {
    .handle_window_tree_updated               = handle_window_tree_updated,
    .handle_key_binding_command               = handle_key_binding_command,
    .handle_compositor_gone                   = handle_compositor_gone,
    .handle_compositor_configuration_reloaded = handle_compositor_configuration_reloaded,
};

static void
report_connection_failure(
    GError *error
)
{
    g_printerr("Cannot connect to the compositor: %s\n", error->message);
    g_error_free(error);
}

/* Titles are shown in the title bars, so every title change matters. */
static void
start_following_titles(
    struct window_title_format_application *application
)
{
    compositor_state_tracker_follow_title_changes(application->compositor_state, true);
    request_title_bar_style(application);
}

static bool
connect_to_compositor(
    struct window_title_format_application *application
)
{
    GError *error = NULL;

    application->compositor_state = compositor_state_tracker_connect(
        application->options->compositor_socket_path,
        &compositor_state_handler,
        application,
        &error
    );

    if (application->compositor_state == NULL) {
        report_connection_failure(error);
        return false;
    }

    start_following_titles(application);

    return true;
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

static void
release_application(
    struct window_title_format_application *application
)
{
    g_hash_table_unref(application->sent_commands);
    compositor_title_bar_style_destroy(application->title_bar_style);
    rendered_text_width_measurement_destroy(application->text_measurement);
    window_title_format_settings_destroy(application->settings);
    g_main_loop_unref(application->main_loop);
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

    release_application(&application);

    return ran_successfully;
}
