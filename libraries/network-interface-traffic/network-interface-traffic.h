#ifndef NETWORK_INTERFACE_TRAFFIC_H
#define NETWORK_INTERFACE_TRAFFIC_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Bytes a network interface has received and sent, from the kernel's
 * counters in /sys/class/net/INTERFACE/statistics, and the rate between two
 * readings.
 */

struct network_interface_byte_counts {
    uint64_t received_bytes;
    uint64_t transmitted_bytes;
};

/*
 * net_class_directory is normally /sys/class/net. Names that are not a plain
 * interface name (empty, ".", "..", or holding "/") are refused.
 */
bool network_interface_byte_counts_read(
    const char                           *net_class_directory,
    const char                           *interface_name,
    struct network_interface_byte_counts *counts
);

/* Bytes per second; 0 when no time passed or the counter went back (the interface was reset). */
uint64_t network_interface_byte_rate(uint64_t earlier_bytes, uint64_t later_bytes, int64_t elapsed_microseconds);

#endif
