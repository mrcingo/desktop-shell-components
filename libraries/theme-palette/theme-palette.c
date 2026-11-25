#include <math.h>
#include <string.h>

#include <glib.h>

#include "theme-palette.h"

static const char *const default_colors[THEME_PALETTE_ROLE_COUNT] = {
    [THEME_PALETTE_BACKGROUND]     = "#222222",
    [THEME_PALETTE_TEXT]           = "#eeeeee",
    [THEME_PALETTE_SECONDARY_TEXT] = "#bbbbbb",
    [THEME_PALETTE_ACCENT]         = "#005577",
    [THEME_PALETTE_ACCENT_TEXT]    = "#eeeeee",
    [THEME_PALETTE_BORDER]         = "#444444",
    [THEME_PALETTE_URGENT]         = "#900000",
};

/* The shared theme's variable for each role. */
static const char *const variable_names[THEME_PALETTE_ROLE_COUNT] = {
    [THEME_PALETTE_BACKGROUND]     = "--theme-background-color",
    [THEME_PALETTE_TEXT]           = "--theme-text-color",
    [THEME_PALETTE_SECONDARY_TEXT] = "--theme-secondary-text-color",
    [THEME_PALETTE_ACCENT]         = "--theme-accent-color",
    [THEME_PALETTE_ACCENT_TEXT]    = "--theme-accent-text-color",
    [THEME_PALETTE_BORDER]         = "--theme-border-color",
    [THEME_PALETTE_URGENT]         = "--theme-urgent-color",
};

static const char default_font_family[] = "monospace";

enum {
    DEFAULT_FONT_SIZE  = 10,
    SMALLEST_FONT_SIZE = 4,
    LARGEST_FONT_SIZE  = 72,
};

static bool
is_hexadecimal_text(
    const char *text,
    size_t      length
)
{
    for (size_t i = 0; i < length; i++) {
        if (!g_ascii_isxdigit(text[i])) {
            return false;
        }
    }

    return true;
}

/* The color forms sway and style sheets use: "#rgb", "#rrggbb", "#rrggbbaa". */
static const struct {
    size_t text_length;
    size_t digits_per_channel;
    size_t channel_count;
} color_forms[] = {
    {4, 1, 3},
    {7, 2, 3},
    {9, 2, 4},
};

/* A single digit stands for itself twice, as "#f00" means "#ff0000". */
static double
read_channel(
    const char *digits,
    size_t      digits_per_channel
)
{
    char pair[3] = {digits[0], digits[digits_per_channel - 1], '\0'};

    return (double)strtol(pair, NULL, 16) / 255.0;
}

static void
read_channels(
    const char           *digits,
    size_t                form_index,
    struct palette_color *color
)
{
    double channels[4]        = {0, 0, 0, 1};
    size_t digits_per_channel = color_forms[form_index].digits_per_channel;

    for (size_t i = 0; i < color_forms[form_index].channel_count; i++) {
        channels[i] = read_channel(digits + i * digits_per_channel, digits_per_channel);
    }

    *color = (struct palette_color){channels[0], channels[1], channels[2], channels[3]};
}

/* The index in color_forms, or -1 when the text has none of the forms. */
static int
find_color_form(
    const char *text
)
{
    size_t length = strlen(text);

    if (text[0] != '#') {
        return -1;
    }

    for (size_t i = 0; i < G_N_ELEMENTS(color_forms); i++) {
        if (color_forms[i].text_length == length) {
            return (int)i;
        }
    }

    return -1;
}

/* The color form's index, or -1 when text is no color in any of the forms. */
static int
find_valid_color_form(
    const char *text
)
{
    if (text == NULL) {
        return -1;
    }

    int form_index = find_color_form(text);

    if (form_index < 0) {
        return -1;
    }

    return is_hexadecimal_text(text + 1, strlen(text) - 1) ? form_index : -1;
}

bool
palette_color_parse(
    const char           *text,
    struct palette_color *color
)
{
    int form_index = find_valid_color_form(text);

    if (form_index < 0) {
        return false;
    }

    read_channels(text + 1, (size_t)form_index, color);

    return true;
}

static int
channel_byte(
    double channel
)
{
    return (int)lround(CLAMP(channel, 0.0, 1.0) * 255.0);
}

char *
palette_color_format(
    struct palette_color color
)
{
    int red   = channel_byte(color.red);
    int green = channel_byte(color.green);
    int blue  = channel_byte(color.blue);

    if (color.alpha >= 1.0) {
        return g_strdup_printf("#%02x%02x%02x", red, green, blue);
    }

    return g_strdup_printf("rgba(%d, %d, %d, %.3f)", red, green, blue, CLAMP(color.alpha, 0.0, 1.0));
}

static double
mix_channel(
    double first,
    double second,
    double fraction
)
{
    return first + (second - first) * fraction;
}

struct palette_color
palette_color_mix(
    struct palette_color first,
    struct palette_color second,
    double               fraction
)
{
    return (struct palette_color){
        mix_channel(first.red, second.red, fraction),
        mix_channel(first.green, second.green, fraction),
        mix_channel(first.blue, second.blue, fraction),
        mix_channel(first.alpha, second.alpha, fraction),
    };
}

struct palette_color
palette_color_with_alpha(
    struct palette_color color,
    double               alpha
)
{
    color.alpha = alpha;

    return color;
}

/* sRGB to linear light, as relative luminance is defined (WCAG 2). */
static double
linear_channel(
    double channel
)
{
    if (channel <= 0.04045) {
        return channel / 12.92;
    }

    return pow((channel + 0.055) / 1.055, 2.4);
}

double
palette_color_luminance(
    struct palette_color color
)
{
    return 0.2126 * linear_channel(color.red) + 0.7152 * linear_channel(color.green) + 0.0722 * linear_channel(color.blue);
}

struct theme_palette *
theme_palette_create_default(void)
{
    struct theme_palette *palette = g_new0(struct theme_palette, 1);

    for (int role = 0; role < THEME_PALETTE_ROLE_COUNT; role++) {
        palette_color_parse(default_colors[role], &palette->colors[role]);
    }

    palette->font_family = g_strdup(default_font_family);
    palette->font_size   = DEFAULT_FONT_SIZE;

    return palette;
}

void
theme_palette_destroy(
    struct theme_palette *palette
)
{
    if (palette == NULL) {
        return;
    }

    g_free(palette->font_family);
    g_free(palette);
}

bool
theme_palette_set_color(
    struct theme_palette   *palette,
    enum theme_palette_role role,
    const char             *text
)
{
    struct palette_color color;

    if (!palette_color_parse(text, &color)) {
        return false;
    }

    palette->colors[role] = color;

    return true;
}

/* The size when the word is a whole number of points in a sensible range, else 0. */
static int
read_font_size(
    const char *word
)
{
    guint64 size = 0;

    if (!g_ascii_string_to_unsigned(word, 10, SMALLEST_FONT_SIZE, LARGEST_FONT_SIZE, &size, NULL)) {
        return 0;
    }

    return (int)size;
}

/* Takes the size off the end of words, if there is one. */
static int
take_font_size(
    char **words
)
{
    guint word_count = g_strv_length(words);

    if (word_count < 2) {
        return 0;
    }

    int size = read_font_size(words[word_count - 1]);

    if (size > 0) {
        g_clear_pointer(&words[word_count - 1], g_free);
    }

    return size;
}

/* Names reach other programs' configuration files, so only plain characters are kept. */
static bool
is_plain_font_character(
    char character
)
{
    if (g_ascii_isalnum(character)) {
        return true;
    }

    return strchr(" -_.,", character) != NULL;
}

/* sway accepts "pango:Name 10" and "\"Name\" 10"; the prefix, quotes and anything unusual go. */
static char *
clean_font_description(
    const char *description
)
{
    const char *without_prefix = g_str_has_prefix(description, "pango:") ? description + 6 : description;
    GString    *cleaned        = g_string_new(NULL);

    for (const char *character = without_prefix; *character != '\0'; character++) {
        if (is_plain_font_character(*character)) {
            g_string_append_c(cleaned, *character);
        }
    }

    return g_strstrip(g_string_free(cleaned, FALSE));
}

static void
replace_font(
    struct theme_palette *palette,
    char                 *family,
    int                   size
)
{
    g_free(palette->font_family);

    palette->font_family = family;

    if (size > 0) {
        palette->font_size = size;
    }
}

/* The family without a trailing size, which goes to size; NULL when nothing is left. */
static char *
split_font_description(
    const char *description,
    int        *size
)
{
    g_autofree char *text   = clean_font_description(description);
    g_auto(GStrv)    words  = g_strsplit(text, " ", -1);
    g_autofree char *family = NULL;

    *size  = take_font_size(words);
    family = g_strstrip(g_strjoinv(" ", words));

    return family[0] == '\0' ? NULL : g_steal_pointer(&family);
}

static bool
apply_font_description(
    struct theme_palette *palette,
    const char           *description
)
{
    int   size   = 0;
    char *family = split_font_description(description, &size);

    if (family == NULL) {
        return false;
    }

    replace_font(palette, family, size);

    return true;
}

bool
theme_palette_set_font(
    struct theme_palette *palette,
    const char           *description
)
{
    if (description == NULL) {
        return false;
    }

    return apply_font_description(palette, description);
}

/* Comments become spaces, so a commented-out declaration never counts. */
static char *
blank_out_comments(
    const char *style_sheet_text
)
{
    char *text = g_strdup(style_sheet_text);

    for (char *comment = strstr(text, "/*"); comment != NULL; comment = strstr(comment, "/*")) {
        char *comment_end = strstr(comment + 2, "*/");
        char *blank_end   = comment_end != NULL ? comment_end + 2 : comment + strlen(comment);

        memset(comment, ' ', (size_t)(blank_end - comment));
    }

    return text;
}

/* The value after "NAME:" up to ";" or "}", trimmed; NULL without a colon. */
static char *
read_declaration_value(
    const char *after_name
)
{
    const char *value_start = after_name + strspn(after_name, " \t\n");

    if (*value_start != ':') {
        return NULL;
    }

    value_start++;

    return g_strstrip(g_strndup(value_start, strcspn(value_start, ";}")));
}

/* Every declaration of the variable is applied in order, so the last valid one wins, as in CSS. */
static void
apply_variable(
    struct theme_palette   *palette,
    enum theme_palette_role role,
    const char             *text
)
{
    const char *name        = variable_names[role];
    size_t      name_length = strlen(name);

    for (const char *found = strstr(text, name); found != NULL; found = strstr(found + name_length, name)) {
        g_autofree char *value = read_declaration_value(found + name_length);

        theme_palette_set_color(palette, role, value);
    }
}

void
theme_palette_apply_variables(
    struct theme_palette *palette,
    const char           *style_sheet_text
)
{
    g_autofree char *text = blank_out_comments(style_sheet_text);

    for (int role = 0; role < THEME_PALETTE_ROLE_COUNT; role++) {
        apply_variable(palette, (enum theme_palette_role)role, text);
    }
}

bool
theme_palette_is_dark(
    const struct theme_palette *palette
)
{
    return palette_color_luminance(palette->colors[THEME_PALETTE_BACKGROUND]) < 0.18;
}
