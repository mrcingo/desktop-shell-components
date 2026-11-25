#ifndef NOTIFICATION_RULES_H
#define NOTIFICATION_RULES_H

#include <stdbool.h>

#include "notification-request.h"
#include "program-configuration-file.h"

/*
 * Settings for one application's notifications, from sections such as
 *
 *   [app:Slack]
 *   play-sound = always
 *   urgency = critical
 *   timeout = 30
 *   ignore = false
 *
 * The name after "app:" matches the application name or desktop entry the
 * notification gives, ignoring case. Unset settings are NOTIFICATION_RULE_UNSET.
 */

enum notification_sound_choice {
    NOTIFICATION_SOUND_UNLESS_SUPPRESSED,
    NOTIFICATION_SOUND_ALWAYS,
    NOTIFICATION_SOUND_NEVER,
};

enum {
    NOTIFICATION_RULE_UNSET = -1,
};

/* The setting values of enum notification_sound_choice, in order, NULL-terminated. */
extern const char *const notification_sound_choice_names[];

/* The setting values of enum notification_urgency, in order, NULL-terminated. */
extern const char *const notification_urgency_names[];

struct notification_rule {
    char *application_name;
    int   sound_choice;    // enum notification_sound_choice or NOTIFICATION_RULE_UNSET
    int   urgency;         // enum notification_urgency or NOTIFICATION_RULE_UNSET
    int   timeout_seconds; // 0 for never, or NOTIFICATION_RULE_UNSET
    bool  ignores;         // accepted but not kept, counted or sounded
};

struct notification_rules;

/* Reads every [app:...] section. Never returns NULL. */
struct notification_rules *notification_rules_load(struct program_configuration_file *configuration);

void notification_rules_destroy(struct notification_rules *rules);

/* What decides whether a notification makes a sound, besides its own hints and its rule. */
struct notification_sound_policy {
    enum notification_sound_choice default_choice;
    bool                           do_not_disturb;
    bool                           critical_sounds_during_do_not_disturb;
};

/*
 * Whether the notification should sound: never for an ignored
 * application or during do-not-disturb (critical ones aside, if allowed);
 * otherwise as the rule's play-sound says, else the default choice.
 * "unless-suppressed" stays quiet for notifications with the
 * suppress-sound hint, which applications set when they play their own.
 * rule may be NULL.
 */
bool notification_rules_should_play_sound(
    const struct notification_sound_policy *policy,
    const struct notification_rule         *rule,
    const struct notification_request      *notification
);

/* desktop_entry may be NULL. NULL when no rule matches. */
const struct notification_rule *
notification_rules_find(const struct notification_rules *rules, const char *application_name, const char *desktop_entry);

#endif
