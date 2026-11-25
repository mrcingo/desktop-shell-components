#include <glib.h>

#include "status-bar-click-decoding.h"

static void
assert_click(
    const char *line,
    const char *expected_block_name,
    int         expected_button
)
{
    struct status_bar_click click = {0};

    g_assert_true(status_bar_click_decode(line, &click));
    g_assert_cmpstr(click.block_name, ==, expected_block_name);
    g_assert_cmpint(click.button, ==, expected_button);
    status_bar_click_clear(&click);
}

static void
test_clicks_are_read(void)
{
    assert_click("{\"name\":\"volume\",\"button\":4,\"x\":1500,\"y\":10}", "volume", 4);
    assert_click(",{\"name\":\"battery\",\"button\":1}", "battery", 1);
    assert_click("  , {\"name\":\"clock\",\"button\":999}", "clock", 32);
}

static void
test_other_lines_are_not_clicks(void)
{
    const char *const lines[] = {"[", "", ",", "{\"button\":1}", "{\"name\":\"x\"}", "{\"name\":5,\"button\":1}", "garbage{"};
    struct status_bar_click click = {0};

    for (size_t i = 0; i < G_N_ELEMENTS(lines); i++) {
        g_assert_false(status_bar_click_decode(lines[i], &click));
    }
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/status-bar-click-decoding/clicks", test_clicks_are_read           },
    {"/status-bar-click-decoding/other",  test_other_lines_are_not_clicks},
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
