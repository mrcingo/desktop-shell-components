#ifndef WAYLAND_PIXEL_FORMAT_CONVERSION_H
#define WAYLAND_PIXEL_FORMAT_CONVERSION_H

#include <stdbool.h>
#include <stdint.h>

#include <gdk/gdk.h>

/*
 * The 32-bit wl_shm pixel formats that GDK can turn into textures without
 * conversion. wl_shm formats name channels from the most significant byte
 * of a little-endian word, so ARGB8888 is stored as B, G, R, A in memory.
 */

enum {
    WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK = -1,
    WAYLAND_PIXEL_FORMAT_BYTES_PER_PIXEL  = 4,
};

/* Formats with alpha rank first, so translucent windows keep it. */
int wayland_pixel_format_preference_rank(uint32_t wayland_format);

bool wayland_pixel_format_convert_to_gdk(uint32_t wayland_format, GdkMemoryFormat *gdk_format);

#endif
