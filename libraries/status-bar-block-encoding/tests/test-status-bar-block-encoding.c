#include <glib.h>

#include "status-bar-block-encoding.h"

static void
test_header_announces_clicks(void)
{
    g_autofree char *with_clicks    = status_bar_protocol_start(true);
    g_autofree char *without_clicks = status_bar_protocol_start(false);

    g_assert_cmpstr(with_clicks, ==, "{\"version\":1,\"click_events\":true}\n[\n");
    g_assert_cmpstr(without_clicks, ==, "{\"version\":1}\n[\n");
}

/* Quotes and backslashes in texts from other programs, such as a network name, stay inside the string. */
static void
test_blocks_are_encoded(void)
{
    const struct status_bar_block blocks[] = {
        {.name = "network", .full_text = "Caf\303\251 \"free\" \\", .separator_block_width = 13},
        {.name               = "battery",
         .full_text          = "<b>9%</b>",
         .color              = "#ff0000",
         .minimum_width_text = "100%",
         .is_urgent          = true,
         .uses_markup        = true},
    };
    g_autofree char *line = status_bar_blocks_encode(blocks, G_N_ELEMENTS(blocks));

    g_assert_cmpstr(
        line,
        ==,
        "[{\"name\":\"network\",\"full_text\":\"Caf\303\251 \\\"free\\\" \\\\\",\"urgent\":false,\"separator_block_width\":13},"
        "{\"name\":\"battery\",\"full_text\":\"<b>9%</b>\",\"color\":\"#ff0000\",\"min_width\":\"100%\",\"urgent\":true,"
        "\"separator_block_width\":0,\"markup\":\"pango\"}],\n"
    );
}

static void
test_no_blocks_encode_an_empty_array(void)
{
    g_autofree char *line = status_bar_blocks_encode(NULL, 0);

    g_assert_cmpstr(line, ==, "[],\n");
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/status-bar-block-encoding/header", test_header_announces_clicks        },
    {"/status-bar-block-encoding/blocks", test_blocks_are_encoded             },
    {"/status-bar-block-encoding/empty",  test_no_blocks_encode_an_empty_array},
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
