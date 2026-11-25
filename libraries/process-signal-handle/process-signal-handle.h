#ifndef PROCESS_SIGNAL_HANDLE_H
#define PROCESS_SIGNAL_HANDLE_H

#include <stdbool.h>

#include <glib.h>

/*
 * A handle on one process (a pidfd), taken when a menu opens, so a signal
 * sent later reaches that process and never a new one that reused its ID.
 * Refuses process IDs 0 and 1 and the calling process itself.
 */

struct process_signal_handle;

/* Returns NULL with error set when the process is gone or refused. */
struct process_signal_handle *process_signal_handle_open(int process_id, GError **error);

void process_signal_handle_close(struct process_signal_handle *handle);

bool process_signal_handle_send(const struct process_signal_handle *handle, int signal_number, GError **error);

#endif
