#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "display-backlight-level.h"

struct display_backlight {
    char   *device_directory;
    guint64 maximum_level;
};

static bool
read_level(
    const char *device_directory,
    const char *file_name,
    guint64    *level
)
{
    g_autofree char *level_path = g_build_filename(device_directory, file_name, NULL);
    g_autofree char *contents   = NULL;

    if (!g_file_get_contents(level_path, &contents, NULL, NULL)) {
        return false;
    }

    return g_ascii_string_to_unsigned(g_strstrip(contents), 10, 0, G_MAXUINT32, level, NULL);
}

/* 0 when the device has no usable maximum level. */
static guint64
read_maximum_level(
    const char *device_directory
)
{
    guint64 maximum_level = 0;

    if (!read_level(device_directory, "max_brightness", &maximum_level)) {
        return 0;
    }

    return maximum_level;
}

/* Takes ownership of device_directory. */
static struct display_backlight *
create_if_usable(
    char *device_directory
)
{
    guint64 maximum_level = read_maximum_level(device_directory);

    if (maximum_level == 0) {
        g_free(device_directory);
        return NULL;
    }

    struct display_backlight *backlight = g_new0(struct display_backlight, 1);

    *backlight = (struct display_backlight){.device_directory = device_directory, .maximum_level = maximum_level};

    return backlight;
}

struct display_backlight *
display_backlight_find(
    const char *backlight_class_directory
)
{
    g_autoptr(GDir) directory = g_dir_open(backlight_class_directory, 0, NULL);

    if (directory == NULL) {
        return NULL;
    }

    const char *entry_name = g_dir_read_name(directory);

    if (entry_name == NULL) {
        return NULL;
    }

    return create_if_usable(g_build_filename(backlight_class_directory, entry_name, NULL));
}

void
display_backlight_destroy(
    struct display_backlight *backlight
)
{
    if (backlight == NULL) {
        return;
    }

    g_free(backlight->device_directory);
    g_free(backlight);
}

int
display_backlight_read_percent(
    const struct display_backlight *backlight
)
{
    guint64 level = 0;

    if (!read_level(backlight->device_directory, "actual_brightness", &level)) {
        return -1;
    }

    return (int)MIN(100, (level * 100 + backlight->maximum_level / 2) / backlight->maximum_level);
}

bool
display_backlight_is_writable(
    const struct display_backlight *backlight
)
{
    g_autofree char *level_path = g_build_filename(backlight->device_directory, "brightness", NULL);

    return access(level_path, W_OK) == 0;
}

/* sysfs files take one write of the whole value; no temporary file and rename. */
static bool
write_level(
    const char *level_path,
    guint64     level
)
{
    g_autofree char *level_text = g_strdup_printf("%" G_GUINT64_FORMAT "\n", level);

    int file_descriptor = open(level_path, O_WRONLY | O_TRUNC | O_CLOEXEC);

    if (file_descriptor < 0) {
        return false;
    }

    ssize_t written_bytes = write(file_descriptor, level_text, strlen(level_text));

    close(file_descriptor);

    return written_bytes == (ssize_t)strlen(level_text);
}

bool
display_backlight_set_percent(
    const struct display_backlight *backlight,
    int                             percent
)
{
    g_autofree char *level_path = g_build_filename(backlight->device_directory, "brightness", NULL);

    guint64 level = (guint64)CLAMP(percent, 0, 100) * backlight->maximum_level / 100;

    return write_level(level_path, MAX(level, 1));
}
