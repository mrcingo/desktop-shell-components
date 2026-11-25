#include <glib.h>
#include <pulse/glib-mainloop.h>
#include <pulse/pulseaudio.h>

#include "audio-volume-control.h"

enum {
    RECONNECT_DELAY_SECONDS = 10,
};

static const char client_name[] = "desktop-shell-system-status-line";

struct audio_volume_control {
    const struct audio_volume_control_handler *handler;
    void                                      *user_data;

    pa_glib_mainloop *main_loop;
    pa_context       *context;
    guint             reconnect_source;

    char                     *default_output_name;
    pa_cvolume                output_volume;
    struct audio_volume_state state;
};

static void connect_to_sound_server(struct audio_volume_control *control);

int
audio_volume_percent_from_level(
    uint32_t volume_level
)
{
    return (int)(((uint64_t)volume_level * 100 + PA_VOLUME_NORM / 2) / PA_VOLUME_NORM);
}

uint32_t
audio_volume_level_from_percent(
    int percent
)
{
    return (uint32_t)((uint64_t)MAX(percent, 0) * PA_VOLUME_NORM / 100);
}

static bool
states_are_equal(
    const struct audio_volume_state *first,
    const struct audio_volume_state *second
)
{
    if (first->is_connected != second->is_connected) {
        return false;
    }

    if (first->volume_percent != second->volume_percent) {
        return false;
    }

    return first->is_muted == second->is_muted;
}

static void
report_state(
    struct audio_volume_control     *control,
    const struct audio_volume_state *new_state
)
{
    if (states_are_equal(&control->state, new_state)) {
        return;
    }

    control->state = *new_state;

    control->handler->handle_volume_changed(control->user_data, &control->state);
}

/* Operations run on their own; their handles are not needed. */
static void
release_operation(
    pa_operation *operation
)
{
    if (operation != NULL) {
        pa_operation_unref(operation);
    }
}

static void
handle_output_information(
    pa_context         *context,
    const pa_sink_info *output,
    int                 is_end_of_list,
    void               *user_data
)
{
    struct audio_volume_control *control = user_data;

    if (output == NULL) {
        return;
    }

    control->output_volume = output->volume;

    report_state(
        control,
        &(struct audio_volume_state){true, audio_volume_percent_from_level(pa_cvolume_avg(&output->volume)), output->mute != 0}
    );
}

static void
handle_server_information(
    pa_context           *context,
    const pa_server_info *server,
    void                 *user_data
)
{
    struct audio_volume_control *control = user_data;

    if (server == NULL) {
        return;
    }

    g_free(control->default_output_name);

    control->default_output_name = g_strdup(server->default_sink_name);

    release_operation(
        pa_context_get_sink_info_by_name(context, control->default_output_name, handle_output_information, control)
    );
}

/* The default output may have changed as well as its volume, so both are read again. */
static void
request_default_output(
    struct audio_volume_control *control
)
{
    release_operation(pa_context_get_server_info(control->context, handle_server_information, control));
}

static void
handle_sound_server_event(
    pa_context                  *context,
    pa_subscription_event_type_t event_type,
    uint32_t                     object_index,
    void                        *user_data
)
{
    request_default_output(user_data);
}

static void
start_following(
    struct audio_volume_control *control
)
{
    pa_context_set_subscribe_callback(control->context, handle_sound_server_event, control);
    release_operation(
        pa_context_subscribe(control->context, PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SERVER, NULL, NULL)
    );
    request_default_output(control);
}

static gboolean
reconnect(
    void *user_data
)
{
    struct audio_volume_control *control = user_data;

    control->reconnect_source = 0;

    connect_to_sound_server(control);

    return G_SOURCE_REMOVE;
}

static void
schedule_reconnect(
    struct audio_volume_control *control
)
{
    report_state(control, &(struct audio_volume_state){0});

    if (control->reconnect_source == 0) {
        control->reconnect_source = g_timeout_add_seconds(RECONNECT_DELAY_SECONDS, reconnect, control);
    }
}

static void
handle_context_state(
    pa_context *context,
    void       *user_data
)
{
    pa_context_state_t state = pa_context_get_state(context);

    if (state == PA_CONTEXT_READY) {
        start_following(user_data);
    }

    if (!PA_CONTEXT_IS_GOOD(state)) {
        schedule_reconnect(user_data);
    }
}

/* Callbacks are detached first, so nothing reaches a control that is going away. */
static void
release_context(
    struct audio_volume_control *control
)
{
    if (control->context == NULL) {
        return;
    }

    pa_context_set_state_callback(control->context, NULL, NULL);
    pa_context_set_subscribe_callback(control->context, NULL, NULL);
    pa_context_disconnect(control->context);
    g_clear_pointer(&control->context, pa_context_unref);
}

static void
connect_to_sound_server(
    struct audio_volume_control *control
)
{
    release_context(control);

    control->context = pa_context_new(pa_glib_mainloop_get_api(control->main_loop), client_name);

    pa_context_set_state_callback(control->context, handle_context_state, control);

    if (pa_context_connect(control->context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
        schedule_reconnect(control);
    }
}

struct audio_volume_control *
audio_volume_control_create(
    const struct audio_volume_control_handler *handler,
    void                                      *user_data
)
{
    struct audio_volume_control *control = g_new0(struct audio_volume_control, 1);

    control->handler   = handler;
    control->user_data = user_data;
    control->main_loop = pa_glib_mainloop_new(NULL);

    connect_to_sound_server(control);

    return control;
}

void
audio_volume_control_destroy(
    struct audio_volume_control *control
)
{
    if (control == NULL) {
        return;
    }

    g_clear_handle_id(&control->reconnect_source, g_source_remove);
    release_context(control);
    pa_glib_mainloop_free(control->main_loop);
    g_free(control->default_output_name);
    g_free(control);
}

const struct audio_volume_state *
audio_volume_control_get_state(
    const struct audio_volume_control *control
)
{
    return &control->state;
}

static bool
can_change_output(
    const struct audio_volume_control *control
)
{
    if (!control->state.is_connected) {
        return false;
    }

    return control->default_output_name != NULL;
}

void
audio_volume_control_toggle_mute(
    struct audio_volume_control *control
)
{
    if (!can_change_output(control)) {
        return;
    }

    release_operation(
        pa_context_set_sink_mute_by_name(control->context, control->default_output_name, !control->state.is_muted, NULL, NULL)
    );
}

static void
step_volume(
    pa_cvolume *volume,
    int         percent_points
)
{
    if (percent_points >= 0) {
        pa_cvolume_inc_clamp(volume, audio_volume_level_from_percent(percent_points), PA_VOLUME_NORM);
        return;
    }

    pa_cvolume_dec(volume, audio_volume_level_from_percent(-percent_points));
}

void
audio_volume_control_change_volume(
    struct audio_volume_control *control,
    int                          percent_points
)
{
    pa_cvolume volume = control->output_volume;

    if (!can_change_output(control)) {
        return;
    }

    step_volume(&volume, percent_points);
    release_operation(pa_context_set_sink_volume_by_name(control->context, control->default_output_name, &volume, NULL, NULL));
}
