#include <string.h>

#include <stdbool.h>

#include <glib.h>

#include "toolkit-style-sheet-writer.h"

/* Colors derived from the palette, by the name templates use after "$". */
struct named_value {
    const char *name;
    char       *value;
};

enum {
    NAMED_VALUE_COUNT = 20,
};

struct template_values {
    struct named_value values[NAMED_VALUE_COUNT];
    size_t             count;
};

static void
add_value(
    struct template_values *values,
    const char             *name,
    char                   *value
)
{
    g_assert(values->count < NAMED_VALUE_COUNT);

    values->values[values->count++] = (struct named_value){name, value};
}

static void
add_color(
    struct template_values *values,
    const char             *name,
    struct palette_color    color
)
{
    add_value(values, name, palette_color_format(color));
}

static struct palette_color
role_color(
    const struct theme_palette *palette,
    enum theme_palette_role     role
)
{
    return palette->colors[role];
}

/* Shades between two palette colors: name, from, toward, how far. Black stands for "darker". */
struct shade {
    const char             *name;
    enum theme_palette_role from;
    int                     toward; // a role, or BLACK
    double                  fraction;
};

enum {
    BLACK = -1,
};

/* Surfaces step from the background toward the text color, like sway's own bar. */
static const struct shade shades[] = {
    {"view",            THEME_PALETTE_BACKGROUND,     BLACK,                    0.18},
    {"surface",         THEME_PALETTE_BACKGROUND,     THEME_PALETTE_TEXT,       0.05},
    {"hover",           THEME_PALETTE_BACKGROUND,     THEME_PALETTE_TEXT,       0.10},
    {"pressed",         THEME_PALETTE_BACKGROUND,     THEME_PALETTE_TEXT,       0.16},
    {"tooltip",         THEME_PALETTE_BACKGROUND,     BLACK,                    0.35},
    {"disabled",        THEME_PALETTE_SECONDARY_TEXT, THEME_PALETTE_BACKGROUND, 0.45},
    {"accent_hover",    THEME_PALETTE_ACCENT,         THEME_PALETTE_TEXT,       0.15},
    {"accent_inactive", THEME_PALETTE_ACCENT,         THEME_PALETTE_BACKGROUND, 0.50},
    {"link",            THEME_PALETTE_ACCENT,         THEME_PALETTE_TEXT,       0.55},
};

static struct palette_color
shade_target(
    const struct theme_palette *palette,
    int                         toward
)
{
    const struct palette_color black = {0, 0, 0, 1};

    if (toward == BLACK) {
        return black;
    }

    return role_color(palette, (enum theme_palette_role)toward);
}

static void
add_shades(
    struct template_values     *values,
    const struct theme_palette *palette
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(shades); i++) {
        struct palette_color from = role_color(palette, shades[i].from);

        add_color(values, shades[i].name, palette_color_mix(from, shade_target(palette, shades[i].toward), shades[i].fraction));
    }
}

/* The palette's own colors, by template name. */
static const struct {
    const char             *name;
    enum theme_palette_role role;
} plain_colors[] = {
    {"background",  THEME_PALETTE_BACKGROUND    },
    {"text",        THEME_PALETTE_TEXT          },
    {"secondary",   THEME_PALETTE_SECONDARY_TEXT},
    {"border",      THEME_PALETTE_BORDER        },
    {"urgent",      THEME_PALETTE_URGENT        },
    {"accent",      THEME_PALETTE_ACCENT        },
    {"accent_text", THEME_PALETTE_ACCENT_TEXT   },
};

static void
add_plain_colors(
    struct template_values     *values,
    const struct theme_palette *palette
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(plain_colors); i++) {
        add_color(values, plain_colors[i].name, role_color(palette, plain_colors[i].role));
    }
}

static void
add_font_values(
    struct template_values     *values,
    const struct theme_palette *palette
)
{
    add_value(values, "font_family", toolkit_style_sheet_safe_font_family(palette->font_family));
    add_value(values, "font_size", g_strdup_printf("%dpt", palette->font_size));
}

static void
collect_template_values(
    struct template_values     *values,
    const struct theme_palette *palette
)
{
    add_plain_colors(values, palette);
    add_shades(values, palette);
    add_font_values(values, palette);
}

static void
clear_template_values(
    struct template_values *values
)
{
    for (size_t i = 0; i < values->count; i++) {
        g_free(values->values[i].value);
    }
}

/* The value for the name starting at text, and its length through name_length; NULL when none matches. */
static const char *
find_value(
    const struct template_values *values,
    const char                   *text,
    size_t                       *name_length
)
{
    size_t length = strspn(text, "abcdefghijklmnopqrstuvwxyz_");

    for (size_t i = 0; i < values->count; i++) {
        if (strlen(values->values[i].name) == length && strncmp(text, values->values[i].name, length) == 0) {
            *name_length = length;
            return values->values[i].value;
        }
    }

    return NULL;
}

/* Appends the value of "$name" at dollar, or the "$" itself, and returns the text after it. */
static const char *
append_value(
    GString                      *output,
    const char                   *dollar,
    const struct template_values *values
)
{
    size_t      name_length = 0;
    const char *value       = find_value(values, dollar + 1, &name_length);

    g_assert(value != NULL); // templates are part of this file; every name must exist

    g_string_append(output, value);

    return dollar + 1 + name_length;
}

static char *
expand_template(
    const char                   *template_text,
    const struct template_values *values
)
{
    GString    *output    = g_string_new(NULL);
    const char *remaining = template_text;

    for (const char *dollar = strchr(remaining, '$'); dollar != NULL; dollar = strchr(remaining, '$')) {
        g_string_append_len(output, remaining, dollar - remaining);

        remaining = append_value(output, dollar, values);
    }

    g_string_append(output, remaining);

    return g_string_free(output, FALSE);
}

static bool
is_safe_font_character(
    char character
)
{
    if (g_ascii_isalnum(character)) {
        return true;
    }

    return strchr(" -_.", character) != NULL;
}

char *
toolkit_style_sheet_safe_font_family(
    const char *font_family
)
{
    GString *safe_family = g_string_new(NULL);

    for (const char *character = font_family; *character != '\0'; character++) {
        if (is_safe_font_character(*character)) {
            g_string_append_c(safe_family, *character);
        }
    }

    return g_string_free(safe_family, FALSE);
}

static char *
write_from_template(
    const char                 *template_text,
    const struct theme_palette *palette
)
{
    struct template_values values = {0};

    collect_template_values(&values, palette);

    char *style_sheet = expand_template(template_text, &values);

    clear_template_values(&values);

    return style_sheet;
}

static const char version_3_template[] =
    "/* Named colors, for programs and themes that use them. */\n"
    "@define-color theme_bg_color $background;\n"
    "@define-color theme_fg_color $text;\n"
    "@define-color theme_base_color $view;\n"
    "@define-color theme_text_color $text;\n"
    "@define-color theme_selected_bg_color $accent;\n"
    "@define-color theme_selected_fg_color $accent_text;\n"
    "@define-color theme_unfocused_bg_color $background;\n"
    "@define-color theme_unfocused_fg_color $secondary;\n"
    "@define-color theme_unfocused_base_color $view;\n"
    "@define-color theme_unfocused_text_color $text;\n"
    "@define-color theme_unfocused_selected_bg_color $accent_inactive;\n"
    "@define-color theme_unfocused_selected_fg_color $accent_text;\n"
    "@define-color insensitive_bg_color $background;\n"
    "@define-color insensitive_fg_color $disabled;\n"
    "@define-color insensitive_base_color $view;\n"
    "@define-color borders $border;\n"
    "@define-color unfocused_borders $border;\n"
    "@define-color error_color $urgent;\n"
    "@define-color content_view_bg $view;\n"
    "@define-color text_view_bg $view;\n"
    "\n"
    "/* Flat, square and in one font, like sway's own bar. */\n"
    "* {\n"
    "    font-family: \"$font_family\";\n"
    "    font-size: $font_size;\n"
    "    background-image: none;\n"
    "    box-shadow: none;\n"
    "    text-shadow: none;\n"
    "    -gtk-icon-shadow: none;\n"
    "    border-radius: 0;\n"
    "    outline-color: $accent;\n"
    "}\n"
    "\n"
    "window, .background, dialog, messagedialog, assistant {\n"
    "    background-color: $background;\n"
    "    color: $text;\n"
    "}\n"
    "\n"
    "decoration {\n"
    "    border: none;\n"
    "    box-shadow: none;\n"
    "}\n"
    "\n"
    "headerbar, .titlebar, menubar, toolbar, .toolbar, .primary-toolbar, actionbar, searchbar, .inline-toolbar {\n"
    "    background-color: $surface;\n"
    "    color: $text;\n"
    "    border-color: $border;\n"
    "}\n"
    "\n"
    "headerbar, .titlebar {\n"
    "    min-height: 0;\n"
    "    padding: 2px 6px;\n"
    "    border-bottom: 1px solid $border;\n"
    "}\n"
    "\n"
    "statusbar frame > border, statusbar > frame > border {\n"
    "    border: none;\n"
    "}\n"
    "\n"
    "statusbar {\n"
    "    background-color: $surface;\n"
    "    color: $secondary;\n"
    "    padding: 2px 6px;\n"
    "    border-top: 1px solid $border;\n"
    "}\n"
    "\n"
    "/* Content: file lists, icon views, text and lists. */\n"
    ".view, treeview.view, iconview, textview text, list, flowbox, .content-view {\n"
    "    background-color: $view;\n"
    "    color: $text;\n"
    "}\n"
    "\n"
    ".sidebar, placessidebar, placessidebar list, stacksidebar, .sidebar .view, .sidebar treeview.view {\n"
    "    background-color: $surface;\n"
    "    color: $text;\n"
    "    border-color: $border;\n"
    "}\n"
    "\n"
    ".sidebar row, placessidebar row, stacksidebar row {\n"
    "    padding: 4px 8px;\n"
    "}\n"
    "\n"
    "/* Single-color icons in the text color: the old full-color ones are dark glyphs made for light backgrounds. */\n"
    ".sidebar, placessidebar, stacksidebar, menu, .menu, popover, toolbar, .toolbar, headerbar, pathbar {\n"
    "    -gtk-icon-style: symbolic;\n"
    "}\n"
    "\n"
    "row:hover, iconview:hover, flowboxchild:hover {\n"
    "    background-color: $hover;\n"
    "}\n"
    "\n"
    ".view:selected, .view:selected:focus, treeview.view:selected, iconview:selected, row:selected, flowboxchild:selected,\n"
    "label:selected, entry selection, textview text selection, *:selected {\n"
    "    background-color: $accent;\n"
    "    color: $accent_text;\n"
    "}\n"
    "\n"
    "row:selected:hover, iconview:selected:hover {\n"
    "    background-color: $accent_hover;\n"
    "}\n"
    "\n"
    "*:selected:backdrop, .view:selected:backdrop, row:selected:backdrop {\n"
    "    background-color: $accent_inactive;\n"
    "}\n"
    "\n"
    "treeview.view header button {\n"
    "    background-color: $surface;\n"
    "    color: $secondary;\n"
    "    border: none;\n"
    "    border-right: 1px solid $border;\n"
    "    border-bottom: 1px solid $border;\n"
    "    padding: 2px 6px;\n"
    "}\n"
    "\n"
    "/* Typing: the location bar, search and every other entry. */\n"
    "entry, spinbutton:not(.vertical) {\n"
    "    min-height: 24px;\n"
    "    padding: 0 6px;\n"
    "    background-color: $view;\n"
    "    color: $text;\n"
    "    border: 1px solid $border;\n"
    "    caret-color: $text;\n"
    "}\n"
    "\n"
    "entry:focus, spinbutton:focus {\n"
    "    border-color: $accent;\n"
    "}\n"
    "\n"
    "entry:disabled, spinbutton:disabled {\n"
    "    color: $disabled;\n"
    "}\n"
    "\n"
    "button {\n"
    "    min-height: 20px;\n"
    "    padding: 3px 8px;\n"
    "    background-color: $surface;\n"
    "    color: $text;\n"
    "    border: 1px solid $border;\n"
    "}\n"
    "\n"
    "button:hover {\n"
    "    background-color: $hover;\n"
    "}\n"
    "\n"
    "button:active, button:checked {\n"
    "    background-color: $accent;\n"
    "    color: $accent_text;\n"
    "    border-color: $accent;\n"
    "}\n"
    "\n"
    "button:disabled {\n"
    "    background-color: $background;\n"
    "    color: $disabled;\n"
    "}\n"
    "\n"
    "button.flat, headerbar button, toolbar button, .toolbar button, .path-bar button, .titlebar button {\n"
    "    background-color: transparent;\n"
    "    border-color: transparent;\n"
    "}\n"
    "\n"
    "button.flat:hover, headerbar button:hover, toolbar button:hover, .toolbar button:hover, .path-bar button:hover {\n"
    "    background-color: $hover;\n"
    "    border-color: $border;\n"
    "}\n"
    "\n"
    "button.flat:checked, .path-bar button:checked, toolbar button:checked {\n"
    "    background-color: $accent;\n"
    "    color: $accent_text;\n"
    "}\n"
    "\n"
    "button.suggested-action {\n"
    "    background-color: $accent;\n"
    "    color: $accent_text;\n"
    "    border-color: $accent;\n"
    "}\n"
    "\n"
    "button.destructive-action {\n"
    "    background-color: $urgent;\n"
    "    color: $accent_text;\n"
    "    border-color: $urgent;\n"
    "}\n"
    "\n"
    "/* Menus: right-click menus, the menu bar and popovers. */\n"
    "menu, .menu, .context-menu, popover, popover.background {\n"
    "    padding: 4px 0;\n"
    "    background-color: $background;\n"
    "    color: $text;\n"
    "    border: 1px solid $border;\n"
    "}\n"
    "\n"
    "menuitem, modelbutton {\n"
    "    padding: 4px 12px;\n"
    "}\n"
    "\n"
    "menuitem:hover, modelbutton:hover, menubar > menuitem:hover {\n"
    "    background-color: $accent;\n"
    "    color: $accent_text;\n"
    "}\n"
    "\n"
    "menuitem:disabled, modelbutton:disabled {\n"
    "    color: $disabled;\n"
    "}\n"
    "\n"
    "menu separator, .menu separator, popover separator {\n"
    "    margin: 3px 0;\n"
    "}\n"
    "\n"
    "tooltip, tooltip.background {\n"
    "    background-color: $tooltip;\n"
    "    color: $text;\n"
    "    border: 1px solid $border;\n"
    "}\n"
    "\n"
    "tooltip * {\n"
    "    background-color: transparent;\n"
    "    color: $text;\n"
    "}\n"
    "\n"
    "scrollbar {\n"
    "    background-color: transparent;\n"
    "    border: none;\n"
    "}\n"
    "\n"
    "scrollbar slider {\n"
    "    min-width: 6px;\n"
    "    min-height: 6px;\n"
    "    border: 2px solid transparent;\n"
    "    background-clip: padding-box;\n"
    "    background-color: $border;\n"
    "}\n"
    "\n"
    "scrollbar slider:hover {\n"
    "    background-color: $secondary;\n"
    "}\n"
    "\n"
    "scrollbar slider:active {\n"
    "    background-color: $accent;\n"
    "}\n"
    "\n"
    "/* Tabs, as in the file manager. */\n"
    "notebook > header {\n"
    "    background-color: $surface;\n"
    "    border-color: $border;\n"
    "}\n"
    "\n"
    "notebook > header tab {\n"
    "    padding: 4px 10px;\n"
    "    color: $secondary;\n"
    "    border: none;\n"
    "}\n"
    "\n"
    "notebook > header tab:hover {\n"
    "    background-color: $hover;\n"
    "    color: $text;\n"
    "}\n"
    "\n"
    "notebook > header tab:checked {\n"
    "    background-color: $background;\n"
    "    color: $text;\n"
    "    box-shadow: inset 0 -2px $accent;\n"
    "}\n"
    "\n"
    "notebook > header.top > tabs > tab:checked, notebook > header tab:checked {\n"
    "    border-bottom: 2px solid $accent;\n"
    "}\n"
    "\n"
    "notebook > header.bottom > tabs > tab:checked {\n"
    "    border-bottom: none;\n"
    "    border-top: 2px solid $accent;\n"
    "}\n"
    "\n"
    "notebook > stack {\n"
    "    background-color: $background;\n"
    "}\n"
    "\n"
    "separator, paned > separator {\n"
    "    min-width: 1px;\n"
    "    min-height: 1px;\n"
    "    background-color: $border;\n"
    "}\n"
    "\n"
    "frame > border, .frame {\n"
    "    border: 1px solid $border;\n"
    "}\n"
    "\n"
    "check, radio {\n"
    "    background-color: $view;\n"
    "    color: $accent_text;\n"
    "    border: 1px solid $border;\n"
    "}\n"
    "\n"
    "check:checked, radio:checked, check:indeterminate, radio:indeterminate {\n"
    "    background-color: $accent;\n"
    "    border-color: $accent;\n"
    "}\n"
    "\n"
    "radio {\n"
    "    border-radius: 100%;\n"
    "}\n"
    "\n"
    "switch {\n"
    "    background-color: $view;\n"
    "    border: 1px solid $border;\n"
    "}\n"
    "\n"
    "switch:checked {\n"
    "    background-color: $accent;\n"
    "}\n"
    "\n"
    "switch slider {\n"
    "    min-width: 18px;\n"
    "    min-height: 18px;\n"
    "    background-color: $text;\n"
    "    border: none;\n"
    "}\n"
    "\n"
    "scale trough, progressbar trough, levelbar trough {\n"
    "    min-height: 4px;\n"
    "    background-color: $view;\n"
    "    border: 1px solid $border;\n"
    "}\n"
    "\n"
    "scale highlight, progressbar progress, levelbar block.filled {\n"
    "    background-color: $accent;\n"
    "    border: none;\n"
    "}\n"
    "\n"
    "scale slider {\n"
    "    min-width: 12px;\n"
    "    min-height: 12px;\n"
    "    background-color: $text;\n"
    "    border: none;\n"
    "}\n"
    "\n"
    "infobar {\n"
    "    background-color: $surface;\n"
    "}\n"
    "\n"
    "infobar.error {\n"
    "    background-color: $urgent;\n"
    "    color: $accent_text;\n"
    "}\n"
    "\n"
    ".dim-label {\n"
    "    color: $secondary;\n"
    "}\n"
    "\n"
    "*:disabled, label:disabled, menuitem:disabled label, modelbutton:disabled label {\n"
    "    color: $disabled;\n"
    "}\n"
    "\n"
    "*:link, link {\n"
    "    color: $link;\n"
    "}\n"
    "\n"
    "spinner {\n"
    "    color: $accent;\n"
    "}\n";

static const char version_4_template[] = "/*\n"
                                         " * Only named colors and libadwaita's color variables: this sheet outranks\n"
                                         " * every program's own, so widget rules here would restyle more than meant.\n"
                                         " */\n"
                                         "\n"
                                         "/* GTK 4's own theme. */\n"
                                         "@define-color theme_bg_color $background;\n"
                                         "@define-color theme_fg_color $text;\n"
                                         "@define-color theme_base_color $view;\n"
                                         "@define-color theme_text_color $text;\n"
                                         "@define-color theme_selected_bg_color $accent;\n"
                                         "@define-color theme_selected_fg_color $accent_text;\n"
                                         "@define-color theme_unfocused_bg_color $background;\n"
                                         "@define-color theme_unfocused_fg_color $secondary;\n"
                                         "@define-color insensitive_fg_color $disabled;\n"
                                         "@define-color borders $border;\n"
                                         "@define-color error_color $urgent;\n"
                                         "\n"
                                         "/* libadwaita, older releases. */\n"
                                         "@define-color accent_color $link;\n"
                                         "@define-color accent_bg_color $accent;\n"
                                         "@define-color accent_fg_color $accent_text;\n"
                                         "@define-color destructive_color $urgent;\n"
                                         "@define-color destructive_bg_color $urgent;\n"
                                         "@define-color destructive_fg_color $accent_text;\n"
                                         "@define-color error_bg_color $urgent;\n"
                                         "@define-color error_fg_color $accent_text;\n"
                                         "@define-color window_bg_color $background;\n"
                                         "@define-color window_fg_color $text;\n"
                                         "@define-color view_bg_color $view;\n"
                                         "@define-color view_fg_color $text;\n"
                                         "@define-color headerbar_bg_color $surface;\n"
                                         "@define-color headerbar_fg_color $text;\n"
                                         "@define-color headerbar_border_color $border;\n"
                                         "@define-color headerbar_backdrop_color $background;\n"
                                         "@define-color sidebar_bg_color $surface;\n"
                                         "@define-color sidebar_fg_color $text;\n"
                                         "@define-color sidebar_backdrop_color $surface;\n"
                                         "@define-color card_bg_color $surface;\n"
                                         "@define-color card_fg_color $text;\n"
                                         "@define-color dialog_bg_color $background;\n"
                                         "@define-color dialog_fg_color $text;\n"
                                         "@define-color popover_bg_color $background;\n"
                                         "@define-color popover_fg_color $text;\n"
                                         "@define-color thumbnail_bg_color $surface;\n"
                                         "@define-color thumbnail_fg_color $text;\n"
                                         "\n"
                                         "/* libadwaita 1.6 and later. */\n"
                                         ":root {\n"
                                         "    --accent-color: $link;\n"
                                         "    --accent-bg-color: $accent;\n"
                                         "    --accent-fg-color: $accent_text;\n"
                                         "    --destructive-color: $urgent;\n"
                                         "    --destructive-bg-color: $urgent;\n"
                                         "    --destructive-fg-color: $accent_text;\n"
                                         "    --error-bg-color: $urgent;\n"
                                         "    --error-fg-color: $accent_text;\n"
                                         "    --window-bg-color: $background;\n"
                                         "    --window-fg-color: $text;\n"
                                         "    --view-bg-color: $view;\n"
                                         "    --view-fg-color: $text;\n"
                                         "    --headerbar-bg-color: $surface;\n"
                                         "    --headerbar-fg-color: $text;\n"
                                         "    --headerbar-border-color: $border;\n"
                                         "    --sidebar-bg-color: $surface;\n"
                                         "    --sidebar-fg-color: $text;\n"
                                         "    --card-bg-color: $surface;\n"
                                         "    --card-fg-color: $text;\n"
                                         "    --dialog-bg-color: $background;\n"
                                         "    --dialog-fg-color: $text;\n"
                                         "    --popover-bg-color: $background;\n"
                                         "    --popover-fg-color: $text;\n"
                                         "    --border-color: $border;\n"
                                         "}\n";

char *
toolkit_style_sheet_write_version_3(
    const struct theme_palette *palette
)
{
    return write_from_template(version_3_template, palette);
}

char *
toolkit_style_sheet_write_version_4(
    const struct theme_palette *palette
)
{
    return write_from_template(version_4_template, palette);
}
