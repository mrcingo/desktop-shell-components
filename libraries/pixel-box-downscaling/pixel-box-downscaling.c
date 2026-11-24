#include <stdbool.h>

#include "pixel-box-downscaling.h"

/* Source span [first, last) that target index maps onto; never empty. */
struct source_span {
    uint32_t first;
    uint32_t last;
};

static struct source_span
span_for_target(
    uint32_t target_index,
    uint32_t target_length,
    uint32_t source_length
)
{
    uint64_t first = (uint64_t)target_index * source_length / target_length;
    uint64_t last  = (uint64_t)(target_index + 1) * source_length / target_length;

    return (struct source_span){(uint32_t)first, (uint32_t)MAX(last, first + 1)};
}

static void
add_row_span(
    uint64_t           sums[PIXEL_BOX_BYTES_PER_PIXEL],
    const uint8_t     *row,
    struct source_span columns
)
{
    for (uint32_t column = columns.first; column < columns.last; column++) {
        for (int channel = 0; channel < PIXEL_BOX_BYTES_PER_PIXEL; channel++) {
            sums[channel] += row[(size_t)column * PIXEL_BOX_BYTES_PER_PIXEL + channel];
        }
    }
}

/* Rounded average, so a flat colour stays exactly the same colour. */
static void
store_average(
    uint8_t       *target_pixel,
    const uint64_t sums[PIXEL_BOX_BYTES_PER_PIXEL],
    uint64_t       pixel_count
)
{
    for (int channel = 0; channel < PIXEL_BOX_BYTES_PER_PIXEL; channel++) {
        target_pixel[channel] = (uint8_t)((sums[channel] + pixel_count / 2) / pixel_count);
    }
}

static void
average_box(
    const struct pixel_box_image *source,
    struct source_span            rows,
    struct source_span            columns,
    uint8_t                      *target_pixel
)
{
    uint64_t sums[PIXEL_BOX_BYTES_PER_PIXEL] = {0};
    uint64_t pixel_count                     = (uint64_t)(rows.last - rows.first) * (columns.last - columns.first);

    for (uint32_t row = rows.first; row < rows.last; row++) {
        add_row_span(sums, source->pixels + (size_t)row * source->row_stride, columns);
    }

    store_average(target_pixel, sums, pixel_count);
}

static void
downscale_target_row(
    const struct pixel_box_image *source,
    uint32_t                      target_row,
    uint32_t                      target_width,
    uint32_t                      target_height,
    uint8_t                      *target_row_pixels
)
{
    struct source_span rows = span_for_target(target_row, target_height, source->height);

    for (uint32_t target_column = 0; target_column < target_width; target_column++) {
        struct source_span columns = span_for_target(target_column, target_width, source->width);

        average_box(source, rows, columns, target_row_pixels + (size_t)target_column * PIXEL_BOX_BYTES_PER_PIXEL);
    }
}

GBytes *
pixel_box_downscale(
    const struct pixel_box_image *source,
    uint32_t                      target_width,
    uint32_t                      target_height
)
{
    g_return_val_if_fail(target_width > 0 && target_height > 0, NULL);

    size_t   target_row_size = (size_t)target_width * PIXEL_BOX_BYTES_PER_PIXEL;
    uint8_t *target_pixels   = g_malloc(target_row_size * target_height);

    for (uint32_t target_row = 0; target_row < target_height; target_row++) {
        downscale_target_row(source, target_row, target_width, target_height, target_pixels + target_row * target_row_size);
    }

    return g_bytes_new_take(target_pixels, target_row_size * target_height);
}

/* Scales by the smaller of the two ratios, computed in 64 bits so large sizes cannot overflow. */
void
pixel_box_fit_size(
    uint32_t  source_width,
    uint32_t  source_height,
    uint32_t  maximum_width,
    uint32_t  maximum_height,
    uint32_t *fitted_width,
    uint32_t *fitted_height
)
{
    bool width_limits = (uint64_t)source_width * maximum_height >= (uint64_t)source_height * maximum_width;

    *fitted_width  = MIN(source_width, maximum_width);
    *fitted_height = MIN(source_height, maximum_height);

    if (width_limits) {
        *fitted_height = (uint32_t)MAX(1, (uint64_t)source_height * *fitted_width / MAX(source_width, 1));
    } else {
        *fitted_width = (uint32_t)MAX(1, (uint64_t)source_width * *fitted_height / MAX(source_height, 1));
    }
}
