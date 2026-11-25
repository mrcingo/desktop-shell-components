#include <glib.h>

#include "window-snap-keyboard-steps.h"

/* Short names, so the tables below read as tables. */
#define FREE         WINDOW_SNAP_STATE_FREE
#define LEFT_HALF    WINDOW_SNAP_STATE_LEFT_HALF
#define RIGHT_HALF   WINDOW_SNAP_STATE_RIGHT_HALF
#define TOP_LEFT     WINDOW_SNAP_STATE_TOP_LEFT_QUARTER
#define TOP_RIGHT    WINDOW_SNAP_STATE_TOP_RIGHT_QUARTER
#define BOTTOM_LEFT  WINDOW_SNAP_STATE_BOTTOM_LEFT_QUARTER
#define BOTTOM_RIGHT WINDOW_SNAP_STATE_BOTTOM_RIGHT_QUARTER
#define MAXIMIZED    WINDOW_SNAP_STATE_MAXIMIZED

/* Rows are states, columns the keys left, right, up and down. */
static const enum window_snap_state transitions[WINDOW_SNAP_STATE_COUNT][WINDOW_SNAP_KEY_COUNT] = {
    [FREE]         = {LEFT_HALF,   RIGHT_HALF,   MAXIMIZED,  FREE        },
    [LEFT_HALF]    = {LEFT_HALF,   FREE,         TOP_LEFT,   BOTTOM_LEFT },
    [RIGHT_HALF]   = {FREE,        RIGHT_HALF,   TOP_RIGHT,  BOTTOM_RIGHT},
    [TOP_LEFT]     = {TOP_LEFT,    TOP_RIGHT,    MAXIMIZED,  LEFT_HALF   },
    [TOP_RIGHT]    = {TOP_LEFT,    TOP_RIGHT,    MAXIMIZED,  RIGHT_HALF  },
    [BOTTOM_LEFT]  = {BOTTOM_LEFT, BOTTOM_RIGHT, LEFT_HALF,  BOTTOM_LEFT },
    [BOTTOM_RIGHT] = {BOTTOM_LEFT, BOTTOM_RIGHT, RIGHT_HALF, BOTTOM_RIGHT},
    [MAXIMIZED]    = {LEFT_HALF,   RIGHT_HALF,   MAXIMIZED,  FREE        },
};

/* Parts of the workspace in halves: from (x0, y0) to (x1, y1), each 0, 1 or 2. */
static const int state_parts[WINDOW_SNAP_STATE_COUNT][4] = {
    [LEFT_HALF]    = {0, 0, 1, 2},
    [RIGHT_HALF]   = {1, 0, 2, 2},
    [TOP_LEFT]     = {0, 0, 1, 1},
    [TOP_RIGHT]    = {1, 0, 2, 1},
    [BOTTOM_LEFT]  = {0, 1, 1, 2},
    [BOTTOM_RIGHT] = {1, 1, 2, 2},
    [MAXIMIZED]    = {0, 0, 2, 2},
};

static const char *const key_words[WINDOW_SNAP_KEY_COUNT] = {"left", "right", "up", "down"};

static const struct {
    const char            *word;
    enum window_snap_state state;
} state_words[] = {
    {"left-half",  LEFT_HALF },
    {"right-half", RIGHT_HALF},
    {"maximize",   MAXIMIZED },
    {"restore",    FREE      },
};

enum window_snap_state
window_snap_state_after_key(
    enum window_snap_state state,
    enum window_snap_key   key
)
{
    return transitions[state][key];
}

static struct compositor_rectangle
part_rectangle(
    const struct compositor_rectangle *workspace,
    const int                         *part
)
{
    int left   = workspace->x + workspace->width * part[0] / 2;
    int top    = workspace->y + workspace->height * part[1] / 2;
    int right  = workspace->x + workspace->width * part[2] / 2;
    int bottom = workspace->y + workspace->height * part[3] / 2;

    return (struct compositor_rectangle){left, top, right - left, bottom - top};
}

bool
window_snap_state_area(
    enum window_snap_state             state,
    const struct compositor_rectangle *workspace,
    struct compositor_rectangle       *area
)
{
    if (state == FREE) {
        return false;
    }

    *area = part_rectangle(workspace, state_parts[state]);

    return true;
}

bool
window_snap_parse_key(
    const char           *word,
    enum window_snap_key *key
)
{
    for (int i = 0; i < WINDOW_SNAP_KEY_COUNT; i++) {
        if (g_strcmp0(word, key_words[i]) == 0) {
            *key = (enum window_snap_key)i;
            return true;
        }
    }

    return false;
}

bool
window_snap_parse_state(
    const char             *word,
    enum window_snap_state *state
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(state_words); i++) {
        if (g_strcmp0(word, state_words[i].word) == 0) {
            *state = state_words[i].state;
            return true;
        }
    }

    return false;
}
