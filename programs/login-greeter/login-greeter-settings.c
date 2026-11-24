#include <glib.h>

#include "login-greeter-settings.h"
#include "program-configuration-file.h"

static const struct program_configuration_key known_keys[] = {
    {"greeter",  "username"            },
    {"greeter",  "session"             },
    {"keyboard", "follow-system-layout"},
    {NULL,       NULL                  },
};

/* An empty value counts as not set. */
static char *
read_optional_text(
    struct program_configuration_file *configuration,
    const char                        *group_name,
    const char                        *key_name
)
{
    char *value = program_configuration_file_read_text(configuration, group_name, key_name, NULL);

    if (value == NULL) {
        return NULL;
    }

    if (value[0] != '\0') {
        return value;
    }

    g_free(value);

    return NULL;
}

static void
read_settings(
    struct login_greeter_settings     *settings,
    struct program_configuration_file *configuration
)
{
    settings->prefilled_username             = read_optional_text(configuration, "greeter", "username");
    settings->default_session_name           = read_optional_text(configuration, "greeter", "session");
    settings->follows_system_keyboard_layout = program_configuration_file_read_boolean(
        configuration,
        "keyboard",
        "follow-system-layout",
        true
    );
}

struct login_greeter_settings *
login_greeter_settings_load(
    const char *override_path
)
{
    struct program_configuration_file *configuration = program_configuration_file_load("login-greeter", override_path);
    struct login_greeter_settings     *settings      = g_new0(struct login_greeter_settings, 1);

    program_configuration_file_report_unknown_keys(configuration, known_keys);
    read_settings(settings, configuration);
    program_configuration_file_destroy(configuration);

    return settings;
}

void
login_greeter_settings_destroy(
    struct login_greeter_settings *settings
)
{
    if (settings == NULL) {
        return;
    }

    g_free(settings->prefilled_username);
    g_free(settings->default_session_name);
    g_free(settings);
}
