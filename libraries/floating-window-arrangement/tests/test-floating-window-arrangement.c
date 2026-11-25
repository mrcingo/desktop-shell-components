#include "floating-window-arrangement.h"

static const struct compositor_rectangle workspace_area = {0, 24, 1920, 1176};

static GArray *
create_frames(
    const struct compositor_rectangle *frames,
    guint                              frame_count
)
{
    GArray *array = g_array_new(FALSE, FALSE, sizeof(struct compositor_rectangle));

    g_array_append_vals(array, frames, frame_count);

    return array;
}

/* A window bigger than the screen is shrunk to it and centred; a smaller one is left alone. */
static void
test_fitting(void)
{
    const struct compositor_rectangle big   = {-500, -24, 3000, 2500};
    const struct compositor_rectangle small = {100, 100, 800, 600};
    struct compositor_rectangle       fitted;

    g_assert_true(floating_window_arrangement_fit(&big, &workspace_area, &fitted));
    g_assert_cmpint(fitted.x * 10000 + fitted.y, ==, 0 * 10000 + 24);
    g_assert_cmpint(fitted.width * 10000 + fitted.height, ==, 1920 * 10000 + 1176);
    g_assert_false(floating_window_arrangement_fit(&small, &workspace_area, &fitted));
}

/* Two windows already at the centre: the third steps past both corners. */
static void
test_cascading(void)
{
    const struct compositor_rectangle others[] = {
        {560, 312, 800, 600},
        {592, 344, 800, 600}
    };
    const struct compositor_rectangle new_frame = {560, 312, 800, 600};

    g_autoptr(GArray) other_frames = create_frames(others, G_N_ELEMENTS(others));

    struct compositor_rectangle cascaded;

    g_assert_true(floating_window_arrangement_cascade(&new_frame, &workspace_area, other_frames, 32, &cascaded));
    g_assert_cmpint(cascaded.x * 10000 + cascaded.y, ==, 624 * 10000 + 376);
}

static const struct compositor_rectangle corner_window      = {1100, 580, 800, 600};
static const struct compositor_rectangle free_corner_window = {10, 30, 800, 600};

/* Past the bottom right it starts again at the top left; a free corner needs no cascade. */
static void
test_cascade_wraps(void)
{
    g_autoptr(GArray) other_frames = create_frames(&corner_window, 1);

    struct compositor_rectangle cascaded;

    g_assert_true(floating_window_arrangement_cascade(&corner_window, &workspace_area, other_frames, 32, &cascaded));
    g_assert_cmpint(cascaded.x * 10000 + cascaded.y, ==, 0 * 10000 + 24);
    g_assert_false(floating_window_arrangement_cascade(&free_corner_window, &workspace_area, other_frames, 32, &cascaded));
}

static const struct compositor_rectangle two_monitor_areas[] = {
    {0,    24, 1920, 1176},
    {1920, 59, 1280, 965 },
};
static const struct compositor_rectangle above_screen  = {300, -2000, 800, 600};
static const struct compositor_rectangle beside_screen = {3150, 300, 800, 600};

/* Above the screen, or right of every monitor: brought fully onto the nearest one. */
static void
test_bringing_back(void)
{
    g_autoptr(GArray) workspace_areas = create_frames(two_monitor_areas, G_N_ELEMENTS(two_monitor_areas));

    struct compositor_rectangle above;
    struct compositor_rectangle beside;

    g_assert_true(floating_window_arrangement_bring_back(&above_screen, 24, workspace_areas, &above));
    g_assert_true(floating_window_arrangement_bring_back(&beside_screen, 24, workspace_areas, &beside));
    g_assert_cmpint(above.x * 10000 + above.y, ==, 300 * 10000 + 24);
    g_assert_cmpint(beside.x + beside.width, ==, 3200);
}

/* A title bar with enough of it on some monitor is left alone, also across two monitors. */
static void
test_reachable_title_bar_stays(void)
{
    const struct compositor_rectangle areas[] = {
        workspace_area,
        {1920, 59, 1280, 965}
    };
    const struct compositor_rectangle straddle = {1800, 300, 800, 600};
    const struct compositor_rectangle mostly   = {-700, 300, 800, 600};

    g_autoptr(GArray) workspace_areas = create_frames(areas, G_N_ELEMENTS(areas));

    struct compositor_rectangle brought_back;

    g_assert_false(floating_window_arrangement_bring_back(&straddle, 24, workspace_areas, &brought_back));
    g_assert_false(floating_window_arrangement_bring_back(&mostly, 24, workspace_areas, &brought_back));
}

static const struct compositor_rectangle partly_on_screen = {1346, 865, 704, 526};
static const struct compositor_rectangle fully_on_screen  = {100, 100, 704, 526};

/* After a monitor went away: partly off screen is not enough, the whole window comes onto a monitor. */
static void
test_bringing_fully_on_screen(void)
{
    g_autoptr(GArray) workspace_areas = create_frames(&workspace_area, 1);

    struct compositor_rectangle brought_back;

    g_assert_true(floating_window_arrangement_bring_fully_on_screen(&partly_on_screen, workspace_areas, &brought_back));
    g_assert_cmpint(brought_back.x * 10000 + brought_back.y, ==, (1920 - 704) * 10000 + (1200 - 526));
    g_assert_false(floating_window_arrangement_bring_fully_on_screen(&fully_on_screen, workspace_areas, &brought_back));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/floating-window-arrangement/fitting",   test_fitting                  },
    {"/floating-window-arrangement/cascading", test_cascading                },
    {"/floating-window-arrangement/wrapping",  test_cascade_wraps            },
    {"/floating-window-arrangement/back",      test_bringing_back            },
    {"/floating-window-arrangement/reachable", test_reachable_title_bar_stays},
    {"/floating-window-arrangement/fully",     test_bringing_fully_on_screen },
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
