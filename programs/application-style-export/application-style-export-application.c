#include <stdio.h>

#include <glib.h>

#include "application-style-export-application.h"
#include "application-style-export-settings.h"
#include "desktop-interface-settings.h"
#include "established-theme-reading.h"
#include "managed-file-writing.h"
#include "toolkit-style-sheet-writer.h"
#include "utility-program-style-writer.h"

/* How long to wait for the sway bar before using the rest of the theme alone. */
static const guint bar_wait_milliseconds = 500;

/* The first line of every written file; deleting it makes the file the user's own. */
static const char style_sheet_marker[] = "/* Written by desktop-shell-application-style-export; changes here are replaced. "
                                         "Delete this line to keep the file as "
                                         "yours. */";
static const char configuration_marker
    [] = "# Written by desktop-shell-application-style-export; changes here are replaced. Delete this line to keep the file as "
         "yours.";

struct export_context {
    const struct application_style_export_options  *options;
    const struct application_style_export_settings *settings;
    const struct theme_palette                     *palette;
    bool                                            had_failure;
};

static void
report_outcome(
    struct export_context    *context,
    const char               *relative_path,
    enum managed_file_outcome outcome,
    GError                   *error
)
{
    if (outcome == MANAGED_FILE_LEFT_TO_USER) {
        g_printerr(
            "%s/%s is your own file, so it was left as it is\n",
            context->options->configuration_directory,
            relative_path
        );
    }

    if (outcome == MANAGED_FILE_FAILED) {
        g_printerr("%s\n", error != NULL ? error->message : relative_path);

        context->had_failure = true;
    }
}

/* Takes ownership of contents. */
static void
write_managed_file(
    struct export_context *context,
    const char            *relative_path,
    const char            *marker_line,
    char                  *contents
)
{
    g_autoptr(GError) error = NULL;

    const char               *directory = context->options->configuration_directory;
    enum managed_file_outcome outcome   = managed_file_write(directory, relative_path, marker_line, contents, &error);

    report_outcome(context, relative_path, outcome, error);
    g_free(contents);
}

static void
export_toolkit_3(
    struct export_context *context
)
{
    write_managed_file(context, "gtk-3.0/gtk.css", style_sheet_marker, toolkit_style_sheet_write_version_3(context->palette));
}

static void
export_toolkit_4(
    struct export_context *context
)
{
    write_managed_file(context, "gtk-4.0/gtk.css", style_sheet_marker, toolkit_style_sheet_write_version_4(context->palette));
}

static bool
prefers_dark(
    const struct export_context *context
)
{
    if (context->settings->color_scheme == APPLICATION_STYLE_EXPORT_SCHEME_DARK) {
        return true;
    }

    if (context->settings->color_scheme == APPLICATION_STYLE_EXPORT_SCHEME_LIGHT) {
        return false;
    }

    return theme_palette_is_dark(context->palette);
}

static char *
font_name(
    const struct theme_palette *palette
)
{
    return g_strdup_printf("%s %d", palette->font_family, palette->font_size);
}

static void
set_toolkit_settings(
    struct export_context *context,
    const char            *relative_path
)
{
    g_autofree char *font = font_name(context->palette);

    const char *const keys[]   = {"gtk-application-prefer-dark-theme", "gtk-font-name", NULL};
    const char *const values[] = {prefers_dark(context) ? "true" : "false", font, NULL};

    g_autoptr(GError) error = NULL;

    const char *directory = context->options->configuration_directory;

    report_outcome(
        context,
        relative_path,
        managed_file_set_key_file_values(directory, relative_path, "Settings", keys, values, &error),
        error
    );
}

static void
export_toolkit_settings(
    struct export_context *context
)
{
    set_toolkit_settings(context, "gtk-3.0/settings.ini");
    set_toolkit_settings(context, "gtk-4.0/settings.ini");
}

static void
export_desktop_settings(
    struct export_context *context
)
{
    g_autofree char *font = font_name(context->palette);

    desktop_interface_settings_apply(prefers_dark(context), font);
}

static void
export_lock_screen(
    struct export_context *context
)
{
    write_managed_file(
        context,
        "swaylock/config",
        configuration_marker,
        utility_program_style_write_lock_screen(context->palette)
    );
}

static void
export_prompts(
    struct export_context *context
)
{
    write_managed_file(
        context,
        "swaynag/config",
        configuration_marker,
        utility_program_style_write_prompt_bar(context->palette)
    );
}

static void (*const target_exporters[APPLICATION_STYLE_EXPORT_TARGET_COUNT])(struct export_context *context) = {
    [APPLICATION_STYLE_EXPORT_TOOLKIT_3]        = export_toolkit_3,
    [APPLICATION_STYLE_EXPORT_TOOLKIT_4]        = export_toolkit_4,
    [APPLICATION_STYLE_EXPORT_TOOLKIT_SETTINGS] = export_toolkit_settings,
    [APPLICATION_STYLE_EXPORT_DESKTOP_SETTINGS] = export_desktop_settings,
    [APPLICATION_STYLE_EXPORT_LOCK_SCREEN]      = export_lock_screen,
    [APPLICATION_STYLE_EXPORT_PROMPTS]          = export_prompts,
};

static void
export_enabled_targets(
    struct export_context *context
)
{
    for (int target = 0; target < APPLICATION_STYLE_EXPORT_TARGET_COUNT; target++) {
        if (context->settings->exports_target[target]) {
            target_exporters[target](context);
        }
    }
}

/* Shell-quoted, so a script can pass them on with eval. */
static void
print_launcher_arguments(
    const struct theme_palette *palette
)
{
    g_auto(GStrv) arguments = utility_program_style_launcher_arguments(palette);

    for (char **argument = arguments; *argument != NULL; argument++) {
        g_autofree char *quoted = g_shell_quote(*argument);

        printf("%s%s", argument == arguments ? "" : " ", quoted);
    }

    printf("\n");
}

static struct theme_palette *
read_palette(
    const struct application_style_export_options *options
)
{
    g_autofree char *user_style_sheet_path = g_build_filename(
        options->configuration_directory,
        "desktop-shell-components",
        "shared-theme.css",
        NULL
    );

    return established_theme_read(options->compositor_socket_path, user_style_sheet_path, bar_wait_milliseconds);
}

static bool
export_or_print(
    struct export_context *context
)
{
    if (context->options->prints_launcher_arguments) {
        print_launcher_arguments(context->palette);
        return true;
    }

    export_enabled_targets(context);

    return !context->had_failure;
}

/* Printing the launcher's options is a question, answered even when exporting is turned off. */
static bool
has_work(
    const struct application_style_export_options  *options,
    const struct application_style_export_settings *settings
)
{
    if (options->prints_launcher_arguments) {
        return true;
    }

    return settings->is_enabled;
}

static bool
read_theme_and_export(
    const struct application_style_export_options  *options,
    const struct application_style_export_settings *settings
)
{
    struct theme_palette *palette   = read_palette(options);
    struct export_context context   = {.options = options, .settings = settings, .palette = palette};
    bool                  succeeded = export_or_print(&context);

    theme_palette_destroy(palette);

    return succeeded;
}

bool
application_style_export_application_run(
    const struct application_style_export_options *options
)
{
    struct application_style_export_settings *settings = application_style_export_settings_load(
        options->configuration_override_path
    );
    bool succeeded = true;

    if (has_work(options, settings)) {
        succeeded = read_theme_and_export(options, settings);
    }

    application_style_export_settings_destroy(settings);

    return succeeded;
}
