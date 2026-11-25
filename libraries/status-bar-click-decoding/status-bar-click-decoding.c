#include <string.h>

#include <glib.h>
#include <json-glib/json-glib.h>

#include "status-bar-click-decoding.h"
#include "typed-json-member-lookup.h"

/* Clicks are small objects; anything longer is not one. */
enum {
    MAXIMUM_CLICK_LINE_BYTES = 4096,
    LARGEST_BUTTON_NUMBER    = 32,
};

/* The click object without the protocol's leading "," and surrounding spaces. */
static const char *
skip_separator(
    const char *line
)
{
    while (g_ascii_isspace(*line)) {
        line++;
    }

    if (*line == ',') {
        line++;
    }

    return line;
}

/* False unless the object names a block and a button. */
static bool
read_button(
    JsonObject *click_object,
    int64_t    *button
)
{
    if (typed_json_lookup_string_member(click_object, "name") == NULL) {
        return false;
    }

    return typed_json_lookup_integer_member(click_object, "button", button);
}

static bool
read_click(
    JsonObject              *click_object,
    struct status_bar_click *click
)
{
    int64_t button = 0;

    if (!read_button(click_object, &button)) {
        return false;
    }

    *click = (struct status_bar_click){
        .block_name = g_strdup(typed_json_lookup_string_member(click_object, "name")),
        .button     = (int)CLAMP(button, 0, LARGEST_BUTTON_NUMBER),
    };

    return true;
}

bool
status_bar_click_decode(
    const char              *line,
    struct status_bar_click *click
)
{
    if (strlen(line) > MAXIMUM_CLICK_LINE_BYTES) {
        return false;
    }

    g_autoptr(JsonNode) click_node = json_from_string(skip_separator(line), NULL);

    return read_click(typed_json_root_object(click_node), click);
}

void
status_bar_click_clear(
    struct status_bar_click *click
)
{
    g_free(click->block_name);

    *click = (struct status_bar_click){0};
}
