#define _DEFAULT_SOURCE // flock()

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <glib.h>

#include "single-instance-lock.h"

struct single_instance_lock {
    int file_descriptor;
};

/* $XDG_RUNTIME_DIR is private to the user, so no other user can take the lock. */
static char *
lock_file_path(
    const char *instance_name
)
{
    const char *display_name = g_getenv("WAYLAND_DISPLAY");

    if (display_name == NULL) {
        display_name = "wayland-0";
    }

    char *file_name = g_strdup_printf("%s-%s.lock", instance_name, display_name);
    char *path      = g_build_filename(g_get_user_runtime_dir(), file_name, NULL);

    g_free(file_name);

    return path;
}

static int
open_lock_file(
    const char *instance_name
)
{
    char *path            = lock_file_path(instance_name);
    int   file_descriptor = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);

    if (file_descriptor < 0) {
        g_warning("Cannot open %s: %s", path, g_strerror(errno));
    }

    g_free(path);

    return file_descriptor;
}

/* Returns false, closing the file, if another copy holds the lock. */
static bool
take_exclusive_lock(
    int file_descriptor
)
{
    if (flock(file_descriptor, LOCK_EX | LOCK_NB) == 0) {
        return true;
    }

    close(file_descriptor);

    return false;
}

static struct single_instance_lock *
wrap_locked_file(
    int file_descriptor
)
{
    struct single_instance_lock *lock = g_new(struct single_instance_lock, 1);

    lock->file_descriptor = file_descriptor;

    return lock;
}

struct single_instance_lock *
single_instance_lock_acquire(
    const char *instance_name
)
{
    int file_descriptor = open_lock_file(instance_name);

    if (file_descriptor < 0) {
        return NULL;
    }

    if (!take_exclusive_lock(file_descriptor)) {
        return NULL;
    }

    return wrap_locked_file(file_descriptor);
}

void
single_instance_lock_release(
    struct single_instance_lock *lock
)
{
    if (lock == NULL) {
        return;
    }

    close(lock->file_descriptor);
    g_free(lock);
}
