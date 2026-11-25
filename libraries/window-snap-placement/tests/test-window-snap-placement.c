#include <glib.h>

#include "window-snap-placement.h"

static char *no_marks[] = {NULL};

/* Two tiled windows side by side under a 24 pixel bar, and a floating one being dragged. */
static const struct compositor_window zed = {
    .container_id     = 1,
    .application_name = "Zed",
    .content_x        = 0,
    .content_y        = 48,
    .content_width    = 960,
    .content_height   = 1152,
    .title_bar_height = 24,
    .border_width     = 2,
    .parent_layout    = COMPOSITOR_CONTAINER_LAYOUT_SIDE_BY_SIDE,
    .is_visible       = true,
    .marks            = no_marks,
};

static const struct compositor_window chrome = {
    .container_id     = 2,
    .application_name = "Chrome",
    .content_x        = 960,
    .content_y        = 48,
    .content_width    = 960,
    .content_height   = 1152,
    .title_bar_height = 24,
    .border_width     = 2,
    .parent_layout    = COMPOSITOR_CONTAINER_LAYOUT_SIDE_BY_SIDE,
    .is_visible       = true,
    .marks            = no_marks,
};

static const struct compositor_window floater = {
    .container_id     = 9,
    .application_name = "foot",
    .content_x        = 600,
    .content_y        = 424,
    .content_width    = 600,
    .content_height   = 376,
    .title_bar_height = 24,
    .border_width     = 6,
    .is_floating      = true,
    .is_visible       = true,
    .marks            = no_marks,
};

static const struct window_snap_border_widths border_widths = {.floating_border_width = 6, .tiled_border_width = 2};

static GPtrArray *
create_window_list(
    const struct compositor_window *const *windows,
    size_t                                 window_count
)
{
    GPtrArray *list = g_ptr_array_new();

    for (size_t i = 0; i < window_count; i++) {
        g_ptr_array_add(list, (gpointer)windows[i]);
    }

    return list;
}

static struct window_snap_scene
create_scene(
    const struct compositor_window *dragged_window,
    GPtrArray                      *windows
)
{
    return (struct window_snap_scene){
        .output_area    = {0, 0,  1920, 1200},
        .workspace_area = {0, 24, 1920, 1176},
        .dragged_window = dragged_window,
        .windows        = windows,
        .edge_zone_size = 24,
        .drag_start_x   = 700,
        .drag_start_y   = 418,
    };
}

struct expected_placement {
    int                         pointer_x;
    int                         pointer_y;
    enum window_snap_action     action;
    const char                 *label;
    struct compositor_rectangle preview;
};

static const struct expected_placement placements_with_tiled_windows[] = {
    {5,    600,  WINDOW_SNAP_FILLS_SCREEN_AREA, "Left half",            {0, 24, 960, 1176}   },
    {1915, 600,  WINDOW_SNAP_FILLS_SCREEN_AREA, "Right half",           {960, 24, 960, 1176} },
    {5,    30,   WINDOW_SNAP_FILLS_SCREEN_AREA, "Top left quarter",     {0, 24, 960, 588}    },
    {1915, 1195, WINDOW_SNAP_FILLS_SCREEN_AREA, "Bottom right quarter", {960, 612, 960, 588} },
    {900,  5,    WINDOW_SNAP_FILLS_SCREEN_AREA, "Maximize",             {0, 24, 1920, 1176}  },
    {50,   5,    WINDOW_SNAP_FILLS_SCREEN_AREA, "Top left quarter",     {0, 24, 960, 588}    },
    {900,  1195, WINDOW_SNAP_FILLS_SCREEN_AREA, "Bottom half",          {0, 612, 1920, 588}  },
    {1800, 600,  WINDOW_SNAP_TILES_BESIDE,      "Right of Chrome",      {1440, 24, 480, 1176}},
    {100,  600,  WINDOW_SNAP_TILES_BESIDE,      "Left of Zed",          {0, 24, 480, 1176}   },
    {1400, 300,  WINDOW_SNAP_TILES_BESIDE,      "Above Chrome",         {960, 24, 960, 588}  },
    {1400, 1100, WINDOW_SNAP_TILES_BESIDE,      "Below Chrome",         {960, 612, 960, 588} },
    {1400, 30,   WINDOW_SNAP_TABS_WITH,         "Tab with Chrome",      {960, 24, 960, 1176} },
};

static void
assert_placement(
    const struct window_snap_scene  *scene,
    const struct expected_placement *expected
)
{
    struct window_snap_placement placement;

    window_snap_placement_find(scene, expected->pointer_x, expected->pointer_y, &placement);
    g_assert_cmpint(placement.action, ==, expected->action);
    g_assert_cmpstr(placement.label, ==, expected->label);
    g_assert_cmpmem(&placement.preview, sizeof(placement.preview), &expected->preview, sizeof(expected->preview));
    window_snap_placement_clear(&placement);
}

static void
test_zones_with_tiled_windows(void)
{
    const struct compositor_window *const windows[] = {&zed, &chrome, &floater};

    g_autoptr(GPtrArray) list = create_window_list(windows, G_N_ELEMENTS(windows));

    struct window_snap_scene scene = create_scene(&floater, list);

    for (size_t i = 0; i < G_N_ELEMENTS(placements_with_tiled_windows); i++) {
        assert_placement(&scene, &placements_with_tiled_windows[i]);
    }
}

/* On an empty workspace a floating window follows the pointer by the drag's offset. */
static void
test_floating_window_moves_freely(void)
{
    const struct compositor_window *const windows[] = {&floater};

    g_autoptr(GPtrArray) list = create_window_list(windows, G_N_ELEMENTS(windows));

    struct window_snap_scene        scene    = create_scene(&floater, list);
    const struct expected_placement expected = {
        800,
        518,
        WINDOW_SNAP_MOVES_FREELY,
        "Move",
        {700, 500, 600, 400}
    };

    assert_placement(&scene, &expected);
}

/* A tiled window over itself stays where it is. */
static void
test_tiled_window_over_itself_does_nothing(void)
{
    const struct compositor_window *const windows[] = {&zed, &chrome};

    g_autoptr(GPtrArray) list = create_window_list(windows, G_N_ELEMENTS(windows));

    struct window_snap_scene        scene    = create_scene(&zed, list);
    const struct expected_placement expected = {
        300,
        600,
        WINDOW_SNAP_DOES_NOTHING,
        NULL,
        {0, 0, 0, 0}
    };

    assert_placement(&scene, &expected);
}

/* Hidden tabs are never targets. */
static void
test_hidden_windows_are_not_targets(void)
{
    struct compositor_window              hidden_chrome = chrome;
    const struct compositor_window *const windows[]     = {&zed, &hidden_chrome};
    const struct expected_placement       expected      = {
        1400,
        600,
        WINDOW_SNAP_MOVES_FREELY,
        "Move",
        {1300, 582, 600, 400}
    };

    hidden_chrome.is_visible = false;

    g_autoptr(GPtrArray) list = create_window_list(windows, G_N_ELEMENTS(windows));

    struct window_snap_scene scene = create_scene(&floater, list);

    assert_placement(&scene, &expected);
}

struct expected_command {
    int         pointer_x;
    int         pointer_y;
    const char *command;
};

static const struct expected_command commands[] = {
    {5,
     600, "[con_id=9] floating enable, border normal 6, resize set width 960 px height 1176 px, move absolute position 0 24"},
    {1800,
     600, "[con_id=2] mark --add _window-snap-drop; [con_id=9] floating disable, border normal 2, move container to mark "
     "_window-snap-drop; [con_id=2] unmark _window-snap-drop"                                                      },
    {100,
     600, "[con_id=1] mark --add _window-snap-drop; [con_id=9] floating disable, border normal 2, move container to mark "
     "_window-snap-drop, move left; [con_id=1] unmark _window-snap-drop"                                           },
    {1400,
     300, "[con_id=2] split v; [con_id=2] mark --add _window-snap-drop; [con_id=9] floating disable, border normal 2, move "
     "container to mark _window-snap-drop, move up; [con_id=2] unmark _window-snap-drop"                           },
    {1400,
     30,  "[con_id=2] split v, layout tabbed; [con_id=2] mark --add _window-snap-drop; [con_id=9] floating disable, border normal "
     "2, move container to mark _window-snap-drop; [con_id=2] unmark _window-snap-drop"                             },
};

static void
assert_command(
    const struct window_snap_scene *scene,
    const struct expected_command  *expected
)
{
    struct window_snap_placement placement;

    window_snap_placement_find(scene, expected->pointer_x, expected->pointer_y, &placement);

    g_autofree char *command = window_snap_placement_build_command(&placement, scene->dragged_window, &border_widths);

    g_assert_cmpstr(command, ==, expected->command);
    window_snap_placement_clear(&placement);
}

static void
test_commands_are_built(void)
{
    const struct compositor_window *const windows[] = {&zed, &chrome, &floater};

    g_autoptr(GPtrArray) list = create_window_list(windows, G_N_ELEMENTS(windows));

    struct window_snap_scene scene = create_scene(&floater, list);

    for (size_t i = 0; i < G_N_ELEMENTS(commands); i++) {
        assert_command(&scene, &commands[i]);
    }
}

/* Joining a tabbed container needs no new one, and nothing happening gives no command. */
static void
test_tabbed_target_and_nothing(void)
{
    struct compositor_window     tabbed_chrome = chrome;
    struct window_snap_placement nothing       = {.action = WINDOW_SNAP_DOES_NOTHING};
    struct window_snap_placement as_tab        = {.action = WINDOW_SNAP_TABS_WITH, .target = &tabbed_chrome};

    tabbed_chrome.parent_layout = COMPOSITOR_CONTAINER_LAYOUT_TABBED;

    g_autofree char *tab_command = window_snap_placement_build_command(&as_tab, &floater, &border_widths);

    g_assert_true(g_str_has_prefix(tab_command, "[con_id=2] mark --add"));
    g_assert_null(window_snap_placement_build_command(&nothing, &floater, &border_widths));
}

static void
find_restored_placement(
    struct window_snap_placement *placement
)
{
    const struct compositor_window *const windows[] = {&floater};

    g_autoptr(GPtrArray) list = create_window_list(windows, G_N_ELEMENTS(windows));

    struct window_snap_scene scene = create_scene(&floater, list);

    scene.restored_width  = 300;
    scene.restored_height = 200;

    window_snap_placement_find(&scene, 1000, 700, placement);
}

/* A maximized window dragged away gets its old size back, under the pointer. */
static void
test_dragging_restores_size(void)
{
    struct window_snap_placement placement;

    find_restored_placement(&placement);

    g_autofree char *command = window_snap_placement_build_command(&placement, &floater, &border_widths);

    g_assert_cmpint(placement.preview.x * 10000 + placement.preview.y, ==, 950 * 10000 + 682);
    g_assert_cmpstr(
        command,
        ==,
        "[con_id=9] floating enable, border normal 6, resize set width 300 px height 200 px, move absolute position 950 682"
    );
    window_snap_placement_clear(&placement);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-snap-placement/zones",           test_zones_with_tiled_windows             },
    {"/window-snap-placement/free-move",       test_floating_window_moves_freely         },
    {"/window-snap-placement/over-itself",     test_tiled_window_over_itself_does_nothing},
    {"/window-snap-placement/hidden-targets",  test_hidden_windows_are_not_targets       },
    {"/window-snap-placement/commands",        test_commands_are_built                   },
    {"/window-snap-placement/tabbed-target",   test_tabbed_target_and_nothing            },
    {"/window-snap-placement/restore-on-drag", test_dragging_restores_size               },
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
