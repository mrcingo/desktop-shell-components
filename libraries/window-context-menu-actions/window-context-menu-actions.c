#include <inttypes.h>
#include <signal.h>

#include "window-context-menu-actions.h"

struct action_context {
    const struct window_context_menu_target *target;
    struct compositor_ipc_client            *ipc_client;
    GdkClipboard                            *clipboard;
};

static void
run_window_command(
    struct action_context *context,
    const char            *command
)
{
    g_autofree char *addressed_command = g_strdup_printf("[con_id=%" PRId64 "] %s", context->target->container_id, command);

    compositor_ipc_client_send_request(context->ipc_client, COMPOSITOR_IPC_REQUEST_RUN_COMMAND, addressed_command, NULL, NULL);
}

/* Compositor commands, by action name. */
static const struct {
    const char *action_name;
    const char *command;
} window_commands[] = {
    {"close",              "kill"                },
    {"toggle-floating",    "floating toggle"     },
    {"toggle-fullscreen",  "fullscreen toggle"   },
    {"toggle-sticky",      "sticky toggle"       },
    {"move-to-scratchpad", "move scratchpad"     },
    {"center",             "move position center"},
};

static const char *const output_directions[] = {"left", "right", "up", "down", NULL};
static const char *const border_styles[]     = {"normal", "pixel", "none", NULL};
static const char *const container_layouts[] = {"splith", "splitv", "tabbed", "stacking", NULL};

/* Commands that take one of a fixed set of words, by action name. */
static const struct {
    const char        *action_name;
    const char        *command_prefix;
    const char *const *allowed_choices;
} choice_commands[] = {
    {"move-to-output", "move container to output ", output_directions},
    {"set-border",     "border ",                   border_styles    },
    {"set-layout",     "layout ",                   container_layouts},
};

/* Signals, by action name. */
static const struct {
    const char *action_name;
    int         signal_number;
} process_signals[] = {
    {"terminate-process", SIGTERM},
    {"force-close",       SIGKILL},
    {"pause-process",     SIGSTOP},
    {"resume-process",    SIGCONT},
};

static void
activate_window_command(
    GSimpleAction *action,
    GVariant      *parameter,
    gpointer       user_data
)
{
    const char *action_name = g_action_get_name(G_ACTION(action));

    for (size_t i = 0; i < G_N_ELEMENTS(window_commands); i++) {
        if (g_strcmp0(action_name, window_commands[i].action_name) == 0) {
            run_window_command(user_data, window_commands[i].command);
        }
    }
}

/* The parameter comes from the menu, but is checked against the allowed words all the same. */
static void
run_choice_command(
    struct action_context *context,
    size_t                 command_index,
    const char            *choice
)
{
    if (!g_strv_contains((const char *const *)choice_commands[command_index].allowed_choices, choice)) {
        return;
    }

    g_autofree char *command = g_strconcat(choice_commands[command_index].command_prefix, choice, NULL);

    run_window_command(context, command);
}

static void
activate_choice_command(
    GSimpleAction *action,
    GVariant      *parameter,
    gpointer       user_data
)
{
    const char *action_name = g_action_get_name(G_ACTION(action));

    for (size_t i = 0; i < G_N_ELEMENTS(choice_commands); i++) {
        if (g_strcmp0(action_name, choice_commands[i].action_name) == 0) {
            run_choice_command(user_data, i, g_variant_get_string(parameter, NULL));
        }
    }
}

static void
send_process_signal(
    struct action_context *context,
    int                    signal_number
)
{
    g_autoptr(GError) error = NULL;

    if (!process_signal_handle_send(context->target->signal_handle, signal_number, &error)) {
        g_warning("%s", error->message);
    }
}

static void
activate_process_signal(
    GSimpleAction *action,
    GVariant      *parameter,
    gpointer       user_data
)
{
    const char *action_name = g_action_get_name(G_ACTION(action));

    for (size_t i = 0; i < G_N_ELEMENTS(process_signals); i++) {
        if (g_strcmp0(action_name, process_signals[i].action_name) == 0) {
            send_process_signal(user_data, process_signals[i].signal_number);
        }
    }
}

gboolean
window_context_menu_is_workspace_number(
    const char *text
)
{
    guint64 number = 0;

    return g_ascii_string_to_unsigned(text, 10, 1, 99, &number, NULL);
}

static void
activate_move_to_workspace(
    GSimpleAction *action,
    GVariant      *parameter,
    gpointer       user_data
)
{
    const char *workspace_number = g_variant_get_string(parameter, NULL);

    if (!window_context_menu_is_workspace_number(workspace_number)) {
        return;
    }

    g_autofree char *command = g_strdup_printf("move container to workspace number %s", workspace_number);

    run_window_command(user_data, command);
}

static char *
copy_process_id_text(
    const struct window_context_menu_target *target
)
{
    if (target->process == NULL) {
        return NULL;
    }

    return g_strdup_printf("%d", target->process->process_id);
}

static char *
copy_title_text(
    const struct window_context_menu_target *target
)
{
    return g_strdup(target->title);
}

static char *
copy_application_name_text(
    const struct window_context_menu_target *target
)
{
    return g_strdup(target->application_name);
}

static char *
copy_command_line_text(
    const struct window_context_menu_target *target
)
{
    if (target->process == NULL) {
        return NULL;
    }

    return g_strdup(target->process->command_line);
}

static char *
copy_working_directory_text(
    const struct window_context_menu_target *target
)
{
    if (target->process == NULL) {
        return NULL;
    }

    return g_strdup(target->process->working_directory);
}

/* Texts to copy, by action name; NULL when the target has none. */
static const struct {
    const char *action_name;
    char *(*copy_text)(const struct window_context_menu_target *target);
} copied_texts[] = {
    {"copy-process-id",        copy_process_id_text       },
    {"copy-title",             copy_title_text            },
    {"copy-application-name",  copy_application_name_text },
    {"copy-command-line",      copy_command_line_text     },
    {"copy-working-directory", copy_working_directory_text},
};

static void
copy_to_clipboard(
    struct action_context *context,
    char *(*copy_text)(const struct window_context_menu_target *target)
)
{
    g_autofree char *text = copy_text(context->target);

    if (text == NULL) {
        return;
    }

    gdk_clipboard_set_text(context->clipboard, text);
}

static void
activate_copy(
    GSimpleAction *action,
    GVariant      *parameter,
    gpointer       user_data
)
{
    const char *action_name = g_action_get_name(G_ACTION(action));

    for (size_t i = 0; i < G_N_ELEMENTS(copied_texts); i++) {
        if (g_strcmp0(action_name, copied_texts[i].action_name) == 0) {
            copy_to_clipboard(user_data, copied_texts[i].copy_text);
        }
    }
}

static const GActionEntry action_entries[] = {
    {"close",                  activate_window_command,    NULL, NULL,    NULL, {0}},
    {"toggle-floating",        activate_window_command,    NULL, "false", NULL, {0}},
    {"toggle-fullscreen",      activate_window_command,    NULL, "false", NULL, {0}},
    {"toggle-sticky",          activate_window_command,    NULL, "false", NULL, {0}},
    {"move-to-scratchpad",     activate_window_command,    NULL, NULL,    NULL, {0}},
    {"move-to-workspace",      activate_move_to_workspace, "s",  NULL,    NULL, {0}},
    {"move-to-output",         activate_choice_command,    "s",  NULL,    NULL, {0}},
    {"set-border",             activate_choice_command,    "s",  NULL,    NULL, {0}},
    {"set-layout",             activate_choice_command,    "s",  NULL,    NULL, {0}},
    {"center",                 activate_window_command,    NULL, NULL,    NULL, {0}},
    {"terminate-process",      activate_process_signal,    NULL, NULL,    NULL, {0}},
    {"force-close",            activate_process_signal,    NULL, NULL,    NULL, {0}},
    {"pause-process",          activate_process_signal,    NULL, NULL,    NULL, {0}},
    {"resume-process",         activate_process_signal,    NULL, NULL,    NULL, {0}},
    {"copy-process-id",        activate_copy,              NULL, NULL,    NULL, {0}},
    {"copy-title",             activate_copy,              NULL, NULL,    NULL, {0}},
    {"copy-application-name",  activate_copy,              NULL, NULL,    NULL, {0}},
    {"copy-command-line",      activate_copy,              NULL, NULL,    NULL, {0}},
    {"copy-working-directory", activate_copy,              NULL, NULL,    NULL, {0}},
};

static void
set_action_enabled(
    GActionGroup *group,
    const char   *action_name,
    bool          is_enabled
)
{
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(group), action_name);

    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), is_enabled);
}

static void
set_action_checked(
    GActionGroup *group,
    const char   *action_name,
    bool          is_checked
)
{
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(group), action_name);

    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(is_checked));
}

static bool
process_is_stopped(
    const struct window_context_menu_target *target
)
{
    if (target->process == NULL) {
        return false;
    }

    return target->process->is_stopped;
}

/* Signals need a handle; only one of pause and resume applies at a time. */
static void
enable_process_actions(
    GActionGroup                            *group,
    const struct window_context_menu_target *target
)
{
    bool can_signal = target->signal_handle != NULL;
    bool is_stopped = process_is_stopped(target);

    set_action_enabled(group, "terminate-process", can_signal);
    set_action_enabled(group, "force-close", can_signal);
    set_action_enabled(group, "pause-process", can_signal && !is_stopped);
    set_action_enabled(group, "resume-process", can_signal && is_stopped);
}

static void
enable_copy_actions(
    GActionGroup                            *group,
    const struct window_context_menu_target *target
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(copied_texts); i++) {
        g_autofree char *text = copied_texts[i].copy_text(target);

        set_action_enabled(group, copied_texts[i].action_name, text != NULL);
    }
}

/* Only floating windows can be sticky or centered. */
static void
show_window_states(
    GActionGroup                            *group,
    const struct window_context_menu_target *target
)
{
    set_action_checked(group, "toggle-floating", target->is_floating);
    set_action_checked(group, "toggle-fullscreen", target->is_fullscreen);
    set_action_checked(group, "toggle-sticky", target->is_sticky);
    set_action_enabled(group, "toggle-sticky", target->is_floating);
    set_action_enabled(group, "center", target->is_floating);
}

/* The group owns the context, which its actions receive. */
static struct action_context *
attach_action_context(
    GSimpleActionGroup                      *group,
    const struct window_context_menu_target *target,
    struct compositor_ipc_client            *ipc_client,
    GdkClipboard                            *clipboard
)
{
    struct action_context *context = g_new0(struct action_context, 1);

    *context = (struct action_context){.target = target, .ipc_client = ipc_client, .clipboard = clipboard};

    g_object_set_data_full(G_OBJECT(group), "window-context-menu-action-context", context, g_free);

    return context;
}

GActionGroup *
window_context_menu_actions_create(
    const struct window_context_menu_target *target,
    struct compositor_ipc_client            *ipc_client,
    GdkClipboard                            *clipboard
)
{
    GSimpleActionGroup    *group   = g_simple_action_group_new();
    struct action_context *context = attach_action_context(group, target, ipc_client, clipboard);

    g_action_map_add_action_entries(G_ACTION_MAP(group), action_entries, G_N_ELEMENTS(action_entries), context);
    enable_process_actions(G_ACTION_GROUP(group), target);
    enable_copy_actions(G_ACTION_GROUP(group), target);
    show_window_states(G_ACTION_GROUP(group), target);

    return G_ACTION_GROUP(group);
}
