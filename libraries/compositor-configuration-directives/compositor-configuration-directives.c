#include <string.h>

#include <glib.h>

#include "compositor-configuration-directives.h"

struct directive_search {
    const char *directive_name;
    int         block_depth;
    char       *last_arguments;
};

/* Colors such as #222222 are not comments: sway only treats lines starting with "#" as comments. */
static int
count_block_depth_change(
    const char *line
)
{
    int depth_change = 0;

    for (const char *character = line; *character != '\0'; character++) {
        depth_change += (*character == '{') - (*character == '}');
    }

    return depth_change;
}

/* The arguments when the line starts with the directive's name as a whole word, else NULL. */
static const char *
find_arguments(
    const char *trimmed_line,
    const char *directive_name
)
{
    size_t name_length = strlen(directive_name);

    if (strncmp(trimmed_line, directive_name, name_length) != 0) {
        return NULL;
    }

    if (!g_ascii_isspace(trimmed_line[name_length])) {
        return NULL;
    }

    return trimmed_line + name_length;
}

static void
remember_arguments_of_top_level_line(
    struct directive_search *search,
    char                    *trimmed_line
)
{
    const char *arguments = find_arguments(trimmed_line, search->directive_name);

    if (arguments == NULL) {
        return;
    }

    g_free(search->last_arguments);

    search->last_arguments = g_strstrip(g_strdup(arguments));
}

static void
search_line(
    struct directive_search *search,
    char                    *line
)
{
    char *trimmed_line = g_strstrip(line);

    if (trimmed_line[0] == '#') {
        return;
    }

    if (search->block_depth == 0) {
        remember_arguments_of_top_level_line(search, trimmed_line);
    }

    search->block_depth = MAX(0, search->block_depth + count_block_depth_change(trimmed_line));
}

char *
compositor_configuration_find_directive(
    const char *configuration_text,
    const char *directive_name
)
{
    g_auto(GStrv) lines = g_strsplit(configuration_text, "\n", -1);

    struct directive_search search = {.directive_name = directive_name};

    for (char **line = lines; *line != NULL; line++) {
        search_line(&search, *line);
    }

    return search.last_arguments;
}
