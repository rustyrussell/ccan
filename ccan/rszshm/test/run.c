#define _GNU_SOURCE
#include <err.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ccan/rszshm/rszshm.h>
#include <ccan/tap/tap.h>

#include <sys/mman.h>
#include <sys/file.h>

int fail_close, fail_flock, fail_ftruncate, fail_msync, fail_munmap, fail_open;
#define close(...)	(fail_close	? errno = 9000, -1 : close(__VA_ARGS__))
#define flock(...)	(fail_flock	? errno = 9001, -1 : flock(__VA_ARGS__))
#define ftruncate(...)	(fail_ftruncate	? errno = 9002, -1 : ftruncate(__VA_ARGS__))
#define msync(...)	(fail_msync	? errno = 9003, -1 : msync(__VA_ARGS__))
#define munmap(...)	(fail_munmap	? errno = 9004, -1 : munmap(__VA_ARGS__))
#define open(...)	(fail_open	? errno = 9005, -1 : open(__VA_ARGS__))

int fail_read, short_read;
#define read(...)	(fail_read	? errno = 9006, -1 : short_read ? 1 : read(__VA_ARGS__))

int fail_mmap_anon, fail_mmap_fixed, bad_mmap_addr;
#define mmap(adr, len, rw, flags, fd, off) (					\
	fail_mmap_anon  && (flags) & MAP_ANON  ? errno = 9010, MAP_FAILED :	\
	fail_mmap_fixed && (flags) & MAP_FIXED ? errno = 9011, MAP_FAILED :	\
	bad_mmap_addr ? NULL :							\
	mmap(adr, len, rw, flags, fd, off)					\
)
#include <ccan/rszshm/rszshm.c>

#define noerr(x) ({ int n = (x); if (n == -1) err(1, "%s", #x); n; })

#define longstr \
".................................................................................................................................."

static jmp_buf j;
static struct sigaction sa;
static void segvjmp(int signum)
{
	longjmp(j, 1);
}

int main(void)
{
	plan_tests(37);

	ok1(rszshm_mk(NULL, 0, NULL) == NULL && errno == EINVAL);

	struct rszshm s, t;
	ok1(rszshm_mk(&s, 0, NULL) == NULL && errno == EINVAL);

	ok1(rszshm_mk(&s, 4096, longstr) == NULL && errno == EINVAL);

	fail_mmap_anon = 1;
	ok1(rszshm_mk(&s, 4096, NULL) == NULL && errno == 9010);
	rszshm_rm(&s);
	fail_mmap_anon = 0;

	fail_open = 1;
	ok1(rszshm_mk(&s, 4096, NULL) == NULL && errno == 9005);
	rszshm_rm(&s);
	fail_open = 0;

	fail_ftruncate = 1;
	ok1(rszshm_mk(&s, 4096, NULL) == NULL && errno == 9002);
	rszshm_rm(&s);
	fail_ftruncate = 0;

	fail_mmap_fixed = 1;
	ok1(rszshm_mk(&s, 4096, NULL) == NULL && errno == 9011);
	rszshm_rm(&s);
	fail_mmap_fixed = 0;

	fail_msync = 1;
	ok1(rszshm_mk(&s, 4096, NULL) == NULL && errno == 9003);
	rszshm_rm(&s);
	fail_msync = 0;

	ok1(rszshm_mk(&s, 4096, NULL) != NULL);

	struct rszshm_scan scan = RSZSHM_DFLT_SCAN;
	scan.iter = 1;
	ok1(rszshm_mk(&t, 4096, NULL, scan) == NULL && errno == ENOSPC);

	ok1(rszshm_dt(&s) == 0);
	ok1(rszshm_rm(&s) == 0);

	long pgsz = sysconf(_SC_PAGE_SIZE);
	scan.len = UINT64_MAX - pgsz;
	ok1(rszshm_mk(&t, 4096, NULL, scan) == NULL && errno == ENOMEM);

	ok1(rszshm_mk(&t, 4096, "foo/bar_XXXXXX/0") == NULL && errno == ENOENT);

	struct rszshm *r;
	ok1(rszshm_mkm(r, 4096, NULL) != NULL);

	pid_t p, *pp;
	noerr(p = fork());
	char *fname = strdupa(r->fname);
	if (p)
		waitpid(p, NULL, 0);
	else {
		ok1(rszshm_free(r) == 0);

		struct rszshm *q;
		ok1(rszshm_atm(q, fname) != NULL);

		*((pid_t *) q->dat) = getpid();

		ok1(rszshm_up(q) == 0);
		ok1(rszshm_grow(q) == 1);
		ok1(rszshm_free(q) == 0);
		exit(0);
	}
	pp = (pid_t *) r->dat;
	ok1(p == *pp);

	fail_mmap_fixed = 1;
	ok1(rszshm_up(r) == -1 && errno == 9011);
	fail_mmap_fixed = 0;

	ok1(rszshm_grow(r) == 1);

	ok1(rszshm_dt(r) == 0);

	sa.sa_handler = segvjmp;
	sa.sa_flags = SA_RESETHAND;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);
	if (setjmp(j) == 0)
		fail("still mapped after detach: %d", *pp);
	else
		pass("access after detach gives segv, OK!");

	ok1(rszshm_at(r, longstr) == NULL && errno == EINVAL);

	fail_open = 1;
	ok1(rszshm_at(r, fname) == NULL && errno == 9005);
	fail_open = 0;

	fail_read = 1;
	ok1(rszshm_at(r, fname) == NULL && errno == 9006);
	fail_read = 0;

	short_read = 1;
	ok1(rszshm_at(r, fname) == NULL && errno == ENODATA);
	short_read = 0;

	fail_mmap_anon = 1;
	ok1(rszshm_at(r, fname) == NULL && errno == 9010);
	fail_mmap_anon = 0;

	bad_mmap_addr = 1;
	ok1(rszshm_at(r, fname) == NULL && errno == ENOSPC);
	bad_mmap_addr = 0;

	fail_mmap_fixed = 1;
	ok1(rszshm_at(r, fname) == NULL && errno == 9011);
	fail_mmap_fixed = 0;

	ok1(rszshm_at(r, fname) != NULL);
	ok1(p == *pp);

	struct rszshm_hdr save = *r->hdr;
	r->hdr->flen = r->flen;
	r->hdr->max  = r->flen;
	ok1(rszshm_grow(r) == -1 && errno == ENOMEM);
	*r->hdr = save;

	fail_flock = 1;
	ok1(rszshm_grow(r) == -1 && errno == 9001);
	fail_flock = 0;

	fail_ftruncate = 1;
	ok1(rszshm_grow(r) == -1 && errno == 9002);
	fail_ftruncate = 0;

	ok1(rszshm_grow(r) == 1);
	ok1(rszshm_dt(r) == 0);
	ok1(rszshm_rm(r) == 0);

	r->fname[0] = '\0';
	ok1(rszshm_rmdir(r) == -1 && errno == ENOTDIR);

	ok1(rszshm_free(r) == 0);

	return exit_status();
}
