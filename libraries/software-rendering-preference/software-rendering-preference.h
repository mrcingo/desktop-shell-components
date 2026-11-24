#ifndef SOFTWARE_RENDERING_PREFERENCE_H
#define SOFTWARE_RENDERING_PREFERENCE_H

/*
 * Small, mostly static interfaces such as panels and the login screen draw
 * with GTK's Cairo renderer: it needs no GPU context, which costs about
 * 25 MB per process with the Vulkan renderer and over 100 MB with OpenGL,
 * measured on a Radeon 680M. Call before GTK starts. A GSK_RENDERER the
 * user set is kept.
 */
void prefer_software_rendering_for_simple_interface(void);

#endif
