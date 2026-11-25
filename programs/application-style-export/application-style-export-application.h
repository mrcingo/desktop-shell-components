#ifndef APPLICATION_STYLE_EXPORT_APPLICATION_H
#define APPLICATION_STYLE_EXPORT_APPLICATION_H

#include <stdbool.h>

/*
 * Writes the established look into other programs' per-user style files,
 * once, and exits: the toolkit style sheets and settings, the desktop
 * interface settings, the lock screen's and the prompt bar's
 * configuration. Or prints the launcher's options instead.
 */

struct application_style_export_options {
    const char *compositor_socket_path;      // NULL when not run inside sway
    const char *configuration_directory;     // $XDG_CONFIG_HOME
    const char *configuration_override_path; // NULL for the usual places
    bool        prints_launcher_arguments;
};

/* Returns false when a file could not be written. */
bool application_style_export_application_run(const struct application_style_export_options *options);

#endif
