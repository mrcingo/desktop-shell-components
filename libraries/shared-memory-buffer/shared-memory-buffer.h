#ifndef SHARED_MEMORY_BUFFER_H
#define SHARED_MEMORY_BUFFER_H

#include <stddef.h>

#include <glib.h>

/*
 * Anonymous memory shared with another process through a file descriptor,
 * as used for wl_shm pixel buffers. The file is sealed against shrinking
 * and growing, so the other process can map it without risking SIGBUS.
 */

struct shared_memory_buffer {
    void  *mapped_bytes;
    size_t size_in_bytes;

    int file_descriptor; // -1 once closed
};

/* Returns NULL and logs a warning if the memory cannot be created. */
struct shared_memory_buffer *shared_memory_buffer_create(size_t size_in_bytes);

/* The descriptor is only needed until the other process has received it. */
void shared_memory_buffer_close_file_descriptor(struct shared_memory_buffer *buffer);

void shared_memory_buffer_destroy(struct shared_memory_buffer *buffer);

/*
 * Hands the mapping to a GBytes, which unmaps it when released, and frees
 * the buffer. Avoids copying large images.
 */
GBytes *shared_memory_buffer_convert_to_bytes(struct shared_memory_buffer *buffer);

#endif
