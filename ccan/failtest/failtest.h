#ifndef CCAN_FAILTEST_H
#define CCAN_FAILTEST_H
#include <sys/types.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ccan/compiler/compiler.h>

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
	FAILTEST_PIPE,
	FAILTEST_READ,
	FAILTEST_WRITE,
	FAILTEST_FCNTL,
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
	int dup_fd;
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
	off_t old_filelen;
	off_t saved_len;
	void *saved_contents;
	int dup_fd;
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
	enum failtest_call_type type;
	/* Where we were called from. */
	const char *file;
	unsigned int line;
	/* Did we fail? */
	bool fail;
	/* What we set errno to. */
	int error;
	/* How do we clean this up? */
	void (*cleanup)(void *u);
	/* The actual call data. */
	union {
		struct calloc_call calloc;
		struct malloc_call malloc;
		struct realloc_call realloc;
		struct open_call open;
		struct pipe_call pipe;
		struct read_call read;
		struct write_call write;
		struct fcntl_call fcntl;
	} u;
};

/**
 * failtest_hook - whether a certain call should fail or not.
 * @history: the ordered history of all failtest calls.
 * @num: the number of elements in @history (greater than 0)
 *
 * The default value of this hook is failtest_default_hook(), which returns
 * true (ie. yes, fail the call).
 *
 * You can override it, and avoid failing certain calls.  The parameters
 * of the call (but not the return value(s)) will be filled in for the last
 * call.
 *
 * Example:
 *	static bool dont_fail_allocations(struct failtest_call *history,
 *					  unsigned num)
 *	{
 *		return history[num-1].type != FAILTEST_MALLOC
 *			&& history[num-1].type != FAILTEST_CALLOC
 *			&& history[num-1].type != FAILTEST_REALLOC;
 *	}
 *	...
 *		failtest_hook = dont_fail_allocations;
 */
extern bool (*failtest_hook)(struct failtest_call *history, unsigned num);

/**
 * failtest_exit_check - hook for additional checks on a failed child.
 * @history: the ordered history of all failtest calls.
 * @num: the number of elements in @history (greater than 0)
 *
 * Your program might have additional checks to do on failure, such as
 * check that a file is not corrupted, or than an error message has been
 * logged.
 *
 * If this returns false, the path to this failure will be printed and the
 * overall test will fail.
 */
extern bool (*failtest_exit_check)(struct failtest_call *history,
				   unsigned num);

/* This usually fails the call. */
bool failtest_default_hook(struct failtest_call *history, unsigned num);

/**
 * failtest_timeout_ms - how long to wait before killing child.
 *
 * Default is 20,000 (20 seconds).
 */
extern unsigned int failtest_timeout_ms;
#endif /* CCAN_FAILTEST_H */
