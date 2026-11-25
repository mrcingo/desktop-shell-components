#ifndef FILESYSTEM_FREE_SPACE_H
#define FILESYSTEM_FREE_SPACE_H

#include <stdbool.h>
#include <stdint.h>

/* Size and space left on the filesystem holding a path. */

struct filesystem_space {
    uint64_t total_bytes;
    uint64_t available_bytes; // what an ordinary user can still write
};

/* False if the path cannot be examined or the sizes do not fit 64 bits. */
bool filesystem_space_read(const char *path, struct filesystem_space *space);

#endif
