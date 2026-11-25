#include <stdbool.h>
#include <string.h>

#include "window-screenshot-file-naming.h"

/* Room for the name before it is shortened: replaced bytes of long names never reach the result. */
enum {
    LONGEST_NAME_EXAMINED = 4 * WINDOW_SCREENSHOT_LONGEST_APPLICATION_NAME,
};

static bool
is_kept_character(
    char character
)
{
    if (g_ascii_isalnum(character)) {
        return true;
    }

    if (character == '\0') {
        return false;
    }

    return strchr(" .-_", character) != NULL;
}

/* strchr would also find the terminating NUL, so it is checked first. */
static bool
is_trimmed_character(
    char character
)
{
    if (character == '\0') {
        return false;
    }

    return strchr(" ._", character) != NULL;
}

/* Bytes of multibyte characters are replaced too, so the result is plain ASCII. */
static void
replace_unsafe_characters(
    char *text
)
{
    for (char *character = text; *character != '\0'; character++) {
        if (!is_kept_character(*character)) {
            *character = '_';
        }
    }
}

static size_t
count_leading_trimmed_characters(
    const char *text
)
{
    size_t count = 0;

    while (is_trimmed_character(text[count])) {
        count++;
    }

    return count;
}

static size_t
length_without_trailing_trimmed_characters(
    const char *text,
    size_t      length
)
{
    while (length > 0) {
        if (!is_trimmed_character(text[length - 1])) {
            return length;
        }

        length--;
    }

    return 0;
}

static char *
trim_edges(
    const char *text
)
{
    const char *start  = text + count_leading_trimmed_characters(text);
    size_t      length = length_without_trailing_trimmed_characters(start, strlen(start));

    return g_strndup(start, length);
}

static const char *
text_or_empty(
    const char *text
)
{
    if (text == NULL) {
        return "";
    }

    return text;
}

char *
window_screenshot_safe_application_name(
    const char *application_name
)
{
    g_autofree char *examined_name = g_strndup(text_or_empty(application_name), LONGEST_NAME_EXAMINED);

    replace_unsafe_characters(examined_name);

    g_autofree char *shortened_name = trim_edges(examined_name);

    shortened_name[MIN(strlen(shortened_name), WINDOW_SCREENSHOT_LONGEST_APPLICATION_NAME)] = '\0';

    return trim_edges(shortened_name);
}

/* Nothing for the first attempt, "-2" from the second on. */
static char *
format_attempt_number(
    unsigned attempt
)
{
    if (attempt <= 1) {
        return g_strdup("");
    }

    return g_strdup_printf("-%u", attempt);
}

/* " - Slack" when there is a name, followed by the attempt number. */
static char *
format_name_suffix(
    const char *safe_application_name,
    unsigned    attempt
)
{
    g_autofree char *number = format_attempt_number(attempt);

    if (safe_application_name[0] == '\0') {
        return g_strdup(number);
    }

    return g_strdup_printf(" - %s%s", safe_application_name, number);
}

char *
window_screenshot_file_name(
    GDateTime  *taken_at,
    const char *application_name,
    unsigned    attempt
)
{
    g_autofree char *safe_application_name = window_screenshot_safe_application_name(application_name);
    g_autofree char *time_text             = g_date_time_format(taken_at, "%Y-%m-%d %H%M%S");
    g_autofree char *suffix                = format_name_suffix(safe_application_name, attempt);

    return g_strdup_printf("Screenshot %s%s.png", time_text, suffix);
}

static char *
default_folder(
    const char *pictures_folder,
    const char *home_folder
)
{
    if (pictures_folder != NULL) {
        return g_build_filename(pictures_folder, "Screenshots", NULL);
    }

    return g_build_filename(home_folder, "Pictures", "Screenshots", NULL);
}

/* "~/x" and "x" are inside the home folder; "/x" stays as written. */
static char *
expand_configured_folder(
    const char *configured_folder,
    const char *home_folder
)
{
    if (g_str_has_prefix(configured_folder, "~/")) {
        return g_build_filename(home_folder, configured_folder + 2, NULL);
    }

    if (g_path_is_absolute(configured_folder)) {
        return g_strdup(configured_folder);
    }

    return g_build_filename(home_folder, configured_folder, NULL);
}

char *
window_screenshot_resolve_folder(
    const char *configured_folder,
    const char *pictures_folder,
    const char *home_folder
)
{
    if (configured_folder == NULL) {
        return default_folder(pictures_folder, home_folder);
    }

    if (configured_folder[0] == '\0') {
        return default_folder(pictures_folder, home_folder);
    }

    return expand_configured_folder(configured_folder, home_folder);
}
