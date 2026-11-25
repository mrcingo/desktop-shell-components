/*
 * Points and clicks in the Wayland compositor in $WAYLAND_DISPLAY through a
 * virtual pointer, for testing mouse handling inside a headless compositor
 * that has no real pointer. Each argument is one step, 300 ms apart:
 *
 *   move:X,Y      move to pixel X,Y of a 1920x1200 layout (the test output)
 *   left-click    press and release the left button
 *   middle-click  press and release the middle button
 *   right-click   press and release the right button
 *   wait          do nothing for one step
 *
 * Example, right-click the first title bar: send-virtual-pointer-events move:100,30 right-click
 */

#define _DEFAULT_SOURCE // usleep()

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <wayland-client.h>

#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

enum {
    STEP_DURATION_IN_MICROSECONDS = 300 * 1000,
    LAYOUT_WIDTH                  = 1920,
    LAYOUT_HEIGHT                 = 1200,
    BUTTON_STATE_RELEASED         = 0,
    BUTTON_STATE_PRESSED          = 1,

    /* Linux input event codes, which Wayland button events carry. */
    BUTTON_CODE_LEFT   = 0x110,
    BUTTON_CODE_RIGHT  = 0x111,
    BUTTON_CODE_MIDDLE = 0x112,
};

struct virtual_pointer_user {
    struct wl_display                      *display;
    struct wl_seat                         *seat;
    struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
    struct zwlr_virtual_pointer_v1         *pointer;
    uint32_t                                event_time;
};

static void
bind_needed_global(
    void               *user_data,
    struct wl_registry *registry,
    uint32_t            global_name,
    const char         *interface_name,
    uint32_t            version
)
{
    struct virtual_pointer_user *user = user_data;

    if (strcmp(interface_name, wl_seat_interface.name) == 0) {
        user->seat = wl_registry_bind(registry, global_name, &wl_seat_interface, 1);
    }

    if (strcmp(interface_name, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
        user->pointer_manager = wl_registry_bind(registry, global_name, &zwlr_virtual_pointer_manager_v1_interface, 1);
    }
}

static void
ignore_global_removal(
    void               *user_data,
    struct wl_registry *registry,
    uint32_t            global_name
)
{}

static const struct wl_registry_listener binding_listener = {
    .global        = bind_needed_global,
    .global_remove = ignore_global_removal,
};

static void
click(
    struct virtual_pointer_user *user,
    uint32_t                     button_code
)
{
    zwlr_virtual_pointer_v1_button(user->pointer, user->event_time++, button_code, BUTTON_STATE_PRESSED);
    zwlr_virtual_pointer_v1_frame(user->pointer);
    zwlr_virtual_pointer_v1_button(user->pointer, user->event_time++, button_code, BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(user->pointer);
}

/* Returns false for a malformed move step. */
static bool
move_to(
    struct virtual_pointer_user *user,
    const char                  *coordinates
)
{
    unsigned int x = 0;
    unsigned int y = 0;

    if (sscanf(coordinates, "%u,%u", &x, &y) != 2) {
        return false;
    }

    zwlr_virtual_pointer_v1_motion_absolute(user->pointer, user->event_time++, x, y, LAYOUT_WIDTH, LAYOUT_HEIGHT);
    zwlr_virtual_pointer_v1_frame(user->pointer);

    return true;
}

static const struct {
    const char *step;
    uint32_t    button_code;
} click_steps[] = {
    {"left-click",   BUTTON_CODE_LEFT  },
    {"middle-click", BUTTON_CODE_MIDDLE},
    {"right-click",  BUTTON_CODE_RIGHT },
};

/* Returns false when the step is not a click. */
static bool
perform_click_step(
    struct virtual_pointer_user *user,
    const char                  *step
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(click_steps); i++) {
        if (strcmp(step, click_steps[i].step) == 0) {
            click(user, click_steps[i].button_code);
            return true;
        }
    }

    return false;
}

/* Returns false for an unknown step. */
static bool
perform_step(
    struct virtual_pointer_user *user,
    const char                  *step
)
{
    if (g_str_has_prefix(step, "move:")) {
        return move_to(user, step + strlen("move:"));
    }

    if (perform_click_step(user, step)) {
        return true;
    }

    return strcmp(step, "wait") == 0;
}

/* Each step is sent and given time to take effect before the next. */
static bool
perform_step_and_wait(
    struct virtual_pointer_user *user,
    const char                  *step
)
{
    if (!perform_step(user, step)) {
        fprintf(stderr, "Unknown step: %s\n", step);
        return false;
    }

    wl_display_flush(user->display);
    usleep(STEP_DURATION_IN_MICROSECONDS);

    return true;
}

static bool
perform_steps(
    struct virtual_pointer_user *user,
    int                          step_count,
    char                        *steps[]
)
{
    for (int i = 0; i < step_count; i++) {
        if (!perform_step_and_wait(user, steps[i])) {
            return false;
        }
    }

    return true;
}

static bool
connect_virtual_pointer(
    struct virtual_pointer_user *user
)
{
    user->display = wl_display_connect(NULL);

    if (user->display == NULL) {
        fprintf(stderr, "Cannot connect to the Wayland display\n");
        return false;
    }

    wl_registry_add_listener(wl_display_get_registry(user->display), &binding_listener, user);
    wl_display_roundtrip(user->display);

    return user->pointer_manager != NULL;
}

static bool
point_and_click(
    struct virtual_pointer_user *user,
    int                          step_count,
    char                        *steps[]
)
{
    user->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(user->pointer_manager, user->seat);

    bool succeeded = perform_steps(user, step_count, steps);

    wl_display_roundtrip(user->display);
    wl_display_disconnect(user->display);

    return succeeded;
}

int
main(
    int   argument_count,
    char *arguments[]
)
{
    struct virtual_pointer_user user = {.event_time = 1};

    if (!connect_virtual_pointer(&user)) {
        fprintf(stderr, "The compositor offers no virtual pointer\n");
        return 1;
    }

    return point_and_click(&user, argument_count - 1, arguments + 1) ? 0 : 1;
}
