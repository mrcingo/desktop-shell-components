#ifndef DISPLAY_BACKLIGHT_LEVEL_H
#define DISPLAY_BACKLIGHT_LEVEL_H

#include <stdbool.h>

/*
 * The screen's backlight, from the kernel's backlight class
 * (/sys/class/backlight/DEVICE): its level as a percentage, and changing it
 * when the file is writable without extra privileges.
 */

struct display_backlight;

/* The first backlight device under backlight_class_directory, or NULL if there is none. */
struct display_backlight *display_backlight_find(const char *backlight_class_directory);

void display_backlight_destroy(struct display_backlight *backlight);

/* 0 to 100, or -1 if the level cannot be read. */
int display_backlight_read_percent(const struct display_backlight *backlight);

bool display_backlight_is_writable(const struct display_backlight *backlight);

/* Never turns the backlight fully off: the lowest level set is 1. */
bool display_backlight_set_percent(const struct display_backlight *backlight, int percent);

#endif
