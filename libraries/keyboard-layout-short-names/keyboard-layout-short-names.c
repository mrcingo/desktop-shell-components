#include <string.h>

#include <glib.h>

#include "keyboard-layout-short-names.h"

/* Files larger than this are not rules files; the real one is about 1.5 MB. */
enum {
    MAXIMUM_RULES_FILE_BYTES = 16 << 20,
};

/*
 * The rules file nests each layout as
 *   <layout><configItem><name>es</name><description>Spanish</description></configItem>
 *     <variantList><variant><configItem>...<description>Spanish (Dvorak)</description>
 * so the layout's own name is the first <name> inside <layout>.
 */
struct layout_search {
    const char *layout_description;
    char       *current_layout_name; // name of the layout being read
    char       *found_layout_name;
    GString    *element_text;
};

static void
handle_element_start(
    GMarkupParseContext *context,
    const char          *element_name,
    const char         **attribute_names,
    const char         **attribute_values,
    void                *user_data,
    GError             **error
)
{
    struct layout_search *search = user_data;

    g_string_truncate(search->element_text, 0);

    if (strcmp(element_name, "layout") == 0) {
        g_clear_pointer(&search->current_layout_name, g_free);
    }
}

static void
remember_layout_name(
    struct layout_search *search
)
{
    if (search->current_layout_name == NULL) {
        search->current_layout_name = g_strdup(search->element_text->str);
    }
}

/* Stopping the parse early is reported as an error; the caller ignores it once a name is found. */
static void
check_description(
    struct layout_search *search,
    GError              **error
)
{
    if (g_strcmp0(search->element_text->str, search->layout_description) != 0) {
        return;
    }

    search->found_layout_name = g_strdup(search->current_layout_name);

    g_set_error_literal(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "found");
}

static void
handle_element_end(
    GMarkupParseContext *context,
    const char          *element_name,
    void                *user_data,
    GError             **error
)
{
    struct layout_search *search = user_data;

    if (strcmp(element_name, "name") == 0) {
        remember_layout_name(search);
    }

    if (strcmp(element_name, "description") == 0) {
        check_description(search, error);
    }
}

static void
handle_text(
    GMarkupParseContext *context,
    const char          *text,
    gsize                text_length,
    void                *user_data,
    GError             **error
)
{
    struct layout_search *search = user_data;

    g_string_append_len(search->element_text, text, (gssize)text_length);
}

static const GMarkupParser rules_parser = {
    .start_element = handle_element_start,
    .end_element   = handle_element_end,
    .text          = handle_text,
};

static char *
search_rules_text(
    const char *rules_text,
    gsize       rules_length,
    const char *layout_description
)
{
    struct layout_search search  = {.layout_description = layout_description, .element_text = g_string_new(NULL)};
    GMarkupParseContext *context = g_markup_parse_context_new(&rules_parser, G_MARKUP_IGNORE_QUALIFIED, &search, NULL);

    g_markup_parse_context_parse(context, rules_text, (gssize)rules_length, NULL);
    g_markup_parse_context_free(context);
    g_string_free(search.element_text, TRUE);
    g_free(search.current_layout_name);

    return search.found_layout_name;
}

char *
keyboard_layout_short_name_find(
    const char *rules_file_path,
    const char *layout_description
)
{
    g_autofree char *rules_text = NULL;

    gsize rules_length = 0;

    if (!g_file_get_contents(rules_file_path, &rules_text, &rules_length, NULL)) {
        return NULL;
    }

    if (rules_length > MAXIMUM_RULES_FILE_BYTES) {
        return NULL;
    }

    return search_rules_text(rules_text, rules_length, layout_description);
}
