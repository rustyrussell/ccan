#ifndef _CNT_H
#define _CNT_H

#include <sys/types.h>

/**
 * struct szcnt - counter handle
 * @nm: counter file name
 * @tmp: temporary file name,
 *       used when remaking counter as sparse file
 * @fd: file descriptor
 * @cnt: the count
 */
typedef struct szcnt {
	char *nm, *tmp;
	int fd;
	off_t cnt;
} szcnt;

/**
 * szcnt_new() - calloc a new handle
 *
 * init fd to -1
 *
 * Return: ptr to new szcnt
 */
szcnt *szcnt_new(void);

/**
 * szcnt_free() - free a handle made with szcnt_new()
 * @h: handle
 *
 * returns result of szcnt_close()
 */
int szcnt_free(szcnt *h);

/**
 * szcnt_open() - open a counter file on disk
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
off_t szcnt_open(szcnt *h, const char *path);

/**
 * szcnt_close() - close file and free file names
 * @h: handle
 *
 * handle is "zeroed" and can be reused with szcnt_open()
 *
 * Return: -1 if close(2) error, 0 otherwise
 */
int szcnt_close(szcnt *);

/**
 * szcnt_zero() - set counter and file to zero
 * @h: handle
 *
 * Return: -1 on error, 0 on success
 */
int szcnt_zero(szcnt *);

/**
 * szcnt_sync() - initialize h->cnt with file size
 * @h: handle
 *
 * Return: -1 on error, h->cnt on success
 */
off_t szcnt_sync(szcnt *);

/**
 * szcnt_inc() - increment counter
 * @h: handle
 *
 * increment counter and make file sparse on 4 KiB boundaries
 *
 * Return: -1 on error, 0 on success
 */
int szcnt_inc(szcnt *);

#endif
