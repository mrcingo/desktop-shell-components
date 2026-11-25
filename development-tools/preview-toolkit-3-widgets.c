/*
 * A window of common GTK 3 widgets laid out like a file manager: menu bar,
 * toolbar with a location entry, a sidebar, a file list with a selected
 * row, tabs, buttons, checks, a switch, a slider, a progress bar and a
 * status bar. For looking at generated style sheets in a headless
 * compositor; built only on request.
 */

#include <gtk/gtk.h>

static const char *const sidebar_places[] = {"Home", "Desktop", "Documents", "Downloads", "Pictures", "Trash"};
static const char *const file_rows[][3]   = {
    {"Programming", "-",      "Folder"   },
    {"notes.txt",   "4.1 KB", "Text"     },
    {"photo.png",   "2.3 MB", "PNG image"},
    {"report.pdf",  "812 KB", "PDF"      },
    {"archive.tar", "18 MB",  "Archive"  },
};

static GtkWidget *
create_menu_bar(void)
{
    GtkWidget        *menu_bar = gtk_menu_bar_new();
    const char *const labels[] = {"File", "Edit", "View", "Go", "Help"};

    for (size_t i = 0; i < G_N_ELEMENTS(labels); i++) {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), gtk_menu_item_new_with_label(labels[i]));
    }

    return menu_bar;
}

static GtkWidget *
create_location_entry(void)
{
    GtkWidget *location = gtk_entry_new();

    gtk_entry_set_text(GTK_ENTRY(location), "/home/monkey/Documents");

    return location;
}

static GtkWidget *
create_location_bar(void)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    gtk_box_pack_start(GTK_BOX(bar), gtk_button_new_from_icon_name("go-previous", GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), gtk_button_new_from_icon_name("go-next", GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), create_location_entry(), TRUE, TRUE, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), "toolbar");

    return bar;
}

static GtkWidget *
create_place_label(
    const char *place
)
{
    GtkWidget *label = gtk_label_new(place);

    gtk_label_set_xalign(GTK_LABEL(label), 0);

    return label;
}

static GtkWidget *
create_sidebar(void)
{
    GtkWidget *list = gtk_list_box_new();

    for (size_t i = 0; i < G_N_ELEMENTS(sidebar_places); i++) {
        gtk_list_box_insert(GTK_LIST_BOX(list), create_place_label(sidebar_places[i]), -1);
    }

    gtk_list_box_select_row(GTK_LIST_BOX(list), gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), 2));
    gtk_style_context_add_class(gtk_widget_get_style_context(list), "sidebar");

    return list;
}

static GtkListStore *
create_file_store(void)
{
    GtkListStore *store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    for (size_t i = 0; i < G_N_ELEMENTS(file_rows); i++) {
        gtk_list_store_insert_with_values(store, NULL, -1, 0, file_rows[i][0], 1, file_rows[i][1], 2, file_rows[i][2], -1);
    }

    return store;
}

static void
add_text_column(
    GtkWidget  *view,
    const char *title,
    int         column_index
)
{
    GtkTreeViewColumn
        *column = gtk_tree_view_column_new_with_attributes(title, gtk_cell_renderer_text_new(), "text", column_index, NULL);

    gtk_tree_view_column_set_expand(column, column_index == 0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
}

static void
select_second_row(
    GtkWidget *view
)
{
    GtkTreePath *selected_row = gtk_tree_path_new_from_indices(1, -1);

    gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), selected_row);
    gtk_tree_path_free(selected_row);
}

static GtkWidget *
create_file_list(void)
{
    g_autoptr(GtkListStore) store = create_file_store();

    GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

    add_text_column(view, "Name", 0);
    add_text_column(view, "Size", 1);
    add_text_column(view, "Type", 2);
    select_second_row(view);

    return view;
}

static GtkWidget *
create_suggested_button(void)
{
    GtkWidget *button = gtk_button_new_with_label("Open");

    gtk_style_context_add_class(gtk_widget_get_style_context(button), "suggested-action");

    return button;
}

static GtkWidget *
create_slider(void)
{
    GtkWidget *slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);

    gtk_range_set_value(GTK_RANGE(slider), 40);
    gtk_widget_set_size_request(slider, 120, -1);

    return slider;
}

static GtkWidget *
create_progress_bar(void)
{
    GtkWidget *progress = gtk_progress_bar_new();

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0.6);

    return progress;
}

/* Centered vertically, as applications place small controls in a row. */
static void
pack_centered(
    GtkWidget *row,
    GtkWidget *child
)
{
    gtk_widget_set_valign(child, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(row), child, FALSE, FALSE, 0);
}

static GtkWidget *
create_controls(void)
{
    GtkWidget *row        = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *children[] = {
        gtk_button_new_with_label("Cancel"),
        create_suggested_button(),
        gtk_check_button_new_with_label("Hidden files"),
        gtk_switch_new(),
        create_slider(),
        create_progress_bar(),
    };

    for (size_t i = 0; i < G_N_ELEMENTS(children); i++) {
        pack_centered(row, children[i]);
    }

    return row;
}

static GtkWidget *
create_tabs(
    GtkWidget *file_list
)
{
    GtkWidget *tabs = gtk_notebook_new();

    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), file_list, gtk_label_new("Documents"));
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), gtk_label_new("Downloads"), gtk_label_new("Downloads"));

    return tabs;
}

static GtkWidget *
create_main_column(void)
{
    GtkWidget *column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    gtk_box_pack_start(GTK_BOX(column), create_tabs(create_file_list()), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(column), create_controls(), FALSE, FALSE, 4);

    return column;
}

static GtkWidget *
create_content(void)
{
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    gtk_paned_pack1(GTK_PANED(paned), create_sidebar(), FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), create_main_column(), TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 160);

    return paned;
}

static GtkWidget *
create_status_bar(void)
{
    GtkWidget *statusbar = gtk_statusbar_new();

    gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, "5 items, 21 MB; 1 selected");

    return statusbar;
}

static GtkWidget *
create_window_contents(
    GtkWidget *location_bar
)
{
    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_box_pack_start(GTK_BOX(layout), create_menu_bar(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(layout), location_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(layout), create_content(), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(layout), create_status_bar(), FALSE, FALSE, 0);

    return layout;
}

static GtkWidget *
create_context_menu(void)
{
    GtkWidget        *menu     = gtk_menu_new();
    const char *const labels[] = {"Open", "Open With...", "Copy", "Rename...", "Move to Trash", "Properties"};

    for (size_t i = 0; i < G_N_ELEMENTS(labels); i++) {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_menu_item_new_with_label(labels[i]));
    }

    gtk_widget_set_sensitive(gtk_container_get_children(GTK_CONTAINER(menu))->next->data, FALSE);
    gtk_widget_show_all(menu);

    return menu;
}

/* A right-click menu, opened once the window is shown, so screenshots show menus too. */
static gboolean
open_context_menu(
    gpointer user_data
)
{
    gtk_menu_popup_at_widget(GTK_MENU(create_context_menu()), user_data, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);

    return G_SOURCE_REMOVE;
}

static GtkWidget *
create_preview_window(void)
{
    GtkWidget *window       = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *location_bar = create_location_bar();

    gtk_window_set_title(GTK_WINDOW(window), "Widget preview");
    gtk_container_add(GTK_CONTAINER(window), create_window_contents(location_bar));
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_timeout_add(800, open_context_menu, location_bar);

    return window;
}

int
main(
    int   argc,
    char *argv[]
)
{
    gtk_init(&argc, &argv);
    gtk_widget_show_all(create_preview_window());
    gtk_main();

    return 0;
}
