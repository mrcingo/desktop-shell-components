#include <glib.h>

#include "compositor-bar-appearance.h"
#include "compositor-matched-appearance.h"
#include "shared-style-sheets.h"

struct compositor_matched_appearance {
    struct shared_style_sheets   *style_sheets;
    struct compositor_ipc_client *client;
};

struct compositor_matched_appearance *
compositor_matched_appearance_create(
    GdkDisplay                   *display,
    struct compositor_ipc_client *client,
    const char                   *data_directory,
    const char                   *program_name,
    const char                   *style_sheet_override_path
)
{
    struct compositor_matched_appearance *appearance = g_new0(struct compositor_matched_appearance, 1);

    appearance->client       = client;
    appearance->style_sheets = shared_style_sheets_create(display, data_directory, program_name, style_sheet_override_path);

    return appearance;
}

void
compositor_matched_appearance_destroy(
    struct compositor_matched_appearance *appearance
)
{
    if (appearance == NULL) {
        return;
    }

    shared_style_sheets_destroy(appearance->style_sheets);
    g_free(appearance);
}

/* Without a bar, the GTK theme stays in charge. */
static void
apply_bar_appearance(
    struct compositor_bar_appearance *bar_appearance,
    void                             *user_data
)
{
    struct compositor_matched_appearance *appearance = user_data;

    if (bar_appearance == NULL) {
        shared_style_sheets_set_compositor_theme(appearance->style_sheets, NULL);
        return;
    }

    g_autofree char *style_sheet = compositor_bar_appearance_to_style_sheet(bar_appearance);

    shared_style_sheets_set_compositor_theme(appearance->style_sheets, style_sheet);
    compositor_bar_appearance_destroy(bar_appearance);
}

void
compositor_matched_appearance_refresh(
    struct compositor_matched_appearance *appearance,
    bool                                  follows_sway_bar
)
{
    shared_style_sheets_reload_files(appearance->style_sheets);

    if (!follows_sway_bar) {
        shared_style_sheets_set_compositor_theme(appearance->style_sheets, NULL);
        return;
    }

    compositor_bar_appearance_request(appearance->client, apply_bar_appearance, appearance);
}
