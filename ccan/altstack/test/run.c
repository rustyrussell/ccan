#include "config.h"
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <ccan/tap/tap.h>
#include <ccan/altstack/altstack.h>
#include <stdio.h>

enum {
	getrlimit_	= 1<<0,
	setrlimit_	= 1<<1,
	mmap_		= 1<<2,
	sigaltstack_	= 1<<3,
	sigaction_	= 1<<4,
	munmap_		= 1<<5,
};
int fail, call1, call2;
char *m_;
rlim_t msz_;
#define e(x) (900+(x))
#define seterr(x) (errno = e(x))
#define setcall(x) ((call1 |= !errno ? (x) : 0), (call2 |= errno || out_ ? (x) : 0))
#define getrlimit(...)		(fail&getrlimit_	? (seterr(getrlimit_),		-1) : (setcall(getrlimit_),	getrlimit(__VA_ARGS__)))
#define mmap(...)		(fail&mmap_		? (seterr(mmap_),	(void *)-1) : (setcall(mmap_),		mmap(__VA_ARGS__)))
#define munmap(a, b)		(fail&munmap_		? (seterr(munmap_),		-1) : (setcall(munmap_),	munmap(m_=(a), msz_=(b))))
#define setrlimit(...)		(fail&setrlimit_	? (seterr(setrlimit_),		-1) : (setcall(setrlimit_),	setrlimit(__VA_ARGS__)))
#define sigaltstack(...)	(fail&sigaltstack_	? (seterr(sigaltstack_),	-1) : (setcall(sigaltstack_),	sigaltstack(__VA_ARGS__)))
#define sigaction(...)		(fail&sigaction_	? (seterr(sigaction_),		-1) : (setcall(sigaction_),	sigaction(__VA_ARGS__)))

#define KiB (1024UL)
#define MiB (KiB*KiB)
#define GiB (MiB*KiB)
#define TiB (GiB*KiB)

FILE *mystderr;
#undef stderr
#define stderr mystderr
#undef ok
#include <ccan/altstack/altstack.c>
#undef ok

long used;

static void __attribute__((optimize("O0"))) dn(unsigned long i)
{
	if (used) used = altstack_used();
	if (i) dn(--i);
}
static void *wrap(void *i)
{
	dn((unsigned long) i);
	return wrap;
}

#define chkfail(x, y, z, c1, c2)					\
	do {								\
		call1 = 0;						\
		call2 = 0;						\
		errno = 0;						\
		ok1((fail = x) && (y));					\
		ok1(errno == (z));					\
		ok1(call1 == (c1));					\
		ok1(call2 == (c2));					\
	} while (0);

#define chkok(y, z, c1, c2)						\
	do {								\
		call1 = 0;						\
		call2 = 0;						\
		errno = 0;						\
		fail = 0;						\
		ok1((y));						\
		ok1(errno == (z));					\
		ok1(call1 == (c1));					\
		ok1(call2 == (c2));					\
	} while (0)

int main(void)
{
	long pgsz = sysconf(_SC_PAGESIZE);

	plan_tests(50);

	chkfail(getrlimit_,	altstack(8*MiB, wrap, 0, 0) == -1, e(getrlimit_),
		0,
		0);

	chkfail(setrlimit_,	altstack(8*MiB, wrap, 0, 0) == -1, e(setrlimit_),
		getrlimit_,
		0);

	chkfail(mmap_,		altstack(8*MiB, wrap, 0, 0) == -1, e(mmap_),
		getrlimit_|setrlimit_,
		setrlimit_);

	chkfail(sigaltstack_,	altstack(8*MiB, wrap, 0, 0) == -1, e(sigaltstack_),
		getrlimit_|setrlimit_|mmap_,
		setrlimit_|munmap_);

	chkfail(sigaction_,	altstack(8*MiB, wrap, 0, 0) == -1, e(sigaction_),
		getrlimit_|setrlimit_|mmap_|sigaltstack_,
		setrlimit_|munmap_|sigaltstack_);

	chkfail(munmap_,	altstack(8*MiB, wrap, 0, 0) ==  1, e(munmap_),
		getrlimit_|setrlimit_|mmap_|sigaltstack_|sigaction_,
		setrlimit_|sigaltstack_|sigaction_);
	if (fail = 0, munmap(m_, msz_) == -1)
		err(1, "munmap");

	chkok(			altstack(1*MiB, wrap, (void *) 1000000, 0) == -1, EOVERFLOW,
		getrlimit_|setrlimit_|mmap_|sigaltstack_|sigaction_,
		setrlimit_|munmap_|sigaltstack_|sigaction_);

	// be sure segv catch is repeatable (SA_NODEFER)
	chkok(			altstack(1*MiB, wrap, (void *) 1000000, 0) == -1, EOVERFLOW,
		getrlimit_|setrlimit_|mmap_|sigaltstack_|sigaction_,
		setrlimit_|munmap_|sigaltstack_|sigaction_);

	used = 1;
	chkfail(munmap_,	altstack(1*MiB, wrap, (void *) 1000000, 0) == -1, EOVERFLOW,
		getrlimit_|setrlimit_|mmap_|sigaltstack_|sigaction_,
		setrlimit_|sigaltstack_|sigaction_);
	if (fail = 0, munmap(m_, msz_) == -1)
		err(1, "munmap");

	ok1(altstack_max() == 1*MiB);
	diag("used: %lu", used);
	ok1(used >= 1*MiB - pgsz && used <= 1*MiB + pgsz);

	char *p;
	for(p = altstack_geterr(); *p; p++)
		if (*p >= '0' && *p <= '9')
			*p = '~';

	#define estr "(altstack@~~~) SIGSEGV caught; (altstack@~~~) munmap(m, max): Unknown error ~~~"
	ok1(strcmp(altstack_geterr(), estr) == 0);

	char buf[ALTSTACK_ERR_MAXLEN*2] = {0};
	if ((mystderr = fmemopen(buf, sizeof(buf), "w")) == NULL)
		err(1, "fmemopen");

	altstack_perror();
	fflush(mystderr);
	ok1(strcmp(buf, estr "\n") == 0);

	used = 1;
	chkok(			altstack(8*MiB, wrap, (void *) 1000000, 0) == -1, EOVERFLOW,
		getrlimit_|setrlimit_|mmap_|sigaltstack_|sigaction_,
		setrlimit_|munmap_|sigaltstack_|sigaction_);

	diag("used: %lu", used);
	ok1(used >= 8*MiB - pgsz && used <= 8*MiB + pgsz);

	used = 0;
	chkok(			altstack(8*MiB, wrap, (void *) 100000, 0) == 0, 0,
		getrlimit_|setrlimit_|mmap_|sigaltstack_|sigaction_|munmap_,
		setrlimit_|munmap_|sigaltstack_|sigaction_);

	used = 1;
	altstack_rsp_save();
	dn(0);
	diag("used: %lu", used);
	ok1(used == 32 || used == 40);

	return exit_status();
}
