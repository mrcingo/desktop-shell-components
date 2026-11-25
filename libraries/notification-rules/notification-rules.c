#include <string.h>

#include <glib.h>

#include "notification-rules.h"

static const char section_prefix[] = "app:";

const char *const notification_sound_choice_names[] = {"unless-suppressed", "always", "never", NULL};
const char *const notification_urgency_names[]      = {"low", "normal", "critical", NULL};

enum {
    LONGEST_TIMEOUT_SECONDS = 24 * 60 * 60,
};

struct notification_rules {
    GPtrArray *rules; // struct notification_rule
};

static void
destroy_rule(
    gpointer data
)
{
    struct notification_rule *rule = data;

    g_free(rule->application_name);
    g_free(rule);
}

static struct notification_rule *
read_rule(
    struct program_configuration_file *configuration,
    const char                        *section_name
)
{
    struct notification_rule *rule = g_new0(struct notification_rule, 1);

    *rule = (struct notification_rule){
        .application_name = g_strdup(section_name + strlen(section_prefix)),
        .sound_choice     = program_configuration_file_read_choice(
            configuration,
            section_name,
            "play-sound",
            notification_sound_choice_names,
            NOTIFICATION_RULE_UNSET
        ),
        .urgency = program_configuration_file_read_choice(
            configuration,
            section_name,
            "urgency",
            notification_urgency_names,
            NOTIFICATION_RULE_UNSET
        ),
        .timeout_seconds = program_configuration_file_read_integer(
            configuration,
            section_name,
            "timeout",
            NOTIFICATION_RULE_UNSET,
            0,
            LONGEST_TIMEOUT_SECONDS
        ),
        .ignores = program_configuration_file_read_boolean(configuration, section_name, "ignore", false),
    };

    return rule;
}

struct notification_rules *
notification_rules_load(
    struct program_configuration_file *configuration
)
{
    struct notification_rules *rules = g_new0(struct notification_rules, 1);

    g_auto(GStrv) section_names = program_configuration_file_list_groups_with_prefix(configuration, section_prefix);

    rules->rules = g_ptr_array_new_with_free_func(destroy_rule);

    for (size_t i = 0; section_names[i] != NULL; i++) {
        g_ptr_array_add(rules->rules, read_rule(configuration, section_names[i]));
    }

    return rules;
}

void
notification_rules_destroy(
    struct notification_rules *rules
)
{
    if (rules == NULL) {
        return;
    }

    g_ptr_array_unref(rules->rules);
    g_free(rules);
}

static bool
names_match(
    const char *rule_name,
    const char *given_name
)
{
    if (given_name == NULL) {
        return false;
    }

    return g_ascii_strcasecmp(rule_name, given_name) == 0;
}

static bool
rule_matches(
    const struct notification_rule *rule,
    const char                     *application_name,
    const char                     *desktop_entry
)
{
    if (names_match(rule->application_name, application_name)) {
        return true;
    }

    return names_match(rule->application_name, desktop_entry);
}

const struct notification_rule *
notification_rules_find(
    const struct notification_rules *rules,
    const char                      *application_name,
    const char                      *desktop_entry
)
{
    for (guint i = 0; i < rules->rules->len; i++) {
        const struct notification_rule *rule = g_ptr_array_index(rules->rules, i);

        if (rule_matches(rule, application_name, desktop_entry)) {
            return rule;
        }
    }

    return NULL;
}

static bool
is_ignored(
    const struct notification_rule *rule
)
{
    if (rule == NULL) {
        return false;
    }

    return rule->ignores;
}

static bool
is_silenced_by_do_not_disturb(
    const struct notification_sound_policy *policy,
    const struct notification_request      *notification
)
{
    if (!policy->do_not_disturb) {
        return false;
    }

    if (notification->urgency != NOTIFICATION_URGENCY_CRITICAL) {
        return true;
    }

    return !policy->critical_sounds_during_do_not_disturb;
}

static enum notification_sound_choice
choose_sound(
    const struct notification_sound_policy *policy,
    const struct notification_rule         *rule
)
{
    if (rule == NULL) {
        return policy->default_choice;
    }

    if (rule->sound_choice == NOTIFICATION_RULE_UNSET) {
        return policy->default_choice;
    }

    return rule->sound_choice;
}

static bool
choice_allows_sound(
    enum notification_sound_choice     choice,
    const struct notification_request *notification
)
{
    if (choice == NOTIFICATION_SOUND_UNLESS_SUPPRESSED) {
        return !notification->suppresses_sound;
    }

    return choice == NOTIFICATION_SOUND_ALWAYS;
}

bool
notification_rules_should_play_sound(
    const struct notification_sound_policy *policy,
    const struct notification_rule         *rule,
    const struct notification_request      *notification
)
{
    if (is_ignored(rule)) {
        return false;
    }

    if (is_silenced_by_do_not_disturb(policy, notification)) {
        return false;
    }

    return choice_allows_sound(choose_sound(policy, rule), notification);
}
