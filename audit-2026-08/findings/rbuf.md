# Audit: ccan/rbuf

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: rbuf.h (174 lines), rbuf.c (106 lines), _info,
ccan/rbuf/test/ (run.c, run-all.c, run-open.c, run-partial-read.c,
run-term-eof.c). Dependency: ccan/membuf (ccan/tcon header-only) —
treated as given; only misuse of its documented behavior would be
flagged (one cross-module note in R9). Suspects from the brief:
partial reads, EOF/error handling, buffer growth, memmove arithmetic,
the rbuf_read_str NUL-termination path. The EOF-handling suspect
confirmed in rbuf_fill (F1); the rest disproved (R1-R8).

## Mechanical checks

- ccanlint (before auditor additions): **51/56 PASS**, every check
  passes; shortfall is only partial credit on tests_coverage (+1/6).
- After adding the auditor regression test: ccanlint **44/48 FAIL** —
  exactly as designed: tests_pass drops to +5/6 because run-fill-eof
  fails 6/7 against the current code (tests_compile passes for it).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/rbuf/rbuf.c directly; linked with ccan/membuf/membuf.c and
  ccan/tap/tap.c; cwd = ccan/rbuf; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - run-open: **5/5 clean**; run-partial-read: **160/160 clean**.
  - run, run-all, run-term-eof abort on a single UBSan diagnostic
    *inside the dependency*: `ccan/membuf/membuf.h:86:19: runtime
    error: applying zero offset to null pointer` — membuf_elems_()
    computing `mb->elems + mb->start * elemsize` (NULL + 0) from
    membuf_prepare_space_ (membuf.c:21), reached via the documented
    `rbuf_init(&in, fd, NULL, 0, ...)` (see R9).
  - Re-run with `-fno-sanitize=pointer-overflow` (the membuf-internal
    check) to let the suites complete: **all 5 tests pass, 347/347
    subtests, zero further sanitizer diagnostics** (run 164, run-all 8,
    run-open 5, run-partial-read 160, run-term-eof 10).
- -m32: skipped; module pulls in errno.h (missing 32-bit system
  headers on this host, per audit brief).
- Reproducers/probes in /tmp/rbuf-asan/ (temporary, not committed):
  repro-fill-eof.c, shadow/ccan/rbuf/rbuf.c (repair-direction patch
  used to validate the regression test).

## Findings

## F1 — CONFIRMED (FIXED in 748a0692): rbuf_fill() returns non-NULL at EOF (and never sets errno = 0); the documented usage loop never terminates

- Location: ccan/rbuf/rbuf.c:62-69, function rbuf_fill:
  ```c
  void *rbuf_fill(struct rbuf *rbuf)
  {
      if (!rbuf_len(rbuf)) {
          if (get_more(rbuf) < 0)
              return NULL;
      }
      return rbuf_start(rbuf);
  }
  ```
  At EOF get_more (rbuf.c:30-50) returns 0 (read(2) returned 0 at
  rbuf.c:44-46, errno untouched). 0 is not < 0, so rbuf_fill falls
  through to `return rbuf_start(rbuf)`, which is
  `mb->elems + mb->start` — always non-NULL here, because get_more has
  just allocated the buffer (or the caller supplied one) before the
  read that hit EOF.
- Documented contract (rbuf.h:60-80): "If there is nothing more to
  read, it will return NULL with errno set to 0", and the header's own
  example is:
  ```c
  while (rbuf_fill(&in)) {
      printf("%.*s\n", (int)rbuf_len(&in), rbuf_start(&in));
      rbuf_consume(&in, rbuf_len(&in));
  }
  if (errno)
      err(1, "reading foo");
  ```
  Both halves of the EOF contract are violated: the return is
  non-NULL, and nothing in rbuf.c ever sets errno = 0 on this path
  (the only `errno = 0` in the module is rbuf_read_str's EOF branch,
  rbuf.c:93).
- Reachable path: any file consumed to EOF through rbuf_fill —
  including the header's documented example — or an rbuf_fill on an
  empty file.
- Caller preconditions: none violated — this is the exact documented
  usage pattern.
- Concrete incorrect consequence, observed
  (/tmp/rbuf-asan/repro-fill-eof.c, clang ASan+UBSan build):
  ```
  case1: iters=100001 (file is 12 bytes -> expect 1 fill + NULL at EOF)
  case1: loop DID NOT TERMINATE (infinite loop at EOF), errno after loop = 0
  case2: at EOF: p=0x521000002900 (doc says NULL) len=0 errno=0 (doc says 0)
  case3: empty file: p=0x521000003d00 (doc says NULL) len=0 errno=0
  ```
  i.e. the documented `while (rbuf_fill(&in)) { ...; rbuf_consume(&in,
  rbuf_len(&in)); }` loop spins forever at EOF: every iteration
  returns a non-NULL pointer with rbuf_len() == 0, consumes 0 bytes,
  and issues another read(2) that returns 0 — a permanent busy loop
  (one syscall per iteration) for any caller that follows the header.
  A caller that instead trusts "NULL at EOF" after a single fill also
  misreads EOF as "0 valid bytes available". (errno happened to be 0
  in the repro because it was pre-cleared; the code never clears it —
  with a stale errno the post-loop `if (errno)` check would also
  misreport, though the infinite loop means callers never get there.)
  No in-tree caller uses rbuf_fill (tools/tools.c:87,
  tools/depends.c:36 and ccan/crypto/shachain/tools/shachain48.c:21
  all use rbuf_read_str), so the blast radius is external users of the
  documented API. None of the existing tests ever call rbuf_fill at
  EOF (run-all.c only fills before EOF), which is why this survived.
- Regression test: ccan/rbuf/test/run-fill-eof.c (alarm(10)-bounded,
  7 subtests: documented loop terminates at EOF; errno == 0 after the
  loop; direct rbuf_fill at EOF returns NULL with errno == 0; empty
  file's first fill returns NULL with errno == 0). Currently fails
  6/7 (`not ok 1,2,4,5,6,7`); must pass after repair.
- Repair direction: treat get_more's 0 as EOF in rbuf_fill:
  ```c
  if (!rbuf_len(rbuf)) {
      ssize_t r = get_more(rbuf);
      if (r <= 0) {
          if (r == 0)
              errno = 0;
          return NULL;
      }
  }
  return rbuf_start(rbuf);
  ```
  Validated against /tmp/rbuf-asan/shadow with only this change:
  run-fill-eof passes 7/7 and the existing suite still passes 347/347
  under ASan+UBSan.

## Rejected candidates (disproved)

- R1: partial reads (suspect list). get_more reads at most
  membuf_num_space() and membuf_add()s exactly r bytes; rbuf_read_str
  rescans only newly arrived bytes (prev/r bookkeeping keeps
  prev <= num_elems and the memchr length == bytes just read).
  run-partial-read.c (read() shimmed to return 1 byte per call)
  passes 160/160 under ASan+UBSan. Rejected.
- R2: rbuf_read_str EOF NUL-termination path (suspect list),
  rbuf.c:97-101: `assert(membuf_num_space(&rbuf->m) > 0); ret =
  membuf_consume(&rbuf->m, len); ret[len] = '\0';`. The space is
  guaranteed even under NDEBUG: get_more returns 0 only after its
  pre-read check `if (!membuf_num_space(&rbuf->m)) return -1;`
  (rbuf.c:41-42) passed, and read() does not shrink num_space, so
  ret[len] writes into the byte get_more reserved. The exact-fit
  buffer case (file size == buf_max, expandfn failing) is exercised by
  run-term-eof.c — ASan-clean. Rejected.
- R3: memchr on NULL/empty buffer in rbuf_read_str (rbuf.c:78-81):
  the `membuf_num_elems(&rbuf->m) == prev` short-circuit (commit
  d87a25dc) prevents memchr(NULL, ..., 0); the second clause only runs
  with num_elems > prev >= 0, so elems is non-NULL and the length is
  non-negative. Rejected.
- R4: memmove arithmetic (suspect list): rbuf performs no memmove
  itself; all buffer movement/growth is inside ccan/membuf
  (membuf_prepare_space_), which is the dependency's domain. rbuf
  uses only the documented membuf API correctly. Rejected.
- R5: buffer growth in get_more (rbuf.c:34-42): the
  `max_elems == 0 -> rbuf_good_size(), else prepare_space(m, 1)`
  logic matches rbuf_open's documented "resized to rbuf_good_size()
  on first rbuf_fill" (rbuf.h:39-40); expandfn failure is detected via
  the documented membuf_num_space() == 0 check (membuf.h:163-165) and
  returns -1 with errno ENOMEM — covered by run.c (tests 3-6) and
  run-all.c (tests 2-5). Rejected.
- R6: EINTR/short-failure handling in get_more: a read() failing with
  EINTR returns NULL with errno == EINTR, which honors "If a read
  fails, then NULL is also returned" (rbuf.h:64); buffer state is
  untouched, so the caller can retry. No false success, no corrupted
  state (contrast pipecmd F3). Rejected.
- R7: rbuf_good_size (rbuf.c:21-28): fstat failure falls back to 4096
  (tested by run-open.c:22); the `st.st_blksize >= 4096` guard makes
  the blksize_t -> size_t conversion safe on any plausible platform.
  Rejected.
- R8: rbuf_fill_all at EOF on an empty file returns a non-NULL
  rbuf_start() with rbuf_len() == 0: its own contract (rbuf.h:115-124)
  promises NULL only on read/expandfn failure, "otherwise returns
  @rbuf->start" — consistent. (The contrast with rbuf_fill's
  stronger documented EOF contract is exactly F1.) Rejected.
- R9: UBSan "applying zero offset to null pointer" at membuf.h:86,
  reached through rbuf's documented `rbuf_init(&in, fd, NULL, 0, ...)`
  (first get_more -> membuf_prepare_space_ -> membuf_elems_ on a NULL
  elems). Cross-module note for the orchestrator: the arithmetic is
  entirely inside ccan/membuf's inline membuf_elems_ (also reachable
  via rbuf_start() on a never-filled NULL/0 rbuf); rbuf does not
  misuse the membuf API, so this is for membuf's own audit, not an
  rbuf defect. Benign on every real target, but it does abort
  -fno-sanitize-recover builds of rbuf's own test suite. Rejected
  (for rbuf).
- R10: rbuf_consume/rbuf_start/rbuf_len inline wrappers: thin membuf
  wrappers; consuming more than rbuf_len() trips membuf_consume_'s
  assert and is a documented-precondition violation, not a defect.
  Rejected.
- R11: rbuf_open (rbuf.c:10-19): returns false with open(2)'s errno
  on failure and leaves *rbuf untouched; run-open.c covers both
  branches. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/rbuf/test/run-fill-eof.c — proves F1. alarm(10)-bounded, 7
  subtests; includes ccan/rbuf/rbuf.c per run-test convention
  (ccanlint only links module objects into api tests); creates and
  unlinks its own temp files. Currently fails 6/7 against the current
  code (plain and ASan builds: `not ok 1,2,4,5,6,7`); passes 7/7
  against /tmp/rbuf-asan/shadow with the F1 repair applied, and the
  existing suite still passes 347/347 against that shadow.
- ccanlint with this file present: 44/48 FAIL, solely because
  run-fill-eof fails against the current code (tests_pass +5/6).

No production files were modified (rbuf.h, rbuf.c, _info untouched;
verified by git status — the only new path under ccan/rbuf is
test/run-fill-eof.c).
