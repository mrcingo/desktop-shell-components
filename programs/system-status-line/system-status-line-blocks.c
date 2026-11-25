#include <inttypes.h>

#include "human-readable-quantities.h"
#include "status-bar-block-encoding.h"
#include "system-status-line-blocks.h"
#include "text-placeholder-expansion.h"

enum block_severity {
    BLOCK_SEVERITY_NORMAL,
    BLOCK_SEVERITY_WARNING,
    BLOCK_SEVERITY_URGENT,
};

struct render_context {
    const struct system_status_line_settings *settings;
    const struct system_status_line_inputs   *inputs;
    enum system_status_line_block_kind        kind;
};

/* What one block shows; a NULL text hides it. */
struct block_content {
    char               *text;
    enum block_severity severity;
};

enum {
    BYTES_PER_KIBIBYTE = 1024,
};

/* Padded numbers keep a fixed width, so neighbouring blocks do not move as values change. */
static char *
format_number(
    int  value,
    bool pads_numbers
)
{
    if (pads_numbers) {
        return g_strdup_printf("%3d", value);
    }

    return g_strdup_printf("%d", value);
}

static char *
format_size(
    uint64_t bytes,
    bool     pads_numbers
)
{
    g_autofree char *size = human_readable_byte_size(bytes);

    if (pads_numbers) {
        return g_strdup_printf("%4s", size);
    }

    return g_steal_pointer(&size);
}

/* A threshold of -1 is off. */
static bool
reaches_or_exceeds(
    int value,
    int threshold
)
{
    if (threshold < 0) {
        return false;
    }

    return value >= threshold;
}

static bool
reaches_or_falls_below(
    int value,
    int threshold
)
{
    if (threshold < 0) {
        return false;
    }

    return value <= threshold;
}

/* Higher is worse, as for processor use. */
static enum block_severity
severity_rising(
    int value,
    int warning_threshold,
    int urgent_threshold
)
{
    if (reaches_or_exceeds(value, urgent_threshold)) {
        return BLOCK_SEVERITY_URGENT;
    }

    if (reaches_or_exceeds(value, warning_threshold)) {
        return BLOCK_SEVERITY_WARNING;
    }

    return BLOCK_SEVERITY_NORMAL;
}

/* Lower is worse, as for battery charge or free disk space. */
static enum block_severity
severity_falling(
    int value,
    int warning_threshold,
    int urgent_threshold
)
{
    if (reaches_or_falls_below(value, urgent_threshold)) {
        return BLOCK_SEVERITY_URGENT;
    }

    if (reaches_or_falls_below(value, warning_threshold)) {
        return BLOCK_SEVERITY_WARNING;
    }

    return BLOCK_SEVERITY_NORMAL;
}

static const struct system_status_line_block_settings *
block_settings(
    const struct render_context *context
)
{
    return &context->settings->blocks[context->kind];
}

static char *
escape_value(
    const char *value
)
{
    if (value == NULL) {
        return g_strdup("");
    }

    return g_markup_escape_text(value, -1);
}

/* Values come from other programs (network names) and are escaped; the format itself may hold markup. */
static char *
expand_format(
    const struct render_context   *context,
    const char                    *format_key,
    const struct text_placeholder *placeholders,
    size_t                         placeholder_count
)
{
    g_autofree struct text_placeholder *escaped        = g_new0(struct text_placeholder, MAX(placeholder_count, 1));
    g_autoptr(GPtrArray)                escaped_values = g_ptr_array_new_with_free_func(g_free);

    for (size_t i = 0; i < placeholder_count; i++) {
        char *escaped_value = escape_value(placeholders[i].value);

        g_ptr_array_add(escaped_values, escaped_value);

        escaped[i] = (struct text_placeholder){placeholders[i].name, escaped_value};
    }

    return text_placeholder_expand(
        system_status_line_settings_format(context->settings, context->kind, format_key),
        escaped,
        placeholder_count
    );
}

enum {
    MAXIMUM_PLACEHOLDERS = 8,
};

/* Placeholders whose values the renderer made and frees after expanding. */
struct owned_placeholders {
    struct text_placeholder items[MAXIMUM_PLACEHOLDERS];
    char                   *owned_values[MAXIMUM_PLACEHOLDERS];
    size_t                  count;
};

/* Takes ownership of value. */
static void
add_owned(
    struct owned_placeholders *placeholders,
    const char                *name,
    char                      *value
)
{
    g_assert(placeholders->count < MAXIMUM_PLACEHOLDERS);

    placeholders->owned_values[placeholders->count] = value;

    placeholders->items[placeholders->count++] = (struct text_placeholder){name, value};
}

static void
add_copy(
    struct owned_placeholders *placeholders,
    const char                *name,
    const char                *value
)
{
    add_owned(placeholders, name, g_strdup(value));
}

static void
free_owned(
    struct owned_placeholders *placeholders
)
{
    for (size_t i = 0; i < placeholders->count; i++) {
        g_free(placeholders->owned_values[i]);
    }
}

/* Expands the format, frees the placeholders and fills the block. */
static bool
show_expanded(
    const struct render_context *context,
    struct block_content        *content,
    const char                  *format_key,
    struct owned_placeholders   *placeholders,
    enum block_severity          severity
)
{
    content->text     = expand_format(context, format_key, placeholders->items, placeholders->count);
    content->severity = severity;

    free_owned(placeholders);

    return true;
}

static enum block_severity
rising_severity_of(
    const struct render_context *context,
    int                          value
)
{
    const struct system_status_line_block_settings *block = block_settings(context);

    return severity_rising(value, block->warning_threshold, block->urgent_threshold);
}

static enum block_severity
falling_severity_of(
    const struct render_context *context,
    int                          value
)
{
    const struct system_status_line_block_settings *block = block_settings(context);

    return severity_falling(value, block->warning_threshold, block->urgent_threshold);
}

/* ---- network */

static const char *
connected_format_key(
    const struct network_manager_status *network
)
{
    if (network->connection_kind == NETWORK_CONNECTION_KIND_WIRELESS) {
        return "format-wifi";
    }

    if (network->connection_kind == NETWORK_CONNECTION_KIND_WIRED) {
        return "format-ethernet";
    }

    return "format-other";
}

/* An unknown connectivity (the check is off) counts as reachable. */
static bool
reaches_internet(
    const struct network_manager_status *network
)
{
    if (network->connectivity == NETWORK_CONNECTIVITY_FULL) {
        return true;
    }

    return network->connectivity == NETWORK_CONNECTIVITY_UNKNOWN;
}

static const char *
problem_format_key(
    const struct network_manager_status *network,
    enum block_severity                 *severity
)
{
    if (network->connectivity == NETWORK_CONNECTIVITY_PORTAL) {
        *severity = BLOCK_SEVERITY_WARNING;
        return "format-portal";
    }

    *severity = BLOCK_SEVERITY_URGENT;

    return "format-no-internet";
}

static const char *
network_format_key(
    const struct network_manager_status *network,
    enum block_severity                 *severity
)
{
    if (network->primary_connection_path == NULL) {
        *severity = BLOCK_SEVERITY_WARNING;
        return "format-disconnected";
    }

    if (!reaches_internet(network)) {
        return problem_format_key(network, severity);
    }

    *severity = BLOCK_SEVERITY_NORMAL;

    return connected_format_key(network);
}

static const char *
vpn_text(
    bool vpn_is_active
)
{
    if (vpn_is_active) {
        return " VPN";
    }

    return "";
}

static void
add_network_placeholders(
    struct owned_placeholders           *placeholders,
    const struct network_manager_status *network,
    bool                                 pads_numbers
)
{
    add_copy(placeholders, "name", network->connection_name);
    add_owned(placeholders, "signal", format_number(MAX(network->wifi_signal_percent, 0), pads_numbers));
    add_copy(placeholders, "interface", network->interface_name);
    add_copy(placeholders, "vpn", vpn_text(network->vpn_is_active));
}

static bool
render_network(
    const struct render_context *context,
    struct block_content        *content
)
{
    const struct network_manager_status *network      = &context->inputs->service_values->network;
    struct owned_placeholders            placeholders = {0};
    enum block_severity                  severity     = BLOCK_SEVERITY_NORMAL;

    if (!network->service_is_running) {
        return false;
    }

    add_network_placeholders(&placeholders, network, context->settings->pads_numbers);

    return show_expanded(context, content, network_format_key(network, &severity), &placeholders, severity);
}

/* ---- traffic */

static bool
render_traffic(
    const struct render_context *context,
    struct block_content        *content
)
{
    const struct polled_system_values *values       = context->inputs->polled_values;
    struct owned_placeholders          placeholders = {0};

    if (!values->has_traffic) {
        return false;
    }

    add_owned(&placeholders, "down", format_size(values->download_bytes_per_second, context->settings->pads_numbers));
    add_owned(&placeholders, "up", format_size(values->upload_bytes_per_second, context->settings->pads_numbers));

    return show_expanded(context, content, "format", &placeholders, BLOCK_SEVERITY_NORMAL);
}

/* ---- bluetooth */

static const char *
bluetooth_format_key(
    const struct bluetooth_adapter_status *bluetooth
)
{
    if (!bluetooth->is_powered) {
        return "format-off";
    }

    if (bluetooth->connected_device_count == 0) {
        return "format-on";
    }

    return "format-connected";
}

static bool
render_bluetooth(
    const struct render_context *context,
    struct block_content        *content
)
{
    const struct bluetooth_adapter_status *bluetooth    = &context->inputs->service_values->bluetooth;
    struct owned_placeholders              placeholders = {0};

    if (!bluetooth->adapter_is_present) {
        return false;
    }

    add_owned(&placeholders, "connected", g_strdup_printf("%d", bluetooth->connected_device_count));

    return show_expanded(context, content, bluetooth_format_key(bluetooth), &placeholders, BLOCK_SEVERITY_NORMAL);
}

/* ---- processor, temperature, brightness: one number each; -1 means unknown */

static bool
render_number(
    const struct render_context *context,
    struct block_content        *content,
    const char                  *placeholder_name,
    int                          value
)
{
    struct owned_placeholders placeholders = {0};

    if (value < 0) {
        return false;
    }

    add_owned(&placeholders, placeholder_name, format_number(value, context->settings->pads_numbers));

    return show_expanded(context, content, "format", &placeholders, rising_severity_of(context, value));
}

static bool
render_processor(
    const struct render_context *context,
    struct block_content        *content
)
{
    return render_number(context, content, "usage", context->inputs->polled_values->processor_usage_percent);
}

static bool
render_temperature(
    const struct render_context *context,
    struct block_content        *content
)
{
    const struct polled_system_values *values = context->inputs->polled_values;

    if (!values->has_processor_temperature) {
        return false;
    }

    return render_number(context, content, "celsius", values->processor_temperature_celsius);
}

/* Brightness is not a problem at any level. */
static bool
render_brightness(
    const struct render_context *context,
    struct block_content        *content
)
{
    struct owned_placeholders placeholders = {0};
    int                       percent      = context->inputs->polled_values->backlight_percent;

    if (percent < 0) {
        return false;
    }

    add_owned(&placeholders, "percent", format_number(percent, context->settings->pads_numbers));

    return show_expanded(context, content, "format", &placeholders, BLOCK_SEVERITY_NORMAL);
}

/* ---- memory and disk */

static uint64_t
used_part(
    uint64_t total,
    uint64_t free_part
)
{
    return total - MIN(free_part, total);
}

static int
share_percent(
    uint64_t part,
    uint64_t total
)
{
    if (total == 0) {
        return 0;
    }

    return (int)(part * 100 / total);
}

static void
add_memory_placeholders(
    struct owned_placeholders *placeholders,
    const struct memory_usage *memory,
    bool                       pads_numbers
)
{
    uint64_t used_kibibytes      = used_part(memory->total_kibibytes, memory->available_kibibytes);
    uint64_t swap_used_kibibytes = used_part(memory->swap_total_kibibytes, memory->swap_free_kibibytes);

    add_owned(placeholders, "used", format_size(used_kibibytes * BYTES_PER_KIBIBYTE, pads_numbers));
    add_owned(placeholders, "total", format_size(memory->total_kibibytes * BYTES_PER_KIBIBYTE, pads_numbers));
    add_owned(placeholders, "available", format_size(memory->available_kibibytes * BYTES_PER_KIBIBYTE, pads_numbers));
    add_owned(placeholders, "percent", format_number(share_percent(used_kibibytes, memory->total_kibibytes), pads_numbers));
    add_owned(placeholders, "swap_used", format_size(swap_used_kibibytes * BYTES_PER_KIBIBYTE, pads_numbers));
}

static bool
render_memory(
    const struct render_context *context,
    struct block_content        *content
)
{
    const struct polled_system_values *values       = context->inputs->polled_values;
    struct owned_placeholders          placeholders = {0};

    if (!values->has_memory) {
        return false;
    }

    add_memory_placeholders(&placeholders, &values->memory, context->settings->pads_numbers);

    int used_percent = share_percent(
        used_part(values->memory.total_kibibytes, values->memory.available_kibibytes),
        values->memory.total_kibibytes
    );

    return show_expanded(context, content, "format", &placeholders, rising_severity_of(context, used_percent));
}

static void
add_disk_placeholders(
    struct owned_placeholders     *placeholders,
    const struct filesystem_space *disk,
    bool                           pads_numbers
)
{
    add_owned(placeholders, "free", format_size(disk->available_bytes, pads_numbers));
    add_owned(placeholders, "used", format_size(used_part(disk->total_bytes, disk->available_bytes), pads_numbers));
    add_owned(placeholders, "total", format_size(disk->total_bytes, pads_numbers));
    add_owned(
        placeholders,
        "free_percent",
        format_number(share_percent(disk->available_bytes, disk->total_bytes), pads_numbers)
    );
}

static bool
render_disk(
    const struct render_context *context,
    struct block_content        *content
)
{
    const struct polled_system_values *values       = context->inputs->polled_values;
    struct owned_placeholders          placeholders = {0};

    if (!values->has_disk) {
        return false;
    }

    add_disk_placeholders(&placeholders, &values->disk, context->settings->pads_numbers);

    return show_expanded(
        context,
        content,
        "format",
        &placeholders,
        falling_severity_of(context, share_percent(values->disk.available_bytes, values->disk.total_bytes))
    );
}

/* ---- volume */

static bool
is_volume_known(
    const struct audio_volume_state *volume
)
{
    if (volume == NULL) {
        return false;
    }

    return volume->is_connected;
}

static const char *
volume_format_key(
    const struct audio_volume_state *volume,
    enum block_severity             *severity
)
{
    if (volume->is_muted) {
        *severity = BLOCK_SEVERITY_WARNING;
        return "format-muted";
    }

    *severity = BLOCK_SEVERITY_NORMAL;

    return "format";
}

static bool
render_volume(
    const struct render_context *context,
    struct block_content        *content
)
{
    const struct audio_volume_state *volume       = context->inputs->volume;
    struct owned_placeholders        placeholders = {0};
    enum block_severity              severity     = BLOCK_SEVERITY_NORMAL;

    if (!is_volume_known(volume)) {
        return false;
    }

    add_owned(&placeholders, "volume", format_number(volume->volume_percent, context->settings->pads_numbers));

    return show_expanded(context, content, volume_format_key(volume, &severity), &placeholders, severity);
}

/* ---- power profile and keyboard layout: one text each */

static bool
render_text(
    const struct render_context *context,
    struct block_content        *content,
    const char                  *placeholder_name,
    const char                  *value
)
{
    struct owned_placeholders placeholders = {0};

    if (value == NULL) {
        return false;
    }

    add_copy(&placeholders, placeholder_name, value);

    return show_expanded(context, content, "format", &placeholders, BLOCK_SEVERITY_NORMAL);
}

static bool
render_power_profile(
    const struct render_context *context,
    struct block_content        *content
)
{
    return render_text(context, content, "profile", context->inputs->service_values->power_profile.active_profile);
}

static bool
render_keyboard_layout(
    const struct render_context *context,
    struct block_content        *content
)
{
    return render_text(context, content, "layout", context->inputs->keyboard_layout);
}

/* ---- battery */

static const struct {
    enum battery_charge_state state;
    const char               *format_key;
} battery_formats[] = {
    {BATTERY_CHARGE_STATE_DISCHARGING,   "format-discharging"},
    {BATTERY_CHARGE_STATE_CHARGING,      "format-charging"   },
    {BATTERY_CHARGE_STATE_FULLY_CHARGED, "format-full"       },
};

static const char *
battery_format_key(
    enum battery_charge_state state
)
{
    for (size_t i = 0; i < G_N_ELEMENTS(battery_formats); i++) {
        if (battery_formats[i].state == state) {
            return battery_formats[i].format_key;
        }
    }

    return "format-other";
}

static int64_t
battery_seconds_left(
    const struct battery_charge_status *battery
)
{
    if (battery->state == BATTERY_CHARGE_STATE_CHARGING) {
        return battery->seconds_until_full;
    }

    return battery->seconds_until_empty;
}

/* Time to empty while discharging, to full while charging; empty when unknown. */
static char *
battery_time_text(
    const struct battery_charge_status *battery
)
{
    int64_t seconds = battery_seconds_left(battery);

    if (seconds <= 0) {
        return g_strdup("");
    }

    return human_readable_duration(seconds);
}

/* Only a draining battery is a problem. */
static enum block_severity
battery_severity(
    const struct render_context        *context,
    const struct battery_charge_status *battery
)
{
    if (battery->state != BATTERY_CHARGE_STATE_DISCHARGING) {
        return BLOCK_SEVERITY_NORMAL;
    }

    return falling_severity_of(context, battery->percent);
}

static bool
render_battery(
    const struct render_context *context,
    struct block_content        *content
)
{
    const struct battery_charge_status *battery      = &context->inputs->service_values->battery;
    struct owned_placeholders           placeholders = {0};

    if (!battery->is_present) {
        return false;
    }

    add_owned(&placeholders, "percent", format_number(battery->percent, context->settings->pads_numbers));
    add_owned(&placeholders, "time", battery_time_text(battery));

    return show_expanded(
        context,
        content,
        battery_format_key(battery->state),
        &placeholders,
        battery_severity(context, battery)
    );
}

/* ---- clock: its formats are strftime-style, not placeholders */

static bool
render_clock(
    const struct render_context *context,
    struct block_content        *content
)
{
    const char *format = system_status_line_settings_clock_format(
        context->settings,
        context->inputs->clock_shows_alternate_format
    );
    char *text = g_date_time_format(context->inputs->now, format);

    if (text == NULL) {
        text = g_date_time_format(context->inputs->now, "%a %d %b %H:%M"); // the format was invalid
    }

    content->text     = text;
    content->severity = BLOCK_SEVERITY_NORMAL;

    return true;
}

/* In the order of enum system_status_line_block_kind. */
static bool (*const renderers[SYSTEM_STATUS_LINE_BLOCK_KIND_COUNT])(
    const struct render_context *context,
    struct block_content        *content
) = {
    render_network,
    render_traffic,
    render_bluetooth,
    render_processor,
    render_temperature,
    render_memory,
    render_disk,
    render_volume,
    render_brightness,
    render_power_profile,
    render_battery,
    render_keyboard_layout,
    render_clock,
};

/* ---- the whole line */

static bool
parse_whole_document(
    GMarkupParseContext *parser,
    const char          *document
)
{
    if (!g_markup_parse_context_parse(parser, document, -1, NULL)) {
        return false;
    }

    return g_markup_parse_context_end_parse(parser, NULL);
}

/* Formats may hold Pango markup; a mistake in it would blank the block, so it then shows as plain text. */
static bool
is_valid_markup(
    const char *text
)
{
    static const GMarkupParser no_callbacks = {0};

    g_autofree char *document = g_strconcat("<markup>", text, "</markup>", NULL);

    GMarkupParseContext *parser   = g_markup_parse_context_new(&no_callbacks, 0, NULL, NULL);
    bool                 is_valid = parse_whole_document(parser, document);

    g_markup_parse_context_free(parser);

    return is_valid;
}

static void
make_markup_safe(
    struct block_content *content
)
{
    if (is_valid_markup(content->text)) {
        return;
    }

    char *escaped_text = g_markup_escape_text(content->text, -1);

    g_free(content->text);

    content->text = escaped_text;
}

static const char *
color_for(
    const struct system_status_line_settings *settings,
    enum block_severity                       severity
)
{
    if (severity == BLOCK_SEVERITY_WARNING) {
        return settings->warning_color;
    }

    return settings->text_color;
}

static struct status_bar_block
describe_block(
    const struct system_status_line_settings *settings,
    enum system_status_line_block_kind        kind,
    const struct block_content               *content
)
{
    return (struct status_bar_block){
        .name                  = system_status_line_block_name(kind),
        .full_text             = content->text,
        .color                 = color_for(settings, content->severity),
        .separator_block_width = settings->separator_width,
        .is_urgent             = content->severity == BLOCK_SEVERITY_URGENT,
        .uses_markup           = true,
    };
}

static void
render_block(
    const struct render_context *context,
    GArray                      *blocks,
    GPtrArray                   *texts
)
{
    struct block_content content = {0};

    if (!renderers[context->kind](context, &content)) {
        return;
    }

    make_markup_safe(&content);
    g_ptr_array_add(texts, content.text);

    struct status_bar_block block = describe_block(context->settings, context->kind, &content);

    g_array_append_val(blocks, block);
}

char *
system_status_line_render(
    const struct system_status_line_settings *settings,
    const struct system_status_line_inputs   *inputs
)
{
    g_autoptr(GArray)    blocks = g_array_new(FALSE, FALSE, sizeof(struct status_bar_block));
    g_autoptr(GPtrArray) texts  = g_ptr_array_new_with_free_func(g_free);

    for (size_t i = 0; i < settings->shown_block_count; i++) {
        const struct render_context context = {settings, inputs, settings->shown_blocks[i]};

        render_block(&context, blocks, texts);
    }

    return status_bar_blocks_encode((const struct status_bar_block *)blocks->data, blocks->len);
}
