#include <stddef.h>
#include <string.h>

#include <pango/pango.h>

#include "compositor-bar-appearance.h"
#include "typed-json-member-lookup.h"

/* Which sway bar color fills which field. */
static const struct {
    const char *sway_color_name;
    size_t      field_offset;
} color_fields[] = {
    {"background",              offsetof(struct compositor_bar_appearance, background_color)    },
    {"statusline",              offsetof(struct compositor_bar_appearance, text_color)          },
    {"inactive_workspace_text", offsetof(struct compositor_bar_appearance, secondary_text_color)},
    {"focused_workspace_bg",    offsetof(struct compositor_bar_appearance, accent_color)        },
    {"focused_workspace_text",  offsetof(struct compositor_bar_appearance, accent_text_color)   },
    {"active_workspace_border", offsetof(struct compositor_bar_appearance, border_color)        },
    {"urgent_workspace_bg",     offsetof(struct compositor_bar_appearance, urgent_color)        },
};

/* Which field sets which shared theme variable. */
static const struct {
    const char *theme_variable;
    size_t      field_offset;
} theme_variables[] = {
    {"--theme-background-color",     offsetof(struct compositor_bar_appearance, background_color)    },
    {"--theme-text-color",           offsetof(struct compositor_bar_appearance, accent_text_color)   },
    {"--theme-secondary-text-color", offsetof(struct compositor_bar_appearance, secondary_text_color)},
    {"--theme-accent-color",         offsetof(struct compositor_bar_appearance, accent_color)        },
    {"--theme-accent-text-color",    offsetof(struct compositor_bar_appearance, accent_text_color)   },
    {"--theme-border-color",         offsetof(struct compositor_bar_appearance, border_color)        },
    {"--theme-urgent-color",         offsetof(struct compositor_bar_appearance, urgent_color)        },
};

static char **
field_at(
    const struct compositor_bar_appearance *appearance,
    size_t                                  field_offset
)
{
    return (char **)((const char *)appearance + field_offset);
}

/* sway accepts "pango:" in front of a font; GTK does not. */
static char *
font_without_prefix(
    const char *font
)
{
    if (font == NULL) {
        return NULL;
    }

    if (g_str_has_prefix(font, "pango:")) {
        return g_strdup(font + strlen("pango:"));
    }

    return g_strdup(font);
}

struct compositor_bar_appearance *
compositor_bar_appearance_parse(
    JsonNode *bar_configuration
)
{
    JsonObject                       *configuration = typed_json_root_object(bar_configuration);
    JsonObject                       *colors        = typed_json_lookup_object_member(configuration, "colors");
    struct compositor_bar_appearance *appearance    = g_new0(struct compositor_bar_appearance, 1);

    appearance->font = font_without_prefix(typed_json_lookup_string_member(configuration, "font"));

    for (size_t i = 0; i < G_N_ELEMENTS(color_fields); i++) {
        *field_at(appearance, color_fields[i].field_offset) = g_strdup(
            typed_json_lookup_string_member(colors, color_fields[i].sway_color_name)
        );
    }

    return appearance;
}

void
compositor_bar_appearance_destroy(
    struct compositor_bar_appearance *appearance
)
{
    if (appearance == NULL) {
        return;
    }

    for (size_t i = 0; i < G_N_ELEMENTS(color_fields); i++) {
        g_free(*field_at(appearance, color_fields[i].field_offset));
    }

    g_free(appearance->font);
    g_free(appearance);
}

static void
append_theme_variables(
    GString                                *style_sheet,
    const struct compositor_bar_appearance *appearance
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(theme_variables); i++) {
        const char *color = *field_at(appearance, theme_variables[i].field_offset);

        if (color != NULL) {
            g_string_append_printf(style_sheet, "    %s: %s;\n", theme_variables[i].theme_variable, color);
        }
    }
}

/* Absolute sizes are in device pixels, the others in points, as Pango defines them. */
static const char *
font_size_unit(
    const PangoFontDescription *description
)
{
    if (pango_font_description_get_size_is_absolute(description)) {
        return "px";
    }

    return "pt";
}

static void
append_font_size(
    GString                    *style_sheet,
    const PangoFontDescription *description
)
{
    double size = (double)pango_font_description_get_size(description) / PANGO_SCALE;

    if (size <= 0) {
        return;
    }

    g_string_append_printf(style_sheet, "    font-size: %g%s;\n", size, font_size_unit(description));
}

static void
append_font_family(
    GString                    *style_sheet,
    const PangoFontDescription *description
)
{
    const char *family = pango_font_description_get_family(description);

    if (family != NULL) {
        g_string_append_printf(style_sheet, "    font-family: \"%s\";\n", family);
    }
}

static void
append_font(
    GString    *style_sheet,
    const char *font
)
{
    PangoFontDescription *description = pango_font_description_from_string(font);

    g_string_append(style_sheet, "* {\n");
    append_font_family(style_sheet, description);
    append_font_size(style_sheet, description);
    g_string_append(style_sheet, "}\n");
    pango_font_description_free(description);
}

char *
compositor_bar_appearance_to_style_sheet(
    const struct compositor_bar_appearance *appearance
)
{
    GString *style_sheet = g_string_new(":root {\n");

    append_theme_variables(style_sheet, appearance);
    g_string_append(style_sheet, "}\n");

    if (appearance->font != NULL) {
        append_font(style_sheet, appearance->font);
    }

    return g_string_free(style_sheet, FALSE);
}

struct appearance_request {
    struct compositor_ipc_client      *client;
    compositor_bar_appearance_callback callback;
    void                              *user_data;
};

static void
finish_request(
    struct appearance_request        *request,
    struct compositor_bar_appearance *appearance
)
{
    request->callback(appearance, request->user_data);
    g_free(request);
}

static void
handle_bar_configuration(
    JsonNode *reply_payload,
    void     *user_data
)
{
    if (reply_payload == NULL) {
        finish_request(user_data, NULL);
        return;
    }

    finish_request(user_data, compositor_bar_appearance_parse(reply_payload));
}

static const char *
first_string_element(
    JsonArray *array
)
{
    if (typed_json_count_array_elements(array) == 0) {
        return NULL;
    }

    JsonNode *element = json_array_get_element(array, 0);

    if (json_node_get_value_type(element) != G_TYPE_STRING) {
        return NULL;
    }

    return json_node_get_string(element);
}

/* The reply to a GET_BAR_CONFIG without a payload is the list of bar names. */
static const char *
first_bar_name(
    JsonNode *reply_payload
)
{
    if (reply_payload == NULL) {
        return NULL;
    }

    if (!JSON_NODE_HOLDS_ARRAY(reply_payload)) {
        return NULL;
    }

    return first_string_element(json_node_get_array(reply_payload));
}

static void
handle_bar_names(
    JsonNode *reply_payload,
    void     *user_data
)
{
    struct appearance_request *request  = user_data;
    const char                *bar_name = first_bar_name(reply_payload);

    if (bar_name == NULL) {
        finish_request(request, NULL);
        return;
    }

    compositor_ipc_client_send_request(
        request->client,
        COMPOSITOR_IPC_REQUEST_GET_BAR_CONFIG,
        bar_name,
        handle_bar_configuration,
        request
    );
}

void
compositor_bar_appearance_request(
    struct compositor_ipc_client      *client,
    compositor_bar_appearance_callback callback,
    void                              *user_data
)
{
    struct appearance_request *request = g_new(struct appearance_request, 1);

    request->client    = client;
    request->callback  = callback;
    request->user_data = user_data;

    compositor_ipc_client_send_request(client, COMPOSITOR_IPC_REQUEST_GET_BAR_CONFIG, NULL, handle_bar_names, request);
}
