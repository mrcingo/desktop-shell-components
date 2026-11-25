#include <glib.h>

#include "audio-volume-control.h"

static void
test_levels_convert_to_percent(void)
{
    g_assert_cmpint(audio_volume_percent_from_level(0), ==, 0);
    g_assert_cmpint(audio_volume_percent_from_level(audio_volume_level_from_percent(45)), ==, 45);
    g_assert_cmpint(audio_volume_percent_from_level(audio_volume_level_from_percent(150)), ==, 150);
    g_assert_cmpuint(audio_volume_level_from_percent(-5), ==, 0);
}

static void
ignore_volume_change(
    void                            *user_data,
    const struct audio_volume_state *state
)
{}

static const struct audio_volume_control_handler handler = {
    .handle_volume_changed = ignore_volume_change,
};

/* Without a sound server the control waits to reconnect; destroying it then must be clean. */
static void
test_missing_sound_server_is_survived(void)
{
    g_setenv("PULSE_SERVER", "unix:/nonexistent-sound-server-socket", TRUE);

    struct audio_volume_control *control = audio_volume_control_create(&handler, NULL);

    g_main_context_iteration(NULL, FALSE);
    g_assert_false(audio_volume_control_get_state(control)->is_connected);
    audio_volume_control_toggle_mute(control);
    audio_volume_control_change_volume(control, 5);
    audio_volume_control_destroy(control);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/audio-volume-control/conversion",     test_levels_convert_to_percent       },
    {"/audio-volume-control/missing-server", test_missing_sound_server_is_survived},
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
