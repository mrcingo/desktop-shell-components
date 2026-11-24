#ifndef REPLACED_EXECUTABLE_RESTART_H
#define REPLACED_EXECUTABLE_RESTART_H

/*
 * After a package upgrade the old program keeps running from its deleted
 * file, and copies started on configuration reload leave because it holds
 * the single-instance lock. This restarts the program from its new file,
 * with the same command line, so an upgrade takes effect on the next
 * "swaymsg reload".
 */

struct replaced_executable_restart;

/* Call before option parsing changes the arguments; they are copied. */
struct replaced_executable_restart *replaced_executable_restart_create(int argument_count, char *arguments[]);

void replaced_executable_restart_destroy(struct replaced_executable_restart *restart);

/*
 * If the running file was replaced, replaces this process with the new
 * file (file descriptors opened with O_CLOEXEC, such as the instance lock,
 * close). Returns only when nothing was replaced or the new file cannot run.
 */
void replaced_executable_restart_if_replaced(const struct replaced_executable_restart *restart);

/* The installed path when link_target names a deleted file, else NULL. Free with g_free(). For tests. */
char *replaced_executable_find_new_path(const char *link_target);

#endif
