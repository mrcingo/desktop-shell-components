#ifndef WINDOW_SCREENSHOT_FILE_NAMING_H
#define WINDOW_SCREENSHOT_FILE_NAMING_H

#include <glib.h>

/*
 * Names and places for window screenshots, as Windows names them:
 * "Screenshot 2026-11-25 113045 - Slack.png" in Pictures/Screenshots.
 */

enum {
    WINDOW_SCREENSHOT_LONGEST_APPLICATION_NAME = 40,
};

/*
 * The application name reduced to letters, digits, spaces, dots, dashes and
 * underscores (others become underscores), without leading or trailing
 * spaces, dots or underscores, at most WINDOW_SCREENSHOT_LONGEST_APPLICATION_NAME
 * characters. May be empty. Free with g_free().
 */
char *window_screenshot_safe_application_name(const char *application_name);

/*
 * The file name for attempt 1, 2, 3...: attempt 1 has no number, later ones
 * end in "-2", "-3", so an existing file is never replaced. Free with g_free().
 */
char *window_screenshot_file_name(GDateTime *taken_at, const char *application_name, unsigned attempt);

/*
 * The folder to save in: configured_folder when set ("~/" means the home
 * folder; a relative path is inside it), else pictures_folder/Screenshots,
 * else home/Pictures/Screenshots. pictures_folder may be NULL. Free with g_free().
 */
char *window_screenshot_resolve_folder(const char *configured_folder, const char *pictures_folder, const char *home_folder);

#endif
