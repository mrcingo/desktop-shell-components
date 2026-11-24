#include <wayland-client-protocol.h>

#include "wayland-pixel-format-conversion.h"

static void
test_alpha_formats_rank_first(void)
{
    g_assert_cmpint(
        wayland_pixel_format_preference_rank(WL_SHM_FORMAT_ARGB8888),
        <,
        wayland_pixel_format_preference_rank(WL_SHM_FORMAT_XRGB8888)
    );
    g_assert_cmpint(
        wayland_pixel_format_preference_rank(WL_SHM_FORMAT_ABGR8888),
        <,
        wayland_pixel_format_preference_rank(WL_SHM_FORMAT_XBGR8888)
    );
    g_assert_cmpint(wayland_pixel_format_preference_rank(WL_SHM_FORMAT_RGB565), ==, WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK);
}

/* XBGR8888 is what sway offers with a GLES or Vulkan renderer. */
static void
test_byte_order_matches_gdk(void)
{
    GdkMemoryFormat gdk_format;

    g_assert_true(wayland_pixel_format_convert_to_gdk(WL_SHM_FORMAT_XBGR8888, &gdk_format));
    g_assert_cmpint(gdk_format, ==, GDK_MEMORY_R8G8B8X8);

    g_assert_true(wayland_pixel_format_convert_to_gdk(WL_SHM_FORMAT_ARGB8888, &gdk_format));
    g_assert_cmpint(gdk_format, ==, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED);

    g_assert_false(wayland_pixel_format_convert_to_gdk(WL_SHM_FORMAT_RGB565, &gdk_format));
}

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/wayland-pixel-format-conversion/alpha-first", test_alpha_formats_rank_first);
    g_test_add_func("/wayland-pixel-format-conversion/byte-order", test_byte_order_matches_gdk);

    return g_test_run();
}
