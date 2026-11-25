#include <unistd.h>

#include <glib/gstdio.h>

#include "notification-rules.h"

static struct program_configuration_file *
load_contents(
    const char *contents,
    char      **path
)
{
    int file = g_file_open_tmp("notification-rules-test-XXXXXX.ini", path, NULL);

    g_assert_true(g_file_set_contents(*path, contents, -1, NULL));
    close(file);

    return program_configuration_file_load("notification-rules-test", *path);
}

static struct notification_rules *
load_example_rules(void)
{
    g_autofree char *path = NULL;

    struct program_configuration_file *configuration = load_contents(
        "[app:Slack]\nplay-sound = always\nurgency = critical\n[app:spam]\nignore = true\ntimeout = 3\n",
        &path
    );
    struct notification_rules *rules = notification_rules_load(configuration);

    program_configuration_file_destroy(configuration);
    g_unlink(path);

    return rules;
}

static void
test_rules_are_read_and_matched(void)
{
    struct notification_rules      *rules = load_example_rules();
    const struct notification_rule *slack = notification_rules_find(rules, "slack", NULL);

    g_assert_cmpint(slack->sound_choice, ==, NOTIFICATION_SOUND_ALWAYS);
    g_assert_cmpint(slack->urgency + slack->timeout_seconds, ==, 2 + NOTIFICATION_RULE_UNSET);
    g_assert_true(notification_rules_find(rules, "Other", "SPAM")->ignores);
    g_assert_null(notification_rules_find(rules, "foot", NULL));
    notification_rules_destroy(rules);
}

/* An application that plays its own sound sets suppress-sound and is left alone by default. */
static void
test_sound_decisions(void)
{
    static const struct notification_request quiet_app = {.urgency = NOTIFICATION_URGENCY_NORMAL, .suppresses_sound = true};
    static const struct notification_request critical  = {.urgency = NOTIFICATION_URGENCY_CRITICAL};
    static const struct notification_rule    always    = {
        .sound_choice = NOTIFICATION_SOUND_ALWAYS,
        .urgency      = NOTIFICATION_RULE_UNSET
    };

    struct notification_sound_policy policy = {
        .default_choice                        = NOTIFICATION_SOUND_UNLESS_SUPPRESSED,
        .critical_sounds_during_do_not_disturb = true
    };

    g_assert_true(
        !notification_rules_should_play_sound(&policy, NULL, &quiet_app) &&
        notification_rules_should_play_sound(&policy, &always, &quiet_app)
    );

    policy.do_not_disturb = true;

    g_assert_false(
        notification_rules_should_play_sound(&policy, &always, &quiet_app) ||
        !notification_rules_should_play_sound(&policy, NULL, &critical)
    );
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/notification-rules/read-and-match", test_rules_are_read_and_matched},
    {"/notification-rules/sound",          test_sound_decisions           },
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add_func(registered_tests[i].test_path, registered_tests[i].test_function);
    }

    return g_test_run();
}
