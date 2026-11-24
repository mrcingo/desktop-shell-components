#include <glib.h>

#include "text-ellipsis-truncation.h"

static const struct {
    const char *text;
    size_t      maximum_characters;
    const char *expected_text;
} truncation_cases[] = {
    {"short",                                            10, "short"                },
    {"exactly10!",                                       10, "exactly10!"           },
    {"much longer text",                                 10, "much lo..."           },
    {"cut at a space here",                              7,  "cut..."               },
    {"\303\251\303\251\303\251\303\251\303\251\303\251", 5,  "\303\251\303\251..."  },
    {"anything",                                         2,  ".."                   },
    {"anything",                                         0,  ""                     },
    {"bad \377 byte",                                    20, "bad \357\277\275 byte"},
};

static void
assert_truncation(
    size_t case_index
)
{
    g_autofree char *text = text_ellipsis_truncate(
        truncation_cases[case_index].text,
        truncation_cases[case_index].maximum_characters
    );

    g_assert_cmpstr(text, ==, truncation_cases[case_index].expected_text);
}

static void
test_texts_are_truncated(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(truncation_cases); i++) {
        assert_truncation(i);
    }
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/text-ellipsis-truncation/cases", test_texts_are_truncated},
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
