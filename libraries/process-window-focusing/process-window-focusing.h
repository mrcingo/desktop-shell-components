#ifndef PROCESS_WINDOW_FOCUSING_H
#define PROCESS_WINDOW_FOCUSING_H

#include <stdbool.h>

#include "compositor-ipc-client.h"

/*
 * Brings up the window of a process (see process-window-lookup.h): reads
 * the compositor's window tree, picks the window and focuses it by its
 * container ID, which also switches to its workspace and tab.
 */

/* focused tells whether a window was found and asked to take focus. */
typedef void (*process_window_focusing_callback)(bool focused, void *user_data);

/* application_name may be NULL; callback may be NULL. The client must outlive the request. */
void process_window_focusing_focus(
    struct compositor_ipc_client    *client,
    int                              process_id,
    const char                      *application_name,
    process_window_focusing_callback callback,
    void                            *user_data
);

#endif
