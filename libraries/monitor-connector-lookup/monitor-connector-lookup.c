#include "monitor-connector-lookup.h"

/* The display's monitor list keeps its own reference, so the item can be released at once. */
static GdkMonitor *
borrow_monitor(
    GListModel *monitors,
    guint       monitor_index
)
{
    GdkMonitor *monitor = g_list_model_get_item(monitors, monitor_index);

    g_object_unref(monitor);

    return monitor;
}

static gboolean
monitor_has_connector(
    GdkMonitor *monitor,
    const char *connector_name
)
{
    return g_strcmp0(gdk_monitor_get_connector(monitor), connector_name) == 0;
}

GdkMonitor *
monitor_connector_lookup_find(
    GdkDisplay *display,
    const char *connector_name
)
{
    GListModel *monitors      = gdk_display_get_monitors(display);
    guint       monitor_count = g_list_model_get_n_items(monitors);

    for (guint i = 0; i < monitor_count; i++) {
        GdkMonitor *monitor = borrow_monitor(monitors, i);

        if (monitor_has_connector(monitor, connector_name)) {
            return monitor;
        }
    }

    return NULL;
}
