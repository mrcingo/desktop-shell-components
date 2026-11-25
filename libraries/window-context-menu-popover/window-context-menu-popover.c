#include "window-context-menu-popover.h"
#include "text-ellipsis-truncation.h"
#include "window-context-menu-actions.h"

enum {
    MAXIMUM_HEADER_CHARACTERS = 60,
    WORKSPACE_MENU_SIZE       = 10,
};

struct open_menu {
    GtkWidget                                        *popover;
    struct window_context_menu_target                *target;
    const struct window_context_menu_popover_handler *handler;
    void                                             *user_data;
};

/* Placeholders that add_header_labels fills with labels, which cannot be clicked or focused. */
static const char *const header_line_identifiers[] = {"window-title", "window-process"};

static void
append_header_placeholder(
    GMenu      *section,
    const char *identifier
)
{
    GMenuItem *item = g_menu_item_new(NULL, NULL);

    g_menu_item_set_attribute(item, "custom", "s", identifier);
    g_menu_append_item(section, item);
    g_object_unref(item);
}

static GMenu *
create_header_section(void)
{
    GMenu *section = g_menu_new();

    for (size_t i = 0; i < G_N_ELEMENTS(header_line_identifiers); i++) {
        append_header_placeholder(section, header_line_identifiers[i]);
    }

    return section;
}

static GtkWidget *
create_header_label(
    const char *text
)
{
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_widget_add_css_class(label, "window-context-menu-header");

    return label;
}

static void
add_header_labels(
    GtkWidget                               *popover,
    const struct window_context_menu_target *target
)
{
    g_autofree char *shortened_title  = text_ellipsis_truncate(target->title, MAXIMUM_HEADER_CHARACTERS);
    g_autofree char *process_overview = window_context_menu_target_describe_process(target);

    gtk_popover_menu_add_child(GTK_POPOVER_MENU(popover), create_header_label(shortened_title), header_line_identifiers[0]);
    gtk_popover_menu_add_child(GTK_POPOVER_MENU(popover), create_header_label(process_overview), header_line_identifiers[1]);
}

static GMenu *
create_closing_section(void)
{
    GMenu *section = g_menu_new();

    g_menu_append(section, "Close", "window.close");
    g_menu_append(section, "End process (SIGTERM)", "window.terminate-process");
    g_menu_append(section, "Force close (SIGKILL)", "window.force-close");
    g_menu_append(section, "Pause process", "window.pause-process");
    g_menu_append(section, "Resume process", "window.resume-process");

    return section;
}

static GMenu *
create_copy_submenu(void)
{
    GMenu *submenu = g_menu_new();

    g_menu_append(submenu, "Process ID", "window.copy-process-id");
    g_menu_append(submenu, "Title", "window.copy-title");
    g_menu_append(submenu, "Application name", "window.copy-application-name");
    g_menu_append(submenu, "Command line", "window.copy-command-line");
    g_menu_append(submenu, "Working directory", "window.copy-working-directory");

    return submenu;
}

static GMenu *
create_workspace_submenu(void)
{
    GMenu *submenu = g_menu_new();

    for (int number = 1; number <= WORKSPACE_MENU_SIZE; number++) {
        g_autofree char *label           = g_strdup_printf("Workspace %d", number);
        g_autofree char *detailed_action = g_strdup_printf("window.move-to-workspace::%d", number);

        g_menu_append(submenu, label, detailed_action);
    }

    return submenu;
}

/* The section keeps its own reference to the submenu. */
static void
append_owned_submenu(
    GMenu      *section,
    const char *label,
    GMenu      *submenu
)
{
    g_menu_append_submenu(section, label, G_MENU_MODEL(submenu));
    g_object_unref(submenu);
}

static GMenu *
create_copy_section(void)
{
    GMenu *section = g_menu_new();

    append_owned_submenu(section, "Copy", create_copy_submenu());

    return section;
}

/* Submenu items that pass a word to one action: {label, word}. */
struct labelled_choice {
    const char *label;
    const char *choice;
};

static const struct labelled_choice output_choices[] = {
    {"Left",  "left" },
    {"Right", "right"},
    {"Up",    "up"   },
    {"Down",  "down" },
};

static const struct labelled_choice border_choices[] = {
    {"Title bar",   "normal"},
    {"Thin border", "pixel" },
    {"No border",   "none"  },
};

static const struct labelled_choice layout_choices[] = {
    {"Side by side",         "splith"  },
    {"On top of each other", "splitv"  },
    {"Tabbed",               "tabbed"  },
    {"Stacked",              "stacking"},
};

static GMenu *
create_choice_submenu(
    const char                   *action_name,
    const struct labelled_choice *choices,
    size_t                        choice_count
)
{
    GMenu *submenu = g_menu_new();

    for (size_t i = 0; i < choice_count; i++) {
        g_autofree char *detailed_action = g_strdup_printf("window.%s::%s", action_name, choices[i].choice);

        g_menu_append(submenu, choices[i].label, detailed_action);
    }

    return submenu;
}

static GMenu *
create_state_section(void)
{
    GMenu *section = g_menu_new();

    g_menu_append(section, "Floating", "window.toggle-floating");
    g_menu_append(section, "Fullscreen", "window.toggle-fullscreen");
    g_menu_append(section, "Sticky", "window.toggle-sticky");
    g_menu_append(section, "Center", "window.center");

    return section;
}

static GMenu *
create_arrangement_section(void)
{
    GMenu *section = g_menu_new();

    append_owned_submenu(
        section,
        "Container layout",
        create_choice_submenu("set-layout", layout_choices, G_N_ELEMENTS(layout_choices))
    );
    append_owned_submenu(section, "Border", create_choice_submenu("set-border", border_choices, G_N_ELEMENTS(border_choices)));

    return section;
}

static GMenu *
create_moving_section(void)
{
    GMenu *section = g_menu_new();

    append_owned_submenu(section, "Move to workspace", create_workspace_submenu());
    append_owned_submenu(
        section,
        "Move to monitor",
        create_choice_submenu("move-to-output", output_choices, G_N_ELEMENTS(output_choices))
    );
    g_menu_append(section, "Move to scratchpad", "window.move-to-scratchpad");

    return section;
}

static void
append_section(
    GMenu *menu,
    GMenu *section
)
{
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);
}

/* The menu's sections, top to bottom. */
static GMenu *(*const section_creators[])(void) = {
    create_header_section,
    create_closing_section,
    create_copy_section,
    create_state_section,
    create_arrangement_section,
    create_moving_section,
};

static GMenuModel *
create_menu_model(void)
{
    GMenu *menu = g_menu_new();

    for (size_t i = 0; i < G_N_ELEMENTS(section_creators); i++) {
        append_section(menu, section_creators[i]());
    }

    return G_MENU_MODEL(menu);
}

static void
report_menu_closed(
    const struct open_menu *menu
)
{
    if (menu->handler == NULL) {
        return;
    }

    if (menu->handler->handle_closed == NULL) {
        return;
    }

    menu->handler->handle_closed(menu->user_data);
}

/* The parent may already be gone, as a panel rebuilt meanwhile. */
static void
remove_from_parent(
    GtkWidget *popover
)
{
    if (gtk_widget_get_parent(popover) == NULL) {
        return;
    }

    gtk_widget_unparent(popover);
}

/* Runs after the chosen action, which still needs the target while it runs. */
static gboolean
remove_closed_menu(
    gpointer user_data
)
{
    struct open_menu *menu = user_data;

    remove_from_parent(menu->popover);
    window_context_menu_target_destroy(menu->target);
    g_object_unref(menu->popover);
    report_menu_closed(menu);
    g_free(menu);

    return G_SOURCE_REMOVE;
}

static void
handle_popover_closed(
    GtkPopover *popover,
    gpointer    user_data
)
{
    g_idle_add(remove_closed_menu, user_data);
}

/* Flat, like sway's own bar, and starting at the pointed place as menus do; the shared theme styles it. */
static void
style_popover(
    GtkWidget *popover
)
{
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_set_halign(popover, GTK_ALIGN_START);
    gtk_widget_add_css_class(popover, "desktop-shell-component");
    gtk_widget_add_css_class(popover, "window-context-menu");
}

static void
attach_window_actions(
    GtkWidget                         *popover,
    struct window_context_menu_target *target,
    struct compositor_ipc_client      *ipc_client,
    GdkClipboard                      *clipboard
)
{
    GActionGroup *actions = window_context_menu_actions_create(target, ipc_client, clipboard);

    gtk_widget_insert_action_group(popover, "window", actions);
    g_object_unref(actions);
}

static GtkWidget *
create_popover(
    GtkWidget                         *parent,
    struct window_context_menu_target *target,
    struct compositor_ipc_client      *ipc_client
)
{
    g_autoptr(GMenuModel) model = create_menu_model();

    GtkWidget *popover = gtk_popover_menu_new_from_model_full(model, GTK_POPOVER_MENU_NESTED);

    add_header_labels(popover, target);
    attach_window_actions(popover, target, ipc_client, gtk_widget_get_clipboard(parent));
    gtk_widget_set_parent(popover, parent);
    style_popover(popover);

    return popover;
}

void
window_context_menu_popover_show(
    GtkWidget                                        *parent,
    const GdkRectangle                               *pointing_to,
    struct window_context_menu_target                *target,
    struct compositor_ipc_client                     *ipc_client,
    const struct window_context_menu_popover_handler *handler,
    void                                             *user_data
)
{
    struct open_menu *menu = g_new0(struct open_menu, 1);

    /* Held until the menu is removed, even if the parent goes first. */
    *menu = (struct open_menu){
        .popover   = g_object_ref(create_popover(parent, target, ipc_client)),
        .target    = target,
        .handler   = handler,
        .user_data = user_data,
    };

    g_signal_connect(menu->popover, "closed", G_CALLBACK(handle_popover_closed), menu);
    gtk_popover_set_pointing_to(GTK_POPOVER(menu->popover), pointing_to);
    gtk_popover_popup(GTK_POPOVER(menu->popover));
}
