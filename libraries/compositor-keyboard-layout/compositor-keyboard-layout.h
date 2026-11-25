#ifndef COMPOSITOR_KEYBOARD_LAYOUT_H
#define COMPOSITOR_KEYBOARD_LAYOUT_H

#include <json-glib/json-glib.h>

/*
 * The active keyboard layout as the compositor names it ("Spanish"), from
 * its GET_INPUTS reply or from an input event. With several keyboards, the
 * first one that has a layout counts. Free results with g_free().
 */

/* NULL when the reply lists no keyboard with a layout. */
char *compositor_keyboard_layout_from_inputs(JsonNode *inputs_reply);

/* NULL unless the event reports a keyboard's layout or keymap change. */
char *compositor_keyboard_layout_from_input_event(JsonNode *input_event);

#endif
