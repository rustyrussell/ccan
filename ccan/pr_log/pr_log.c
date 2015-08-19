/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include "pr_log.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <ccan/str/str.h>

#define DEBUG_NEED_INIT INT_MIN
static int debug = DEBUG_NEED_INIT;

bool debug_is(int lvl)
{
	return lvl <= debug_level();
}

int debug_level(void)
{
	if (debug != DEBUG_NEED_INIT)
		return debug;
	char *c = getenv("DEBUG");
	if (!c) {
		debug = CCAN_PR_LOG_DEFAULT_LEVEL;
		return debug;
	}

	debug = atoi(c);
	return debug;
}

void pr_log_(char const *fmt, ...)
{
	int level = INT_MIN;
	if (fmt[0] == '<' && cisdigit(fmt[1]) && fmt[2] == '>')
		level = fmt[1] - '0';

	if (!debug_is(level))
		return;

	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}
