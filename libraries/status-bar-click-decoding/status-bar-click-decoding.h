#ifndef STATUS_BAR_CLICK_DECODING_H
#define STATUS_BAR_CLICK_DECODING_H

#include <stdbool.h>

/*
 * Clicks on status blocks, as swaybar writes them to the status command's
 * standard input: an endless JSON array with one click object per line
 * (see swaybar-protocol(7)).
 */

enum status_bar_mouse_button {
    STATUS_BAR_BUTTON_LEFT        = 1,
    STATUS_BAR_BUTTON_MIDDLE      = 2,
    STATUS_BAR_BUTTON_RIGHT       = 3,
    STATUS_BAR_BUTTON_SCROLL_UP   = 4,
    STATUS_BAR_BUTTON_SCROLL_DOWN = 5,
};

struct status_bar_click {
    char *block_name; // the name of the clicked block
    int   button;     // X11 button number, see enum status_bar_mouse_button
};

/*
 * Reads one line. The opening "[" and lines that are not a click with a
 * block name and button (the protocol puts a "," before every later click)
 * return false.
 */
bool status_bar_click_decode(const char *line, struct status_bar_click *click);

void status_bar_click_clear(struct status_bar_click *click);

#endif
