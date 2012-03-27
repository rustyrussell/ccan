#include <ccan/asprintf/asprintf.h>
/* Include the C files directly. */

#include <stdarg.h>
/* Override vasprintf for testing. */
#if HAVE_ASPRINTF
#define vasprintf my_vasprintf
static int my_vasprintf(char **strp, const char *fmt, va_list ap);
#else
#include <stdio.h>
#define vsnprintf my_vsnprintf
static int my_vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif

#include <ccan/asprintf/asprintf.c>
#include <ccan/tap/tap.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static bool fail;

#if HAVE_ASPRINTF
#undef vasprintf
static int my_vasprintf(char **strp, const char *fmt, va_list ap)
{
	if (fail) {
		/* Set strp to crap. */
		*strp = (char *)(long)1;
		return -1;
	}
	return vasprintf(strp, fmt, ap);
}
#else
#undef vsnprintf
static int my_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	if (fail) {
		return -1;
	}
	return vsnprintf(str, size, format, ap);
}
#endif

int main(void)
{
	char *p, nul = '\0';
	int ret;

	/* This is how many tests you plan to run */
	plan_tests(8);

	fail = false;
	p = afmt("Test %u%cafter-nul", 1, nul);
	ok1(p);
	ok1(strlen(p) == strlen("Test 1"));
	ok1(memcmp(p, "Test 1\0after-nul\0", 17) == 0);
	free(p);

	ret = asprintf(&p, "Test %u%cafter-nul", 1, nul);
	ok1(ret == 16);
	ok1(p);
	ok1(strlen(p) == strlen("Test 1"));
	ok1(memcmp(p, "Test 1\0after-nul\0", 17) == 0);
	free(p);

	fail = true;
	p = afmt("Test %u%cafter-nul", 1, nul);
	ok1(p == NULL);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
