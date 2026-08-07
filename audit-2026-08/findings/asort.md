# Audit: ccan/asort

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: asort.h (32 lines, macro over qsort_r or _asort), asort.c (455
lines, vendored glibc 2.43 mergesort + heapsort fallback, commit
fe99a8e0, 2026-07-01), _info and ccan/asort/test/ only. Dep ccan/order
(total_order_cast -> typesafe_cb_cast: the cmp/ctx expressions appear
only inside __typeof__/__builtin_types_compatible_p, never evaluated —
verified in typesafe_cb.h:32-36, so no double evaluation via the cast).
testdep ccan/array_size. config.h here: HAVE_QSORT_R_PRIVATE_LAST=1, so
the default build maps asort() straight onto glibc qsort_r and asort.c
compiles to an empty object; the vendored fallback (#if
!HAVE_QSORT_R_PRIVATE_LAST, asort.c:4-455) was audited by reading and
by exercising it with the feature macro forced to 0.

## Mechanical checks

- ccanlint (before auditor additions): **35/40 FAIL**. Every check
  passes except objects_build_without_features, which fails to compile
  asort.c under the reduced-features config
  (HAVE_QSORT_R_PRIVATE_LAST=0): "error: static declaration of
  '__mempcpy' follows non-static declaration" at asort.c:41 (this is
  finding F1). tests_pass, tests_pass_valgrind,
  tests_pass_valgrind_noleaks, examples_compile, examples_run all PASS.
- After adding the two auditor tests: ccanlint **28/41 FAIL** — the
  failing check is tests_compile, because test/run-fallback-build.c
  forces the fallback on and hits the same asort.c:41 error (F1);
  reduced-features checks are skipped behind the tests_compile
  dependency. Same root cause, surfaced as designed.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; run.c #includes
  ccan/asort/asort.c directly; linked with ccan/tap/tap.c only;
  per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **run.c 8/8 ok, zero sanitizer diagnostics** (default qsort_r path;
  compile_fail-context-type.c is compile-only, covered by ccanlint).
- Fallback path under clang 18 ASan+UBSan: run.c rebuilt with a config
  forced to HAVE_QSORT_R_PRIVATE_LAST=0 (/tmp/asort-asan/config.h) and
  a /tmp harness copy of asort.c with `__mempcpy` renamed to work
  around F1 (test harness only — production files untouched): **8/8 ok,
  zero diagnostics**.
- Fallback stress test (/tmp/asort-asan/stress.c, harness copy, ASan+
  UBSan): int arrays n=0..299 with heavy duplication; 200000-int malloc
  path; mergesort stability check (key,original-index pairs) n=2..1999;
  44-byte structs (indirect/pointer sort) n=2..499 with sortedness and
  multiset-preservation checks; misaligned 3-byte elements (SWAP_BYTES);
  heapsort_r called directly (malloc-failure fallback) on ints n up to
  3641 and 44-byte structs; 10000 doubles (SWAP_WORDS_64):
  **STRESS ALL OK, zero diagnostics**. Mergesort confirmed stable in
  every run.
- Malloc-failure fallback end-to-end (/tmp/asort-asan/repro-mallocfail.c,
  malloc interposed to return NULL inside asort.c): _asort sorted
  100000 ints correctly via heapsort_r. Clean.
- -m32: run.c (default path) builds and passes 8/8 under clang -m32
  ASan+UBSan (with HAVE_QSORT_R_PRIVATE_LAST=1 asort.c compiles to
  nothing, so the errno.h 32-bit header problem does not bite). The
  fallback harness cannot be built -m32 (errno.h -> missing 32-bit
  asm headers, known host limitation); fallback size_t arithmetic
  reviewed for 32-bit instead (no overflow possible for valid arrays,
  see R1/R8).
- Reproducers/probes in /tmp/asort-asan/ (temporary, not committed):
  config.h (HAVE_QSORT_R_PRIVATE_LAST=0 variant), asort-harness.c
  (rename workaround), run-fallback.c, stress.c, repro-mallocfail.c,
  repro-include-order.c, run-fallback-build-simrepair.c.

## Findings

## F1 — CONFIRMED (FIXED in 0d54244f + d878b036): the qsort_r fallback (asort.c) does not compile on glibc — static `__mempcpy` collides with glibc string.h's extern declaration

- Location: ccan/asort/asort.c:41-44, function __mempcpy:
  ```c
  static inline void *__mempcpy(void *dst, const void *src, size_t n)
  ```
- Reachable path: any build on glibc with HAVE_QSORT_R_PRIVATE_LAST=0.
  The fallback exists precisely for platforms/configs without the
  glibc-convention qsort_r, and HAVE_QSORT_R_PRIVATE_LAST=0 on glibc is
  a configuration the module's own tooling exercises: ccanlint's
  reduce_features check regenerates config.h with features off, and
  objects_build_without_features then fails. Also reachable via
  hand-written/cross-compilation configs that mis-detect qsort_r.
- Mechanism: glibc's <string.h> declares
  `extern void *__mempcpy(void *restrict, const void *restrict, size_t)`
  under __USE_MISC (string.h:371-399, /usr/include/string.h:397), and
  __USE_MISC is enabled by _DEFAULT_SOURCE, i.e. in every default
  (-std=gnu*) build — no _GNU_SOURCE needed. The vendored file then
  defines a `static inline` function of the same name: constraint
  violation, hard error.
- Observed output (gcc 13 and clang 18, asort.c compiled with config
  forced to HAVE_QSORT_R_PRIVATE_LAST=0):
  ```
  ccan/asort/asort.c:41:21: error: static declaration of '__mempcpy'
  follows non-static declaration
  /usr/include/string.h:397:14: note: previous declaration ... is here
  ```
  Identical error from ccanlint's objects_build_without_features
  (baseline score 35/40 FAIL). The error persists with config.h's
  _GNU_SOURCE line removed (it is _DEFAULT_SOURCE, not _GNU_SOURCE,
  that exposes the declaration); only strict-ANSI mode
  (`gcc -std=c99`, __USE_MISC off) compiles the fallback — verified.
- Caller preconditions: none violated; this is a plain build failure of
  a configuration the module ships and its lint suite tests. The
  portability adaptations in commit fe99a8e0 renamed glibc-internal
  helpers to "local static inline helpers" but kept a name glibc
  reserves in its public headers; __memswap (asort.c:48) is fine
  because glibc never declares it publicly.
- Concrete consequence: on glibc the fallback is unbuildable dead code
  in default builds; the module fails its own ccanlint suite today.
  Non-glibc targets of the fallback (BSD, macOS — whose string.h has no
  __mempcpy) are unaffected.
- Regression test: ccan/asort/test/run-fallback-build.c — #undefs
  HAVE_QSORT_R_PRIVATE_LAST after config.h, includes asort.c, and
  sorts via the fallback (stack-buffer mergesort, malloc-path
  mergesort, and 44-byte indirect sort; 3 subtests, alarm(10)-bounded).
  Today it FAILS TO COMPILE on glibc (tests_compile FAIL in ccanlint).
  Verified to pass 3/3 under clang ASan+UBSan once the collision is
  removed (simulated repair via the renamed /tmp harness copy).
- Repair direction: rename the helper and its two call sites, e.g.
  `asort_mempcpy` (asort.c:41, :57, :319, :325) — the rename used for
  the /tmp harness copy is exactly this and builds+passes cleanly.
  (Guarding with #ifdef __GLIBC__ to use the libc __mempcpy would also
  work, but a private name is simpler and portable.)

## F2 — CONFIRMED (FIXED in a13c9b97): on the qsort_r path, asort() expands to a call of undeclared qsort_r whenever a libc header was included before <ccan/asort/asort.h> (hard error on clang >= 15 / gcc >= 14)

- Location: ccan/asort/asort.h:25-26:
  ```c
  #if HAVE_QSORT_R_PRIVATE_LAST
  #define _asort(b, n, s, cmp, ctx) qsort_r(b, n, s, cmp, ctx)
  ```
- Reachable path: default config on glibc (HAVE_QSORT_R_PRIVATE_LAST=1),
  user translation unit that includes any libc header (<stdio.h> etc.)
  before <ccan/asort/asort.h> and does not itself define _GNU_SOURCE.
  glibc declares qsort_r only under _GNU_SOURCE; config.h does
  `#define _GNU_SOURCE` (config.h:4-6) but asort.h includes config.h
  only after the user's earlier libc includes have already processed
  <features.h>, so the define comes too late. Including system headers
  before a library header is ordinary, unconstrained usage — the
  module's docs state no include-order precondition.
- Observed output (/tmp/asort-asan/repro-include-order.c: stdio.h,
  stdlib.h, string.h, then ccan/asort/asort.h):
  - clang 18 -std=gnu17 (default flags): `error: call to undeclared
    function 'qsort_r'; ISO C99 and later do not support implicit
    function declarations` — hard build failure.
  - gcc 13: `warning: implicit declaration of function 'qsort_r'` —
    compiles, links and runs on x86-64 only by luck of the calling
    convention (implicit int-returning decl for a void function);
    gcc >= 14 turns this into an error by default.
- Concrete consequence: asort is unusable in that include order on
  modern compilers; on older gcc the call relies on an implicit
  declaration (constraint violation).
- Regression test: ccan/asort/test/run-include-order.c — includes
  stdio/stdlib/string/stdbool/unistd before <ccan/asort/asort.h> and
  sorts a 7-element array (1 subtest, alarm(10)-bounded). Today: fails
  to compile under clang 18 (the finding); compiles with a warning and
  passes under gcc 13 (so ccanlint, which drives gcc, still passes it —
  the test's value is on clang/newer-gcc builds). After repair it must
  compile warning-free everywhere and pass.
- Repair direction: stop calling qsort_r from the header macro — define
  a real `_asort()` function in asort.c unconditionally and let it call
  qsort_r internally when HAVE_QSORT_R_PRIVATE_LAST (asort.c is always
  compiled with config.h included first, where _GNU_SOURCE is
  effective). This also removes the empty-object build and keeps one
  ABI entry point on all platforms.

## Rejected candidates (disproved)

- R1: Mid-computation / size overflow. msort_with_tmp splits via
  `n1 = n / 2; n2 = n - n1` (asort.c:246-247) — no (lo+hi)/2 pattern.
  `total_size = total_elems * size` (asort.c:437) cannot overflow
  size_t for any array that actually exists (nmemb*size <= object size
  <= SIZE_MAX). The indirect path's `2 * total_elems * sizeof(void *) +
  size` (asort.c:440) is bounded by (16/33) * nmemb*size + size since it
  is only taken for size > 32 — no overflow on 32- or 64-bit. Rejected.
- R2: Stability guarantee. Neither _info nor asort.h promises
  stability ("The resulting array will be in ascending sorted order",
  asort.h:16). The mergesort is stable (`cmp(b1, b2, arg) <= 0` takes
  the left run, asort.c:263 etc.; verified for n=2..1999 in the stress
  test), but the malloc-failure heapsort fallback and the default
  glibc qsort_r path are not — since no stability is documented, no
  contract is violated. The commit message of fe99a8e0 advertises
  "stable mergesort" but the public docs do not; noted, rejected.
- R3: Allocation failure. qsort_r_malloc (asort.c:412-427) saves errno
  around malloc, returns false on failure, and _asort falls back to
  heapsort_r (asort.c:449-451). Verified end-to-end with malloc
  interposed to fail (100000 ints sorted correctly, ASan-clean) and by
  calling heapsort_r directly across sizes/element widths. Rejected.
- R4: Comparison callback contract. Only the sign of cmp's return is
  used (`< 0`, `<= 0`, `>= 0`) — INT_MIN-safe. Callbacks receive only
  pointers to real elements (direct paths: b1/b2 inside the array runs;
  indirect path: tp[] entries built from base, asort.c:348-352), never
  NULL or out-of-array — verified under ASan by the stress test.
  total_order_cast enforces the cmp/base/ctx type match at compile
  time; mismatch is covered by the existing
  test/compile_fail-context-type.c. Rejected.
- R5: void * arithmetic (asort.c:90-92, 103-105, 137-145, 206, 344-346).
  GNU C extension; ccanlint shows -Wpointer-arith warnings but the
  module already requires GNU-ish compiler builtins via ccan/order and
  ccan/typesafe_cb. Speculative portability; rejected.
- R6: Alignment. get_swap_type (asort.c:170-183) gates the u32/u64
  aliased swaps on both size divisibility and base alignment; the merge
  temp is _Alignas(uint64_t) on stack (asort.c:444) or malloc'd; the
  indirect path's tmp_storage is only accessed via memcpy
  (asort.c:364-378). Stress exercised a deliberately misaligned base
  (SWAP_BYTES) and 44-byte structs under ASan+UBSan: clean. Rejected.
- R7: Heap index overflow `2 * k + 1` (asort.c:132-135): needs k near
  SIZE_MAX/2, i.e. an array far larger than the address space.
  Unreachable; rejected as speculative.
- R8: asort macro double evaluation (asort.h:21-23): `base` appears in
  (base) and sizeof(*(base)) — the sizeof evaluates its operand only
  for VLA element types; `ctx` appears inside __typeof__ (unevaluated)
  plus once as the call argument; `num` once. Only side-effecting
  arguments with VLA-typed elements would double-evaluate — exotic and
  undocumented; rejected.
- R9: Self-comparison. Commit fe99a8e0 replaced the old quicksort
  precisely to avoid cmp(pivot, pivot): mergesort compares across
  disjoint runs, heapsort's do_swap never gets a == b (n >= 1 at
  asort.c:206). The default glibc qsort_r path DID self-compare in
  practice (run.c diag: "comparator called with identical pointers:
  yes"), which the C standard permits and run.c:80-84 explicitly
  documents. No defect in either path; rejected.
- R10: qsort_r shim signature (asort.h:26): matches glibc
  qsort_r(base, nmemb, size, cmp, arg) with
  cmp = int (*)(const void *, const void *, void *) = _total_order_cb;
  the configurator probe (tools/configurator/configurator.c:354-356)
  compiles a matching test under _GNU_SOURCE. BSD-style qsort_r
  platforms correctly get HAVE_QSORT_R_PRIVATE_LAST=0 and the fallback
  (subject to F1). The shim logic itself is sound; the two real defects
  around it are F1 and F2. Rejected otherwise.
- R11: test/run.c pseudo_random_array (run.c:43): `i * (INT_MAX/4 - 7)`
  wraps through unsigned — deterministic, test-only, and the sort
  assertions remain valid. Not a production defect; noted, rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/asort/test/run-fallback-build.c — proves F1. Forces
  HAVE_QSORT_R_PRIVATE_LAST=0 and includes asort.c; FAILS TO COMPILE on
  glibc today (ccanlint tests_compile FAIL, 28/41). Passes 3/3 under
  clang ASan+UBSan with the collision renamed (simulated repair), so it
  must compile and pass after F1 is repaired. alarm(10)-bounded.
- ccan/asort/test/run-include-order.c — proves F2. Includes libc
  headers before <ccan/asort/asort.h>; fails to compile under clang 18
  today ("call to undeclared function 'qsort_r'"), compiles with a
  warning and passes 1/1 under gcc 13. Must compile warning-free and
  pass after F2 is repaired. alarm(10)-bounded.

No production files were modified (asort.h, asort.c, _info untouched;
verified by git status — the only new paths under ccan/asort are the
two tests above).
