#include <json-glib/json-glib.h>

#include "status-bar-block-encoding.h"

char *
status_bar_protocol_start(
    bool reports_clicks
)
{
    if (reports_clicks) {
        return g_strdup("{\"version\":1,\"click_events\":true}\n[\n");
    }

    return g_strdup("{\"version\":1}\n[\n");
}

static void
add_optional_text(
    JsonBuilder *builder,
    const char  *member_name,
    const char  *text
)
{
    if (text == NULL) {
        return;
    }

    json_builder_set_member_name(builder, member_name);
    json_builder_add_string_value(builder, text);
}

static void
add_flags(
    JsonBuilder                   *builder,
    const struct status_bar_block *block
)
{
    json_builder_set_member_name(builder, "urgent");
    json_builder_add_boolean_value(builder, block->is_urgent);
    json_builder_set_member_name(builder, "separator_block_width");
    json_builder_add_int_value(builder, block->separator_block_width);

    if (block->uses_markup) {
        add_optional_text(builder, "markup", "pango");
    }
}

static void
add_texts(
    JsonBuilder                   *builder,
    const struct status_bar_block *block
)
{
    add_optional_text(builder, "name", block->name);
    add_optional_text(builder, "full_text", block->full_text);
    add_optional_text(builder, "color", block->color);
    add_optional_text(builder, "background", block->background);
    add_optional_text(builder, "min_width", block->minimum_width_text);
}

static void
add_block(
    JsonBuilder                   *builder,
    const struct status_bar_block *block
)
{
    json_builder_begin_object(builder);
    add_texts(builder, block);
    add_flags(builder, block);
    json_builder_end_object(builder);
}

static char *
generate_text(
    JsonBuilder *builder
)
{
    g_autoptr(JsonNode)      root      = json_builder_get_root(builder);
    g_autoptr(JsonGenerator) generator = json_generator_new();

    json_generator_set_root(generator, root);

    return json_generator_to_data(generator, NULL);
}

char *
status_bar_blocks_encode(
    const struct status_bar_block *blocks,
    size_t                         block_count
)
{
    g_autoptr(JsonBuilder) builder = json_builder_new();

    json_builder_begin_array(builder);

    for (size_t i = 0; i < block_count; i++) {
        add_block(builder, &blocks[i]);
    }

    json_builder_end_array(builder);

    g_autofree char *array_text = generate_text(builder);

    return g_strconcat(array_text, ",\n", NULL);
}
