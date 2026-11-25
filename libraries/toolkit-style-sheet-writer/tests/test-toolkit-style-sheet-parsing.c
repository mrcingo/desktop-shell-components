/*
 * Loads a generated style sheet into the toolkit's own parser and fails on
 * any parsing error. Built twice, against GTK 3 and GTK 4; the version is
 * chosen with TOOLKIT_MAJOR_VERSION.
 */

#include <gtk/gtk.h>

#include "toolkit-style-sheet-writer.h"

static void
fail_on_parsing_error(
    GtkCssProvider *provider,
    GtkCssSection  *section,
    GError         *error,
    gpointer        user_data
)
{
    g_error("Style sheet error: %s", error->message);
}

static char *
write_style_sheet(
    const struct theme_palette *palette
)
{
#if TOOLKIT_MAJOR_VERSION == 3
    return toolkit_style_sheet_write_version_3(palette);
#else
    return toolkit_style_sheet_write_version_4(palette);
#endif
}

static void
load_into_provider(
    GtkCssProvider *provider,
    const char     *style_sheet
)
{
#if TOOLKIT_MAJOR_VERSION == 3
    gtk_css_provider_load_from_data(provider, style_sheet, -1, NULL);
#else
    gtk_css_provider_load_from_string(provider, style_sheet);
#endif
}

static void
test_style_sheet_parses(void)
{
    struct theme_palette *palette = theme_palette_create_default();

    g_autofree char *style_sheet = write_style_sheet(palette);

    GtkCssProvider *provider = gtk_css_provider_new();

    g_signal_connect(provider, "parsing-error", G_CALLBACK(fail_on_parsing_error), NULL);
    load_into_provider(provider, style_sheet);
    g_object_unref(provider);
    theme_palette_destroy(palette);
}

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/toolkit-style-sheet-writer/parses", test_style_sheet_parses);

    return g_test_run();
}
