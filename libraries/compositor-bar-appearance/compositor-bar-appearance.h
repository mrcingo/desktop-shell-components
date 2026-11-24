#ifndef COMPOSITOR_BAR_APPEARANCE_H
#define COMPOSITOR_BAR_APPEARANCE_H

#include <json-glib/json-glib.h>

#include "compositor-ipc-client.h"

/*
 * The colors and font of the user's sway bar, read from sway's
 * GET_BAR_CONFIG reply, so components can match the bar the user already
 * configured instead of bringing their own look. Every field is NULL when
 * the bar does not set it.
 */

struct compositor_bar_appearance {
    char *font;                 // a Pango font description, such as "Monaspace Neon 10"
    char *background_color;     // bar background
    char *text_color;           // status line text
    char *secondary_text_color; // text of workspaces without focus
    char *accent_color;         // focused workspace background
    char *accent_text_color;    // focused workspace text
    char *border_color;         // active workspace border
    char *urgent_color;         // urgent workspace background
};

struct compositor_bar_appearance *compositor_bar_appearance_parse(JsonNode *bar_configuration);

void compositor_bar_appearance_destroy(struct compositor_bar_appearance *appearance);

/* GTK CSS that sets the shared theme's variables and the font. Free with g_free(). */
char *compositor_bar_appearance_to_style_sheet(const struct compositor_bar_appearance *appearance);

/* appearance is NULL when sway has no bar or the request failed; the callback owns it. */
typedef void (*compositor_bar_appearance_callback)(struct compositor_bar_appearance *appearance, void *user_data);

/* Reads the first bar's appearance: one request for the bar names, one for its configuration. */
void compositor_bar_appearance_request(
    struct compositor_ipc_client      *client,
    compositor_bar_appearance_callback callback,
    void                              *user_data
);

#endif
