#ifndef STATUS_BAR_BLOCK_ENCODING_H
#define STATUS_BAR_BLOCK_ENCODING_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The status line protocol of i3bar and swaybar (see swaybar-protocol(7)):
 * a header, then an endless JSON array whose elements are the blocks
 * shown at one moment. Free every result with g_free().
 */

struct status_bar_block {
    const char *name;                  // reported back in click events
    const char *full_text;             // Pango markup when uses_markup
    const char *color;                 // "#rrggbb", or NULL for the bar's text color
    const char *background;            // "#rrggbb", or NULL for the bar's background
    const char *minimum_width_text;    // the block is at least as wide as this text; NULL for none
    int         separator_block_width; // pixels after the block
    bool        is_urgent;             // drawn in the bar's urgent colors
    bool        uses_markup;
};

/* The header line and the opening "[" of the endless array. */
char *status_bar_protocol_start(bool reports_clicks);

/* One element of the endless array, a line ending in ",\n". */
char *status_bar_blocks_encode(const struct status_bar_block *blocks, size_t block_count);

#endif
