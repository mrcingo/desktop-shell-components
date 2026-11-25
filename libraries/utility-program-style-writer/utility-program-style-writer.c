#include <math.h>

#include <glib.h>

#include "utility-program-style-writer.h"

/* "rrggbb" or "rrggbbaa", the form these programs take colors in. */
static char *
plain_hexadecimal(
    struct palette_color color
)
{
    int red   = (int)lround(CLAMP(color.red, 0.0, 1.0) * 255.0);
    int green = (int)lround(CLAMP(color.green, 0.0, 1.0) * 255.0);
    int blue  = (int)lround(CLAMP(color.blue, 0.0, 1.0) * 255.0);
    int alpha = (int)lround(CLAMP(color.alpha, 0.0, 1.0) * 255.0);

    if (alpha == 255) {
        return g_strdup_printf("%02x%02x%02x", red, green, blue);
    }

    return g_strdup_printf("%02x%02x%02x%02x", red, green, blue, alpha);
}

static char *
role_hexadecimal(
    const struct theme_palette *palette,
    enum theme_palette_role     role
)
{
    return plain_hexadecimal(palette->colors[role]);
}

static void
append_setting(
    GString    *text,
    const char *key,
    char       *value
)
{
    g_string_append_printf(text, "%s=%s\n", key, value);
    g_free(value);
}

static const struct palette_color transparent = {0, 0, 0, 0};

/* The ring: accent while typing, border color at rest, urgent on a wrong password. */
static void
append_lock_screen_ring(
    GString                    *text,
    const struct theme_palette *palette
)
{
    append_setting(text, "ring-color", role_hexadecimal(palette, THEME_PALETTE_BORDER));
    append_setting(text, "ring-clear-color", role_hexadecimal(palette, THEME_PALETTE_SECONDARY_TEXT));
    append_setting(text, "ring-ver-color", role_hexadecimal(palette, THEME_PALETTE_ACCENT));
    append_setting(text, "ring-wrong-color", role_hexadecimal(palette, THEME_PALETTE_URGENT));
    append_setting(text, "key-hl-color", role_hexadecimal(palette, THEME_PALETTE_ACCENT));
    append_setting(text, "bs-hl-color", role_hexadecimal(palette, THEME_PALETTE_URGENT));
}

static void
append_lock_screen_inside(
    GString                    *text,
    const struct theme_palette *palette
)
{
    append_setting(text, "inside-color", role_hexadecimal(palette, THEME_PALETTE_BACKGROUND));
    append_setting(text, "inside-clear-color", role_hexadecimal(palette, THEME_PALETTE_BACKGROUND));
    append_setting(text, "inside-ver-color", role_hexadecimal(palette, THEME_PALETTE_ACCENT));
    append_setting(text, "inside-wrong-color", role_hexadecimal(palette, THEME_PALETTE_URGENT));
    append_setting(text, "line-color", plain_hexadecimal(transparent));
    append_setting(text, "separator-color", plain_hexadecimal(transparent));
}

static void
append_lock_screen_text(
    GString                    *text,
    const struct theme_palette *palette
)
{
    append_setting(text, "text-color", role_hexadecimal(palette, THEME_PALETTE_TEXT));
    append_setting(text, "text-clear-color", role_hexadecimal(palette, THEME_PALETTE_TEXT));
    append_setting(text, "text-ver-color", role_hexadecimal(palette, THEME_PALETTE_ACCENT_TEXT));
    append_setting(text, "text-wrong-color", role_hexadecimal(palette, THEME_PALETTE_ACCENT_TEXT));
    append_setting(text, "layout-bg-color", role_hexadecimal(palette, THEME_PALETTE_BACKGROUND));
    append_setting(text, "layout-text-color", role_hexadecimal(palette, THEME_PALETTE_TEXT));
}

/* The background, and text twice the size of the theme's, as the lock screen is read from afar. */
static void
append_lock_screen_basics(
    GString                    *text,
    const struct theme_palette *palette
)
{
    append_setting(text, "color", role_hexadecimal(palette, THEME_PALETTE_BACKGROUND));
    append_setting(text, "font", g_strdup(palette->font_family));
    append_setting(text, "font-size", g_strdup_printf("%d", palette->font_size * 2));
}

char *
utility_program_style_write_lock_screen(
    const struct theme_palette *palette
)
{
    GString *text = g_string_new(NULL);

    append_lock_screen_basics(text, palette);
    append_lock_screen_ring(text, palette);
    append_lock_screen_inside(text, palette);
    append_lock_screen_text(text, palette);

    return g_string_free(text, FALSE);
}

static void
append_prompt_bar_colors(
    GString                    *text,
    const struct theme_palette *palette,
    enum theme_palette_role     bottom_border_role
)
{
    append_setting(text, "background", role_hexadecimal(palette, THEME_PALETTE_BACKGROUND));
    append_setting(text, "text", role_hexadecimal(palette, THEME_PALETTE_TEXT));
    append_setting(text, "border", role_hexadecimal(palette, THEME_PALETTE_BORDER));
    append_setting(text, "border-bottom", role_hexadecimal(palette, bottom_border_role));
    append_setting(text, "button-background", role_hexadecimal(palette, THEME_PALETTE_ACCENT));
    append_setting(text, "button-text", role_hexadecimal(palette, THEME_PALETTE_ACCENT_TEXT));
}

/* Both kinds of prompt sway shows; an error is marked by its bottom line. */
char *
utility_program_style_write_prompt_bar(
    const struct theme_palette *palette
)
{
    GString *text = g_string_new(NULL);

    append_setting(text, "font", g_strdup_printf("%s %d", palette->font_family, palette->font_size));
    g_string_append(text, "\n[warning]\n");
    append_prompt_bar_colors(text, palette, THEME_PALETTE_ACCENT);
    g_string_append(text, "\n[error]\n");
    append_prompt_bar_colors(text, palette, THEME_PALETTE_URGENT);

    return g_string_free(text, FALSE);
}

static void
append_argument(
    GPtrArray  *arguments,
    const char *option,
    char       *value
)
{
    g_ptr_array_add(arguments, g_strdup(option));
    g_ptr_array_add(arguments, value);
}

/* Normal, prompt and selected entries: background then text color each. */
static const struct {
    const char             *option;
    enum theme_palette_role role;
} launcher_color_options[] = {
    {"-N", THEME_PALETTE_BACKGROUND    },
    {"-n", THEME_PALETTE_SECONDARY_TEXT},
    {"-M", THEME_PALETTE_ACCENT        },
    {"-m", THEME_PALETTE_ACCENT_TEXT   },
    {"-S", THEME_PALETTE_ACCENT        },
    {"-s", THEME_PALETTE_ACCENT_TEXT   },
};

char **
utility_program_style_launcher_arguments(
    const struct theme_palette *palette
)
{
    GPtrArray *arguments = g_ptr_array_new();

    append_argument(arguments, "-f", g_strdup_printf("%s %d", palette->font_family, palette->font_size));

    for (size_t i = 0; i < G_N_ELEMENTS(launcher_color_options); i++) {
        append_argument(arguments, launcher_color_options[i].option, role_hexadecimal(palette, launcher_color_options[i].role));
    }

    g_ptr_array_add(arguments, NULL);

    return (char **)g_ptr_array_free(arguments, FALSE);
}
