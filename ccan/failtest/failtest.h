/* Licensed under LGPL - see LICENSE file for details */
#ifndef CCAN_FAILTEST_H
#define CCAN_FAILTEST_H
#include "config.h"
#if HAVE_FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#include <sys/types.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ccan/compiler/compiler.h>
#include <ccan/tlist/tlist.h>

/**
 * failtest_init - initialize the failtest module
 * @argc: the number of commandline arguments
 * @argv: the commandline argument array
 *
 * This initializes the module, and in particular if argv[1] is "--failpath="
 * then it ensures that failures follow that pattern.  This allows easy
 * debugging of complex failure paths.
 */
void failtest_init(int argc, char *argv[]);

/**
 * failtest_exit - clean up and exit the test
 * @status: the status (usually exit_status() from ccan/tap).
 *
 * This cleans up and changes to files made in this child, and exits the test.
 * It also calls your failtest_default_hook, if any.
 *
 * A child which does not exit via failtest_exit() will cause the overall test
 * to fail.
 */
void NORETURN failtest_exit(int status);

/**
 * enum failtest_call_type - discriminator for failtest_call.u
 */
enum failtest_call_type {
	FAILTEST_MALLOC,
	FAILTEST_CALLOC,
	FAILTEST_REALLOC,
	FAILTEST_OPEN,
	FAILTEST_CLOSE,
	FAILTEST_PIPE,
	FAILTEST_READ,
	FAILTEST_WRITE,
	FAILTEST_FCNTL,
	FAILTEST_MMAP,
	FAILTEST_LSEEK
};

struct calloc_call {
	void *ret;
	size_t nmemb;
	size_t size;
};

struct malloc_call {
	void *ret;
	size_t size;
};

struct realloc_call {
	void *ret;
	void *ptr;
	size_t size;
};

struct open_call {
	int ret;
	const char *pathname;
	int flags;
	mode_t mode;
	bool always_save;
	bool closed;
	/* This is used for O_TRUNC opens on existing files. */
	struct contents_saved *saved;
};

struct close_call {
	int fd;
};

struct pipe_call {
	int ret;
	int fds[2];
	bool closed[2];
};

struct read_call {
	ssize_t ret;
	off_t off;
	int fd;
	void *buf;
	size_t count;
};

struct write_call {
	ssize_t ret;
	int fd;
	const void *buf;
	size_t count;
	off_t off;
	bool is_pwrite;
	struct failtest_call *opener;
	struct contents_saved *saved;
};

struct fcntl_call {
	int ret;
	int fd;
	int cmd;
	union {
		struct flock fl;
		long l;
		int i;
	} arg;
};

struct mmap_call {
	void *ret;
	void *addr;
	size_t length;
	int prot;
	int flags;
	int fd;
	off_t offset;
	struct failtest_call *opener;
	struct contents_saved *saved;
};

struct lseek_call {
	ssize_t ret;
	int fd;
	off_t offset;
	int whence;
	off_t old_off;
};

/**
 * struct failtest_call - description of a call redirected to failtest module
 * @type: the call type
 * @file: the filename of the caller
 * @line: the line number of the caller
 * @fail: did this call fail
 * @error: the errno (if any)
 * @u: the union of call data
 *
 * This structure is used to represent the ordered history of calls.
 *
 * See Also:
 *	failtest_hook, failtest_exit_check
 */
struct failtest_call {
	/* We're in the history list. */
	struct list_node list;
	enum failtest_call_type type;
	/* Where we were called from. */
	const char *file;
	unsigned int line;
	/* Did we fail? */
	bool fail;
	/* What we set errno to. */
	int error;
	/* How do we clean this up? */
	void (*cleanup)(void *u, bool restore);
	/* Should their program have cleaned up? */
	bool can_leak;
	/* Backtrace of call chain. */
	void **backtrace;
	unsigned int backtrace_num;
	/* The actual call data. */
	union {
		struct calloc_call calloc;
		struct malloc_call malloc;
		struct realloc_call realloc;
		struct open_call open;
		struct close_call close;
		struct pipe_call pipe;
		struct read_call read;
		struct write_call write;
		struct fcntl_call fcntl;
		struct mmap_call mmap;
		struct lseek_call lseek;
	} u;
};

/* This defines struct tlist_calls. */
TLIST_TYPE(calls, struct failtest_call);

enum failtest_result {
	/* Yes try failing this call. */
	FAIL_OK,
	/* No, don't try failing this call. */
	FAIL_DONT_FAIL,
	/* Try failing this call but don't go too far down that path. */
	FAIL_PROBE,
};

/**
 * failtest_hook - whether a certain call should fail or not.
 * @history: the ordered history of all failtest calls.
 *
 * The default value of this hook is failtest_default_hook(), which returns
 * FAIL_OK (ie. yes, fail the call).
 *
 * You can override it, and avoid failing certain calls.  The parameters
 * of the call (but not the return value(s)) will be filled in for the last
 * call.
 *
 * Example:
 *	static enum failtest_result dont_fail_alloc(struct tlist_calls *history)
 *	{
 *		struct failtest_call *call;
 *		call = tlist_tail(history, list);
 *		if (call->type == FAILTEST_MALLOC
 *			|| call->type == FAILTEST_CALLOC
 *			|| call->type == FAILTEST_REALLOC)
 *			return FAIL_DONT_FAIL;
 *		return FAIL_OK;
 *	}
 *	...
 *		failtest_hook = dont_fail_alloc;
 */
extern enum failtest_result (*failtest_hook)(struct tlist_calls *history);

/**
 * failtest_exit_check - hook for additional checks on a failed child.
 * @history: the ordered history of all failtest calls.
 *
 * Your program might have additional checks to do on failure, such as
 * check that a file is not corrupted, or than an error message has been
 * logged.
 *
 * If this returns false, the path to this failure will be printed and the
 * overall test will fail.
 */
extern bool (*failtest_exit_check)(struct tlist_calls *history);

/**
 * failtest_has_failed - determine if a failure has occurred.
 *
 * Sometimes you want to exit immediately if you've experienced an
 * injected failure.  This is useful when you have four separate tests
 * in your test suite, and you don't want to do the next one if you've
 * had a failure in a previous one.
 */
extern bool failtest_has_failed(void);

/**
 * failtest_timeout_ms - how long to wait before killing child.
 *
 * Default is 20,000 (20 seconds).
 */
extern unsigned int failtest_timeout_ms;
#endif /* CCAN_FAILTEST_H */
