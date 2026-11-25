#include <sys/statvfs.h>

#include <glib.h>

#include "filesystem-free-space.h"

bool
filesystem_space_read(
    const char              *path,
    struct filesystem_space *space
)
{
    struct statvfs statistics = {0};

    if (statvfs(path, &statistics) != 0) {
        return false;
    }

    if (!g_uint64_checked_mul(&space->total_bytes, statistics.f_blocks, statistics.f_frsize)) {
        return false;
    }

    return g_uint64_checked_mul(&space->available_bytes, statistics.f_bavail, statistics.f_frsize);
}
