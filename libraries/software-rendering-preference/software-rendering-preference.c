#include <glib.h>

#include "software-rendering-preference.h"

void
prefer_software_rendering_for_simple_interface(void)
{
    g_setenv("GSK_RENDERER", "cairo", FALSE);
}
