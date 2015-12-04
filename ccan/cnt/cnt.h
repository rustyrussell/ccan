#ifndef _CNT_H
#define _CNT_H

#include <sys/types.h>

/**
 * struct cnth - counter handle
 * @nm: counter file name
 * @tmp: temporary file name,
 *       used when remaking counter as sparse file
 * @fd: file descriptor
 * @cnt: the count
 */
typedef struct cnth {
	char *nm, *tmp;
	int fd;
	off_t cnt;
} cnth;

/**
 * cnt_new() - calloc a new handle
 *
 * init fd to -1
 *
 * Return: ptr to new cnth
 */
cnth *cnt_new(void);

/**
 * cnt_free() - free a handle made with cnt_new()
 * @h: handle
 *
 * returns result of cnt_close()
 */
int cnt_free(cnth *h);

/**
 * cnt_open() - open a counter file on disk
 * @h: handle
 * @path: file to open
 *
 * opens and initializes h->cnt with file size;
 * mallocs to hold file names
 *
 * path, if it exists, must be a regular file.
 *
 * Return: -1 on error, h->cnt on success
 */
off_t cnt_open(cnth *h, const char *path);

/**
 * cnt_close() - close file and free file names
 * @h: handle
 *
 * handle is "zeroed" and can be reused with cnt_open()
 *
 * Return: -1 if close(2) error, 0 otherwise
 */
int cnt_close(cnth *);

/**
 * cnt_zero() - set counter and file to zero
 * @h: handle
 *
 * Return: -1 on error, 0 on success
 */
int cnt_zero(cnth *);

/**
 * cnt_sync() - initialize h->cnt with file size
 * @h: handle
 *
 * Return: -1 on error, h->cnt on success
 */
off_t cnt_sync(cnth *);

/**
 * cnt_inc() - increment counter
 * @h: handle
 *
 * increment counter and make file sparse on 4 KiB boundaries
 *
 * Return: -1 on error, 0 on success
 */
int cnt_inc(cnth *);

#endif
