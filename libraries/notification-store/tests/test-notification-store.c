#include "notification-store.h"

static struct notification_request *
create_notification(
    const char *summary,
    const char *replacement_tag
)
{
    struct notification_request *notification = g_rc_box_new0(struct notification_request);

    notification->summary         = g_strdup(summary);
    notification->action_keys     = g_new0(char *, 1);
    notification->replacement_tag = g_strdup(replacement_tag);

    return notification;
}

/* Adds a new notification and drops the caller's reference. */
static uint32_t
add(
    struct notification_store *store,
    const char                *summary,
    const char                *replacement_tag,
    uint32_t                   replaces_id,
    GPtrArray                 *removed
)
{
    struct notification_request *notification = create_notification(summary, replacement_tag);

    notification_store_add(store, notification, replaces_id, removed);

    uint32_t id = notification->id;

    notification_request_unref(notification);

    return id;
}

struct store_fixture {
    struct notification_store *store;
    GPtrArray                 *removed;
};

static void
create_store(
    struct store_fixture *fixture,
    gconstpointer         maximum_count
)
{
    fixture->store   = notification_store_create(GPOINTER_TO_UINT(maximum_count));
    fixture->removed = g_ptr_array_new_with_free_func((GDestroyNotify)notification_request_unref);
}

static void
destroy_store(
    struct store_fixture *fixture,
    gconstpointer         maximum_count
)
{
    notification_store_destroy(fixture->store);
    g_ptr_array_unref(fixture->removed);
}

static void
test_ids_and_replacement(
    struct store_fixture *fixture,
    gconstpointer         maximum_count
)
{
    uint32_t first  = add(fixture->store, "first", NULL, 0, fixture->removed);
    uint32_t second = add(fixture->store, "second", NULL, 0, fixture->removed);

    g_assert_cmpuint(second, ==, first + 1);
    g_assert_cmpuint(add(fixture->store, "first again", NULL, first, fixture->removed), ==, first);
    g_assert_cmpuint(fixture->removed->len, ==, 1);
    g_assert_cmpstr(notification_store_find_latest_unread(fixture->store)->summary, ==, "first again");
}

/* Run with a maximum of two. */
static void
test_tags_replace_and_maximum_drops_oldest(
    struct store_fixture *fixture,
    gconstpointer         maximum_count
)
{
    uint32_t first = add(fixture->store, "volume 10", "volume", 0, fixture->removed);

    g_assert_cmpuint(add(fixture->store, "volume 20", "volume", 0, fixture->removed), ==, first);
    add(fixture->store, "a", NULL, 0, fixture->removed);
    add(fixture->store, "b", NULL, 0, fixture->removed);
    g_assert_cmpuint(fixture->removed->len, ==, 2);
    g_assert_null(notification_store_find(fixture->store, first));
}

static void
test_unread_counting(
    struct store_fixture *fixture,
    gconstpointer         maximum_count
)
{
    g_autoptr(GPtrArray) newly_read = g_ptr_array_new_with_free_func((GDestroyNotify)notification_request_unref);

    add(fixture->store, "a", NULL, 0, fixture->removed);
    add(fixture->store, "b", NULL, 0, fixture->removed);
    g_assert_cmpuint(notification_store_count_unread(fixture->store), ==, 2);
    notification_store_mark_all_read(fixture->store, newly_read);
    g_assert_cmpuint(newly_read->len + notification_store_count_unread(fixture->store), ==, 2);
}

typedef void (*store_test)(struct store_fixture *fixture, gconstpointer maximum_count);

static const struct {
    const char *test_path;
    store_test  test_function;
    guint       maximum_count;
} registered_tests[] = {
    {"/notification-store/ids",    test_ids_and_replacement,                   10},
    {"/notification-store/tags",   test_tags_replace_and_maximum_drops_oldest, 2 },
    {"/notification-store/unread", test_unread_counting,                       10},
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        gconstpointer maximum_count = GUINT_TO_POINTER(registered_tests[i].maximum_count);

        g_test_add(
            registered_tests[i].test_path,
            struct store_fixture,
            maximum_count,
            create_store,
            registered_tests[i].test_function,
            destroy_store
        );
    }

    return g_test_run();
}
