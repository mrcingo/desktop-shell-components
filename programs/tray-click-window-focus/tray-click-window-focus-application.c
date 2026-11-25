#include <json-glib/json-glib.h>

#include "compositor-ipc-client.h"
#include "process-window-focusing.h"
#include "status-notifier-activation-watcher.h"
#include "tray-click-window-focus-application.h"
#include "tray-click-window-focus-settings.h"
#include "typed-json-member-lookup.h"

struct tray_click_window_focus_application {
    const struct tray_click_window_focus_options *options;
    GMainLoop                                    *main_loop;

    struct compositor_ipc_client              *compositor;
    struct status_notifier_activation_watcher *watcher;
    struct tray_click_window_focus_settings   *settings;
    guint                                      second_attempt_source;
    int                                        clicked_process_id;
};

static void
focus_clicked_application(
    struct tray_click_window_focus_application *application
)
{
    process_window_focusing_focus(application->compositor, application->clicked_process_id, NULL, NULL, NULL);
}

/* An application hidden in its tray may only map its window after the click. */
static gboolean
focus_again(
    gpointer user_data
)
{
    struct tray_click_window_focus_application *application = user_data;

    application->second_attempt_source = 0;

    focus_clicked_application(application);

    return G_SOURCE_REMOVE;
}

static void
plan_second_attempt(
    struct tray_click_window_focus_application *application
)
{
    guint delay = (guint)application->settings->second_attempt_milliseconds;

    g_clear_handle_id(&application->second_attempt_source, g_source_remove);

    if (delay == 0) {
        return;
    }

    application->second_attempt_source = g_timeout_add(delay, focus_again, application);
}

static void
handle_item_activated(
    void *user_data,
    int   process_id
)
{
    struct tray_click_window_focus_application *application = user_data;

    if (!application->settings->is_enabled) {
        return;
    }

    application->clicked_process_id = process_id;

    focus_clicked_application(application);
    plan_second_attempt(application);
}

static const struct status_notifier_activation_watcher_handler watcher_handler = {
    .handle_item_activated = handle_item_activated,
};

static void
load_settings(
    struct tray_click_window_focus_application *application
)
{
    tray_click_window_focus_settings_destroy(application->settings);

    application->settings = tray_click_window_focus_settings_load(application->options->configuration_override_path);
}

/* sway reports a configuration reload as a workspace event with change "reload". */
static bool
is_configuration_reload(
    uint32_t  event_type,
    JsonNode *event_payload
)
{
    if (event_type != COMPOSITOR_IPC_EVENT_WORKSPACE_CHANGED) {
        return false;
    }

    if (!JSON_NODE_HOLDS_OBJECT(event_payload)) {
        return false;
    }

    return g_strcmp0(typed_json_lookup_string_member(json_node_get_object(event_payload), "change"), "reload") == 0;
}

static void
handle_compositor_event(
    void     *user_data,
    uint32_t  event_type,
    JsonNode *event_payload
)
{
    struct tray_click_window_focus_application *application = user_data;

    if (event_type == COMPOSITOR_IPC_EVENT_COMPOSITOR_SHUTTING_DOWN) {
        g_main_loop_quit(application->main_loop);
        return;
    }

    if (is_configuration_reload(event_type, event_payload)) {
        replaced_executable_restart_if_replaced(application->options->executable_restart);
        load_settings(application);
    }
}

/* The program belongs to the compositor's session and ends with it. */
static void
handle_compositor_lost(
    void *user_data
)
{
    struct tray_click_window_focus_application *application = user_data;

    g_main_loop_quit(application->main_loop);
}

static const struct compositor_ipc_client_handler compositor_handler = {
    .handle_event           = handle_compositor_event,
    .handle_connection_lost = handle_compositor_lost,
};

static bool
connect_to_compositor(
    struct tray_click_window_focus_application *application
)
{
    g_autoptr(GError) error = NULL;

    application->compositor = compositor_ipc_client_connect(
        application->options->compositor_socket_path,
        &compositor_handler,
        application,
        &error
    );

    if (application->compositor == NULL) {
        g_printerr("Cannot connect to the compositor: %s\n", error->message);
        return false;
    }

    compositor_ipc_client_send_request(
        application->compositor,
        COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS,
        "[\"workspace\", \"shutdown\"]",
        NULL,
        NULL
    );

    return true;
}

static bool
watch_tray_clicks(
    struct tray_click_window_focus_application *application
)
{
    g_autoptr(GError) error = NULL;

    application->watcher = status_notifier_activation_watcher_create(NULL, &watcher_handler, application, &error);

    if (application->watcher == NULL) {
        g_printerr("Cannot watch tray icon clicks: %s\n", error->message);
        return false;
    }

    return true;
}

static bool
run_until_compositor_exits(
    struct tray_click_window_focus_application *application
)
{
    if (!connect_to_compositor(application)) {
        return false;
    }

    if (!watch_tray_clicks(application)) {
        return false;
    }

    g_main_loop_run(application->main_loop);

    return true;
}

static void
release_application(
    struct tray_click_window_focus_application *application
)
{
    g_clear_handle_id(&application->second_attempt_source, g_source_remove);
    status_notifier_activation_watcher_destroy(application->watcher);
    g_clear_pointer(&application->compositor, compositor_ipc_client_destroy);
    tray_click_window_focus_settings_destroy(application->settings);
    g_main_loop_unref(application->main_loop);
}

/* With focusing turned off the program still runs, so a reload can turn it back on. */
bool
tray_click_window_focus_application_run(
    const struct tray_click_window_focus_options *options
)
{
    struct tray_click_window_focus_application application = {
        .options   = options,
        .main_loop = g_main_loop_new(NULL, FALSE),
    };

    load_settings(&application);

    bool ran_successfully = run_until_compositor_exits(&application);

    release_application(&application);

    return ran_successfully;
}
