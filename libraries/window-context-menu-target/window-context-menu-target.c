#include <glib.h>

#include "window-context-menu-target.h"

/* An X11 window's process ID belongs to the X server; signalling it would end every X11 window. */
static void
take_process(
    struct window_context_menu_target *target,
    const struct compositor_window    *window,
    const char                        *proc_directory
)
{
    if (window->is_x11_window) {
        return;
    }

    target->process       = process_information_snapshot_read(proc_directory, window->process_id);
    target->signal_handle = process_signal_handle_open(window->process_id, NULL);
}

struct window_context_menu_target *
window_context_menu_target_create(
    const struct compositor_window *window,
    const char                     *proc_directory
)
{
    struct window_context_menu_target *target = g_new0(struct window_context_menu_target, 1);

    *target = (struct window_context_menu_target){
        .container_id     = window->container_id,
        .title            = g_strdup(window->title),
        .application_name = g_strdup(window->application_name),
        .is_floating      = window->is_floating,
        .is_fullscreen    = window->is_fullscreen,
        .is_sticky        = window->is_sticky,
        .is_x11_window    = window->is_x11_window,
    };

    take_process(target, window, proc_directory);

    return target;
}

void
window_context_menu_target_destroy(
    struct window_context_menu_target *target
)
{
    if (target == NULL) {
        return;
    }

    process_signal_handle_close(target->signal_handle);
    process_information_snapshot_destroy(target->process);
    g_free(target->title);
    g_free(target->application_name);
    g_free(target);
}

char *
window_context_menu_target_describe_process(
    const struct window_context_menu_target *target
)
{
    if (target->is_x11_window) {
        return g_strdup_printf("%s  X11 window", target->application_name);
    }

    if (target->process == NULL) {
        return g_strdup_printf("%s  process gone", target->application_name);
    }

    g_autofree char *memory_size = process_information_format_memory_size(target->process->resident_memory_bytes);

    return g_strdup_printf("%s  PID %d  %s", target->application_name, target->process->process_id, memory_size);
}
