#include "config.h"
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ccan/tap/tap.h>

/* We don't actually want it to exit... */
static jmp_buf exited;
#define exit(status) longjmp(exited, (status) + 1)

#define printf saved_printf
static int saved_printf(const char *fmt, ...);

#define fprintf saved_fprintf
static int saved_fprintf(FILE *ignored, const char *fmt, ...);

#define vfprintf saved_vfprintf
static int saved_vfprintf(FILE *ignored, const char *fmt, va_list ap);

/* Hack to avoid a memory leak which valgrind complains about. */
#define realloc set_realloc
static void *set_realloc(void *ptr, size_t size);

#define free set_free
static void set_free(void *ptr);

/* Include the C files directly. */
#include <ccan/failtest/failtest.c>

#undef realloc
#undef free

static char *buffer;
static void *set_realloc(void *ptr, size_t size)
{
	return buffer = realloc(ptr, size);
}

static void set_free(void *ptr)
{
	if (ptr == buffer)
		buffer = NULL;
	free(ptr);
}

static char *output = NULL;

static int saved_vprintf(const char *fmt, va_list ap)
{
	int ret;
	int len = 0;
	va_list ap2;

	va_copy(ap2, ap);
	ret = vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);

	if (output)
		len = strlen(output);

	output = realloc(output, len + ret + 1);
	return vsprintf(output + len, fmt, ap);
}

static int saved_vfprintf(FILE *ignored, const char *fmt, va_list ap)
{
	return saved_vprintf(fmt, ap);
}

static int saved_printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = saved_vprintf(fmt, ap);
	va_end(ap);
	return ret;
}	

static int saved_fprintf(FILE *ignored, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = saved_vprintf(fmt, ap);
	va_end(ap);
	return ret;
}	

int main(void)
{
	int status;

	plan_tests(3);
	failtest_init(0, NULL);

	status = setjmp(exited);
	if (status == 0) {
		char *p = failtest_malloc(1, "run-malloc.c", 1);
		/* If we just segv, valgrind counts that as a failure.
		 * So kill ourselves creatively. */
		if (!p)
			kill(getpid(), SIGSEGV);
		fail("Expected child to crash!");
	} else {
		ok1(status == 2);
		ok1(strstr(output, "Killed by signal"));
		ok1(strstr(output, "--failpath=M\n"));
	}
	free(buffer);
	return exit_status();
}
