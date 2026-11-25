#define _GNU_SOURCE // syscall()

#include <errno.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <gio/gio.h>

#include "process-signal-handle.h"

struct process_signal_handle {
    int process_file_descriptor;
};

static bool
is_refused_process_id(
    int process_id
)
{
    if (process_id <= 1) {
        return true;
    }

    return process_id == getpid();
}

static void
set_error_from_errno(
    GError    **error,
    const char *action,
    int         error_number
)
{
    g_set_error(error, G_IO_ERROR, g_io_error_from_errno(error_number), "Cannot %s: %s", action, g_strerror(error_number));
}

/* Returns -1 with error set when the process cannot be opened. */
static int
open_process_file_descriptor(
    int      process_id,
    GError **error
)
{
    if (is_refused_process_id(process_id)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "Refusing to signal process %d", process_id);
        return -1;
    }

    int process_file_descriptor = (int)syscall(SYS_pidfd_open, process_id, 0);

    if (process_file_descriptor < 0) {
        set_error_from_errno(error, "open the process", errno);
    }

    return process_file_descriptor;
}

struct process_signal_handle *
process_signal_handle_open(
    int      process_id,
    GError **error
)
{
    int process_file_descriptor = open_process_file_descriptor(process_id, error);

    if (process_file_descriptor < 0) {
        return NULL;
    }

    struct process_signal_handle *handle = g_new0(struct process_signal_handle, 1);

    handle->process_file_descriptor = process_file_descriptor;

    return handle;
}

void
process_signal_handle_close(
    struct process_signal_handle *handle
)
{
    if (handle == NULL) {
        return;
    }

    close(handle->process_file_descriptor);
    g_free(handle);
}

bool
process_signal_handle_send(
    const struct process_signal_handle *handle,
    int                                 signal_number,
    GError                            **error
)
{
    if (syscall(SYS_pidfd_send_signal, handle->process_file_descriptor, signal_number, NULL, 0) == 0) {
        return true;
    }

    set_error_from_errno(error, "signal the process", errno);

    return false;
}
