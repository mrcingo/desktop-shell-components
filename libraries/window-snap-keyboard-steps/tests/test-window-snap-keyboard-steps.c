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

/* Win+Left on a window against the left edge crosses to the monitor on the left, as its right side. */
static void
test_monitor_crossings(void)
{
    enum window_snap_state crossed = WINDOW_SNAP_STATE_FREE;

    g_assert_true(window_snap_state_crosses_monitors(WINDOW_SNAP_STATE_LEFT_HALF, WINDOW_SNAP_KEY_LEFT, &crossed));
    g_assert_cmpint(crossed, ==, WINDOW_SNAP_STATE_RIGHT_HALF);
    g_assert_true(window_snap_state_crosses_monitors(WINDOW_SNAP_STATE_TOP_RIGHT_QUARTER, WINDOW_SNAP_KEY_RIGHT, &crossed));
    g_assert_cmpint(crossed, ==, WINDOW_SNAP_STATE_TOP_LEFT_QUARTER);
    g_assert_false(window_snap_state_crosses_monitors(WINDOW_SNAP_STATE_RIGHT_HALF, WINDOW_SNAP_KEY_LEFT, &crossed));
    g_assert_false(window_snap_state_crosses_monitors(WINDOW_SNAP_STATE_MAXIMIZED, WINDOW_SNAP_KEY_LEFT, &crossed));
}

static struct compositor_output *
create_output(
    int x,
    int y,
    int width,
    int height
)
{
    struct compositor_output *output = g_new0(struct compositor_output, 1);

    output->output_area = (struct compositor_rectangle){x, y, width, height};

    return output;
}

/* Three monitors in a row, the middle one taller; a fourth far below shares no rows with them. */
static GPtrArray *
create_monitor_row(void)
{
    GPtrArray *outputs = g_ptr_array_new_with_free_func(g_free);

    g_ptr_array_add(outputs, create_output(0, 0, 1920, 1080));
    g_ptr_array_add(outputs, create_output(1920, 0, 2560, 1440));
    g_ptr_array_add(outputs, create_output(4480, 200, 1280, 1024));
    g_ptr_array_add(outputs, create_output(0, 3000, 1920, 1080));

    return outputs;
}

/* Up and Down never lead to another monitor. */
static void
test_no_vertical_neighbours(void)
{
    GPtrArray                          *outputs = create_monitor_row();
    const struct compositor_window_tree tree    = {.outputs = outputs};

    g_assert_null(window_snap_find_neighbour_output(&tree, g_ptr_array_index(outputs, 1), WINDOW_SNAP_KEY_UP));
    g_ptr_array_unref(outputs);
}

static void
test_neighbour_monitors(void)
{
    GPtrArray                          *outputs = create_monitor_row();
    const struct compositor_window_tree tree    = {.outputs = outputs};
    const struct compositor_output     *middle  = g_ptr_array_index(outputs, 1);

    g_assert_true(window_snap_find_neighbour_output(&tree, middle, WINDOW_SNAP_KEY_LEFT) == g_ptr_array_index(outputs, 0));
    g_assert_true(window_snap_find_neighbour_output(&tree, middle, WINDOW_SNAP_KEY_RIGHT) == g_ptr_array_index(outputs, 2));
    g_assert_null(window_snap_find_neighbour_output(&tree, g_ptr_array_index(outputs, 0), WINDOW_SNAP_KEY_LEFT));
    g_ptr_array_unref(outputs);
}

static const struct compositor_rectangle big_area         = {0, 0, 2000, 1000};
static const struct compositor_rectangle small_area       = {3000, 100, 1000, 500};
static const struct compositor_rectangle translated_frame = {1000, 250, 400, 300};
static const struct compositor_rectangle huge_frame       = {0, 0, 1800, 900};

/* Same size when it fits, same relative place, never outside. */
static void
test_frame_translation(void)
{
    struct compositor_rectangle moved  = window_snap_translate_frame(&translated_frame, &big_area, &small_area);
    struct compositor_rectangle shrunk = window_snap_translate_frame(&huge_frame, &big_area, &small_area);

    g_assert_cmpint(moved.x * 10000 + moved.width, ==, 3375 * 10000 + 400);
    g_assert_cmpint(shrunk.height * 10000 + shrunk.x + shrunk.width, ==, 500 * 10000 + 4000);
}

static void
test_snap_corners(void)
{
    const struct compositor_rectangle workspace  = {0, 24, 1920, 1176};
    const struct compositor_rectangle at_right   = {961, 25, 960, 1176};
    const struct compositor_rectangle at_bottom  = {0, 612, 960, 588};
    const struct compositor_rectangle moved_away = {300, 200, 960, 600};

    g_assert_true(window_snap_frame_is_at_snap_corner(&at_right, &workspace));
    g_assert_true(window_snap_frame_is_at_snap_corner(&at_bottom, &workspace));
    g_assert_false(window_snap_frame_is_at_snap_corner(&moved_away, &workspace));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-snap-keyboard-steps/sequences",              test_key_sequences         },
    {"/window-snap-keyboard-steps/areas",                  test_areas                 },
    {"/window-snap-keyboard-steps/words",                  test_words                 },
    {"/window-snap-keyboard-steps/crossings",              test_monitor_crossings     },
    {"/window-snap-keyboard-steps/neighbours",             test_neighbour_monitors    },
    {"/window-snap-keyboard-steps/no-vertical-neighbours", test_no_vertical_neighbours},
    {"/window-snap-keyboard-steps/translation",            test_frame_translation     },
    {"/window-snap-keyboard-steps/corners",                test_snap_corners          },
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
