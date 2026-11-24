#define _GNU_SOURCE // memfd_create() and file sealing

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>

#include "shared-memory-buffer.h"

static const unsigned int file_seals = F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL;

/* Logs the failed step with errno and returns false, for use in a return. */
static bool
report_failure(
    const char *failed_step
)
{
    g_warning("%s failed: %s", failed_step, g_strerror(errno));

    return false;
}

static int
open_anonymous_memory_file(void)
{
    int file_descriptor = memfd_create("shared-memory-buffer", MFD_CLOEXEC | MFD_ALLOW_SEALING);

    if (file_descriptor < 0) {
        report_failure("memfd_create");
    }

    return file_descriptor;
}

static bool
resize_and_seal_memory_file(
    int    file_descriptor,
    size_t size_in_bytes
)
{
    if (ftruncate(file_descriptor, (off_t)size_in_bytes) < 0) {
        return report_failure("ftruncate");
    }

    if (fcntl(file_descriptor, F_ADD_SEALS, file_seals) < 0) {
        return report_failure("Sealing shared memory");
    }

    return true;
}

static int
create_sealed_memory_file(
    size_t size_in_bytes
)
{
    int file_descriptor = open_anonymous_memory_file();

    if (file_descriptor < 0) {
        return -1;
    }

    if (resize_and_seal_memory_file(file_descriptor, size_in_bytes)) {
        return file_descriptor;
    }

    close(file_descriptor);

    return -1;
}

static struct shared_memory_buffer *
create_buffer_record(
    void  *mapped_bytes,
    size_t size_in_bytes,
    int    file_descriptor
)
{
    struct shared_memory_buffer *buffer = g_new(struct shared_memory_buffer, 1);

    *buffer = (struct shared_memory_buffer){
        .mapped_bytes    = mapped_bytes,
        .size_in_bytes   = size_in_bytes,
        .file_descriptor = file_descriptor,
    };

    return buffer;
}

/* Takes over file_descriptor, closing it if the mapping fails. */
static struct shared_memory_buffer *
map_shared_memory_buffer(
    int    file_descriptor,
    size_t size_in_bytes
)
{
    void *mapped_bytes = mmap(NULL, size_in_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);

    if (mapped_bytes == MAP_FAILED) {
        report_failure("mmap");
        close(file_descriptor);

        return NULL;
    }

    return create_buffer_record(mapped_bytes, size_in_bytes, file_descriptor);
}

struct shared_memory_buffer *
shared_memory_buffer_create(
    size_t size_in_bytes
)
{
    g_return_val_if_fail(size_in_bytes > 0, NULL);

    int file_descriptor = create_sealed_memory_file(size_in_bytes);

    if (file_descriptor < 0) {
        return NULL;
    }

    return map_shared_memory_buffer(file_descriptor, size_in_bytes);
}

void
shared_memory_buffer_close_file_descriptor(
    struct shared_memory_buffer *buffer
)
{
    if (buffer->file_descriptor < 0) {
        return;
    }

    close(buffer->file_descriptor);

    buffer->file_descriptor = -1;
}

void
shared_memory_buffer_destroy(
    struct shared_memory_buffer *buffer
)
{
    if (buffer == NULL) {
        return;
    }

    shared_memory_buffer_close_file_descriptor(buffer);
    munmap(buffer->mapped_bytes, buffer->size_in_bytes);
    g_free(buffer);
}

struct bytes_mapping {
    void  *mapped_bytes;
    size_t size_in_bytes;
};

static void
unmap_bytes_mapping(
    void *data
)
{
    struct bytes_mapping *mapping = data;

    munmap(mapping->mapped_bytes, mapping->size_in_bytes);
    g_free(mapping);
}

GBytes *
shared_memory_buffer_convert_to_bytes(
    struct shared_memory_buffer *buffer
)
{
    struct bytes_mapping *mapping = g_new(struct bytes_mapping, 1);

    mapping->mapped_bytes  = buffer->mapped_bytes;
    mapping->size_in_bytes = buffer->size_in_bytes;

    shared_memory_buffer_close_file_descriptor(buffer);
    g_free(buffer);

    return g_bytes_new_with_free_func(mapping->mapped_bytes, mapping->size_in_bytes, unmap_bytes_mapping, mapping);
}
