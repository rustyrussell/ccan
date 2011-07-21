/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_GRAB_FILE_H
#define CCAN_GRAB_FILE_H
#include <stdio.h> // For size_t

/**
 * grab_fd - read all of a file descriptor into memory
 * @ctx: the context to tallocate from (often NULL)
 * @fd: the file descriptor to read from
 * @size: the (optional) size of the file
 *
 * This function reads from the given file descriptor until no more
 * input is available.  The content is talloced off @ctx, and the size
 * of the file places in @size if it's non-NULL.  For convenience, the
 * byte after the end of the content will always be NUL.
 *
 * Example:
 *	#include <ccan/str_talloc/str_talloc.h>
 *	#include <ccan/talloc/talloc.h>
 *	...
 *	// Return all of standard input, as lines.
 *	static char **read_stdin_as_lines(void)
 *	{
 *		char **lines, *all;
 *
 *		all = grab_fd(NULL, 0, NULL);
 *		if (!all)
 *			return NULL;
 *		lines = strsplit(NULL, all, "\n");
 *		talloc_free(all);
 *		return lines;
 *	}
 */
void *grab_fd(const void *ctx, int fd, size_t *size);

/**
 * grab_file - read all of a file (or stdin) into memory
 * @ctx: the context to tallocate from (often NULL)
 * @filename: the file to read (NULL for stdin)
 * @size: the (optional) size of the file
 *
 * This function reads from the given file until no more input is
 * available.  The content is talloced off @ctx, and the size of the
 * file places in @size if it's non-NULL.  For convenience, the byte
 * after the end of the content will always be NUL.
 *
 * Example:
 *	// Return all of a given file, as lines.
 *	static char **read_file_as_lines(const char *filename)
 *	{
 *		char **lines, *all;
 *
 *		all = grab_file(NULL, filename, NULL);
 *		if (!all)
 *			return NULL;
 *		lines = strsplit(NULL, all, "\n");
 *		talloc_free(all);
 *		return lines;
 *	}
 */
void *grab_file(const void *ctx, const char *filename, size_t *size);
#endif /* CCAN_GRAB_FILE_H */
