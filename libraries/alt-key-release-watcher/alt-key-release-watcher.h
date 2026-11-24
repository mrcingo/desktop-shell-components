#ifndef ALT_KEY_RELEASE_WATCHER_H
#define ALT_KEY_RELEASE_WATCHER_H

#include <gtk/gtk.h>

/*
 * Reports when the user lets go of Alt while a window has the keyboard, as
 * an Alt+Tab switcher needs. Three signals are watched, because each one
 * alone can miss the release:
 *
 *   1. a key release of Alt in the window,
 *   2. the keyboard's modifier state losing Alt while the window is active,
 *   3. Alt already being up when the window becomes active, which happens
 *      when Alt+Tab is tapped faster than the window appears.
 *
 * The callback may run more than once per press; the receiver ignores
 * releases it does not expect.
 */

typedef void (*alt_key_release_callback)(void *user_data);

struct alt_key_release_watcher;

struct alt_key_release_watcher *
alt_key_release_watcher_create(GtkWindow *window, alt_key_release_callback callback, void *user_data);

void alt_key_release_watcher_destroy(struct alt_key_release_watcher *watcher);

#endif
