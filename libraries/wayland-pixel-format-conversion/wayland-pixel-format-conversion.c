#include <wayland-client-protocol.h>

#include "wayland-pixel-format-conversion.h"

#if G_BYTE_ORDER != G_LITTLE_ENDIAN
#error "The format table assumes a little-endian machine"
#endif

/* In order of preference. */
static const struct {
    uint32_t        wayland_format;
    GdkMemoryFormat gdk_format;
} supported_formats[] = {
    {WL_SHM_FORMAT_ARGB8888, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED},
    {WL_SHM_FORMAT_ABGR8888, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED},
    {WL_SHM_FORMAT_XRGB8888, GDK_MEMORY_B8G8R8X8              },
    {WL_SHM_FORMAT_XBGR8888, GDK_MEMORY_R8G8B8X8              },
};

int
wayland_pixel_format_preference_rank(
    uint32_t wayland_format
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(supported_formats); i++) {
        if (supported_formats[i].wayland_format == wayland_format) {
            return (int)i;
        }
    }

    return WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK;
}

bool
wayland_pixel_format_convert_to_gdk(
    uint32_t         wayland_format,
    GdkMemoryFormat *gdk_format
)
{
    int rank = wayland_pixel_format_preference_rank(wayland_format);

    if (rank == WAYLAND_PIXEL_FORMAT_UNSUPPORTED_RANK) {
        return false;
    }

    *gdk_format = supported_formats[rank].gdk_format;

    return true;
}
