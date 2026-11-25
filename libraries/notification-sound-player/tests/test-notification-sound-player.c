#include <glib/gstdio.h>

#include "notification-sound-player.h"

static char *
create_temporary_sound(void)
{
    char *path = NULL;
    int   file = g_file_open_tmp("notification-sound-XXXXXX.wav", &path, NULL);

    g_close(file, NULL);

    return path;
}

static void
assert_found(
    const char *sound_file,
    const char *sound_name,
    const char *expected_path
)
{
    g_autofree char *found = notification_sound_find(sound_file, sound_name, "/fallback.wav");

    g_assert_cmpstr(found, ==, expected_path);
}

/* Missing files, relative paths and names that could leave the theme fall back. */
static void
test_sounds_are_found(void)
{
    g_autofree char *existing = create_temporary_sound();
    g_autofree char *uri      = g_filename_to_uri(existing, NULL, NULL);

    assert_found(uri, NULL, existing);
    assert_found("/no/such/file.wav", "../../etc/passwd", "/fallback.wav");
    assert_found("relative.wav", NULL, "/fallback.wav");
    g_unlink(existing);
}

/* "sh -c" stands in for the player and writes its arguments where the test reads them. */
static char *
create_recording_command(
    const char *record_path
)
{
    return g_strdup_printf("sh -c 'echo \"$0 $1\" >> %s' {volume}", record_path);
}

static void
wait_for_player(
    const char *record_path
)
{
    for (int i = 0; i < 200 && !g_file_test(record_path, G_FILE_TEST_EXISTS); i++) {
        g_main_context_iteration(NULL, FALSE);
        g_usleep(10000);
    }

    for (int i = 0; i < 20; i++) {
        g_main_context_iteration(NULL, FALSE);
        g_usleep(10000);
    }
}

static struct notification_sound_player *
create_recording_player(
    const char *record_path
)
{
    g_autofree char *command = create_recording_command(record_path);

    g_unlink(record_path);

    return notification_sound_player_create(command, 60000);
}

static void
assert_recorded(
    const char *record_path,
    const char *expected_text
)
{
    g_autofree char *recorded = NULL;

    g_assert_true(g_file_get_contents(record_path, &recorded, NULL, NULL));
    g_assert_cmpstr(recorded, ==, expected_text);
}

/* The first sound plays with its volume; one asked for at once after it is skipped. */
static void
test_sounds_play_with_volume_and_minimum_interval(void)
{
    g_autofree char *record_path = g_build_filename(g_get_tmp_dir(), "notification-sound-player-test.txt", NULL);

    struct notification_sound_player *player = create_recording_player(record_path);

    g_assert_true(notification_sound_player_play(player, "/chime.wav", 70));
    wait_for_player(record_path);
    g_assert_false(notification_sound_player_play(player, "/chime.wav", 70));
    assert_recorded(record_path, "0.70 /chime.wav\n");
    notification_sound_player_destroy(player);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/notification-sound-player/find", test_sounds_are_found                            },
    {"/notification-sound-player/play", test_sounds_play_with_volume_and_minimum_interval},
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
