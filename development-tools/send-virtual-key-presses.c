/*
 * Types into the Wayland compositor in $WAYLAND_DISPLAY through a virtual
 * keyboard, for testing key handling inside a headless compositor that has
 * no real keyboard. Each argument is one step, 300 ms apart:
 *
 *   alt-down   press and hold Alt (also shift-down, super-down)
 *   alt-up     release Alt (also shift-up, super-up)
 *   tab        press and release Tab
 *   escape     press and release Escape
 *   print      press and release Print Screen
 *   wait       do nothing for one step
 *
 * Example, Alt+Tab twice then release: send-virtual-key-presses alt-down tab tab alt-up
 */

#define _GNU_SOURCE // memfd_create()

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <glib.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "virtual-keyboard-unstable-v1-client-protocol.h"

enum {
    STEP_DURATION_IN_MICROSECONDS = 300 * 1000,
    KEYMAP_FORMAT_XKB_V1          = 1,
    KEY_STATE_RELEASED            = 0,
    KEY_STATE_PRESSED             = 1,

    /* Linux input event codes, which Wayland key events carry. */
    KEY_CODE_ESCAPE     = 1,
    KEY_CODE_TAB        = 15,
    KEY_CODE_PRINT      = 99, // KEY_SYSRQ, the Print Screen key
    KEY_CODE_LEFT_ALT   = 56,
    KEY_CODE_LEFT_SHIFT = 42,
    KEY_CODE_LEFT_SUPER = 125,
};

struct virtual_typist {
    struct wl_display                      *display;
    struct wl_seat                         *seat;
    struct zwp_virtual_keyboard_manager_v1 *keyboard_manager;
    struct zwp_virtual_keyboard_v1         *keyboard;
    uint32_t                                event_time;
    uint32_t                                held_modifiers; // modifier mask of the keys held now
};

/* Modifier keys, with their masks in the default keymap. */
static const struct {
    const char *name;
    uint32_t    key_code;
    uint32_t    modifier_mask;
} modifier_keys[] = {
    {"alt",   KEY_CODE_LEFT_ALT,   1 << 3}, // Mod1
    {"shift", KEY_CODE_LEFT_SHIFT, 1 << 0},
    {"super", KEY_CODE_LEFT_SUPER, 1 << 6}, // Mod4
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
    struct virtual_typist *typist = user_data;

    if (strcmp(interface_name, wl_seat_interface.name) == 0) {
        typist->seat = wl_registry_bind(registry, global_name, &wl_seat_interface, 1);
    }

    if (strcmp(interface_name, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
        typist->keyboard_manager = wl_registry_bind(registry, global_name, &zwp_virtual_keyboard_manager_v1_interface, 1);
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

static char *
describe_us_keymap(void)
{
    struct xkb_rule_names us_layout = {.layout = "us"};
    struct xkb_context   *context   = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap    *keymap    = xkb_keymap_new_from_names(context, &us_layout, XKB_KEYMAP_COMPILE_NO_FLAGS);
    char                 *text      = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);

    xkb_keymap_unref(keymap);
    xkb_context_unref(context);

    return text;
}

/* The keymap travels as a file holding its text, terminating NUL included. Returns -1 on failure. */
static int
create_keymap_file(
    size_t *keymap_size
)
{
    char *keymap_text = describe_us_keymap();
    int   keymap_file = memfd_create("virtual-keymap", MFD_CLOEXEC);

    *keymap_size = strlen(keymap_text) + 1;

    bool is_written = write(keymap_file, keymap_text, *keymap_size) == (ssize_t)*keymap_size;

    free(keymap_text);

    return is_written ? keymap_file : -1;
}

static void
send_keymap(
    struct virtual_typist *typist,
    int                    keymap_file,
    size_t                 keymap_size
)
{
    zwp_virtual_keyboard_v1_keymap(typist->keyboard, KEYMAP_FORMAT_XKB_V1, keymap_file, (uint32_t)keymap_size);
    close(keymap_file);
}

static bool
create_virtual_keyboard(
    struct virtual_typist *typist
)
{
    size_t keymap_size = 0;
    int    keymap_file = create_keymap_file(&keymap_size);

    if (keymap_file < 0) {
        return false;
    }

    typist->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(typist->keyboard_manager, typist->seat);

    send_keymap(typist, keymap_file, keymap_size);

    return wl_display_roundtrip(typist->display) >= 0;
}

static void
send_key(
    struct virtual_typist *typist,
    uint32_t               key_code,
    uint32_t               key_state
)
{
    zwp_virtual_keyboard_v1_key(typist->keyboard, typist->event_time++, key_code, key_state);
}

static void
tap_key(
    struct virtual_typist *typist,
    uint32_t               key_code
)
{
    send_key(typist, key_code, KEY_STATE_PRESSED);
    send_key(typist, key_code, KEY_STATE_RELEASED);
}

static void
set_modifier(
    struct virtual_typist *typist,
    size_t                 modifier_index,
    bool                   is_held
)
{
    uint32_t mask = modifier_keys[modifier_index].modifier_mask;

    typist->held_modifiers = is_held ? typist->held_modifiers | mask : typist->held_modifiers & ~mask;

    send_key(typist, modifier_keys[modifier_index].key_code, is_held ? KEY_STATE_PRESSED : KEY_STATE_RELEASED);
    zwp_virtual_keyboard_v1_modifiers(typist->keyboard, typist->held_modifiers, 0, 0, 0);
}

/* Whether step_name is "NAME-down" or "NAME-up" for this modifier, and which. */
static bool
matches_modifier_step(
    const char *step_name,
    const char *modifier_name,
    bool       *is_down
)
{
    size_t name_length = strlen(modifier_name);

    if (strncmp(step_name, modifier_name, name_length) != 0) {
        return false;
    }

    *is_down = strcmp(step_name + name_length, "-down") == 0;

    return *is_down || strcmp(step_name + name_length, "-up") == 0;
}

/* Steps such as "super-down" and "shift-up"; returns false for other steps. */
static bool
perform_modifier_step(
    struct virtual_typist *typist,
    const char            *step_name
)
{
    bool is_down = false;

    for (size_t i = 0; i < G_N_ELEMENTS(modifier_keys); i++) {
        if (matches_modifier_step(step_name, modifier_keys[i].name, &is_down)) {
            set_modifier(typist, i, is_down);
            return true;
        }
    }

    return false;
}

static void
tap_tab(
    struct virtual_typist *typist
)
{
    tap_key(typist, KEY_CODE_TAB);
}

static void
tap_print(
    struct virtual_typist *typist
)
{
    tap_key(typist, KEY_CODE_PRINT);
}

static void
tap_escape(
    struct virtual_typist *typist
)
{
    tap_key(typist, KEY_CODE_ESCAPE);
}

static void
do_nothing(
    struct virtual_typist *typist
)
{}

static const struct {
    const char *step_name;

    void (*perform)(struct virtual_typist *typist);
} steps[] = {
    {"tab",    tap_tab   },
    {"escape", tap_escape},
    {"print",  tap_print },
    {"wait",   do_nothing},
};

static void
perform_step(
    struct virtual_typist *typist,
    const char            *step_name
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(steps); i++) {
        if (strcmp(steps[i].step_name, step_name) == 0) {
            steps[i].perform(typist);
        }
    }

    perform_modifier_step(typist, step_name);
    wl_display_roundtrip(typist->display);
    usleep(STEP_DURATION_IN_MICROSECONDS);
}

static bool
connect_typist(
    struct virtual_typist *typist
)
{
    typist->display = wl_display_connect(NULL);

    if (typist->display == NULL) {
        return false;
    }

    wl_registry_add_listener(wl_display_get_registry(typist->display), &binding_listener, typist);
    wl_display_roundtrip(typist->display);

    return typist->seat != NULL && typist->keyboard_manager != NULL;
}

static bool
prepare_typist(
    struct virtual_typist *typist
)
{
    if (!connect_typist(typist)) {
        return false;
    }

    return create_virtual_keyboard(typist);
}

int
main(
    int   argument_count,
    char *arguments[]
)
{
    struct virtual_typist typist = {0};

    if (!prepare_typist(&typist)) {
        fprintf(stderr, "The compositor offers no seat or no virtual keyboard\n");
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argument_count; i++) {
        perform_step(&typist, arguments[i]);
    }

    return EXIT_SUCCESS;
}
