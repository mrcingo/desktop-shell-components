#ifndef PIXEL_BOX_DOWNSCALING_H
#define PIXEL_BOX_DOWNSCALING_H

#include <stddef.h>
#include <stdint.h>

#include <glib.h>

/*
 * Shrinks an image of 4-byte pixels by averaging every block of source
 * pixels that maps onto one target pixel (a box filter). Each byte is
 * averaged on its own, so the result is correct for any channel order and
 * for premultiplied alpha, and keeps the source's pixel format.
 *
 * One pass reads the source row by row, so a large window capture costs
 * a single sequential sweep through memory.
 */

enum {
    PIXEL_BOX_BYTES_PER_PIXEL = 4,
};

struct pixel_box_image {
    const uint8_t *pixels;
    uint32_t       width;
    uint32_t       height;
    size_t         row_stride; // bytes from one row to the next
};

/* The largest size that fits in maximum_width x maximum_height and keeps the aspect ratio, never upscaled. */
void pixel_box_fit_size(
    uint32_t  source_width,
    uint32_t  source_height,
    uint32_t  maximum_width,
    uint32_t  maximum_height,
    uint32_t *fitted_width,
    uint32_t *fitted_height
);

/* Returns target_width * target_height pixels, rows packed without padding. */
GBytes *pixel_box_downscale(const struct pixel_box_image *source, uint32_t target_width, uint32_t target_height);

#endif
