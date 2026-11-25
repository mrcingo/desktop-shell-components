#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "memory-usage-reading.h"

static const struct {
    const char *key;
    size_t      field_offset;
} memory_fields[] = {
    {"MemTotal:",     offsetof(struct memory_usage, total_kibibytes)     },
    {"MemAvailable:", offsetof(struct memory_usage, available_kibibytes) },
    {"SwapTotal:",    offsetof(struct memory_usage, swap_total_kibibytes)},
    {"SwapFree:",     offsetof(struct memory_usage, swap_free_kibibytes) },
};

/* The number after the key, or 0 if the line is malformed. */
static uint64_t
parse_kibibytes(
    const char *line,
    const char *key
)
{
    uint64_t kibibytes = 0;

    if (sscanf(line + strlen(key), "%" SCNu64, &kibibytes) != 1) {
        return 0;
    }

    return kibibytes;
}

static void
parse_line(
    const char          *line,
    struct memory_usage *usage
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(memory_fields); i++) {
        if (g_str_has_prefix(line, memory_fields[i].key)) {
            *(uint64_t *)((char *)usage + memory_fields[i].field_offset) = parse_kibibytes(line, memory_fields[i].key);
        }
    }
}

bool
memory_usage_parse(
    const char          *memory_information_text,
    struct memory_usage *usage
)
{
    g_auto(GStrv) lines = g_strsplit(memory_information_text, "\n", -1);

    *usage = (struct memory_usage){0};

    for (char **line = lines; *line != NULL; line++) {
        parse_line(*line, usage);
    }

    if (usage->total_kibibytes == 0) {
        return false;
    }

    return usage->available_kibibytes > 0;
}

bool
memory_usage_read(
    struct memory_usage *usage
)
{
    g_autofree char *memory_information_text = NULL;

    if (!g_file_get_contents("/proc/meminfo", &memory_information_text, NULL, NULL)) {
        return false;
    }

    return memory_usage_parse(memory_information_text, usage);
}
