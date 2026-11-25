#include <string.h>

#include <glib/gstdio.h>

#include "recolored-symbolic-icons.h"

static const char svg_marker[] = "<!-- Written by a test -->";

static const char
    home_icon[] = "<?xml version=\"1.0\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\"><path fill=\"#2e3436\" d=\"M1 1\"/>"
                  "<path style=\"fill:#2E3434\"/><path fill=\"#33d17a\"/><path fill=\"#2223\"/></svg>\n";

static const char icon_with_nul[] = "<svg>\0<path fill=\"#2e3436\"/></svg>";

static const struct {
    const char *source;
    const char *reason;
} refused_icons[] = {
    {"<svg><script>alert(1)</script><path fill=\"#2e3436\"/></svg>",            "script"       },
    {"<svg><image href=\"file:///etc/passwd\"/><path fill=\"#2e3436\"/></svg>", "outside link" },
    {"<!DOCTYPE svg [<!ENTITY a \"b\">]><svg><path fill=\"#2e3436\"/></svg>",   "entities"     },
    {"<svg><path fill=\"#33d17a\"/></svg>",                                     "no foreground"},
    {"<html><path fill=\"#2e3436\"/></html>",                                   "not svg"      },
    {"<?xml version=\"1.0\" <svg><path fill=\"#2e3436\"/></svg>",               "broken prolog"},
};

/* Foreground grays change, in any spelling case; status colors and longer hex runs stay; the declaration goes. */
static void
test_foreground_is_recolored(void)
{
    g_autofree char *recolored = recolored_symbolic_icon_recolor(home_icon, strlen(home_icon), "#bbbbbb");

    g_assert_cmpstr(
        recolored,
        ==,
        "<svg xmlns=\"http://www.w3.org/2000/svg\"><path fill=\"#bbbbbb\" d=\"M1 1\"/><path style=\"fill:#bbbbbb\"/>"
        "<path fill=\"#33d17a\"/><path fill=\"#2223\"/></svg>\n"
    );
}

static void
test_odd_icons_are_refused(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(refused_icons); i++) {
        g_autofree char *recolored = recolored_symbolic_icon_recolor(
            refused_icons[i].source,
            strlen(refused_icons[i].source),
            "#bbbbbb"
        );

        g_assert_null(recolored);
    }

    g_assert_null(recolored_symbolic_icon_recolor(home_icon, strlen(home_icon), "red; x"));
    g_assert_null(recolored_symbolic_icon_recolor(icon_with_nul, sizeof(icon_with_nul) - 1, "#bbbbbb"));
}

static void
test_names_must_be_plain(void)
{
    g_assert_true(recolored_symbolic_icon_is_plain_name("user-home"));
    g_assert_false(recolored_symbolic_icon_is_plain_name("../user-home"));
    g_assert_false(recolored_symbolic_icon_is_plain_name("User Home"));
    g_assert_false(recolored_symbolic_icon_is_plain_name(""));
}

static void
write_text(
    const char *directory,
    const char *relative_path,
    const char *text
)
{
    g_autofree char *path   = g_build_filename(directory, relative_path, NULL);
    g_autofree char *folder = g_path_get_dirname(path);

    g_mkdir_with_parents(folder, 0755);
    g_file_set_contents(path, text, -1, NULL);
}

static bool
exists(
    const char *directory,
    const char *relative_path
)
{
    g_autofree char *path = g_build_filename(directory, relative_path, NULL);

    return g_file_test(path, G_FILE_TEST_EXISTS);
}

static bool
write_theme(
    const char        *source_directory,
    const char        *data_directory,
    const char *const *icon_names
)
{
    const struct recolored_symbolic_icons_request request = {
        .source_directory  = source_directory,
        .data_directory    = data_directory,
        .overlay_directory = "icons/Adwaita/symbolic",
        .icon_names        = icon_names,
        .new_color         = "#bbbbbb",
        .svg_marker_line   = svg_marker,
    };

    return recolored_symbolic_icons_write(&request, NULL);
}

static char *
create_fake_source(void)
{
    char *source_directory = g_dir_make_tmp("recolored-symbolic-source-XXXXXX", NULL);

    write_text(source_directory, "places/user-home-symbolic.svg", home_icon);
    write_text(source_directory, "actions/document-open-recent-symbolic.svg", home_icon);

    return source_directory;
}

/* Both icons, plus a file of the user's own in the theme folder; missing and odd names are skipped. */
static void
write_first_theme(
    const char *source_directory,
    const char *data_directory
)
{
    const char *const both[] = {"user-home", "document-open-recent", "no-such-icon", "../escape", NULL};

    g_assert_true(write_theme(source_directory, data_directory, both));
    g_assert_true(exists(data_directory, "icons/Adwaita/symbolic/actions/document-open-recent-symbolic.svg"));
    write_text(data_directory, "icons/Adwaita/symbolic/places/mine-symbolic.svg", "<svg/>");
}

/* Wanted icons are written; one no longer wanted goes; the user's own file and missing sources are left alone. */
static void
test_theme_is_written_and_kept_tidy(void)
{
    g_autofree char *source_directory = create_fake_source();
    g_autofree char *data_directory   = g_dir_make_tmp("recolored-symbolic-data-XXXXXX", NULL);

    const char *const only_home[] = {"user-home", NULL};

    write_first_theme(source_directory, data_directory);
    g_assert_true(
        write_theme(source_directory, data_directory, only_home) &&
        exists(data_directory, "icons/Adwaita/symbolic/places/user-home-symbolic.svg")
    );
    g_assert_false(exists(data_directory, "icons/Adwaita/symbolic/actions/document-open-recent-symbolic.svg"));
    g_assert_true(exists(data_directory, "icons/Adwaita/symbolic/places/mine-symbolic.svg"));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/recolored-symbolic-icons/recolor", test_foreground_is_recolored       },
    {"/recolored-symbolic-icons/refused", test_odd_icons_are_refused         },
    {"/recolored-symbolic-icons/names",   test_names_must_be_plain           },
    {"/recolored-symbolic-icons/overlay", test_theme_is_written_and_kept_tidy},
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add_func(registered_tests[i].test_path, registered_tests[i].test_function);
    }

    return g_test_run();
}
