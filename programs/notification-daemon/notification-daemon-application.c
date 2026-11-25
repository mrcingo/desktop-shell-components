#include <json-glib/json-glib.h>

#include "compositor-ipc-client.h"
#include "notification-daemon-application.h"
#include "notification-daemon-settings.h"
#include "notification-sound-player.h"
#include "notification-specification-server.h"
#include "notification-store.h"
#include "process-window-focusing.h"
#include "typed-json-member-lookup.h"

/* The status bar shows the latest notification's application and summary in about this many characters. */
enum {
    LONGEST_LATEST_TEXT = 120,
};

struct notification_daemon_application {
    const struct notification_daemon_options *options;
    GMainLoop                                *main_loop;
    bool                                      lost_the_name;

    struct notification_daemon_settings      *settings;
    struct notification_store                *store;
    struct notification_sound_player         *sound_player;
    struct notification_specification_server *server;
    struct compositor_ipc_client             *compositor;    // NULL without a compositor
    GHashTable                               *expiry_timers; // notification ID -> GSource ID
    bool                                      do_not_disturb;
};

/* ---- State shown in the status bar ---- */

static char *
describe_latest(
    const struct notification_request *latest
)
{
    if (latest == NULL) {
        return g_strdup("");
    }

    if (latest->application_name[0] == '\0') {
        return notification_request_clean_text(latest->summary, LONGEST_LATEST_TEXT);
    }

    g_autofree char *text = g_strdup_printf("%s: %s", latest->application_name, latest->summary);

    return notification_request_clean_text(text, LONGEST_LATEST_TEXT);
}

static enum notification_urgency
urgency_of(
    const struct notification_request *latest
)
{
    if (latest == NULL) {
        return NOTIFICATION_URGENCY_NORMAL;
    }

    return latest->urgency;
}

static void
publish_center_state(
    struct notification_daemon_application *daemon
)
{
    struct notification_request *latest = notification_store_find_latest_unread(daemon->store);

    g_autofree char *latest_text = describe_latest(latest);

    const struct notification_center_state state = {
        .unread_count   = notification_store_count_unread(daemon->store),
        .latest_text    = latest_text,
        .latest_urgency = urgency_of(latest),
        .do_not_disturb = daemon->do_not_disturb,
    };

    notification_specification_server_publish_center_state(daemon->server, &state);
}

/* ---- Expiry: a notification counts as expired for its sender after its timeout ---- */

struct expiry {
    struct notification_daemon_application *daemon;
    uint32_t                                id;
};

static void
stop_expiry_timer(
    struct notification_daemon_application *daemon,
    uint32_t                                id
)
{
    gpointer source_id = NULL;

    if (!g_hash_table_steal_extended(daemon->expiry_timers, GUINT_TO_POINTER(id), NULL, &source_id)) {
        return;
    }

    g_source_remove(GPOINTER_TO_UINT(source_id));
}

/* Closes the notification for its sender; it stays unread for the status bar. */
static void
close_for_sender(
    struct notification_daemon_application *daemon,
    struct notification_request            *notification,
    enum notification_closed_reason         reason
)
{
    stop_expiry_timer(daemon, notification->id);

    if (!notification->is_open) {
        return;
    }

    notification->is_open = false;

    notification_specification_server_emit_closed(daemon->server, notification->id, reason);
}

static gboolean
expire_notification(
    gpointer user_data
)
{
    struct expiry               *expiry       = user_data;
    struct notification_request *notification = notification_store_find(expiry->daemon->store, expiry->id);

    g_hash_table_steal(expiry->daemon->expiry_timers, GUINT_TO_POINTER(expiry->id)); // the source ends by returning

    if (notification != NULL) {
        notification->is_open = false;

        notification_specification_server_emit_closed(expiry->daemon->server, expiry->id, NOTIFICATION_CLOSED_EXPIRED);
    }

    return G_SOURCE_REMOVE;
}

static int
rule_timeout_seconds(
    const struct notification_rule *rule
)
{
    if (rule == NULL) {
        return NOTIFICATION_RULE_UNSET;
    }

    return rule->timeout_seconds;
}

/* 0 means the notification never expires. A rule wins over the sender, which wins over the urgency's default. */
static guint
choose_timeout_milliseconds(
    const struct notification_daemon_application *daemon,
    const struct notification_request            *notification,
    const struct notification_rule               *rule
)
{
    int rule_seconds = rule_timeout_seconds(rule);

    if (rule_seconds != NOTIFICATION_RULE_UNSET) {
        return (guint)rule_seconds * 1000;
    }

    if (notification->expire_timeout_ms != NOTIFICATION_EXPIRES_AS_SERVER_CHOOSES) {
        return (guint)notification->expire_timeout_ms;
    }

    return (guint)daemon->settings->timeout_seconds[notification->urgency] * 1000;
}

static guint
add_expiry_source(
    struct expiry *expiry,
    guint          timeout_milliseconds
)
{
    return g_timeout_add_full(G_PRIORITY_DEFAULT, timeout_milliseconds, expire_notification, expiry, g_free);
}

static void
start_expiry_timer(
    struct notification_daemon_application *daemon,
    uint32_t                                id,
    guint                                   timeout_milliseconds
)
{
    if (timeout_milliseconds == 0) {
        return;
    }

    struct expiry *expiry = g_new0(struct expiry, 1);

    expiry->daemon = daemon;
    expiry->id     = id;

    g_hash_table_insert(
        daemon->expiry_timers,
        GUINT_TO_POINTER(id),
        GUINT_TO_POINTER(add_expiry_source(expiry, timeout_milliseconds))
    );
}

/* ---- New notifications ---- */

/* A rule may raise or lower the urgency the application chose. */
static void
apply_rule_urgency(
    struct notification_request    *notification,
    const struct notification_rule *rule
)
{
    if (rule == NULL) {
        return;
    }

    if (rule->urgency != NOTIFICATION_RULE_UNSET) {
        notification->urgency = rule->urgency;
    }
}

static bool
is_ignored(
    const struct notification_rule *rule
)
{
    if (rule == NULL) {
        return false;
    }

    return rule->ignores;
}

/*
 * Returns the notification's timeout. An ignored application's
 * notifications are accepted, never counted as unread and expire at once.
 */
static guint
apply_rule(
    struct notification_daemon_application *daemon,
    struct notification_request            *notification,
    const struct notification_rule         *rule
)
{
    apply_rule_urgency(notification, rule);

    if (is_ignored(rule)) {
        notification->is_transient = true;
        return 1;
    }

    return choose_timeout_milliseconds(daemon, notification, rule);
}

/* The application's own sound hints win; otherwise critical notifications get the more insistent sound. */
static const char *
our_sound_for(
    const struct notification_daemon_application *daemon,
    const struct notification_request            *notification
)
{
    if (notification->urgency == NOTIFICATION_URGENCY_CRITICAL) {
        return daemon->settings->critical_sound_file;
    }

    return daemon->settings->sound_file;
}

static void
play_sound_if_wanted(
    struct notification_daemon_application *daemon,
    const struct notification_request      *notification,
    const struct notification_rule         *rule
)
{
    const struct notification_sound_policy policy = {
        .default_choice                        = daemon->settings->sound_choice,
        .do_not_disturb                        = daemon->do_not_disturb,
        .critical_sounds_during_do_not_disturb = daemon->settings->critical_sounds_during_do_not_disturb,
    };

    if (daemon->sound_player == NULL) {
        return;
    }

    if (!notification_rules_should_play_sound(&policy, rule, notification)) {
        return;
    }

    g_autofree char *path = notification_sound_find(
        notification->sound_file,
        notification->sound_name,
        our_sound_for(daemon, notification)
    );

    notification_sound_player_play(daemon->sound_player, path, daemon->settings->sound_volume_percent);
}

/* A replaced notification keeps its ID, so only its timer stops; older ones dropped to make room expire. */
static void
forget_removed_one(
    struct notification_daemon_application *daemon,
    struct notification_request            *old,
    uint32_t                                new_id
)
{
    stop_expiry_timer(daemon, old->id);

    if (old->id != new_id) {
        close_for_sender(daemon, old, NOTIFICATION_CLOSED_EXPIRED);
    }
}

static void
forget_removed(
    struct notification_daemon_application *daemon,
    GPtrArray                              *removed,
    uint32_t                                new_id
)
{
    for (guint i = 0; i < removed->len; i++) {
        forget_removed_one(daemon, g_ptr_array_index(removed, i), new_id);
    }
}

static void
store_notification(
    struct notification_daemon_application *daemon,
    struct notification_request            *notification,
    uint32_t                                replaces_id
)
{
    g_autoptr(GPtrArray) removed = g_ptr_array_new_with_free_func((GDestroyNotify)notification_request_unref);

    notification_store_add(daemon->store, notification, replaces_id, removed);
    forget_removed(daemon, removed, notification->id);
}

/* The store holds its own reference. */
static uint32_t
release_and_return_id(
    struct notification_request *notification
)
{
    uint32_t id = notification->id;

    notification_request_unref(notification);

    return id;
}

static void
store_and_react(
    struct notification_daemon_application *daemon,
    struct notification_request            *notification,
    uint32_t                                replaces_id,
    const struct notification_rule         *rule
)
{
    guint timeout = apply_rule(daemon, notification, rule);

    store_notification(daemon, notification, replaces_id);
    start_expiry_timer(daemon, notification->id, timeout);
    play_sound_if_wanted(daemon, notification, rule);
    publish_center_state(daemon);
}

static uint32_t
handle_notify(
    void                        *user_data,
    struct notification_request *notification,
    uint32_t                     replaces_id
)
{
    struct notification_daemon_application *daemon = user_data;
    const struct notification_rule         *rule   = notification_rules_find(
        daemon->settings->rules,
        notification->application_name,
        notification->desktop_entry
    );

    store_and_react(daemon, notification, replaces_id, rule);

    return release_and_return_id(notification);
}

/* ---- Requests from senders and from the status bar ---- */

/* The specification asks for the signal even when the notification had already expired. */
static void
withdraw(
    struct notification_daemon_application *daemon,
    struct notification_request            *notification
)
{
    stop_expiry_timer(daemon, notification->id);
    notification_specification_server_emit_closed(daemon->server, notification->id, NOTIFICATION_CLOSED_BY_CALL);
    notification_request_unref(notification);
}

/* The sender withdrew it, as an application does once its message was read there. */
static void
handle_close_notification(
    void    *user_data,
    uint32_t id
)
{
    struct notification_daemon_application *daemon       = user_data;
    struct notification_request            *notification = notification_store_take(daemon->store, id);

    if (notification == NULL) {
        return;
    }

    withdraw(daemon, notification);
    publish_center_state(daemon);
}

/* Reading a notification dismisses it for its sender, unless the sender asked it to stay. */
static void
mark_read(
    struct notification_daemon_application *daemon,
    struct notification_request            *notification
)
{
    notification->is_unread = false;

    if (!notification->is_resident) {
        close_for_sender(daemon, notification, NOTIFICATION_CLOSED_DISMISSED);
    }
}

static void
handle_mark_all_read(
    void *user_data
)
{
    struct notification_daemon_application *daemon = user_data;

    g_autoptr(GPtrArray) newly_read = g_ptr_array_new_with_free_func((GDestroyNotify)notification_request_unref);

    notification_store_mark_all_read(daemon->store, newly_read);

    for (guint i = 0; i < newly_read->len; i++) {
        mark_read(daemon, g_ptr_array_index(newly_read, i));
    }

    publish_center_state(daemon);
}

struct activation {
    struct notification_daemon_application *daemon;
    struct notification_request            *notification;
};

/* After the window is up, the sender hears about the click, as it would from a popup. */
static void
finish_activation(
    struct activation *activation
)
{
    struct notification_request *notification = activation->notification;

    if (notification_request_has_action(notification, "default")) {
        notification_specification_server_emit_action_invoked(activation->daemon->server, notification->id, "default");
    }

    mark_read(activation->daemon, notification);
    publish_center_state(activation->daemon);
    notification_request_unref(notification);
    g_free(activation);
}

static void
handle_window_focused(
    bool  focused,
    void *user_data
)
{
    finish_activation(user_data);
}

static void
handle_sender_process(
    int   process_id,
    void *user_data
)
{
    struct activation *activation = user_data;

    if (activation->daemon->compositor == NULL) {
        finish_activation(activation);
        return;
    }

    process_window_focusing_focus(
        activation->daemon->compositor,
        process_id,
        activation->notification->application_name,
        handle_window_focused,
        activation
    );
}

static void
handle_activate_latest(
    void *user_data
)
{
    struct notification_daemon_application *daemon = user_data;
    struct notification_request            *latest = notification_store_find_latest_unread(daemon->store);

    if (latest == NULL) {
        return;
    }

    struct activation *activation = g_new0(struct activation, 1);

    *activation = (struct activation){.daemon = daemon, .notification = notification_request_ref(latest)};

    notification_specification_server_find_sender_process(
        daemon->server,
        latest->sender_bus_name,
        handle_sender_process,
        activation
    );
}

static void
handle_set_do_not_disturb(
    void *user_data,
    bool  do_not_disturb
)
{
    struct notification_daemon_application *daemon = user_data;

    daemon->do_not_disturb = do_not_disturb;

    publish_center_state(daemon);
}

static void
handle_name_lost(
    void *user_data
)
{
    struct notification_daemon_application *daemon = user_data;

    g_printerr("Another program already shows notifications on this session bus; leaving it in charge\n");

    daemon->lost_the_name = true;

    g_main_loop_quit(daemon->main_loop);
}

static const struct notification_specification_server_handler server_handler = {
    .handle_notify             = handle_notify,
    .handle_close_notification = handle_close_notification,
    .handle_activate_latest    = handle_activate_latest,
    .handle_mark_all_read      = handle_mark_all_read,
    .handle_set_do_not_disturb = handle_set_do_not_disturb,
    .handle_name_lost          = handle_name_lost,
};

/* ---- Settings and the compositor ---- */

static void
create_sound_player(
    struct notification_daemon_application *daemon
)
{
    const char *player_command = daemon->settings->sound_player_command;

    daemon->sound_player = notification_sound_player_create(player_command, daemon->settings->minimum_sound_interval_ms);

    if (daemon->sound_player == NULL) {
        g_warning("Cannot understand the sound player \"%s\"; notifications stay silent", player_command);
    }
}

static void
load_settings(
    struct notification_daemon_application *daemon
)
{
    const struct notification_daemon_options *options = daemon->options;

    notification_daemon_settings_destroy(daemon->settings);
    notification_sound_player_destroy(daemon->sound_player);

    daemon->settings = notification_daemon_settings_load(options->configuration_override_path, options->data_directory);

    create_sound_player(daemon);
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
    struct notification_daemon_application *daemon = user_data;

    if (event_type == COMPOSITOR_IPC_EVENT_COMPOSITOR_SHUTTING_DOWN) {
        g_main_loop_quit(daemon->main_loop);
        return;
    }

    if (is_configuration_reload(event_type, event_payload)) {
        replaced_executable_restart_if_replaced(daemon->options->executable_restart);
        load_settings(daemon);
    }
}

/* The daemon belongs to the compositor's session and ends with it. */
static void
handle_compositor_lost(
    void *user_data
)
{
    struct notification_daemon_application *daemon = user_data;

    g_main_loop_quit(daemon->main_loop);
}

static const struct compositor_ipc_client_handler compositor_handler = {
    .handle_event           = handle_compositor_event,
    .handle_connection_lost = handle_compositor_lost,
};

/* Without a compositor, notifications still work; only windows cannot be brought up. */
static struct compositor_ipc_client *
open_compositor_connection(
    struct notification_daemon_application *daemon
)
{
    g_autoptr(GError) error = NULL;

    struct compositor_ipc_client
        *client = compositor_ipc_client_connect(daemon->options->compositor_socket_path, &compositor_handler, daemon, &error);

    if (client == NULL) {
        g_warning("Cannot connect to the compositor: %s", error->message);
    }

    return client;
}

static void
connect_to_compositor(
    struct notification_daemon_application *daemon
)
{
    if (daemon->options->compositor_socket_path == NULL) {
        return;
    }

    daemon->compositor = open_compositor_connection(daemon);

    if (daemon->compositor == NULL) {
        return;
    }

    compositor_ipc_client_send_request(
        daemon->compositor,
        COMPOSITOR_IPC_REQUEST_SUBSCRIBE_TO_EVENTS,
        "[\"workspace\", \"shutdown\"]",
        NULL,
        NULL
    );
}

static void
handle_bus_closed(
    GDBusConnection *connection,
    gboolean         remote_peer_vanished,
    GError          *error,
    gpointer         user_data
)
{
    struct notification_daemon_application *daemon = user_data;

    g_main_loop_quit(daemon->main_loop);
}

/* ---- Running ---- */

static void
start(
    struct notification_daemon_application *daemon
)
{
    const struct notification_daemon_options *options = daemon->options;

    load_settings(daemon);

    daemon->store          = notification_store_create((guint)daemon->settings->history_size);
    daemon->do_not_disturb = daemon->settings->starts_in_do_not_disturb;
    daemon->server         = notification_specification_server_create(
        options->session_bus,
        options->server_version,
        &server_handler,
        daemon
    );

    connect_to_compositor(daemon);
    publish_center_state(daemon);
}

static void
stop(
    struct notification_daemon_application *daemon
)
{
    g_clear_pointer(&daemon->compositor, compositor_ipc_client_destroy);
    g_clear_pointer(&daemon->server, notification_specification_server_destroy);
    g_clear_pointer(&daemon->expiry_timers, g_hash_table_destroy);
    g_clear_pointer(&daemon->store, notification_store_destroy);
    g_clear_pointer(&daemon->sound_player, notification_sound_player_destroy);
    g_clear_pointer(&daemon->settings, notification_daemon_settings_destroy);
}

/* Timers are removed with their table; each owns its expiry record. */
static void
remove_timer(
    gpointer source_id
)
{
    g_source_remove(GPOINTER_TO_UINT(source_id));
}

static void
run_until_done(
    struct notification_daemon_application *daemon
)
{
    GDBusConnection *session_bus    = daemon->options->session_bus;
    gulong           closed_handler = g_signal_connect(session_bus, "closed", G_CALLBACK(handle_bus_closed), daemon);

    start(daemon);
    g_main_loop_run(daemon->main_loop);
    g_signal_handler_disconnect(session_bus, closed_handler);
    stop(daemon);
}

bool
notification_daemon_application_run(
    const struct notification_daemon_options *options
)
{
    struct notification_daemon_application daemon = {
        .options       = options,
        .main_loop     = g_main_loop_new(NULL, FALSE),
        .expiry_timers = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, remove_timer),
    };

    run_until_done(&daemon);
    g_main_loop_unref(daemon.main_loop);

    return !daemon.lost_the_name;
}
