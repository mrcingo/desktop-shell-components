#include <string.h>

#include <glib.h>

#include "foreign-toplevel-registry.h"

struct listed_toplevel {
    struct foreign_toplevel_registry      *registry;
    struct ext_foreign_toplevel_handle_v1 *handle;

    char *identifier; // NULL until the compositor sends it
};

struct foreign_toplevel_registry {
    struct wl_registry                  *wayland_registry;
    struct ext_foreign_toplevel_list_v1 *toplevel_list;

    GHashTable *toplevels_by_identifier; // owns struct listed_toplevel
};

static void
destroy_listed_toplevel(
    void *data
)
{
    struct listed_toplevel *toplevel = data;

    ext_foreign_toplevel_handle_v1_destroy(toplevel->handle);
    g_free(toplevel->identifier);
    g_free(toplevel);
}

static void
handle_toplevel_closed(
    void                                  *data,
    struct ext_foreign_toplevel_handle_v1 *handle
)
{
    struct listed_toplevel *toplevel = data;

    if (toplevel->identifier == NULL) {
        destroy_listed_toplevel(toplevel);
        return;
    }

    g_hash_table_remove(toplevel->registry->toplevels_by_identifier, toplevel->identifier);
}

static void
ignore_toplevel_done(
    void                                  *data,
    struct ext_foreign_toplevel_handle_v1 *handle
)
{}

static void
ignore_toplevel_text(
    void                                  *data,
    struct ext_foreign_toplevel_handle_v1 *handle,
    const char                            *text
)
{}

/* Sent once, before the first done event. */
static void
handle_toplevel_identifier(
    void                                  *data,
    struct ext_foreign_toplevel_handle_v1 *handle,
    const char                            *identifier
)
{
    struct listed_toplevel *toplevel = data;

    if (toplevel->identifier != NULL) {
        return;
    }

    toplevel->identifier = g_strdup(identifier);

    g_hash_table_replace(toplevel->registry->toplevels_by_identifier, toplevel->identifier, toplevel);
}

static const struct ext_foreign_toplevel_handle_v1_listener toplevel_listener = {
    .closed     = handle_toplevel_closed,
    .done       = ignore_toplevel_done,
    .title      = ignore_toplevel_text,
    .app_id     = ignore_toplevel_text,
    .identifier = handle_toplevel_identifier,
};

static void
handle_new_toplevel(
    void                                  *data,
    struct ext_foreign_toplevel_list_v1   *toplevel_list,
    struct ext_foreign_toplevel_handle_v1 *handle
)
{
    struct listed_toplevel *toplevel = g_new0(struct listed_toplevel, 1);

    toplevel->registry = data;
    toplevel->handle   = handle;

    ext_foreign_toplevel_handle_v1_add_listener(handle, &toplevel_listener, toplevel);
}

static void
ignore_list_finished(
    void                                *data,
    struct ext_foreign_toplevel_list_v1 *toplevel_list
)
{}

static const struct ext_foreign_toplevel_list_v1_listener toplevel_list_listener = {
    .toplevel = handle_new_toplevel,
    .finished = ignore_list_finished,
};

static void
handle_global(
    void               *data,
    struct wl_registry *wayland_registry,
    uint32_t            global_name,
    const char         *interface_name,
    uint32_t            version
)
{
    struct foreign_toplevel_registry *registry = data;

    if (strcmp(interface_name, ext_foreign_toplevel_list_v1_interface.name) != 0) {
        return;
    }

    registry->toplevel_list = wl_registry_bind(wayland_registry, global_name, &ext_foreign_toplevel_list_v1_interface, 1);

    ext_foreign_toplevel_list_v1_add_listener(registry->toplevel_list, &toplevel_list_listener, registry);
}

static void
ignore_global_removal(
    void               *data,
    struct wl_registry *wayland_registry,
    uint32_t            global_name
)
{}

static const struct wl_registry_listener wayland_registry_listener = {
    .global        = handle_global,
    .global_remove = ignore_global_removal,
};

struct foreign_toplevel_registry *
foreign_toplevel_registry_create(
    struct wl_display *display
)
{
    struct foreign_toplevel_registry *registry = g_new0(struct foreign_toplevel_registry, 1);

    registry->toplevels_by_identifier = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, destroy_listed_toplevel);
    registry->wayland_registry        = wl_display_get_registry(display);

    wl_registry_add_listener(registry->wayland_registry, &wayland_registry_listener, registry);

    return registry;
}

void
foreign_toplevel_registry_destroy(
    struct foreign_toplevel_registry *registry
)
{
    if (registry == NULL) {
        return;
    }

    g_hash_table_unref(registry->toplevels_by_identifier);
    g_clear_pointer(&registry->toplevel_list, ext_foreign_toplevel_list_v1_destroy);
    wl_registry_destroy(registry->wayland_registry);
    g_free(registry);
}

struct ext_foreign_toplevel_handle_v1 *
foreign_toplevel_registry_find_handle(
    struct foreign_toplevel_registry *registry,
    const char                       *identifier
)
{
    struct listed_toplevel *toplevel = g_hash_table_lookup(registry->toplevels_by_identifier, identifier);

    if (toplevel == NULL) {
        return NULL;
    }

    return toplevel->handle;
}
