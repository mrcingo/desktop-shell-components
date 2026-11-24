#include <gtk/gtk.h>

#include "shared-style-sheets.h"

static const char configuration_directory_name[] = "desktop-shell-components";
static const char shared_theme_file_name[]       = "shared-theme.css";

enum {
    SHIPPED_THEME_PRIORITY    = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION,
    COMPOSITOR_THEME_PRIORITY = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1,
    SHIPPED_PROGRAM_PRIORITY  = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 2,
    SYSTEM_THEME_PRIORITY     = GTK_STYLE_PROVIDER_PRIORITY_USER,
    SYSTEM_PROGRAM_PRIORITY   = GTK_STYLE_PROVIDER_PRIORITY_USER + 1,
    USER_THEME_PRIORITY       = GTK_STYLE_PROVIDER_PRIORITY_USER + 2,
    USER_PROGRAM_PRIORITY     = GTK_STYLE_PROVIDER_PRIORITY_USER + 3,
};

struct style_sheet_file {
    GtkCssProvider *provider;
    char           *path;
};

struct shared_style_sheets {
    GdkDisplay     *display;
    GPtrArray      *files; // struct style_sheet_file, in priority order
    GtkCssProvider *compositor_theme;
};

const char *
shared_style_sheets_resolve_data_directory(
    const char *installed_data_directory
)
{
    const char *development_directory = g_getenv("DESKTOP_SHELL_COMPONENTS_DATA_DIRECTORY");

    if (development_directory != NULL) {
        return development_directory;
    }

    return installed_data_directory;
}

static void
report_style_sheet_error(
    GtkCssProvider *provider,
    GtkCssSection  *section,
    const GError   *error,
    void           *user_data
)
{
    char *location = gtk_css_section_to_string(section);

    g_warning("%s: %s", location, error->message);
    g_free(location);
}

static GtkCssProvider *
add_provider(
    struct shared_style_sheets *style_sheets,
    guint                       priority
)
{
    GtkCssProvider *provider = gtk_css_provider_new();

    g_signal_connect(provider, "parsing-error", G_CALLBACK(report_style_sheet_error), NULL);
    gtk_style_context_add_provider_for_display(style_sheets->display, GTK_STYLE_PROVIDER(provider), priority);

    return provider;
}

/* A missing file loads as an empty style sheet, so creating it later takes effect on reload. */
static void
read_style_sheet_file(
    struct style_sheet_file *file
)
{
    if (!g_file_test(file->path, G_FILE_TEST_IS_REGULAR)) {
        gtk_css_provider_load_from_string(file->provider, "");
        return;
    }

    gtk_css_provider_load_from_path(file->provider, file->path);
}

static void
add_file_layer(
    struct shared_style_sheets *style_sheets,
    char                       *path,
    guint                       priority
)
{
    struct style_sheet_file *file = g_new(struct style_sheet_file, 1);

    file->provider = add_provider(style_sheets, priority);
    file->path     = path;

    read_style_sheet_file(file);
    g_ptr_array_add(style_sheets->files, file);
}

static char *
existing_configuration_file(
    const char *base_directory,
    const char *file_name
)
{
    char *path = g_build_filename(base_directory, configuration_directory_name, file_name, NULL);

    if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        return path;
    }

    g_free(path);

    return NULL;
}

/* The first system configuration directory that has the file, else the first one, as for program settings. */
static char *
system_configuration_path(
    const char *file_name
)
{
    const char *const *system_directories = g_get_system_config_dirs();

    for (size_t i = 0; system_directories[i] != NULL; i++) {
        char *path = existing_configuration_file(system_directories[i], file_name);

        if (path != NULL) {
            return path;
        }
    }

    return g_build_filename(system_directories[0], configuration_directory_name, file_name, NULL);
}

static char *
user_configuration_path(
    const char *file_name
)
{
    return g_build_filename(g_get_user_config_dir(), configuration_directory_name, file_name, NULL);
}

static void
add_configuration_layers(
    struct shared_style_sheets *style_sheets,
    const char                 *program_file_name
)
{
    add_file_layer(style_sheets, system_configuration_path(shared_theme_file_name), SYSTEM_THEME_PRIORITY);
    add_file_layer(style_sheets, system_configuration_path(program_file_name), SYSTEM_PROGRAM_PRIORITY);
    add_file_layer(style_sheets, user_configuration_path(shared_theme_file_name), USER_THEME_PRIORITY);
    add_file_layer(style_sheets, user_configuration_path(program_file_name), USER_PROGRAM_PRIORITY);
}

static char *
shipped_program_path(
    const char *data_directory,
    const char *program_file_name,
    const char *style_sheet_override_path
)
{
    if (style_sheet_override_path != NULL) {
        return g_strdup(style_sheet_override_path);
    }

    return g_build_filename(data_directory, program_file_name, NULL);
}

static void
destroy_style_sheet_file(
    void *data
)
{
    struct style_sheet_file *file = data;

    g_object_unref(file->provider);
    g_free(file->path);
    g_free(file);
}

static void
add_shipped_layers(
    struct shared_style_sheets *style_sheets,
    const char                 *data_directory,
    const char                 *program_file_name,
    const char                 *style_sheet_override_path
)
{
    char *theme_path   = g_build_filename(data_directory, shared_theme_file_name, NULL);
    char *program_path = shipped_program_path(data_directory, program_file_name, style_sheet_override_path);

    add_file_layer(style_sheets, theme_path, SHIPPED_THEME_PRIORITY);
    add_file_layer(style_sheets, program_path, SHIPPED_PROGRAM_PRIORITY);
}

struct shared_style_sheets *
shared_style_sheets_create(
    GdkDisplay *display,
    const char *data_directory,
    const char *program_name,
    const char *style_sheet_override_path
)
{
    struct shared_style_sheets *style_sheets = g_new0(struct shared_style_sheets, 1);

    g_autofree char *program_file_name = g_strconcat(program_name, ".css", NULL);

    *style_sheets = (struct shared_style_sheets){
        .display = display,
        .files   = g_ptr_array_new_with_free_func(destroy_style_sheet_file),
    };

    style_sheets->compositor_theme = add_provider(style_sheets, COMPOSITOR_THEME_PRIORITY);

    add_shipped_layers(style_sheets, data_directory, program_file_name, style_sheet_override_path);
    add_configuration_layers(style_sheets, program_file_name);

    return style_sheets;
}

static void
remove_provider(
    struct shared_style_sheets *style_sheets,
    GtkCssProvider             *provider
)
{
    gtk_style_context_remove_provider_for_display(style_sheets->display, GTK_STYLE_PROVIDER(provider));
}

static void
remove_every_provider(
    struct shared_style_sheets *style_sheets
)
{
    for (guint i = 0; i < style_sheets->files->len; i++) {
        struct style_sheet_file *file = g_ptr_array_index(style_sheets->files, i);

        remove_provider(style_sheets, file->provider);
    }

    remove_provider(style_sheets, style_sheets->compositor_theme);
}

void
shared_style_sheets_destroy(
    struct shared_style_sheets *style_sheets
)
{
    if (style_sheets == NULL) {
        return;
    }

    remove_every_provider(style_sheets);
    g_object_unref(style_sheets->compositor_theme);
    g_ptr_array_unref(style_sheets->files);
    g_free(style_sheets);
}

void
shared_style_sheets_set_compositor_theme(
    struct shared_style_sheets *style_sheets,
    const char                 *style_sheet_text
)
{
    if (style_sheet_text == NULL) {
        gtk_css_provider_load_from_string(style_sheets->compositor_theme, "");
        return;
    }

    gtk_css_provider_load_from_string(style_sheets->compositor_theme, style_sheet_text);
}

void
shared_style_sheets_reload_files(
    struct shared_style_sheets *style_sheets
)
{
    for (guint i = 0; i < style_sheets->files->len; i++) {
        read_style_sheet_file(g_ptr_array_index(style_sheets->files, i));
    }
}
