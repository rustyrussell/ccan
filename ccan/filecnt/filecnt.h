#ifndef _CNT_H
#define _CNT_H

#include <sys/types.h>

/**
 * struct filecnt - counter handle
 * @nm: counter file name
 * @tmp: temporary file name,
 *       used when remaking counter as sparse file
 * @fd: file descriptor
 * @cnt: the count
 */
typedef struct filecnt {
	char *nm, *tmp;
	int fd;
	off_t cnt;
} filecnt;

/**
 * filecnt_new() - calloc a new handle
 *
 * init fd to -1
 *
 * Return: ptr to new filecnt
 */
filecnt *filecnt_new(void);

/**
 * filecnt_free() - free a handle made with filecnt_new()
 * @h: handle
 *
 * returns result of filecnt_close()
 */
int filecnt_free(filecnt *h);

/**
 * filecnt_open() - open a counter file on disk
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
off_t filecnt_open(filecnt *h, const char *path);

/**
 * filecnt_close() - close file and free file names
 * @h: handle
 *
 * handle is "zeroed" and can be reused with filecnt_open()
 *
 * Return: -1 if close(2) error, 0 otherwise
 */
int filecnt_close(filecnt *);

/**
 * filecnt_zero() - set counter and file to zero
 * @h: handle
 *
 * Return: -1 on error, 0 on success
 */
int filecnt_zero(filecnt *);

/**
 * filecnt_sync() - initialize h->cnt with file size
 * @h: handle
 *
 * Return: -1 on error, h->cnt on success
 */
off_t filecnt_sync(filecnt *);

/**
 * filecnt_inc() - increment counter
 * @h: handle
 *
 * increment counter and make file sparse on 4 KiB boundaries
 *
 * Return: -1 on error, 0 on success
 */
int filecnt_inc(filecnt *);

#endif
