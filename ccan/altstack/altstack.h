/* Licensed under Apache License v2.0 - see LICENSE file for details */
#ifndef CCAN_ALTSTACK_H
#define CCAN_ALTSTACK_H
#include "config.h"

#if ! __x86_64__
#error "This code expects the AMD64 ABI, but __x86_64__ is false."
#endif

#include <stddef.h>
#include <sys/resource.h>

#define ALTSTACK_ERR_MAXLEN 128

/**
 * altstack - run a function with a dedicated stack, and then release the memory
 * @max: the maximum size of the new stack
 * @fn: a function to run
 * @arg: an argument passed to fn
 * @out: where to store the return of fn, optional
 *
 * rlimit is set to @max, and an anonymous noreserve mapping is made.
 * A jump buffer is setup and a signal handler established for SIGSEGV.
 * The rsp register is set to the mapped address, with the old rsp value
 * pushed onto the new stack. The provided @fn is called, with @arg as
 * its only argument, from non-stack addresses. Once @fn returns,
 * rsp is popped off the stack. If @out is non-null, it gets the return
 * value from @fn. The region is unmapped and the other changes undone.
 *
 * Error messages are appended to a buffer available via altstack_geterr()
 * and altstack_perror(). errno is set by the failing call, or set to
 * EOVERFLOW in case SIGSEGV is caught.
 *
 * altstack() uses thread-local storage, and should not be nested.
 *
 * Example:
 *	// permit recursion depth over a million
 *	// a contrived example! (-O2 replaces the recursion with a loop)
 *	#include <assert.h>
 *	#include <stdio.h>
 *	#include <stdlib.h>
 *	#include <ccan/altstack/altstack.h>
 *
 *	unsigned depth;
 *
 *	static void dn(unsigned long i)
 *	{
 *		depth++;
 *		if (i) dn(--i);
 *	}
 *
 *	static void *wrap(void *i)
 *	{
 *		dn((unsigned long) i);
 *		return 0;
 *	}
 *
 *	#define MiB (1024UL*1024UL)
 *	int main(int argc, char *argv[])
 *	{
 *		unsigned long n;
 *		assert(argc == 2);
 *		n = strtoul(argv[1], 0, 0);
 *
 *		if (altstack(32*MiB, wrap, (void *) n, 0) != 0)
 *			altstack_perror();
 *
 *		printf("%d\n", depth);
 *
 *		return 0;
 *	}
 *
 * Returns: -1 on error; 0 on success; 1 on error after @fn returns
 */
int altstack(rlim_t max, void *(*fn)(void *), void *arg, void **out);

/**
 * altstack_perror - print error messages to stderr
 */
void altstack_perror(void);

/**
 * altstack_geterr - return the error buffer
 *
 * The error buffer is static thread-local storage.
 * The buffer is reset with each altstack() call.
 *
 * Returns: pointer to the error buffer
 */
char *altstack_geterr(void);

/**
 * altstack_used - return amount of stack used
 *
 * This captures the current rsp value and returns
 * the difference from the initial rsp value.
 *
 * Note: this can be used with any stack, including the original.
 * When using with a non-altstack stack, call altstack_rsp_save()
 * as early as possible to establish the initial value.
 *
 * Returns: difference of rsp values
 */
ptrdiff_t altstack_used(void);

/**
 * altstack_max - return usable stack size
 *
 * Returns: max value from altstack() call
 */
rlim_t altstack_max(void);

/**
 * altstack_remn - return amount of stack remaining
 *
 * Returns: altstack_max() minus altstack_used()
 */
#define altstack_remn() (altstack_max() - altstack_used())

/**
 * altstack_rsp_save - set initial rsp value
 *
 * Capture the current value of rsp for future altstack_used()
 * calculations. altstack() also saves the initial rsp, so
 * this should only be used in non-altstack contexts.
 */
void altstack_rsp_save(void);
#endif
