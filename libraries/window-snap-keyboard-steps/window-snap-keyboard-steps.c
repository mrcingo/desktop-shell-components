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

/* Keys that carry a window against an edge over to the next monitor. */
static const struct {
    enum window_snap_state from;
    enum window_snap_key   key;
    enum window_snap_state to;
} monitor_crossings[] = {
    {LEFT_HALF,    WINDOW_SNAP_KEY_LEFT,  RIGHT_HALF  },
    {TOP_LEFT,     WINDOW_SNAP_KEY_LEFT,  TOP_RIGHT   },
    {BOTTOM_LEFT,  WINDOW_SNAP_KEY_LEFT,  BOTTOM_RIGHT},
    {RIGHT_HALF,   WINDOW_SNAP_KEY_RIGHT, LEFT_HALF   },
    {TOP_RIGHT,    WINDOW_SNAP_KEY_RIGHT, TOP_LEFT    },
    {BOTTOM_RIGHT, WINDOW_SNAP_KEY_RIGHT, BOTTOM_LEFT },
};

static bool
crossing_matches(
    size_t                 crossing_index,
    enum window_snap_state state,
    enum window_snap_key   key
)
{
    if (monitor_crossings[crossing_index].from != state) {
        return false;
    }

    return monitor_crossings[crossing_index].key == key;
}

bool
window_snap_state_crosses_monitors(
    enum window_snap_state  state,
    enum window_snap_key    key,
    enum window_snap_state *state_on_neighbour
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(monitor_crossings); i++) {
        if (crossing_matches(i, state, key)) {
            *state_on_neighbour = monitor_crossings[i].to;
            return true;
        }
    }

    return false;
}

static bool
overlaps_vertically(
    const struct compositor_rectangle *first,
    const struct compositor_rectangle *second
)
{
    if (first->y >= second->y + second->height) {
        return false;
    }

    return second->y < first->y + first->height;
}

/* How far candidate lies beyond output on the key's side; negative when it is not on that side. */
static int
distance_on_side(
    const struct compositor_rectangle *output_area,
    const struct compositor_rectangle *candidate_area,
    enum window_snap_key               key
)
{
    if (key == WINDOW_SNAP_KEY_LEFT) {
        return output_area->x - (candidate_area->x + candidate_area->width);
    }

    return candidate_area->x - (output_area->x + output_area->width);
}

static bool
is_beside(
    const struct compositor_output *output,
    const struct compositor_output *candidate,
    enum window_snap_key            key
)
{
    if (candidate == output) {
        return false;
    }

    if (!overlaps_vertically(&output->output_area, &candidate->output_area)) {
        return false;
    }

    return distance_on_side(&output->output_area, &candidate->output_area, key) >= 0;
}

/* The candidate if it is beside output and nearer than best, else best. */
static const struct compositor_output *
nearer_neighbour(
    const struct compositor_output *output,
    const struct compositor_output *candidate,
    const struct compositor_output *best,
    enum window_snap_key            key
)
{
    if (!is_beside(output, candidate, key)) {
        return best;
    }

    if (best == NULL) {
        return candidate;
    }

    if (distance_on_side(&output->output_area, &candidate->output_area, key) <
        distance_on_side(&output->output_area, &best->output_area, key)) {
        return candidate;
    }

    return best;
}

static bool
is_sideways(
    enum window_snap_key key
)
{
    if (key == WINDOW_SNAP_KEY_LEFT) {
        return true;
    }

    return key == WINDOW_SNAP_KEY_RIGHT;
}

const struct compositor_output *
window_snap_find_neighbour_output(
    const struct compositor_window_tree *tree,
    const struct compositor_output      *output,
    enum window_snap_key                 key
)
{
    const struct compositor_output *best = NULL;

    if (!is_sideways(key)) {
        return NULL;
    }

    for (guint i = 0; i < tree->outputs->len; i++) {
        best = nearer_neighbour(output, g_ptr_array_index(tree->outputs, i), best, key);
    }

    return best;
}

/* Where along one axis a span of size length goes, keeping its relative place. */
static int
translate_span(
    int from_start,
    int from_length,
    int to_start,
    int to_length,
    int position,
    int length
)
{
    double free_before = (double)(position - from_start);
    double free_space  = (double)(from_length - length);
    double share       = free_space > 0 ? CLAMP(free_before / free_space, 0.0, 1.0) : 0.0;

    return to_start + (int)(share * (double)MAX(0, to_length - length) + 0.5);
}

struct compositor_rectangle
window_snap_translate_frame(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *from_area,
    const struct compositor_rectangle *to_area
)
{
    int width  = MIN(frame->width, to_area->width);
    int height = MIN(frame->height, to_area->height);

    return (struct compositor_rectangle){
        translate_span(from_area->x, from_area->width, to_area->x, to_area->width, frame->x, width),
        translate_span(from_area->y, from_area->height, to_area->y, to_area->height, frame->y, height),
        width,
        height,
    };
}

enum {
    CORNER_TOLERANCE = 2, // pixels
};

static bool
is_near(
    int value,
    int wanted
)
{
    return ABS(value - wanted) <= CORNER_TOLERANCE;
}

bool
window_snap_frame_is_at_snap_corner(
    const struct compositor_rectangle *frame,
    const struct compositor_rectangle *workspace
)
{
    bool x_matches = is_near(frame->x, workspace->x) || is_near(frame->x, workspace->x + workspace->width / 2);
    bool y_matches = is_near(frame->y, workspace->y) || is_near(frame->y, workspace->y + workspace->height / 2);

    return x_matches && y_matches;
}
