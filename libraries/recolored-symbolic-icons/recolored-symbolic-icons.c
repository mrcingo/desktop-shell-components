#include <string.h>

#include <glib/gstdio.h>

#include "managed-file-writing.h"
#include "recolored-symbolic-icons.h"

enum {
    LARGEST_SOURCE_ICON = 64 * 1024,
    LONGEST_ICON_NAME   = 64,
};

/* The grays single-color icons are drawn in; the longer spelling comes first, so "#222" never cuts "#222222" short. */
static const char *const foreground_grays[] = {"#2e3436", "#2e3434", "#2d3336", "#474747", "#222222", "#222", NULL};

/* Where a color may stand: attributes and style declarations. */
static const char *const color_properties[] = {"fill=\"", "fill:", "stroke=\"", "stroke:", NULL};

/* Anything that reaches outside the file or runs code; compared in lowercase. */
static const char *const refused_fragments[] = {
    "<script",
    "<!doctype",
    "<!entity",
    "<?xml-stylesheet",
    "href",
    "<foreignobject",
    "url(",
    "@import",
    NULL,
};

static const char xml_declaration_start[] = "<?xml";

/* Plain names only, so a setting can never point the theme outside its folders. */
bool
recolored_symbolic_icon_is_plain_name(
    const char *icon_name
)
{
    size_t length = strlen(icon_name);

    if (length == 0) {
        return false;
    }

    if (length > LONGEST_ICON_NAME) {
        return false;
    }

    return strspn(icon_name, "abcdefghijklmnopqrstuvwxyz0123456789-") == length;
}

static bool
is_hex_color(
    const char *color
)
{
    if (strlen(color) != 7) {
        return false;
    }

    if (color[0] != '#') {
        return false;
    }

    return strspn(color + 1, "0123456789abcdefABCDEF") == 6;
}

static bool
contains_refused_fragment(
    const char *lowercase_text
)
{
    for (const char *const *fragment = refused_fragments; *fragment != NULL; fragment++) {
        if (strstr(lowercase_text, *fragment) != NULL) {
            return true;
        }
    }

    return false;
}

/* Small UTF-8 text without NUL bytes. */
static bool
is_safe_text(
    const char *text,
    gsize       length
)
{
    if (length > LARGEST_SOURCE_ICON) {
        return false;
    }

    if (memchr(text, '\0', length) != NULL) {
        return false;
    }

    return g_utf8_validate(text, (gssize)length, NULL);
}

/* UTF-8 SVG text without NUL bytes, scripts or outside references. */
static bool
is_plain_svg_text(
    const char *svg_text,
    gsize       length
)
{
    if (!is_safe_text(svg_text, length)) {
        return false;
    }

    g_autofree char *lowercase_text = g_ascii_strdown(svg_text, (gssize)length);

    if (strstr(lowercase_text, "<svg") == NULL) {
        return false;
    }

    return !contains_refused_fragment(lowercase_text);
}

static const char *
skip_spaces(
    const char *text
)
{
    while (g_ascii_isspace(*text)) {
        text++;
    }

    return text;
}

/* The text after a leading XML declaration and the space after it; NULL when the declaration never ends. */
static const char *
skip_xml_declaration(
    const char *svg_text
)
{
    if (!g_str_has_prefix(svg_text, xml_declaration_start)) {
        return svg_text;
    }

    const char *declaration_end = strstr(svg_text, "?>");

    if (declaration_end == NULL) {
        return NULL;
    }

    return skip_spaces(declaration_end + 2);
}

/* The length of gray when text starts with exactly that color, else 0. */
static size_t
gray_spelling_length(
    const char *text,
    const char *gray
)
{
    size_t length = strlen(gray);

    if (g_ascii_strncasecmp(text, gray, length) != 0) {
        return 0;
    }

    if (g_ascii_isxdigit(text[length])) {
        return 0;
    }

    return length;
}

/* The length of a foreground gray at text, or 0; it must not run on into more hex digits. */
static size_t
gray_length_at(
    const char *text
)
{
    for (const char *const *gray = foreground_grays; *gray != NULL; gray++) {
        size_t length = gray_spelling_length(text, *gray);

        if (length > 0) {
            return length;
        }
    }

    return 0;
}

struct color_match {
    size_t property_length; // 0 when nothing matches
    size_t color_length;
};

static struct color_match
match_color_at(
    const char *text
)
{
    for (const char *const *property = color_properties; *property != NULL; property++) {
        size_t property_length = strlen(*property);

        if (strncmp(text, *property, property_length) == 0) {
            return (struct color_match){property_length, gray_length_at(text + property_length)};
        }
    }

    return (struct color_match){0, 0};
}

/* Appends the property with the new color; returns the text after the old color. */
static const char *
append_recolored_property(
    GString           *result,
    const char        *text,
    struct color_match match,
    const char        *new_color
)
{
    g_string_append_len(result, text, (gssize)match.property_length);
    g_string_append(result, new_color);

    return text + match.property_length + match.color_length;
}

/* Appends one character, or one recolored property; returns where to go on. */
static const char *
append_next_piece(
    GString    *result,
    const char *text,
    const char *new_color,
    guint      *replacement_count
)
{
    struct color_match match = match_color_at(text);

    if (match.color_length == 0) {
        g_string_append_c(result, *text);
        return text + 1;
    }

    (*replacement_count)++;

    return append_recolored_property(result, text, match, new_color);
}

/* Copies text with every foreground gray in a color property replaced; counts the replacements. */
static char *
replace_grays(
    const char *text,
    const char *new_color,
    guint      *replacement_count
)
{
    GString *result = g_string_new(NULL);

    while (*text != '\0') {
        text = append_next_piece(result, text, new_color, replacement_count);
    }

    return g_string_free(result, FALSE);
}

/* An icon without any foreground gray is not single-color: refused. */
static char *
recolor_body(
    const char *body,
    const char *new_color
)
{
    guint replacement_count = 0;
    char *recolored         = replace_grays(body, new_color, &replacement_count);

    if (replacement_count == 0) {
        g_clear_pointer(&recolored, g_free);
    }

    return recolored;
}

static bool
is_acceptable_input(
    const char *svg_text,
    gsize       length,
    const char *new_color
)
{
    if (!is_hex_color(new_color)) {
        return false;
    }

    return is_plain_svg_text(svg_text, length);
}

char *
recolored_symbolic_icon_recolor(
    const char *svg_text,
    gsize       length,
    const char *new_color
)
{
    if (!is_acceptable_input(svg_text, length, new_color)) {
        return NULL;
    }

    const char *body = skip_xml_declaration(svg_text);

    if (body == NULL) {
        return NULL;
    }

    return recolor_body(body, new_color);
}

/* One run of writing the copies. */
struct theme_writing {
    const struct recolored_symbolic_icons_request *request;
    GPtrArray                                     *source_contexts; // char *, sorted
    GHashTable                                    *written_paths;   // char * relative paths written or kept
    GError                                        *first_error;
};

static void
add_if_plain(
    GPtrArray  *names,
    const char *name
)
{
    if (recolored_symbolic_icon_is_plain_name(name)) {
        g_ptr_array_add(names, g_strdup(name));
    }
}

/* Entries with odd names are skipped: they could never be asked for. */
static void
add_plain_names(
    GPtrArray *names,
    GDir      *directory
)
{
    const char *name;

    while ((name = g_dir_read_name(directory)) != NULL) {
        add_if_plain(names, name);
    }
}

static GPtrArray *
sort_names(
    GPtrArray *names
)
{
    g_ptr_array_sort_values(names, (GCompareFunc)g_strcmp0);

    return names;
}

static GPtrArray *
list_source_contexts(
    const char *source_directory
)
{
    GPtrArray *contexts  = g_ptr_array_new_with_free_func(g_free);
    GDir      *directory = g_dir_open(source_directory, 0, NULL);

    if (directory == NULL) {
        return contexts;
    }

    add_plain_names(contexts, directory);
    g_dir_close(directory);

    return sort_names(contexts);
}

static char *
source_icon_path(
    const struct theme_writing *writing,
    const char                 *context,
    const char                 *icon_name
)
{
    g_autofree char *file_name = g_strconcat(icon_name, "-symbolic.svg", NULL);

    return g_build_filename(writing->request->source_directory, context, file_name, NULL);
}

static bool
context_has_icon(
    const struct theme_writing *writing,
    const char                 *context,
    const char                 *icon_name
)
{
    g_autofree char *path = source_icon_path(writing, context, icon_name);

    return g_file_test(path, G_FILE_TEST_IS_REGULAR);
}

/* The source context holding the icon, or NULL. */
static const char *
find_icon_context(
    const struct theme_writing *writing,
    const char                 *icon_name
)
{
    for (guint i = 0; i < writing->source_contexts->len; i++) {
        if (context_has_icon(writing, g_ptr_array_index(writing->source_contexts, i), icon_name)) {
            return g_ptr_array_index(writing->source_contexts, i);
        }
    }

    return NULL;
}

/* NULL when the file is missing, unreadable or larger than a source icon may be. */
static bool
is_small_enough(
    const char *path
)
{
    GStatBuf status = {0};

    if (g_stat(path, &status) != 0) {
        return false;
    }

    return status.st_size <= LARGEST_SOURCE_ICON;
}

static char *
read_small_file(
    const char *path,
    gsize      *length
)
{
    char *contents = NULL;

    if (!is_small_enough(path)) {
        return NULL;
    }

    g_file_get_contents(path, &contents, length, NULL);

    return contents;
}

/* The recolored icon, or NULL when its source is too large, unreadable or refused. */
static char *
read_recolored_icon(
    const struct theme_writing *writing,
    const char                 *source_path
)
{
    gsize length = 0;

    g_autofree char *contents = read_small_file(source_path, &length);

    if (contents == NULL) {
        return NULL;
    }

    return recolored_symbolic_icon_recolor(contents, length, writing->request->new_color);
}

static char *
overlay_relative_path(
    const struct theme_writing *writing,
    const char                 *context,
    const char                 *file_name
)
{
    return g_strdup_printf("%s/%s/%s", writing->request->overlay_directory, context, file_name);
}

static void
note_outcome(
    struct theme_writing     *writing,
    enum managed_file_outcome outcome,
    GError                   *error
)
{
    if (outcome != MANAGED_FILE_FAILED) {
        return;
    }

    if (writing->first_error == NULL) {
        writing->first_error = g_error_copy(error);
    }
}

static void
write_recolored_icon(
    struct theme_writing *writing,
    const char           *context,
    const char           *icon_name,
    const char           *recolored
)
{
    g_autofree char *file_name = g_strconcat(icon_name, "-symbolic.svg", NULL);

    char *relative_path = overlay_relative_path(writing, context, file_name);

    g_autoptr(GError) error = NULL;

    const struct recolored_symbolic_icons_request *request = writing->request;

    note_outcome(
        writing,
        managed_file_write(request->data_directory, relative_path, request->svg_marker_line, recolored, &error),
        error
    );
    g_hash_table_add(writing->written_paths, relative_path);
}

static void
write_icon_of_context(
    struct theme_writing *writing,
    const char           *context,
    const char           *icon_name
)
{
    g_autofree char *source_path = source_icon_path(writing, context, icon_name);
    g_autofree char *recolored   = read_recolored_icon(writing, source_path);

    if (recolored != NULL) {
        write_recolored_icon(writing, context, icon_name, recolored);
    }
}

/* Missing or refused icons are skipped quietly: the inherited theme still has them. */
static void
write_icon(
    struct theme_writing *writing,
    const char           *icon_name
)
{
    if (!recolored_symbolic_icon_is_plain_name(icon_name)) {
        return;
    }

    const char *context = find_icon_context(writing, icon_name);

    if (context != NULL) {
        write_icon_of_context(writing, context, icon_name);
    }
}

/* Only icons carrying our marker are removed; anything the user put there stays. */
static void
remove_if_stale(
    struct theme_writing *writing,
    const char           *context,
    const char           *file_name
)
{
    g_autofree char  *relative_path = overlay_relative_path(writing, context, file_name);
    g_autoptr(GError) error         = NULL;

    if (g_hash_table_contains(writing->written_paths, relative_path)) {
        return;
    }

    const struct recolored_symbolic_icons_request *request = writing->request;

    note_outcome(writing, managed_file_remove(request->data_directory, relative_path, request->svg_marker_line, &error), error);
}

static GPtrArray *
list_plain_entries(
    const char *directory_path
)
{
    return list_source_contexts(directory_path);
}

static void
remove_stale_entries(
    struct theme_writing *writing,
    GDir                 *directory,
    const char           *context
)
{
    const char *file_name;

    while ((file_name = g_dir_read_name(directory)) != NULL) {
        remove_if_stale(writing, context, file_name);
    }
}

static void
remove_stale_icons_of_context(
    struct theme_writing *writing,
    const char           *overlay_path,
    const char           *context
)
{
    g_autofree char *context_directory = g_build_filename(overlay_path, context, NULL);

    GDir *directory = g_dir_open(context_directory, 0, NULL);

    if (directory == NULL) {
        return;
    }

    remove_stale_entries(writing, directory, context);
    g_dir_close(directory);
}

static void
remove_stale_icons(
    struct theme_writing *writing
)
{
    g_autofree char *overlay_path = g_build_filename(
        writing->request->data_directory,
        writing->request->overlay_directory,
        NULL
    );
    g_autoptr(GPtrArray) contexts = list_plain_entries(overlay_path);

    for (guint i = 0; i < contexts->len; i++) {
        remove_stale_icons_of_context(writing, overlay_path, g_ptr_array_index(contexts, i));
    }
}

static void
clear_theme_writing(
    struct theme_writing *writing
)
{
    g_ptr_array_unref(writing->source_contexts);
    g_hash_table_unref(writing->written_paths);
}

static bool
report_first_error(
    struct theme_writing *writing,
    GError              **error
)
{
    if (writing->first_error == NULL) {
        return true;
    }

    g_propagate_error(error, writing->first_error);

    return false;
}

bool
recolored_symbolic_icons_write(
    const struct recolored_symbolic_icons_request *request,
    GError                                       **error
)
{
    struct theme_writing writing = {
        .request         = request,
        .source_contexts = list_source_contexts(request->source_directory),
        .written_paths   = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL),
    };

    for (const char *const *icon_name = request->icon_names; *icon_name != NULL; icon_name++) {
        write_icon(&writing, *icon_name);
    }

    remove_stale_icons(&writing);
    clear_theme_writing(&writing);

    return report_first_error(&writing, error);
}
