#define _GNU_SOURCE // file sealing

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "shared-memory-buffer.h"

static void
assert_file_cannot_change_size(
    int file_descriptor
)
{
    int seals = fcntl(file_descriptor, F_GET_SEALS);

    g_assert_cmpint(seals & F_SEAL_SHRINK, !=, 0);
    g_assert_cmpint(seals & F_SEAL_GROW, !=, 0);
    g_assert_cmpint(ftruncate(file_descriptor, 1), <, 0);
}

/* What one side writes through the file, the mapping sees. */
static void
assert_mapping_sees_file_writes(
    struct shared_memory_buffer *buffer
)
{
    g_assert_cmpint(pwrite(buffer->file_descriptor, "pixels", 6, 0), ==, 6);
    g_assert_cmpmem(buffer->mapped_bytes, 6, "pixels", 6);
}

static void
test_buffer_is_shared_and_sealed(void)
{
    struct shared_memory_buffer *buffer = shared_memory_buffer_create(4096);

    g_assert_nonnull(buffer);
    g_assert_cmpint(buffer->file_descriptor, >=, 0);
    assert_file_cannot_change_size(buffer->file_descriptor);
    assert_mapping_sees_file_writes(buffer);
    shared_memory_buffer_destroy(buffer);
}

static struct shared_memory_buffer *
create_buffer_holding_text(
    const char *text,
    size_t      size_in_bytes
)
{
    struct shared_memory_buffer *buffer = shared_memory_buffer_create(size_in_bytes);

    memcpy(buffer->mapped_bytes, text, strlen(text));

    return buffer;
}

static void
test_conversion_keeps_the_bytes(void)
{
    struct shared_memory_buffer *buffer = create_buffer_holding_text("texture", 16);
    GBytes                      *bytes  = shared_memory_buffer_convert_to_bytes(buffer);

    g_assert_cmpuint(g_bytes_get_size(bytes), ==, 16);
    g_assert_cmpmem(g_bytes_get_data(bytes, NULL), 7, "texture", 7);
    g_bytes_unref(bytes);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/shared-memory-buffer/shared-and-sealed", test_buffer_is_shared_and_sealed},
    {"/shared-memory-buffer/conversion",        test_conversion_keeps_the_bytes },
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
