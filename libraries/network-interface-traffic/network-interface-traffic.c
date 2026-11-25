#include <string.h>

#include <glib.h>

#include "network-interface-traffic.h"

static bool
is_plain_interface_name(
    const char *interface_name
)
{
    if (interface_name[0] == '\0') {
        return false;
    }

    if (interface_name[0] == '.') {
        return false;
    }

    return strchr(interface_name, '/') == NULL;
}

static bool
parse_counter(
    char     *contents,
    uint64_t *value
)
{
    guint64 parsed_value = 0;

    if (!g_ascii_string_to_unsigned(g_strstrip(contents), 10, 0, G_MAXUINT64, &parsed_value, NULL)) {
        return false;
    }

    *value = parsed_value;

    return true;
}

static bool
read_counter(
    const char *statistics_directory,
    const char *counter_name,
    uint64_t   *value
)
{
    g_autofree char *counter_path = g_build_filename(statistics_directory, counter_name, NULL);
    g_autofree char *contents     = NULL;

    if (!g_file_get_contents(counter_path, &contents, NULL, NULL)) {
        return false;
    }

    return parse_counter(contents, value);
}

bool
network_interface_byte_counts_read(
    const char                           *net_class_directory,
    const char                           *interface_name,
    struct network_interface_byte_counts *counts
)
{
    if (!is_plain_interface_name(interface_name)) {
        return false;
    }

    g_autofree char *statistics_directory = g_build_filename(net_class_directory, interface_name, "statistics", NULL);

    if (!read_counter(statistics_directory, "rx_bytes", &counts->received_bytes)) {
        return false;
    }

    return read_counter(statistics_directory, "tx_bytes", &counts->transmitted_bytes);
}

uint64_t
network_interface_byte_rate(
    uint64_t earlier_bytes,
    uint64_t later_bytes,
    int64_t  elapsed_microseconds
)
{
    if (elapsed_microseconds <= 0) {
        return 0;
    }

    if (later_bytes < earlier_bytes) {
        return 0;
    }

    double bytes_per_microsecond = (double)(later_bytes - earlier_bytes) / (double)elapsed_microseconds;

    return (uint64_t)(bytes_per_microsecond * G_USEC_PER_SEC);
}
