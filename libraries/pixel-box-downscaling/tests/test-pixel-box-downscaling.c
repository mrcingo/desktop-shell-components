#include <string.h>

#include "pixel-box-downscaling.h"

static void
fill_flat_colour(
    uint8_t      *pixels,
    size_t        pixel_count,
    const uint8_t colour[PIXEL_BOX_BYTES_PER_PIXEL]
)
{
    for (size_t i = 0; i < pixel_count; i++) {
        memcpy(pixels + i * PIXEL_BOX_BYTES_PER_PIXEL, colour, PIXEL_BOX_BYTES_PER_PIXEL);
    }
}

/* A 7x5 image of one colour, shrunk to 3x2. */
static GBytes *
downscale_flat_image(
    const uint8_t colour[PIXEL_BOX_BYTES_PER_PIXEL]
)
{
    uint8_t                pixels[7 * 5 * PIXEL_BOX_BYTES_PER_PIXEL];
    struct pixel_box_image source = {pixels, 7, 5, 7 * PIXEL_BOX_BYTES_PER_PIXEL};

    fill_flat_colour(pixels, 7 * 5, colour);

    return pixel_box_downscale(&source, 3, 2);
}

static void
test_flat_colour_stays_exact(void)
{
    static const uint8_t accent[PIXEL_BOX_BYTES_PER_PIXEL] = {0x77, 0x55, 0x00, 0xff};

    GBytes        *scaled     = downscale_flat_image(accent);
    const uint8_t *last_pixel = (const uint8_t *)g_bytes_get_data(scaled, NULL) + 5 * PIXEL_BOX_BYTES_PER_PIXEL;

    g_assert_cmpuint(g_bytes_get_size(scaled), ==, 3 * 2 * PIXEL_BOX_BYTES_PER_PIXEL);
    g_assert_cmpmem(last_pixel, PIXEL_BOX_BYTES_PER_PIXEL, accent, PIXEL_BOX_BYTES_PER_PIXEL);
    g_bytes_unref(scaled);
}

/* A 2x1 image of black and white halves to one mid grey, rounding up from 127.5. */
static void
test_box_average_rounds(void)
{
    static const uint8_t   black_and_white[] = {0, 0, 0, 0, 255, 255, 255, 255};
    struct pixel_box_image source            = {black_and_white, 2, 1, sizeof(black_and_white)};

    GBytes        *scaled        = pixel_box_downscale(&source, 1, 1);
    const uint8_t *scaled_pixels = g_bytes_get_data(scaled, NULL);

    g_assert_cmpuint(scaled_pixels[0], ==, 128);
    g_assert_cmpuint(scaled_pixels[3], ==, 128);
    g_bytes_unref(scaled);
}

/* Padding at the end of each source row must never be averaged in. */
static void
test_row_padding_is_ignored(void)
{
    static const uint8_t   padded_rows[] = {10, 10, 10, 10, 99, 99, 20, 20, 20, 20, 99, 99};
    struct pixel_box_image source        = {padded_rows, 1, 2, 6};

    GBytes *scaled = pixel_box_downscale(&source, 1, 1);

    g_assert_cmpuint(((const uint8_t *)g_bytes_get_data(scaled, NULL))[0], ==, 15);
    g_bytes_unref(scaled);
}

static const struct {
    uint32_t source_width;
    uint32_t source_height;
    uint32_t expected_width;
    uint32_t expected_height;
} fit_cases[] = {
    {1916, 1130, 240, 141}, // a wide window is limited by width
    {100,  400,  37,  150}, // a tall window is limited by height
    {50,   20,   50,  20 }, // a small window is never upscaled
};

static void
assert_fit_case(
    size_t case_index
)
{
    uint32_t width;
    uint32_t height;

    pixel_box_fit_size(fit_cases[case_index].source_width, fit_cases[case_index].source_height, 240, 150, &width, &height);
    g_assert_cmpuint(width, ==, fit_cases[case_index].expected_width);
    g_assert_cmpuint(height, ==, fit_cases[case_index].expected_height);
}

static void
test_fit_keeps_aspect_and_never_upscales(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(fit_cases); i++) {
        assert_fit_case(i);
    }
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/pixel-box-downscaling/flat-colour", test_flat_colour_stays_exact            },
    {"/pixel-box-downscaling/rounding",    test_box_average_rounds                 },
    {"/pixel-box-downscaling/row-padding", test_row_padding_is_ignored             },
    {"/pixel-box-downscaling/fit-size",    test_fit_keeps_aspect_and_never_upscales},
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add_func(registered_tests[i].test_path, registered_tests[i].test_function);
    }

    return g_test_run();
}
