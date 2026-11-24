#ifndef MONITOR_CONNECTOR_LOOKUP_H
#define MONITOR_CONNECTOR_LOOKUP_H

#include <gdk/gdk.h>

/* The display's monitor whose connector is named connector_name, such as eDP-1, or NULL. */
GdkMonitor *monitor_connector_lookup_find(GdkDisplay *display, const char *connector_name);

#endif
