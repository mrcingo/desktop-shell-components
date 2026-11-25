#ifndef NOTIFICATION_SOUND_PLAYER_H
#define NOTIFICATION_SOUND_PLAYER_H

#include <stdbool.h>

/*
 * Plays notification sounds by starting a player program, such as
 * "pw-play --volume={volume}", with the sound file as its last argument:
 * {volume} becomes the volume as a fraction such as 0.70, no shell is
 * involved, and the main loop never waits for the player. One sound plays
 * at a time, and sounds asked for sooner than the minimum interval after
 * the last one are skipped, so a burst of notifications makes one sound.
 */

struct notification_sound_player;

/* player_command is split into arguments like a shell would, but never run by one. Returns NULL when it cannot be split. */
struct notification_sound_player *notification_sound_player_create(const char *player_command, int minimum_interval_ms);

void notification_sound_player_destroy(struct notification_sound_player *player);

/* volume_percent is 0 to 100. Returns whether a player was started. */
bool notification_sound_player_play(struct notification_sound_player *player, const char *sound_path, int volume_percent);

/*
 * The file to play: the "sound-file" hint when it is an existing file,
 * else the "sound-name" hint found in the freedesktop sound theme, else
 * fallback_path. Both hints may be NULL. Free the result with g_free().
 */
char *notification_sound_find(const char *sound_file, const char *sound_name, const char *fallback_path);

#endif
