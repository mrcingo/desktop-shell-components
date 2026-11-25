#include <string.h>

#include <gio/gio.h>

#include "notification-sound-player.h"

static const char *const sound_file_extensions[] = {".oga", ".ogg", ".wav"};

/* Sound names are file names in the theme; anything else could leave the theme's folder. */
static const char sound_name_characters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.";

struct notification_sound_player {
    char        **player_arguments;
    gint64        minimum_interval_us;
    gint64        last_start_time_us; // monotonic; 0 before the first sound
    GCancellable *cancellable;
    bool          is_playing;
};

struct notification_sound_player *
notification_sound_player_create(
    const char *player_command,
    int         minimum_interval_ms
)
{
    char **player_arguments = NULL;

    if (!g_shell_parse_argv(player_command, NULL, &player_arguments, NULL)) {
        return NULL;
    }

    struct notification_sound_player *player = g_new0(struct notification_sound_player, 1);

    *player = (struct notification_sound_player){
        .player_arguments    = player_arguments,
        .minimum_interval_us = (gint64)MAX(minimum_interval_ms, 0) * 1000,
        .cancellable         = g_cancellable_new(),
    };

    return player;
}

void
notification_sound_player_destroy(
    struct notification_sound_player *player
)
{
    if (player == NULL) {
        return;
    }

    g_cancellable_cancel(player->cancellable);
    g_object_unref(player->cancellable);
    g_strfreev(player->player_arguments);
    g_free(player);
}

static bool
is_existing_file(
    const char *path
)
{
    if (!g_path_is_absolute(path)) {
        return false;
    }

    return g_file_test(path, G_FILE_TEST_IS_REGULAR);
}

/* "file:///x.wav" and "/x.wav" name the same file. NULL for a URI that is no local file. */
static char *
path_from_file_hint(
    const char *sound_file
)
{
    if (g_str_has_prefix(sound_file, "file://")) {
        return g_filename_from_uri(sound_file, NULL, NULL);
    }

    return g_strdup(sound_file);
}

static char *
find_sound_file(
    const char *sound_file
)
{
    g_autofree char *path = path_from_file_hint(sound_file);

    if (path == NULL) {
        return NULL;
    }

    if (!is_existing_file(path)) {
        return NULL;
    }

    return g_steal_pointer(&path);
}

static char *
find_in_data_directory(
    const char *data_directory,
    const char *sound_name
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(sound_file_extensions); i++) {
        g_autofree char *file_name = g_strconcat(sound_name, sound_file_extensions[i], NULL);
        g_autofree char *path      = g_build_filename(data_directory, "sounds", "freedesktop", "stereo", file_name, NULL);

        if (is_existing_file(path)) {
            return g_steal_pointer(&path);
        }
    }

    return NULL;
}

static bool
is_safe_sound_name(
    const char *sound_name
)
{
    if (sound_name[0] == '\0') {
        return false;
    }

    if (sound_name[0] == '.') {
        return false;
    }

    return strspn(sound_name, sound_name_characters) == strlen(sound_name);
}

static char *
find_theme_sound(
    const char *sound_name
)
{
    const char *const *system_directories = g_get_system_data_dirs();
    char              *path               = find_in_data_directory(g_get_user_data_dir(), sound_name);

    for (size_t i = 0; system_directories[i] != NULL; i++) {
        if (path != NULL) {
            return path;
        }

        path = find_in_data_directory(system_directories[i], sound_name);
    }

    return path;
}

static char *
find_named_sound(
    const char *sound_name
)
{
    if (sound_name == NULL) {
        return NULL;
    }

    if (!is_safe_sound_name(sound_name)) {
        return NULL;
    }

    return find_theme_sound(sound_name);
}

/* The file hint wins over the name hint; NULL when neither gives an existing sound. */
static char *
find_hinted_sound(
    const char *sound_file,
    const char *sound_name
)
{
    if (sound_file == NULL) {
        return find_named_sound(sound_name);
    }

    char *path = find_sound_file(sound_file);

    if (path != NULL) {
        return path;
    }

    return find_named_sound(sound_name);
}

char *
notification_sound_find(
    const char *sound_file,
    const char *sound_name,
    const char *fallback_path
)
{
    char *path = find_hinted_sound(sound_file, sound_name);

    if (path != NULL) {
        return path;
    }

    return g_strdup(fallback_path);
}

/* The player's arguments with {volume} filled in and the sound appended. */
static char **
build_arguments(
    const struct notification_sound_player *player,
    const char                             *sound_path,
    int                                     volume_percent
)
{
    g_autofree char *volume = g_strdup_printf("%.2f", CLAMP(volume_percent, 0, 100) / 100.0);

    GStrvBuilder *arguments = g_strv_builder_new();

    for (size_t i = 0; player->player_arguments[i] != NULL; i++) {
        g_autoptr(GString) argument = g_string_new(player->player_arguments[i]);

        g_string_replace(argument, "{volume}", volume, 0);
        g_strv_builder_add(arguments, argument->str);
    }

    g_strv_builder_add(arguments, sound_path);

    return g_strv_builder_unref_to_strv(arguments);
}

static void
handle_player_finished(
    GObject      *subprocess,
    GAsyncResult *result,
    gpointer      user_data
)
{
    g_autoptr(GError) error = NULL;

    g_subprocess_wait_finish(G_SUBPROCESS(subprocess), result, &error);

    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        return; // the player object is gone
    }

    ((struct notification_sound_player *)user_data)->is_playing = false;
}

static bool
is_too_soon(
    const struct notification_sound_player *player,
    gint64                                  now_us
)
{
    if (player->last_start_time_us == 0) {
        return false;
    }

    return now_us - player->last_start_time_us < player->minimum_interval_us;
}

static bool
start_player(
    struct notification_sound_player *player,
    char                            **arguments
)
{
    g_autoptr(GError)      error      = NULL;
    g_autoptr(GSubprocess) subprocess = g_subprocess_newv(
        (const char *const *)arguments,
        G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &error
    );

    if (subprocess == NULL) {
        g_warning("Cannot play a notification sound with %s: %s", arguments[0], error->message);
        return false;
    }

    player->is_playing = true;

    g_subprocess_wait_async(subprocess, player->cancellable, handle_player_finished, player);

    return true;
}

bool
notification_sound_player_play(
    struct notification_sound_player *player,
    const char                       *sound_path,
    int                               volume_percent
)
{
    gint64 now_us = g_get_monotonic_time();

    if (player->is_playing) {
        return false;
    }

    if (is_too_soon(player, now_us)) {
        return false;
    }

    g_auto(GStrv) arguments = build_arguments(player, sound_path, volume_percent);

    player->last_start_time_us = now_us;

    return start_player(player, arguments);
}
