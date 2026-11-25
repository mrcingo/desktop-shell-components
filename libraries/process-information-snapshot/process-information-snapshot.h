#ifndef PROCESS_INFORMATION_SNAPSHOT_H
#define PROCESS_INFORMATION_SNAPSHOT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * What /proc tells about a process at one moment. Fields the reader may not
 * see, such as another user's working directory, are NULL or 0.
 */

struct process_information_snapshot {
    int      process_id;
    char    *command_line;          // arguments joined with spaces; NULL if unreadable
    char    *working_directory;     // NULL if unreadable
    uint64_t resident_memory_bytes; // 0 if unreadable
    bool     is_stopped;            // paused with SIGSTOP
};

/* Returns NULL when the process does not exist. proc_directory is "/proc" except in tests. */
struct process_information_snapshot *process_information_snapshot_read(const char *proc_directory, int process_id);

void process_information_snapshot_destroy(struct process_information_snapshot *snapshot);

/* "312 MiB", "1.4 GiB". Free the result with g_free(). */
char *process_information_format_memory_size(uint64_t byte_count);

#endif
