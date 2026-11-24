#ifndef COMPOSITOR_MATCHED_APPEARANCE_H
#define COMPOSITOR_MATCHED_APPEARANCE_H

#include <stdbool.h>

#include <gdk/gdk.h>

#include "compositor-ipc-client.h"

/*
 * A program's style sheets plus, when wanted, the colors and font of the
 * user's sway bar, so the program looks like the rest of the desktop without
 * any configuration. Call refresh after sway reloads its configuration.
 *
 * Destroy before the IPC client, within the same main loop iteration.
 */

struct compositor_matched_appearance;

struct compositor_matched_appearance *compositor_matched_appearance_create(
    GdkDisplay                   *display,
    struct compositor_ipc_client *client,
    const char                   *data_directory,
    const char                   *program_name,
    const char                   *style_sheet_override_path
);

void compositor_matched_appearance_destroy(struct compositor_matched_appearance *appearance);

/* Re-reads every style sheet and, when following the bar, the bar's look. */
void compositor_matched_appearance_refresh(struct compositor_matched_appearance *appearance, bool follows_sway_bar);

#endif
