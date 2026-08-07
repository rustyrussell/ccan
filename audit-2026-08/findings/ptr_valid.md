# Audit: ccan/ptr_valid

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: ptr_valid.h (229 lines), ptr_valid.c (344 lines), _info,
ccan/ptr_valid/test/ (run.c, run-string.c). ~573 LOC. Dep ccan/noerr
used per its documented contract (close_noerr in error paths).
Suspects from the brief: races vs munmap, /proc parsing bugs,
alignment/page math, hugepage edge cases — all four disproved (R1-R4);
the real defects found are the batch page-cache ignoring the read/write
flag (F1), realloc-overwrite leaks on the OOM paths (F2), the broken
errno contract (F3), and the fallback child's exit() (F4).

Documented contract (ptr_valid.h): ptr_valid_read, ptr_valid_write,
ptr_valid_string and ptr_valid each document "Sets errno to EFAULT on
failure"; the batch variants are documented as batched versions of the
same checks. Batch reuse is documented as valid "until the memory map
changes" (ptr_valid.h:89-90).

## Mechanical checks

- ccanlint (before auditor additions): **42/49 PASS**, every check
  passes; partial credit only for tests_coverage (+0/6 — the child
  fallback path is unreachable on this host) and examples_exist (+1/2,
  _info has no Example section).
- After adding the four auditor regression tests: ccanlint **40/51
  FAIL** — as designed: tests_pass (+2/6) and
  tests_pass_without_features (+3/6) fail because the new tests fail
  (run-realloc-leak passes the without-features build, where
  HAVE_PROC_SELF_MAPS is compiled out; run-batch-cache, run-errno and
  run-child-flush fail in both modes).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/ptr_valid.c directly; linked with ccan/noerr and ccan/tap;
  per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0): run
  (30/30 ok) and run-string (14/14 ok) pass cleanly, zero sanitizer
  diagnostics.
- The four auditor regression tests fail identically under ASan+UBSan
  with zero sanitizer diagnostics (the failures are logic errors, not
  memory safety).
- -m32: skipped (missing 32-bit host headers, same as earlier modules).
- Reproducers in /tmp/pv-asan/ (temporary, not committed):
  repro-cache-rw.c, repro-realloc-leak.c, repro-errno.c,
  repro-child-flush.c.

## Findings

## F1 — CONFIRMED (FIXED in 8813e723): ptr_valid_batch()'s single-page cache ignores the read/write flag

- Location: ccan/ptr_valid.c:280-284 and ptr_valid.c:291-294, function
  ptr_valid_batch():
  ```c
  /* We cache single page hits. */
  if (start == end) {
      if (batch->last && batch->last == start)
          return batch->last_ok;
  }
  ...
  if (start == end) {
      batch->last = start;
      batch->last_ok = ret;
  }
  ```
  The cache key is the page frame only; the `write` argument is not
  part of the key.
- Reachable path: a batch that checks the same page once for read and
  once for write, in either order, where the page is read-only (e.g.
  string literals in .rodata, or a PROT_READ mmap). First check
  computes and caches the page result; the second check returns the
  cached value without looking at the requested access mode. The
  non-batch wrappers (ptr_valid_read/ptr_valid_write) are unaffected
  because they build a fresh batch per call; existing test run.c never
  mixes read and write checks on a read-only page within one batch
  (check_batch is only used on RW and unmapped pages), which is why
  this went unnoticed.
- Preconditions: none violated — mixing ptr_valid_batch_read() and
  ptr_valid_batch_write() on one batch is exactly what the header's
  linked-list example does with batch_read + batch_string, and the
  batch API imposes no ordering constraint.
- Concrete incorrect consequence, observed
  (/tmp/pv-asan/repro-cache-rw.c, plain clang build, PROT_READ page):
  ```
  RO page: batch_read=1 batch_write=1 (want 1 0)
  RO page: batch_write=0 batch_read=0 (want 0 1)
  ```
  Direction 1 is the dangerous one: ptr_valid_batch_write() returns
  true for read-only memory, so a caller that trusts it writes and
  takes SIGSEGV — precisely the crash this module exists to prevent.
  Direction 2 is a false negative (read reported invalid on a readable
  page), safe but still wrong per the contract. The same poisoning
  applies in the no-/proc child fallback (cache is checked before the
  maps/child branch at ptr_valid.c:286-289).
- Regression test: ccan/ptr_valid/test/run-batch-cache.c
  (alarm(10)-bounded, 4 subtests). Currently `not ok 2` and
  `not ok 4`; after repair it must pass.
- Repair direction: include the access mode in the cache. Minimal:
  add `bool last_write;` to struct ptr_valid_batch, require
  `batch->last_write == write` for a hit, and store it alongside
  last_ok. (Caching only !write results would also work but is weaker:
  a cached write-success also proves readability, a cached
  write-failure does not disprove it.)

## F2 — CONFIRMED (FIXED in decefacc): realloc-overwrite leaks in grab() and add_map() on OOM

- Location 1: ccan/ptr_valid.c:31-33, function grab():
  ```c
  buffer = realloc(buffer, max*2+1);
  if (!buffer)
      goto close;
  ```
  On realloc failure the original 16k buffer is still allocated but its
  only pointer was overwritten with NULL; `goto close` closes the fd
  and returns NULL without freeing it.
- Location 2: ccan/ptr_valid.c:64-68, function add_map():
  ```c
  *max *= 2;
  map = realloc(map, sizeof(*map) * *max);
  if (!map)
      return NULL;
  ```
  Same pattern; the caller get_proc_maps() (ptr_valid.c:114-117) goes
  to free_buf, which frees the /proc buffer but not the original map
  array.
- Reachable path: /proc/self/maps larger than 16 KiB (grab growth) or
  more than 16 readable mappings (add_map growth) — both ordinary on
  real processes — combined with an allocation failure. No caller
  preconditions involved; allocation failure is a normal runtime
  condition and the function is explicitly written to tolerate it
  (falls back to the child prober).
- Concrete incorrect consequence, observed
  (/tmp/pv-asan/repro-realloc-leak.c: 1000 alternating RO/RW VMAs via
  mprotect to make /proc/self/maps 51 KiB; malloc/realloc/free
  interposed with -Wl,--wrap to inject a failure at each growth call
  and track outstanding allocations):
  ```
  grab-growth realloc failed: live allocs after batch = 1 [16385 bytes]
  add_map-growth realloc failed: live allocs after batch = 1 [384 bytes]
  ```
  One buffer leaked per failed ptr_valid_batch_start(); a long-running
  process that keeps checking pointers under memory pressure leaks
  16 KiB per retry.
- Regression test: ccan/ptr_valid/test/run-realloc-leak.c
  (alarm(20)-bounded, 2 subtests; interposes malloc/realloc/free by
  macro since it includes ptr_valid.c directly). Currently
  `not ok 1` and `not ok 2`; after repair it must pass.
- Repair direction: use a temporary for the realloc result in both
  places (`char *nbuf = realloc(buffer, ...); if (!nbuf) goto free;
  buffer = nbuf;` and likewise with a temporary in add_map, preserving
  the old map pointer for the caller to free — or have add_map free the
  old array itself on failure).

## F3 — CONFIRMED (FIXED in c6280404): "Sets errno to EFAULT on failure" is not honored on the maps path or the alignment path

- Location: ccan/ptr_valid.c:274-275 (alignment failure:
  `if ((intptr_t)p & (alignment - 1)) return false;`) and
  ptr_valid.c:286-296 (check_with_maps() failure propagates to the
  caller without setting errno), function ptr_valid_batch(). EFAULT is
  only ever set inside check_with_child() (ptr_valid.c:254, 260), i.e.
  only in the no-/proc fallback.
- Documented contract: ptr_valid.h:15, 31, 47, 64 — every non-batch
  entry point documents "Sets errno to EFAULT on failure."
- Reachable path: the normal Linux path. ptr_valid_read() on an
  unmapped page, ptr_valid_write() on a read-only page, or any
  misaligned check returns false with errno left untouched (whatever
  the last syscall left there — zero in the reproducer).
- Preconditions: none violated.
- Concrete incorrect consequence, observed
  (/tmp/pv-asan/repro-errno.c, plain clang build):
  ```
  unmapped: ret=0 errno=0 (want ret=0 errno=EFAULT=14)
  misaligned: ret=0 errno=0 (want ret=0 errno=EFAULT)
  readonly-write: ret=0 errno=0 (want ret=0 errno=EFAULT)
  ```
  Callers cannot distinguish "pointer invalid" from incidental stale
  errno, and code written to the documented contract (e.g.
  `if (!ptr_valid_read(p) && errno == EFAULT)`) silently misbehaves.
  Minor severity — no memory unsafety — but a direct contract
  violation. (In the child fallback errno *is* EFAULT, so behavior also
  differs between the two engines for identical inputs.)
- Regression test: ccan/ptr_valid/test/run-errno.c (alarm(10)-bounded,
  3 subtests). Currently `not ok 1..3`; after repair it must pass.
- Repair direction: set `errno = EFAULT` before each `return false` in
  ptr_valid_batch() (alignment failure and the ret == false tail), or
  centrally in the ptr_valid()/ptr_valid_string() wrappers. Note the
  batch functions inherit the same expectation via "Batched version of
  ptr_valid()", so fixing it in ptr_valid_batch() covers both.

## F4 — CONFIRMED (FIXED in 2c396de3): fallback prober child exits via exit() — flushes the parent's inherited stdio buffers a second time

- Location: ccan/ptr_valid.c:199 (`exit(0)` at EOF, the normal exit
  path taken on every ptr_valid_batch_end()) and ptr_valid.c:185, 187,
  197 (`exit(1/2/3)` on protocol errors), function run_child().
- Reachable path: environments without readable /proc/self/maps
  (unchrooted containers/sandboxes without /proc mounted, or platforms
  where HAVE_PROC_SELF_MAPS is 0) — get_proc_maps() fails,
  batch->num_maps is 0, and every check goes through check_with_child().
  create_child() does fflush(stdout) before fork (ptr_valid.c:211), so
  stdout itself is safe, but any *other* fully-buffered FILE with
  pending data at fork time (log files, redirected streams) is
  duplicated into the child; when ptr_valid_batch_end() closes the
  pipe, the child sees EOF and calls exit(0), flushing its inherited
  copy to the shared file offset. The parent later flushes the same
  buffer again. exit() also runs any atexit handlers registered by the
  parent or its libraries in the child.
- Preconditions: none violated — buffered stdio at the time of the call
  is normal, and nothing in ptr_valid.h restricts open streams.
- Concrete incorrect consequence, observed
  (/tmp/pv-asan/repro-child-flush.c: HAVE_PROC_SELF_MAPS forced to 0 —
  the same code path as /proc being unavailable — a fully-buffered FILE
  with 14 pending bytes, one ptr_valid_read(), then parent fflush):
  ```
  file len=28 content='unflushed-dataunflushed-data' (want len=14)
  ```
  Duplicated/corrupted redirected output and logs; potentially
  arbitrary parent cleanup re-run via atexit in the child. Same defect
  class as pipecmd's F4.
- Regression test: ccan/ptr_valid/test/run-child-flush.c
  (alarm(10)-bounded, 2 subtests; forces the fallback by #undef-ing
  HAVE_PROC_SELF_MAPS before including ptr_valid.c). Currently
  `not ok 2`; after repair it must pass.
- Repair direction: replace all four exit() calls in run_child() with
  _exit() (the standard post-fork idiom; the child never legitimately
  needs stdio or atexit handlers).

## Rejected candidates (disproved)

- R1: races vs munmap (TOCTOU). Two distinct windows: (a) between
  ptr_valid_batch_start()'s maps snapshot and the checks, and (b)
  between a check and the caller's dereference. (a) is explicitly
  documented ("this same @batch pointer can be reused until the memory
  map changes", ptr_valid.h:89-90) — stale answers after map changes
  are a documented precondition violation. (b) is inherent to any
  user-space validity probe (the kernel cannot freeze the address
  space) and is the module's documented purpose: it reports what is
  mapped at check time. Rejected.- R2: /proc/self/maps parsing bugs (get_proc_maps, ptr_valid.c:77-131).
  Checked line by line: strtoul on hex addresses cannot overflow
  (kernel addresses fit in unsigned long on LP64 and 32-bit); the
  `-`/space/perm guards reject malformed lines by falling back to the
  child prober (num_maps = 0), which is a safe degradation; a last line
  without a trailing newline terminates the loop correctly (skip_line
  returns NULL); lines without a pathname and special mappings
  ([vdso], [vsyscall], [stack]) parse fine; non-readable mappings
  (---p) are correctly skipped; add_map's growth arithmetic cannot
  overflow for any map count reachable from a proc file. Rejected.
- R3: alignment/page math overflow. ptr_valid.c:277-278 computes
  start/end as (p & ~(pagesize-1)) and ((p + size - 1) & ~(pagesize-1)).
  The (intptr_t) + size_t arithmetic is unsigned (the intptr_t operand
  is converted to size_t), so no signed-overflow UB; wraparound of the
  end below the start requires size within a page of SIZE_MAX, a
  pathological input violating the implied size precondition (the
  macros pass sizeof(*p)). size == 0 gives end one page below start,
  which merely misses the single-page cache and still answers via
  check_with_maps(p, 0, ...), returning the map's writability for a
  zero-byte region — acceptable. getpagesize() is positive on Linux.
  Rejected.
- R4: hugepage edge cases. /proc/self/maps entries for hugetlbfs and
  THP mappings have the same line format; mapping boundaries are
  multiples of the base page size, so the getpagesize()-based page
  quantization stays correct, and per-page permission (and the F1
  cache) is uniform within a hugepage. Rejected.
- R5: check_with_maps spanning logic (ptr_valid.c:140-158). A range
  that extends past the end of its first map is rechecked from
  maps[i].end with the remaining length; the recursion correctly
  rejects gaps (no map contains the boundary) and honours the
  writability of each traversed map. Verified by reasoning plus run.c's
  overrun tests (pagesize+1 across a munmap hole), which pass under
  ASan. Rejected.
- R6: EINTR on the parent-side pipe I/O in check_with_child
  (ptr_valid.c:249-262). Each write is under PIPE_BUF, so atomic; an
  interrupted write or read makes the call return false with errno
  EFAULT and finish_child() reaps the child (waitpid loops on EINTR,
  ptr_valid.c:164). A false negative is the safe direction; unlike
  pipecmd's F3 there is no false-success or zombie. Rejected.
- R7: ptr_valid_batch_start always returns true although the header
  example guards "if (!ptr_valid_batch_start(&batch))" (ptr_valid.h:109).
  Harmless: get_proc_maps() failure is handled by falling back to the
  child prober (num_maps = 0), and the always-true return just makes
  the example's guard dead code. No incorrect consequence. Rejected.
- R8: single-page cache size- and alignment-insensitivity. The cache
  key is the page frame; start == end guarantees the whole checked
  range lies inside one page, and permissions are uniform per page, so
  different sizes within the same page legitimately share one result.
  Misaligned checks return false before the cache is consulted
  (ptr_valid.c:274-275), so alignment cannot poison or be poisoned by
  the cache. The only missing key component is the access mode — that
  is F1. Rejected.- R9: SIGCHLD interactions in finish_child. If the caller reaps
  children in a SIGCHLD handler or sets SIGCHLD to SIG_IGN, waitpid
  returns -1/ECHILD and the loop exits immediately; no hang, no zombie.
  finish_child is only called when child_pid != 0. Rejected.
- R10: run_child touching memory with side effects (MAP_SHARED files,
  device mappings): for write checks it writes back the same byte value
  it read, so no content change; read checks of MMIO can have device
  side effects, but probing such mappings is exactly what the caller
  asked for, and the fallback only runs where /proc is unavailable.
  Inherent to the design. Rejected.
- R11: grab() initial-malloc and read-error paths (ptr_valid.c:24-48):
  malloc failure goes to close (fd closed, nothing to free); read
  failure goes to free then close (buffer freed, fd closed). Only the
  realloc-overwrite site leaks — that is F2. Rejected.
- R12: alignment == 0 passed directly to ptr_valid(): (alignment - 1)
  wraps to SIZE_MAX, so any non-NULL p fails the alignment check and
  NULL falls through to the maps/child check which also fails. Safe
  direction for a nonsense argument. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/ptr_valid/test/run-batch-cache.c — proves F1. Currently fails
  2/4 (`not ok 2`, `not ok 4`: batch_write returns true on a PROT_READ
  page after a cached read, and batch_read returns false after a cached
  write failure); alarm(10)-bounded. Must pass after repair.
- ccan/ptr_valid/test/run-errno.c — proves F3. Currently fails 3/3
  (errno left 0 instead of EFAULT on the unmapped, misaligned and
  read-only-write paths); alarm(10)-bounded. Must pass after repair.
- ccan/ptr_valid/test/run-realloc-leak.c — proves F2 (both sites).
  Currently fails 2/2 (one leaked buffer per injected realloc failure);
  alarm(20)-bounded; interposes malloc/realloc/free by macro since it
  includes ptr_valid.c directly. Must pass after repair.
- ccan/ptr_valid/test/run-child-flush.c — proves F4. Currently fails
  1/2 (`not ok 2`: inherited stdio buffer flushed twice, file has 28
  bytes instead of 14); alarm(10)-bounded; forces the no-/proc fallback
  by #undef-ing HAVE_PROC_SELF_MAPS. Must pass after repair.
- All four #include ccan/ptr_valid/ptr_valid.c per run-test convention
  (ccanlint only links module objects into api tests).
- ccanlint with all four present: 40/51 FAIL, solely because these
  tests fail against the current code (tests_pass +2/6,
  tests_pass_without_features +3/6; all other checks pass).

No production files were modified (ptr_valid.h, ptr_valid.c, _info
untouched; verified by git status — the only new paths under
ccan/ptr_valid are the four tests above).
