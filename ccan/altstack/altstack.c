/* Licensed under Apache License v2.0 - see LICENSE file for details */
#include "config.h"
#include "altstack.h"

#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

static __thread char ebuf[ALTSTACK_ERR_MAXLEN];
static __thread unsigned elen;

#define bang(x)							\
	(elen += snprintf(ebuf + elen, sizeof(ebuf) - elen,	\
		 "%s(altstack@%d) %s%s%s",			\
		 elen  ? "; " : "", __LINE__, (x),		\
		 errno ? ": " : "", errno ? strerror(errno) : ""))

void altstack_perror(void)
{
	fprintf(stderr, "%s\n", ebuf);
}

char *altstack_geterr(void)
{
	return ebuf;
}

static __thread jmp_buf jmp;

static void segvjmp(int signum)
{
	longjmp(jmp, 1);
}

static __thread void *rsp_save_[2];
static __thread rlim_t max_;

rlim_t altstack_max(void) {
	return max_;
}

static ptrdiff_t rsp_save(unsigned i) {
	assert(i < 2);
	asm volatile ("movq %%rsp, %0" : "=g" (rsp_save_[i]));
	return (char *) rsp_save_[0] - (char *) rsp_save_[i];
}

void altstack_rsp_save(void) {
	rsp_save(0);
}

ptrdiff_t altstack_used(void) {
	return rsp_save(1);
}

static __thread void *(*fn_)(void *);
static __thread void *arg_, *out_;

int altstack(rlim_t max, void *(*fn)(void *), void *arg, void **out)
{
	long pgsz = sysconf(_SC_PAGESIZE);
	int ret = -1, undo = 0;
	char *m;
	struct rlimit rl_save;
	struct sigaction sa_save;
	int errno_save;

	assert(max > 0 && fn);
	#define ok(x, y) ({ long __r = (long) (x); if (__r == -1) { bang(#x); if (y) goto out; } __r; })

	fn_  = fn;
	arg_ = arg;
	out_ = 0;
	max_ = max;
	ebuf[elen = 0] = '\0';
	if (out) *out = 0;

	// if the first page below the mapping is in use, we get max-pgsz usable bytes
	// add pgsz to max to guarantee at least max usable bytes
	max += pgsz;

	ok(getrlimit(RLIMIT_STACK, &rl_save), 1);
	ok(setrlimit(RLIMIT_STACK, &(struct rlimit) { max_, rl_save.rlim_max }), 1);
	undo++;

	ok(m = mmap(0, max, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_GROWSDOWN|MAP_NORESERVE, -1, 0), 1);
	undo++;

	if (setjmp(jmp) == 0) {
		unsigned char sigstk[SIGSTKSZ];
		stack_t ss = { .ss_sp = sigstk, .ss_size = sizeof(sigstk) };
		struct sigaction sa = { .sa_handler = segvjmp, .sa_flags = SA_NODEFER|SA_RESETHAND|SA_ONSTACK };

		ok(sigaltstack(&ss, 0), 1);
		undo++;

		sigemptyset(&sa.sa_mask);
		ok(sigaction(SIGSEGV, &sa, &sa_save), 1);
		undo++;

		asm volatile (
			"mov %%rsp, %%r10\n\t"
			"mov %1, %%rsp\n\t"
			"sub $8, %%rsp\n\t"
			"push %%r10"
			: "=r" (rsp_save_[0]) : "0" (m + max) : "r10", "memory");
		out_ = fn_(arg_);
		asm volatile ("pop %%rsp"
			      : : : "memory");
		ret = 0;
		if (out) *out = out_;
	}
	else {
		errno = 0;
		bang("SIGSEGV caught");
		errno = EOVERFLOW;
	}

out:
	errno_save = errno;

	switch (undo) {
	case 4:
		ok(sigaction(SIGSEGV, &sa_save, 0), 0);
	case 3:
		ok(sigaltstack(&(stack_t) { .ss_flags = SS_DISABLE }, 0), 0);
	case 2:
		ok(munmap(m, max), 0);
	case 1:
		ok(setrlimit(RLIMIT_STACK, &rl_save), 0);
	}

	if (errno_save)
		errno = errno_save;
	return !ret && elen ? 1 : ret;
}
