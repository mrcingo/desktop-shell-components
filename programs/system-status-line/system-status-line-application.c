#include <stdio.h>
#include <string.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>

#include "audio-volume-control.h"
#include "clock-boundary-timing.h"
#include "compositor-attention-requests.h"
#include "compositor-keyboard-layout-follower.h"
#include "notification-center-status.h"
#include "polled-system-readings.h"
#include "status-bar-block-encoding.h"
#include "status-bar-click-decoding.h"
#include "system-service-readings.h"
#include "system-status-line-application.h"
#include "system-status-line-blocks.h"
#include "system-status-line-settings.h"

static const char keyboard_rules_file_path[] = "/usr/share/X11/xkb/rules/evdev.xml";

enum {
    SECONDS_PER_MINUTE = 60,
};

struct system_status_line_application {
    const struct system_status_line_options *options;
    struct system_status_line_settings      *settings;
    GMainLoop                               *main_loop;
    bool                                     output_works;

    struct polled_system_readings              *polled_readings;
    struct system_service_readings             *service_readings;
    struct audio_volume_control                *volume_control;    // NULL when the volume is not shown
    struct compositor_keyboard_layout_follower *keyboard_follower; // NULL when the layout is not shown
    struct notification_center_status          *notifications;     // NULL when notifications are not shown
    struct compositor_attention_requests       *attention;         // NULL when attention is not shown

    /* A new notification is emphasized for a few seconds by one one-shot timer. */
    guint             emphasis_source;
    bool              emphasizes_new_notification;
    uint32_t          seen_unread_count;
    char             *seen_latest_text;
    GDataInputStream *click_input;
    GCancellable     *click_cancellable;
    guint             clock_source;
    bool              clock_shows_alternate_format;
    char             *last_written_line;
};

/* ---- output */

static void
stop_on_output_failure(
    struct system_status_line_application *application
)
{
    application->output_works = false;

    g_main_loop_quit(application->main_loop);
}

/* The bar closing the pipe ends the program, as it does for every status command. */
static void
write_text(
    struct system_status_line_application *application,
    const char                            *text
)
{
    if (fputs(text, stdout) == EOF) {
        stop_on_output_failure(application);
        return;
    }

    if (fflush(stdout) != 0) {
        stop_on_output_failure(application);
    }
}

static const struct audio_volume_state *
volume_state_of(
    const struct system_status_line_application *application
)
{
    if (application->volume_control == NULL) {
        return NULL;
    }

    return audio_volume_control_get_state(application->volume_control);
}

static const char *
keyboard_layout_of(
    const struct system_status_line_application *application
)
{
    if (application->keyboard_follower == NULL) {
        return NULL;
    }

    return compositor_keyboard_layout_follower_get_layout_name(application->keyboard_follower);
}

static const struct notification_center_state *
notification_state_of(
    const struct system_status_line_application *application
)
{
    if (application->notifications == NULL) {
        return NULL;
    }

    return notification_center_status_get_state(application->notifications);
}

static int
attention_count_of(
    const struct system_status_line_application *application
)
{
    if (application->attention == NULL) {
        return -1;
    }

    return compositor_attention_requests_get_count(application->attention);
}

static char *
render_current_line(
    const struct system_status_line_application *application
)
{
    g_autoptr(GDateTime) now = g_date_time_new_now_local();

    const struct system_status_line_inputs inputs = {
        .polled_values                = polled_system_readings_get_values(application->polled_readings),
        .service_values               = system_service_readings_get_values(application->service_readings),
        .volume                       = volume_state_of(application),
        .keyboard_layout              = keyboard_layout_of(application),
        .notifications                = notification_state_of(application),
        .attention_count              = attention_count_of(application),
        .emphasizes_new_notification  = application->emphasizes_new_notification,
        .now                          = now,
        .clock_shows_alternate_format = application->clock_shows_alternate_format,
    };

    return system_status_line_render(application->settings, &inputs);
}

/* Unchanged lines are not written: the bar redraws on every line it reads. */
static void
write_status_line(
    struct system_status_line_application *application
)
{
    g_autofree char *line = render_current_line(application);

    if (g_strcmp0(line, application->last_written_line) == 0) {
        return;
    }

    write_text(application, line);
    g_free(application->last_written_line);

    application->last_written_line = g_steal_pointer(&line);
}

/* ---- clock */

static void schedule_clock(struct system_status_line_application *application);

static gboolean
handle_clock_boundary(
    void *user_data
)
{
    struct system_status_line_application *application = user_data;

    application->clock_source = 0;

    schedule_clock(application);
    write_status_line(application);

    return G_SOURCE_REMOVE;
}

static guint
clock_period_seconds(
    const struct system_status_line_application *application
)
{
    const char *format = system_status_line_settings_clock_format(
        application->settings,
        application->clock_shows_alternate_format
    );

    if (clock_format_shows_seconds(format)) {
        return 1;
    }

    return SECONDS_PER_MINUTE;
}

/* Wakes exactly when the shown time changes: each minute, or each second for formats with seconds. */
static void
schedule_clock(
    struct system_status_line_application *application
)
{
    guint delay = clock_milliseconds_until_next_boundary(g_get_real_time(), clock_period_seconds(application));

    g_clear_handle_id(&application->clock_source, g_source_remove);

    if (system_status_line_settings_shows(application->settings, SYSTEM_STATUS_LINE_BLOCK_CLOCK)) {
        application->clock_source = g_timeout_add(delay, handle_clock_boundary, application);
    }
}

/* ---- actions */

static bool
parse_command(
    const char *command_line,
    char     ***arguments
)
{
    g_autoptr(GError) error = NULL;

    if (!g_shell_parse_argv(command_line, NULL, arguments, &error)) {
        g_message("Cannot read the command \"%s\": %s", command_line, error->message);
        return false;
    }

    return true;
}

/* A default action whose program is not installed, such as pavucontrol, quietly does nothing. */
static bool
is_installed(
    const char *program_name
)
{
    g_autofree char *program_path = g_find_program_in_path(program_name);

    return program_path != NULL;
}

/* Without the bar's pipe: anything the program prints would break the status line. */
static void
spawn_detached(
    char      **arguments,
    const char *command_line
)
{
    g_autoptr(GError) error = NULL;

    GSpawnFlags flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDIN_FROM_DEV_NULL | G_SPAWN_STDOUT_TO_DEV_NULL;

    if (!g_spawn_async(NULL, arguments, NULL, flags, NULL, NULL, NULL, &error)) {
        g_message("Cannot run \"%s\": %s", command_line, error->message);
    }
}

/* Commands run without a shell. */
static void
run_command(
    const char *command_line
)
{
    g_auto(GStrv) arguments = NULL;

    if (!parse_command(command_line, &arguments)) {
        return;
    }

    if (!is_installed(arguments[0])) {
        return;
    }

    spawn_detached(arguments, command_line);
}

static void
toggle_mute(
    struct system_status_line_application *application
)
{
    if (application->volume_control != NULL) {
        audio_volume_control_toggle_mute(application->volume_control);
    }
}

static void
raise_volume(
    struct system_status_line_application *application
)
{
    if (application->volume_control != NULL) {
        audio_volume_control_change_volume(application->volume_control, application->settings->volume_step_percent);
    }
}

static void
lower_volume(
    struct system_status_line_application *application
)
{
    if (application->volume_control != NULL) {
        audio_volume_control_change_volume(application->volume_control, -application->settings->volume_step_percent);
    }
}

static void
raise_brightness(
    struct system_status_line_application *application
)
{
    polled_system_readings_change_backlight(application->polled_readings, application->settings->brightness_step_percent);
}

static void
lower_brightness(
    struct system_status_line_application *application
)
{
    polled_system_readings_change_backlight(application->polled_readings, -application->settings->brightness_step_percent);
}

static void
switch_power_profile(
    struct system_status_line_application *application
)
{
    system_service_readings_switch_power_profile(application->service_readings);
}

static void
toggle_bluetooth(
    struct system_status_line_application *application
)
{
    system_service_readings_toggle_bluetooth(application->service_readings);
}

/* Only the clock has a second format today; the action is accepted for every block. */
static void
toggle_clock_format(
    struct system_status_line_application *application
)
{
    application->clock_shows_alternate_format = !application->clock_shows_alternate_format;

    schedule_clock(application);
    write_status_line(application);
}

static void
activate_notification(
    struct system_status_line_application *application
)
{
    if (application->notifications != NULL) {
        notification_center_activate_latest(application->notifications);
    }
}

static void
mark_notifications_read(
    struct system_status_line_application *application
)
{
    if (application->notifications != NULL) {
        notification_center_mark_all_read(application->notifications);
    }
}

static void
toggle_do_not_disturb(
    struct system_status_line_application *application
)
{
    if (application->notifications != NULL) {
        notification_center_toggle_do_not_disturb(application->notifications);
    }
}

static void
focus_urgent_window(
    struct system_status_line_application *application
)
{
    if (application->attention != NULL) {
        compositor_attention_requests_focus_latest(application->attention);
    }
}

static const struct {
    const char *action_name;
    void (*perform)(struct system_status_line_application *application);
} built_in_actions[] = {
    {"toggle-mute",             toggle_mute            },
    {"volume-up",               raise_volume           },
    {"volume-down",             lower_volume           },
    {"brightness-up",           raise_brightness       },
    {"brightness-down",         lower_brightness       },
    {"next-power-profile",      switch_power_profile   },
    {"toggle-bluetooth",        toggle_bluetooth       },
    {"toggle-format",           toggle_clock_format    },
    {"activate-notification",   activate_notification  },
    {"mark-notifications-read", mark_notifications_read},
    {"toggle-do-not-disturb",   toggle_do_not_disturb  },
    {"focus-urgent-window",     focus_urgent_window    },
};

/* A built-in action by name, anything else is a command; empty does nothing. */
static void
perform_action(
    struct system_status_line_application *application,
    const char                            *action
)
{
    if (action[0] == '\0') {
        return;
    }

    for (size_t i = 0; i < G_N_ELEMENTS(built_in_actions); i++) {
        if (strcmp(action, built_in_actions[i].action_name) == 0) {
            built_in_actions[i].perform(application);
            return;
        }
    }

    run_command(action);
}

/* ---- clicks */

static bool
find_block_by_name(
    const char                         *block_name,
    enum system_status_line_block_kind *kind
)
{
    for (size_t i = 0; i < SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT; i++) {
        if (strcmp(block_name, system_status_line_block_name((enum system_status_line_block_kind)i)) == 0) {
            *kind = (enum system_status_line_block_kind)i;
            return true;
        }
    }

    return false;
}

static bool
is_mouse_slot(
    int slot
)
{
    if (slot < 0) {
        return false;
    }

    return slot < SYSTEM_STATUS_LINE_MOUSE_SLOT_COUNT;
}

/* Buttons 1 to 5 are left, middle, right, scroll up and scroll down; others do nothing. */
static void
handle_click(
    struct system_status_line_application *application,
    const struct status_bar_click         *click
)
{
    enum system_status_line_block_kind kind = 0;
    int                                slot = click->button - 1;

    if (!find_block_by_name(click->block_name, &kind)) {
        return;
    }

    if (!is_mouse_slot(slot)) {
        return;
    }

    perform_action(application, application->settings->blocks[kind].mouse_actions[slot]);
}

static void read_next_click(struct system_status_line_application *application);

static void
handle_click_text(
    struct system_status_line_application *application,
    const char                            *line
)
{
    struct status_bar_click click = {0};

    if (status_bar_click_decode(line, &click)) {
        handle_click(application, &click);
    }

    status_bar_click_clear(&click);
}

/* A closed or broken pipe from the bar ends the program; a cancelled read means it is ending already. */
static bool
reading_has_ended(
    struct system_status_line_application *application,
    const char                            *line,
    GError                                *error
)
{
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        return true;
    }

    if (line == NULL) {
        g_main_loop_quit(application->main_loop);
        return true;
    }

    return false;
}

static void
handle_click_line(
    GObject      *input,
    GAsyncResult *result,
    void         *user_data
)
{
    g_autoptr(GError) error = NULL;
    g_autofree char  *line  = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(input), result, NULL, &error);

    if (reading_has_ended(user_data, line, error)) {
        return;
    }

    handle_click_text(user_data, line);
    read_next_click(user_data);
}

static void
read_next_click(
    struct system_status_line_application *application
)
{
    g_data_input_stream_read_line_async(
        application->click_input,
        G_PRIORITY_DEFAULT,
        application->click_cancellable,
        handle_click_line,
        application
    );
}

static void
start_reading_clicks(
    struct system_status_line_application *application
)
{
    g_autoptr(GInputStream) standard_input = g_unix_input_stream_new(0, FALSE);

    application->click_input       = g_data_input_stream_new(standard_input);
    application->click_cancellable = g_cancellable_new();

    read_next_click(application);
}

/* ---- sources */

static void
handle_polled_values(
    void                              *user_data,
    const struct polled_system_values *values
)
{
    write_status_line(user_data);
}

static const struct polled_system_readings_handler polled_readings_handler = {
    .handle_values_read = handle_polled_values,
};

/* The traffic shown is that of the interface carrying the main connection. */
static void
handle_service_values(
    void                               *user_data,
    const struct system_service_values *values
)
{
    struct system_status_line_application *application = user_data;

    polled_system_readings_set_network_interface(application->polled_readings, values->network.interface_name);
    write_status_line(application);
}

static const struct system_service_readings_handler service_readings_handler = {
    .handle_values_changed = handle_service_values,
};

static void
handle_volume_changed(
    void                            *user_data,
    const struct audio_volume_state *state
)
{
    write_status_line(user_data);
}

static const struct audio_volume_control_handler volume_handler = {
    .handle_volume_changed = handle_volume_changed,
};

static void
handle_layout_changed(
    void       *user_data,
    const char *layout_name
)
{
    write_status_line(user_data);
}

static const struct compositor_keyboard_layout_follower_handler keyboard_handler = {
    .handle_layout_changed = handle_layout_changed,
};

static gboolean
end_emphasis(
    void *user_data
)
{
    struct system_status_line_application *application = user_data;

    application->emphasis_source             = 0;
    application->emphasizes_new_notification = false;

    write_status_line(application);

    return G_SOURCE_REMOVE;
}

/* One one-shot timer; a newer notification starts the seconds over. */
static void
start_emphasis(
    struct system_status_line_application *application
)
{
    unsigned emphasis_seconds = (unsigned)application->settings->new_notification_emphasis_seconds;

    if (emphasis_seconds == 0) {
        return;
    }

    g_clear_handle_id(&application->emphasis_source, g_source_remove);

    application->emphasizes_new_notification = true;
    application->emphasis_source             = g_timeout_add_seconds(emphasis_seconds, end_emphasis, application);
}

/* More unread than before, or another latest text while some are unread; never during do-not-disturb. */
static bool
is_new_arrival(
    const struct system_status_line_application *application,
    const struct notification_center_state      *state
)
{
    if (state->does_not_disturb) {
        return false;
    }

    if (state->unread_count > application->seen_unread_count) {
        return true;
    }

    return state->unread_count > 0 && g_strcmp0(state->latest_text, application->seen_latest_text) != 0;
}

static void
remember_notifications(
    struct system_status_line_application  *application,
    const struct notification_center_state *state
)
{
    application->seen_unread_count = state->unread_count;

    g_free(application->seen_latest_text);

    application->seen_latest_text = g_strdup(state->latest_text);
}

static void
handle_notifications_changed(
    void                                   *user_data,
    const struct notification_center_state *state
)
{
    struct system_status_line_application *application = user_data;

    if (is_new_arrival(application, state)) {
        start_emphasis(application);
    }

    remember_notifications(application, state);
    write_status_line(application);
}

static const struct notification_center_status_handler notifications_handler = {
    .handle_state_changed = handle_notifications_changed,
};

static void
handle_attention_count_changed(
    void *user_data,
    int   urgent_window_count
)
{
    write_status_line(user_data);
}

static const struct compositor_attention_requests_handler attention_handler = {
    .handle_count_changed = handle_attention_count_changed,
};

/* Which polled reading each block needs, in the order of enum polled_system_reading. */
static const enum system_status_line_block_kind polled_reading_blocks[POLLED_SYSTEM_READING_KIND_COUNT] = {
    SYSTEM_STATUS_LINE_BLOCK_PROCESSOR,
    SYSTEM_STATUS_LINE_BLOCK_TEMPERATURE,
    SYSTEM_STATUS_LINE_BLOCK_MEMORY,
    SYSTEM_STATUS_LINE_BLOCK_DISK,
    SYSTEM_STATUS_LINE_BLOCK_TRAFFIC,
    SYSTEM_STATUS_LINE_BLOCK_BRIGHTNESS,
};

static void
plan_polled_readings(
    const struct system_status_line_settings *settings,
    struct polled_system_readings_options    *options
)
{
    for (size_t i = 0; i < POLLED_SYSTEM_READING_KIND_COUNT; i++) {
        if (system_status_line_settings_shows(settings, polled_reading_blocks[i])) {
            options->readings |= 1u << i;
            options->interval_seconds[i] = settings->blocks[polled_reading_blocks[i]].interval_seconds;
        }
    }
}

static void
start_polled_readings(
    struct system_status_line_application *application
)
{
    struct polled_system_readings_options options = {
        .disk_path               = application->settings->disk_path,
        .temperature_sensor_name = application->settings->temperature_sensor_name,
    };

    plan_polled_readings(application->settings, &options);

    application->polled_readings = polled_system_readings_create(&options, &polled_readings_handler, application);
}

/* The traffic block needs the network's interface name. */
static const struct {
    enum system_status_line_block_kind kind;
    enum system_service_reading        reading;
} service_reading_blocks[] = {
    {SYSTEM_STATUS_LINE_BLOCK_NETWORK,       SYSTEM_SERVICE_READING_NETWORK      },
    {SYSTEM_STATUS_LINE_BLOCK_TRAFFIC,       SYSTEM_SERVICE_READING_NETWORK      },
    {SYSTEM_STATUS_LINE_BLOCK_BLUETOOTH,     SYSTEM_SERVICE_READING_BLUETOOTH    },
    {SYSTEM_STATUS_LINE_BLOCK_POWER_PROFILE, SYSTEM_SERVICE_READING_POWER_PROFILE},
    {SYSTEM_STATUS_LINE_BLOCK_BATTERY,       SYSTEM_SERVICE_READING_BATTERY      },
};

static unsigned
plan_service_readings(
    const struct system_status_line_settings *settings
)
{
    unsigned readings = 0;

    for (size_t i = 0; i < G_N_ELEMENTS(service_reading_blocks); i++) {
        if (system_status_line_settings_shows(settings, service_reading_blocks[i].kind)) {
            readings |= service_reading_blocks[i].reading;
        }
    }

    return readings;
}

static void
start_service_readings(
    struct system_status_line_application *application
)
{
    unsigned readings = plan_service_readings(application->settings);

    application->service_readings = system_service_readings_create(readings, &service_readings_handler, application);

    polled_system_readings_set_network_interface(
        application->polled_readings,
        system_service_readings_get_values(application->service_readings)->network.interface_name
    );
}

static void
start_volume_control(
    struct system_status_line_application *application
)
{
    if (system_status_line_settings_shows(application->settings, SYSTEM_STATUS_LINE_BLOCK_VOLUME)) {
        application->volume_control = audio_volume_control_create(&volume_handler, application);
    }
}

/* Without the compositor's socket the layout block stays hidden. */
static void
start_keyboard_follower(
    struct system_status_line_application *application
)
{
    const char *socket_path = application->options->compositor_socket_path;

    if (socket_path == NULL) {
        return;
    }

    if (!system_status_line_settings_shows(application->settings, SYSTEM_STATUS_LINE_BLOCK_KEYBOARD_LAYOUT)) {
        return;
    }

    application->keyboard_follower = compositor_keyboard_layout_follower_connect(
        socket_path,
        keyboard_rules_file_path,
        &keyboard_handler,
        application,
        NULL
    );
}

static void
start_notifications(
    struct system_status_line_application *application
)
{
    if (system_status_line_settings_shows(application->settings, SYSTEM_STATUS_LINE_BLOCK_NOTIFICATIONS)) {
        application->notifications = notification_center_status_create(&notifications_handler, application);
    }
}

/* Without the compositor's socket the attention block stays hidden. */
static void
start_attention(
    struct system_status_line_application *application
)
{
    const char *socket_path = application->options->compositor_socket_path;

    if (socket_path == NULL) {
        return;
    }

    if (!system_status_line_settings_shows(application->settings, SYSTEM_STATUS_LINE_BLOCK_ATTENTION)) {
        return;
    }

    application->attention = compositor_attention_requests_connect(socket_path, &attention_handler, application, NULL);
}

static void
start_sources(
    struct system_status_line_application *application
)
{
    start_notifications(application);
    start_attention(application);
    start_polled_readings(application);
    start_service_readings(application);
    start_volume_control(application);
    start_keyboard_follower(application);
    schedule_clock(application);
}

static void
stop_input_and_timers(
    struct system_status_line_application *application
)
{
    g_cancellable_cancel(application->click_cancellable);
    g_clear_handle_id(&application->clock_source, g_source_remove);
    g_clear_handle_id(&application->emphasis_source, g_source_remove);
}

static void
stop_attention_sources(
    struct system_status_line_application *application
)
{
    notification_center_status_destroy(application->notifications);
    compositor_attention_requests_destroy(application->attention);
}

/* Sources go first: their callbacks point at the application. */
static void
stop_everything(
    struct system_status_line_application *application
)
{
    stop_input_and_timers(application);
    stop_attention_sources(application);
    compositor_keyboard_layout_follower_destroy(application->keyboard_follower);
    audio_volume_control_destroy(application->volume_control);
    system_service_readings_destroy(application->service_readings);
    polled_system_readings_destroy(application->polled_readings);
}

static void
free_everything(
    struct system_status_line_application *application
)
{
    g_clear_object(&application->click_input);
    g_clear_object(&application->click_cancellable);
    g_main_loop_unref(application->main_loop);
    system_status_line_settings_destroy(application->settings);
    g_free(application->last_written_line);
    g_free(application->seen_latest_text);
}

static void
start(
    struct system_status_line_application *application
)
{
    g_autofree char *protocol_start = status_bar_protocol_start(true);

    write_text(application, protocol_start);
    start_sources(application);
    start_reading_clicks(application);
    write_status_line(application);
}

bool
system_status_line_application_run(
    const struct system_status_line_options *options
)
{
    struct system_status_line_application application = {
        .options      = options,
        .settings     = system_status_line_settings_load(options->configuration_override_path),
        .main_loop    = g_main_loop_new(NULL, FALSE),
        .output_works = true,
    };

    start(&application);

    if (application.output_works) {
        g_main_loop_run(application.main_loop);
    }

    stop_everything(&application);
    free_everything(&application);

    return true;
}
