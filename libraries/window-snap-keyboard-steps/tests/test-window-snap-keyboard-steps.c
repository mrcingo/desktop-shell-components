#include <glib.h>

#include "window-snap-keyboard-steps.h"

/* Key sequences from a free window, and where they end. */
static const struct {
    const char            *keys;
    enum window_snap_state final_state;
} sequences[] = {
    {"left",          WINDOW_SNAP_STATE_LEFT_HALF           },
    {"left up",       WINDOW_SNAP_STATE_TOP_LEFT_QUARTER    },
    {"right down",    WINDOW_SNAP_STATE_BOTTOM_RIGHT_QUARTER},
    {"up",            WINDOW_SNAP_STATE_MAXIMIZED           },
    {"up down",       WINDOW_SNAP_STATE_FREE                },
    {"left right",    WINDOW_SNAP_STATE_FREE                },
    {"left up right", WINDOW_SNAP_STATE_TOP_RIGHT_QUARTER   },
    {"right down up", WINDOW_SNAP_STATE_RIGHT_HALF          },
    {"up left",       WINDOW_SNAP_STATE_LEFT_HALF           },
};

static enum window_snap_state
press_keys(
    const char *keys
)
{
    g_auto(GStrv) words = g_strsplit(keys, " ", -1);

    enum window_snap_state state = WINDOW_SNAP_STATE_FREE;
    enum window_snap_key   key;

    for (char **word = words; *word != NULL; word++) {
        g_assert_true(window_snap_parse_key(*word, &key));

        state = window_snap_state_after_key(state, key);
    }

    return state;
}

static void
test_key_sequences(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(sequences); i++) {
        g_assert_cmpint(press_keys(sequences[i].keys), ==, sequences[i].final_state);
    }
}

static void
test_areas(void)
{
    const struct compositor_rectangle workspace = {0, 24, 1920, 1176};
    struct compositor_rectangle       area      = {0};

    g_assert_true(window_snap_state_area(WINDOW_SNAP_STATE_TOP_RIGHT_QUARTER, &workspace, &area));
    g_assert_cmpint(area.x * 1000000 + area.y * 1000 + area.height, ==, 960 * 1000000 + 24 * 1000 + 588);
    g_assert_false(window_snap_state_area(WINDOW_SNAP_STATE_FREE, &workspace, &area));
}

static void
test_words(void)
{
    enum window_snap_state state;
    enum window_snap_key   key;

    g_assert_true(window_snap_parse_state("maximize", &state) && state == WINDOW_SNAP_STATE_MAXIMIZED);
    g_assert_true(window_snap_parse_state("restore", &state) && state == WINDOW_SNAP_STATE_FREE);
    g_assert_false(window_snap_parse_state("exec rm", &state));
    g_assert_false(window_snap_parse_key("sideways", &key));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-snap-keyboard-steps/sequences", test_key_sequences},
    {"/window-snap-keyboard-steps/areas",     test_areas        },
    {"/window-snap-keyboard-steps/words",     test_words        },
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
