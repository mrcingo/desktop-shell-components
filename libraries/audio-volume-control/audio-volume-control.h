#ifndef AUDIO_VOLUME_CONTROL_H
#define AUDIO_VOLUME_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The volume and mute state of the default audio output, followed through
 * the PulseAudio protocol (served by PipeWire on most systems): changes are
 * reported as they happen, with no polling. While the sound server is
 * missing, a connection is tried again every few seconds.
 */

struct audio_volume_state {
    bool is_connected;   // false while no sound server or output is known
    int  volume_percent; // average of the channels; may exceed 100
    bool is_muted;
};

struct audio_volume_control_handler {
    /* Called only when the state differs from the last one reported. */
    void (*handle_volume_changed)(void *user_data, const struct audio_volume_state *state);
};

/* Runs on the default GLib main context. */
struct audio_volume_control *audio_volume_control_create(const struct audio_volume_control_handler *handler, void *user_data);

void audio_volume_control_destroy(struct audio_volume_control *control);

const struct audio_volume_state *audio_volume_control_get_state(const struct audio_volume_control *control);

void audio_volume_control_toggle_mute(struct audio_volume_control *control);

/* Raises (positive) or lowers the volume by percent_points; raising stops at 100. */
void audio_volume_control_change_volume(struct audio_volume_control *control, int percent_points);

/* Conversions between PulseAudio volume levels and percentages, where 100% is the normal level. */
int      audio_volume_percent_from_level(uint32_t volume_level);
uint32_t audio_volume_level_from_percent(int percent);

#endif
