/* Licensed under Apache License v2.0 - see LICENSE file for details */
#ifndef CCAN_RSZSHM_H
#define CCAN_RSZSHM_H
#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * struct rszshm_scan - parameters for the free region search
 * @start: first address to test
 * @len: size of region to test
 * @hop: offset of the next test
 * @iter: number of attempts
 *
 * See rszshm_mk for search details.
 */
struct rszshm_scan {
	void *start;
	size_t len;
	size_t hop;
	unsigned iter;
};

#define KiB (1024UL)
#define MiB (KiB*KiB)
#define GiB (MiB*KiB)
#ifdef __x86_64__
#define TiB (GiB*KiB)
#define RSZSHM_DFLT_SCAN (struct rszshm_scan) { (void *) (64*TiB), 4*GiB, 1*TiB, 10 }
#else
#define RSZSHM_DFLT_SCAN (struct rszshm_scan) { (void *) ((1024+512)*MiB), 256*MiB, 256*MiB, 10 }
#endif

/**
 * struct rszshm_hdr - header describing mapped memory
 * @flen: length of the shared file mapping
 * @max: length of the private mapping
 * @addr: address of the mapping
 *
 * The shared region is mapped over the private region.
 * max is the maximum size the shared region can be extended.
 * addr and max are set at creation time and do not change.
 * flen is updated each time the file and shared region is grown.
 */
struct rszshm_hdr {
	size_t flen;
	size_t max;
	void *addr;
};

/**
 * struct rszshm - handle for a mapped region
 * @fd: file descriptor of the mapped file
 * @flen: length of the mapped shared file in this process
 * @fname: path of the mapped file
 * @hdr: pointer to the mapped region header
 * @dat: pointer to the usable space after the header
 * @cap: length of the usable space after the header
 *
 * flen is updated by rszshm_grow, or by rszshm_up.
 */
#define RSZSHM_PATH_MAX 128
#define RSZSHM_DFLT_FNAME "/dev/shm/rszshm_XXXXXX/0"
struct rszshm {
	int fd;
	size_t flen;
	char fname[RSZSHM_PATH_MAX];
	struct rszshm_hdr *hdr;
	void *dat;
	size_t cap;
};

/**
 * rszshm_mk - make and mmap a shareable region
 * @r: pointer to handle
 * @flen: initial length of shared mapping
 * @fname: path to file to be created, may be NULL or contain template
 * @scan: struct specifying search parameters
 *
 * The handle pointed to by r is populated by rszshm_mk. flen is increased
 * by the size of struct rszshm_hdr and rounded up to the next multiple of
 * page size. If the directory portion of fname ends with XXXXXX, mkdtemp(3)
 * is used. If fname is NULL, a default path with template is used.
 *
 * If rszshm_mk is called with only three arguments, a default scan struct
 * is used. To supply a struct via compound literal, wrap the argument in
 * parenthesis to avoid macro failure.
 *
 * rszshm_mk attempts to mmap a region of scan.len size at scan.start address.
 * This is a private anonymous noreserve map used to claim an address space.
 * If the mapping returns a different address, the region is unmapped, and
 * another attempt is made at scan.start - scan.hop. If necessary, the next
 * address tried is scan.start + scan.hop, then scan.start - (2 * scan.hop),
 * and so on for at most scan.iter iterations. The pattern can be visualized
 * as a counterclockwise spiral. If no match is found, NULL is returned and
 * errno is set to ENOSPC.
 *
 * When an mmap returns an address matching the requested address, that region
 * is used. If fname contains a template, mkdtemp(3) is called. A file is
 * created, and extended to flen bytes. It must not already exist. This file
 * is mmap'd over the region using MAP_FIXED. The mapping may later be extended
 * by rszshm_grow consuming more of the claimed address space.
 *
 * The initial portion of the mapped file is populated with a struct rszshm_hdr,
 * and msync called to write out the header.
 *
 * Example:
 *	struct rszshm r, s, t;
 *
 *	if (!rszshm_mk(&r, 4*MiB, NULL))
 *		err(1, "rszshm_mk");
 *	// map at 0x400000000000
 *
 *	if (!rszshm_mk(&s, 4*MiB, "/var/tmp/dat"))
 *		err(1, "rszshm_mk");
 *	// map at 0x3f0000000000
 *
 *	if (!rszshm_mk(&t, 4*MiB, NULL, ((struct rszshm_scan) { (void *) (48*TiB), 4*GiB, 1*TiB, 10 })))
 *		err(1, "rszshm_mk");
 *	// map at 0x300000000000
 *
 * Returns: r->dat address on success, NULL on error
 */
void *rszshm_mk(struct rszshm *r, size_t flen, const char *fname, struct rszshm_scan scan);
#define __4args(a,b,c,d,...) a, b, c, d
#define rszshm_mk(...) rszshm_mk(__4args(__VA_ARGS__, RSZSHM_DFLT_SCAN))

#if HAVE_STATEMENT_EXPR
/**
 * rszshm_mkm - malloc handle and run rszshm_mk
 * @r: pointer to handle
 * @flen: initial length of shared mapping
 * @fname: path to file to be created, may be NULL or contain template
 *
 * Example:
 *	struct rszshm *r;
 *
 *	if (!rszshm_mkm(r, 4*MiB, NULL))
 *		err(1, "rszshm_mkm");
 *
 * Returns: result of rszshm_mk
 */
#define rszshm_mkm(r, fl, fn) ({			\
	void *__p = NULL;				\
	r = malloc(sizeof(*r));				\
	if (r && !(__p = rszshm_mk(r, fl, fn))) {	\
		free(r);				\
		r = 0;					\
	}						\
	__p;						\
})
#endif

/**
 * rszshm_at - mmap ("attach") an existing shared region
 * @r: pointer to handle
 * @fname: path to file
 *
 * rszshm_at lets unrelated processes attach an existing shared region.
 * fname must name a file previously created by rszshm_mk in another process.
 * Note, fork'd children of the creating process inherit the mapping and
 * should *not* call rszshm_at.
 *
 * rszshm_at opens and reads the header from the file. It makes a private
 * anonymous noreserve mapping at the address recorded in the header.
 * If mmap returns an address other than the requested one, munmap
 * is called, errno is set to ENOSPC, and NULL is returned.
 *
 * Once the address space is claimed, the file is mmap'd over the region
 * using MAP_FIXED. The remaining claimed address space will be used by
 * later calls to rszshm_grow. Finally, the handle is populated and r->dat
 * returned.
 *
 * Example:
 *	struct rszshm r;
 *
 *	if (!rszshm_at(&r, "/dev/shm/rszshm_LAsEvt/0"))
 *		err(1, "rszshm_at");
 *
 * Returns: r->dat address on success, NULL on error
 */
void *rszshm_at(struct rszshm *r, const char *fname);

#if HAVE_STATEMENT_EXPR
/**
 * rszshm_atm - malloc handle and run rszshm_at
 * @r: pointer to handle
 * @fname: path to file
 *
 * Example:
 *	struct rszshm *r;
 *
 *	if (!rszshm_atm(r, "/dev/shm/rszshm_LAsEvt/0"))
 *		err(1, "rszshm_atm");
 *
 * Returns: result of rszshm_at
 */
#define rszshm_atm(r, f) ({				\
	void *__p = NULL;				\
	r = malloc(sizeof(*r));				\
	if (r && !(__p = rszshm_at(r, f))) {		\
		free(r);				\
		r = 0;					\
	}						\
	__p;						\
})
#endif

/**
 * rszshm_dt - unmap ("detach") shared region
 * @r: pointer to handle
 *
 * Calls msync, munmap, and close. Resets handle values except fname.
 * (fname is used by rszshm_rm*.)
 *
 * Returns: 0 on success, -1 if any call failed
 */
int rszshm_dt(struct rszshm *r);

/**
 * rszshm_up - update mapping of shared region
 * @r: pointer to handle
 *
 * Check if flen from the region header matches flen from the handle.
 * They will diverge when another process runs rszshm_grow.
 * If they are different, call mmap with the header flen and MAP_FIXED,
 * and update handle.
 *
 * Returns: -1 if mmap fails, 0 for no change, 1 is mapping updated
 */
int rszshm_up(struct rszshm *r);
#define rszshm_up(r) (assert(r), (r)->flen == (r)->hdr->flen ? 0 : rszshm_up(r))

/**
 * rszshm_grow - double the shared region, conditionally
 * @r: pointer to handle
 *
 * If the region is already at capacity, set errno to ENOMEM, and return -1.
 *
 * rszshm_up is called, to see if another process has already grown the region.
 * If not, a lock is acquired and the check repeated, to avoid races.
 * The file is extended, and mmap called with MAP_FIXED. The header and handle
 * are updated.
 *
 * Returns: 1 on success, -1 on error
 */
int rszshm_grow(struct rszshm *r);

/**
 * rszshm_unlink - unlink shared file
 * @r: pointer to handle
 *
 * Returns: result of unlink
 */
int rszshm_unlink(struct rszshm *r);

/**
 * rszshm_rmdir - rmdir of fname directory
 * @r: pointer to handle
 *
 * Returns: result of rmdir
 */
int rszshm_rmdir(struct rszshm *r);

#if HAVE_STATEMENT_EXPR
/**
 * rszshm_rm - remove file and directory
 * @r: pointer to handle
 *
 * Calls rszshm_unlink and rszshm_rmdir.
 *
 * Returns: 0 on success, -1 on error
 */
#define rszshm_rm(r) ({				\
	int __ret;				\
	assert(r);				\
	__ret = rszshm_unlink(r);		\
	if (__ret == 0)				\
		__ret = rszshm_rmdir(r);	\
	__ret;					\
})
#endif

#if HAVE_STATEMENT_EXPR
/**
 * rszshm_free - run rszshm_dt and free malloced handle
 * @r: pointer to handle
 *
 * Returns: result of rszshm_dt
 */
#define rszshm_free(r) ({	\
	int __i = rszshm_dt(r);	\
	free(r);		\
	r = 0;			\
	__i;			\
})
#endif

#endif
