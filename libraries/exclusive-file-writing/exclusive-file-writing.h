#ifndef EXCLUSIVE_FILE_WRITING_H
#define EXCLUSIVE_FILE_WRITING_H

#include <stdbool.h>
#include <sys/types.h>

#include <gio/gio.h>

/*
 * Saving new files, such as screenshots, safely: the file appears under its
 * name only once all of it is written, and an existing file is never
 * replaced. The folder must lie inside an allowed root, symbolic links
 * resolved, so a link cannot send files elsewhere.
 */

/*
 * Opens folder_path for new files, creating missing folders with mode 0700.
 * Returns a descriptor, or -1 with error set when the folder resolves
 * outside allowed_root or cannot be opened.
 */
int exclusive_file_writing_open_folder(const char *folder_path, const char *allowed_root, GError **error);

/*
 * Writes contents as the new file file_name (a plain name, no slash) in the
 * folder. Fails with G_IO_ERROR_EXISTS when the name is taken, so callers
 * can try the next name.
 */
bool exclusive_file_writing_write(int folder_descriptor, const char *file_name, GBytes *contents, mode_t mode, GError **error);

#endif
