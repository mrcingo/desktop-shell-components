#ifndef SYSTEM_STATUS_LINE_BLOCKS_H
#define SYSTEM_STATUS_LINE_BLOCKS_H

#include <stdbool.h>

#include <glib.h>

#include "audio-volume-control.h"
#include "polled-system-readings.h"
#include "system-service-readings.h"
#include "system-status-line-settings.h"

/* Everything the status line shows at one moment. */
struct system_status_line_inputs {
    const struct polled_system_values  *polled_values;
    const struct system_service_values *service_values;
    const struct audio_volume_state    *volume;          // NULL without volume control
    const char                         *keyboard_layout; // NULL until known
    GDateTime                          *now;
    bool                                clock_shows_alternate_format;
};

/* One line of the status bar protocol, ready to write. Free it with g_free(). */
char *
system_status_line_render(const struct system_status_line_settings *settings, const struct system_status_line_inputs *inputs);

#endif
