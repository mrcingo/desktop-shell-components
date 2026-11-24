/*
 * Prints every global the Wayland compositor in $WAYLAND_DISPLAY offers,
 * with its version: a quick way to see which protocols, such as window
 * capture, a compositor supports.
 */

#include <stdio.h>
#include <stdlib.h>

#include <wayland-client.h>

static void
print_global(
    void               *user_data,
    struct wl_registry *registry,
    uint32_t            global_name,
    const char         *interface_name,
    uint32_t            version
)
{
    printf("%s %u\n", interface_name, version);
}

static void
ignore_global_removal(
    void               *user_data,
    struct wl_registry *registry,
    uint32_t            global_name
)
{}

static const struct wl_registry_listener printing_listener = {
    .global        = print_global,
    .global_remove = ignore_global_removal,
};

static void
print_all_globals(
    struct wl_display *display
)
{
    wl_registry_add_listener(wl_display_get_registry(display), &printing_listener, NULL);
    wl_display_roundtrip(display);
}

int
main(void)
{
    struct wl_display *display = wl_display_connect(NULL);

    if (display == NULL) {
        fprintf(stderr, "Cannot connect to the Wayland display\n");
        return EXIT_FAILURE;
    }

    print_all_globals(display);
    wl_display_disconnect(display);

    return EXIT_SUCCESS;
}
