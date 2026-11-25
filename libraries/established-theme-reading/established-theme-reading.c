#include <stddef.h>

#include "established-theme-reading.h"

/* Each bar color and the palette role it sets. */
static const struct {
    size_t                  field_offset;
    enum theme_palette_role role;
} bar_colors[] = {
    {offsetof(struct compositor_bar_appearance, background_color),     THEME_PALETTE_BACKGROUND    },
    {offsetof(struct compositor_bar_appearance, text_color),           THEME_PALETTE_TEXT          },
    {offsetof(struct compositor_bar_appearance, secondary_text_color), THEME_PALETTE_SECONDARY_TEXT},
    {offsetof(struct compositor_bar_appearance, accent_color),         THEME_PALETTE_ACCENT        },
    {offsetof(struct compositor_bar_appearance, accent_text_color),    THEME_PALETTE_ACCENT_TEXT   },
    {offsetof(struct compositor_bar_appearance, border_color),         THEME_PALETTE_BORDER        },
    {offsetof(struct compositor_bar_appearance, urgent_color),         THEME_PALETTE_URGENT        },
};

static const char *
bar_color_text(
    const struct compositor_bar_appearance *appearance,
    size_t                                  field_offset
)
{
    return *(char *const *)((const char *)appearance + field_offset);
}

void
established_theme_apply_bar_appearance(
    struct theme_palette                   *palette,
    const struct compositor_bar_appearance *appearance
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(bar_colors); i++) {
        theme_palette_set_color(palette, bar_colors[i].role, bar_color_text(appearance, bar_colors[i].field_offset));
    }

    theme_palette_set_font(palette, appearance->font);
}

struct bar_reading {
    GMainLoop                        *loop;
    struct compositor_bar_appearance *appearance; // NULL until the bar answered, or when it has none
};

static void
handle_bar_appearance(
    struct compositor_bar_appearance *appearance,
    void                             *user_data
)
{
    struct bar_reading *reading = user_data;

    reading->appearance = appearance;

    g_main_loop_quit(reading->loop);
}

static gboolean
stop_waiting(
    gpointer user_data
)
{
    g_main_loop_quit(user_data);

    return G_SOURCE_CONTINUE;
}

/* Runs a private main loop until the answer or the timeout. */
static void
wait_for_bar(
    struct bar_reading *reading,
    guint               timeout_milliseconds
)
{
    GSource *timeout_source = g_timeout_source_new(timeout_milliseconds);

    g_source_set_callback(timeout_source, stop_waiting, reading->loop, NULL);
    g_source_attach(timeout_source, g_main_loop_get_context(reading->loop));
    g_main_loop_run(reading->loop);
    g_source_destroy(timeout_source);
    g_source_unref(timeout_source);
}

static void
ask_bar(
    struct compositor_ipc_client *client,
    struct bar_reading           *reading,
    guint                         timeout_milliseconds
)
{
    compositor_bar_appearance_request(client, handle_bar_appearance, reading);
    wait_for_bar(reading, timeout_milliseconds);
}

static void
ignore_event(
    void     *user_data,
    uint32_t  event_type,
    JsonNode *event_payload
)
{}

/* A lost connection answers pending requests with NULL, which ends the wait. */
static void
ignore_connection_loss(
    void *user_data
)
{}

static const struct compositor_ipc_client_handler quiet_handler = {
    .handle_event           = ignore_event,
    .handle_connection_lost = ignore_connection_loss,
};

static void
ask_bar_over_socket(
    const char         *compositor_socket_path,
    struct bar_reading *reading,
    guint               timeout_milliseconds
)
{
    struct compositor_ipc_client *client = compositor_ipc_client_connect(compositor_socket_path, &quiet_handler, NULL, NULL);

    if (client == NULL) {
        return;
    }

    ask_bar(client, reading, timeout_milliseconds);
    compositor_ipc_client_destroy(client);
}

static void
release_private_loop(
    GMainLoop    *loop,
    GMainContext *context
)
{
    g_main_loop_unref(loop);
    g_main_context_unref(context);
}

/* The bar's appearance, or NULL; uses its own main context so it can run before any other. */
static struct compositor_bar_appearance *
read_bar_appearance(
    const char *compositor_socket_path,
    guint       timeout_milliseconds
)
{
    GMainContext      *context = g_main_context_new();
    struct bar_reading reading = {.loop = g_main_loop_new(context, FALSE)};

    g_main_context_push_thread_default(context);
    ask_bar_over_socket(compositor_socket_path, &reading, timeout_milliseconds);
    g_main_context_pop_thread_default(context);
    release_private_loop(reading.loop, context);

    return reading.appearance;
}

static void
apply_bar(
    struct theme_palette *palette,
    const char           *compositor_socket_path,
    guint                 timeout_milliseconds
)
{
    if (compositor_socket_path == NULL) {
        return;
    }

    struct compositor_bar_appearance *appearance = read_bar_appearance(compositor_socket_path, timeout_milliseconds);

    if (appearance == NULL) {
        return;
    }

    established_theme_apply_bar_appearance(palette, appearance);
    compositor_bar_appearance_destroy(appearance);
}

static void
apply_user_style_sheet(
    struct theme_palette *palette,
    const char           *user_style_sheet_path
)
{
    g_autofree char *style_sheet_text = NULL;

    if (user_style_sheet_path == NULL) {
        return;
    }

    if (!g_file_get_contents(user_style_sheet_path, &style_sheet_text, NULL, NULL)) {
        return;
    }

    theme_palette_apply_variables(palette, style_sheet_text);
}

struct theme_palette *
established_theme_read(
    const char *compositor_socket_path,
    const char *user_style_sheet_path,
    guint       timeout_milliseconds
)
{
    struct theme_palette *palette = theme_palette_create_default();

    apply_bar(palette, compositor_socket_path, timeout_milliseconds);
    apply_user_style_sheet(palette, user_style_sheet_path);

    return palette;
}
