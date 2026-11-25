#include <glib.h>

#include "text-placeholder-expansion.h"

static const struct text_placeholder memory_placeholders[] = {
    {"used",  "5.1G"},
    {"total", "15G" },
    {"swap",  NULL  },
};

static const struct {
    const char *format;
    const char *expected_text;
} expansion_cases[] = {
    {"MEM {used}/{total}", "MEM 5.1G/15G"},
    {"{swap}|",            "|"           },
    {"{use} {used}",       "{use} 5.1G"  },
    {"open {used",         "open {used"  },
    {"}{used}{",           "}5.1G{"      },
    {"",                   ""            },
};

static void
assert_expansion(
    size_t case_index
)
{
    g_autofree char *text = text_placeholder_expand(
        expansion_cases[case_index].format,
        memory_placeholders,
        G_N_ELEMENTS(memory_placeholders)
    );

    g_assert_cmpstr(text, ==, expansion_cases[case_index].expected_text);
}

static void
test_formats_are_expanded(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(expansion_cases); i++) {
        assert_expansion(i);
    }
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/text-placeholder-expansion/formats", test_formats_are_expanded},
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
