#ifndef FOREIGN_TOPLEVEL_REGISTRY_H
#define FOREIGN_TOPLEVEL_REGISTRY_H

#include <stdbool.h>

#include <wayland-client.h>

#include "ext-foreign-toplevel-list-v1-client-protocol.h"

/*
 * Every window the compositor lists through ext-foreign-toplevel-list-v1,
 * looked up by its stable identifier. sway reports the same identifier in
 * its window tree as foreign_toplevel_identifier, which links the two.
 *
 * Events arrive on the display's default queue, so whatever dispatches that
 * queue, such as GTK's main loop, keeps the registry current.
 */

struct foreign_toplevel_registry;

struct foreign_toplevel_registry *foreign_toplevel_registry_create(struct wl_display *display);

void foreign_toplevel_registry_destroy(struct foreign_toplevel_registry *registry);

/* NULL if the window is unknown or has closed. */
struct ext_foreign_toplevel_handle_v1 *
foreign_toplevel_registry_find_handle(struct foreign_toplevel_registry *registry, const char *identifier);

#endif
