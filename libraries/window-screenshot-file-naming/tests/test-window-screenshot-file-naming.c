#include "window-screenshot-file-naming.h"

static const struct {
    const char *application_name;
    const char *expected_name;
} name_cases[] = {
    {"Slack",                                                   "Slack"                                   },
    {"org.gnome.Nautilus",                                      "org.gnome.Nautilus"                      },
    {"../../etc/passwd",                                        "etc_passwd"                              },
    {"  .hidden. ",                                             "hidden"                                  },
    {"caf\303\251 & tea",                                       "caf__ _ tea"                             },
    {"",                                                        ""                                        },
    {NULL,                                                      ""                                        },
    {"a/very/long/name/that/goes/on/and/on/beyond/forty/chars", "a_very_long_name_that_goes_on_and_on_bey"},
};

static void
test_application_names_are_made_safe(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(name_cases); i++) {
        g_autofree char *safe_name = window_screenshot_safe_application_name(name_cases[i].application_name);

        g_assert_cmpstr(safe_name, ==, name_cases[i].expected_name);
    }
}

/* The second attempt gets a number, so an existing file is never replaced. */
static void
test_file_names_are_numbered(void)
{
    g_autoptr(GDateTime) taken_at = g_date_time_new_local(2026, 11, 25, 11, 30, 45);
    g_autofree char     *first    = window_screenshot_file_name(taken_at, "Slack", 1);
    g_autofree char     *second   = window_screenshot_file_name(taken_at, "Slack", 2);
    g_autofree char     *unnamed  = window_screenshot_file_name(taken_at, "///", 3);

    g_assert_cmpstr(first, ==, "Screenshot 2026-11-25 113045 - Slack.png");
    g_assert_cmpstr(second, ==, "Screenshot 2026-11-25 113045 - Slack-2.png");
    g_assert_cmpstr(unnamed, ==, "Screenshot 2026-11-25 113045-3.png");
}

static const struct {
    const char *configured_folder;
    const char *pictures_folder;
    const char *expected_folder;
} folder_cases[] = {
    {"",            "/home/a/Bilder", "/home/a/Bilder/Screenshots"  },
    {NULL,          NULL,             "/home/a/Pictures/Screenshots"},
    {"~/Shots",     NULL,             "/home/a/Shots"               },
    {"Shots",       NULL,             "/home/a/Shots"               },
    {"/data/shots", NULL,             "/data/shots"                 },
};

static void
test_folders_are_resolved(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(folder_cases); i++) {
        g_autofree char *folder = window_screenshot_resolve_folder(
            folder_cases[i].configured_folder,
            folder_cases[i].pictures_folder,
            "/home/a"
        );

        g_assert_cmpstr(folder, ==, folder_cases[i].expected_folder);
    }
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-screenshot-file-naming/application-names", test_application_names_are_made_safe},
    {"/window-screenshot-file-naming/numbered",          test_file_names_are_numbered        },
    {"/window-screenshot-file-naming/folders",           test_folders_are_resolved           },
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
