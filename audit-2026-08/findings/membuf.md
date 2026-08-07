# Audit: ccan/membuf

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: membuf.h (236 lines), membuf.c (60 lines), _info,
ccan/membuf/test/ (run.c). ~296 LOC. Dependency ccan/tcon treated as
given; only misuse of its documented behavior is flagged (none found).
Suspects from the brief: growth arithmetic overflow (CONFIRMED, F1),
prepare_space failure semantics (holds except when F1 defeats it — the
in-tree json_out mkroom fix at json_out.c:127 relies on exactly this
check), realloc move handling (disproved, R5), tcon canary usage
(disproved, R6). NULL-pointer arithmetic UB found additionally (F2).

Key documented contract (membuf.h): membuf_init's example initializes
with `NULL, 0` (membuf.h:48). membuf_prepare_space documents arbitrary
@num_extra ("the minimum number of elements of space we need",
membuf.h:153) with a defined failure mode: "If you want to check for
expandfn failure (which sets errno to ENOMEM), you can check if
membuf_num_space() is < num_extra which will never otherwise happen"
(membuf.h:163-165). membuf_add documents "We assume expandfn
succeeded" (membuf.h:197) and "@num ... (must be that much space
available!)" (membuf.h:178).

## Mechanical checks

- ccanlint (before auditor additions): **49/50 PASS**, every check
  passes; partial credit only on tests_coverage (+5/6).
- After adding the two auditor regression tests: ccanlint **43/47
  FAIL** — exactly as designed: run-overflow fails 3/6 against the
  current code (tests_pass); run-null-init passes in ccanlint's plain
  build (it only aborts under UBSan, F2).
- Existing tests under clang 18.1.3 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; run.c #includes
  ccan/membuf/membuf.c directly, linked with ccan/tap/tap.c; per-test
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0),
  LP64:
  - run.c: **1412/1412 clean**, zero sanitizer diagnostics (exercises
    growth, doubling, memmove-down, consume, cleanup).
  So on the happy path (non-NULL init, satisfiable sizes) the module is
  sanitizer-clean.
- -m32: not attempted (membuf.c includes errno.h; per audit brief,
  -m32 skipped on this host for such modules).
- Reproducers/probes in /tmp/membuf-asan/ (temporary, not committed):
  repro-overflow.c (F1), probe-disproofs.c (heuristic wrap, sum-only
  wrap, NULL-init UB, memmove path), probe-de.c (add-on-OOM assert,
  memmove delta), probe-tcon.c (canary type-mismatch diagnostic).

## Findings

## F1 — CONFIRMED (FIXED in 4b2d55b8): membuf_prepare_space_() growth size arithmetic overflows size_t; the documented failure check then reports SUCCESS with a wrapped-size (tiny) buffer

- Location: ccan/membuf/membuf.c:45-46, function membuf_prepare_space_:
  ```c
  expand = mb->expandfn(mb, mb->elems,
                        (mb->max_elems + num_extra) * elemsize);
  ```
  followed by membuf.c:50 (`mb->max_elems += num_extra;`).
- Documented contract: membuf.h:153-165 — @num_extra has no documented
  upper bound, and failure is an anticipated, documented outcome
  detectable via `membuf_num_space() < num_extra` "which will never
  otherwise happen". The in-tree json_out mkroom fix (json_out.c:127)
  relies on exactly this check; membuf_realloc (membuf.c:57-60) is the
  documented expandfn.
- Reachable path: caller requests space for a huge element count (e.g.
  a 64-bit count/length taken from a file or protocol message) with
  elemsize > 1, e.g. `membuf_prepare_space(&mb, SIZE_MAX/4 + 1)` on a
  MEMBUF(int) with max_elems=4: the sum 4 + 2^62 does not wrap, but the
  product (2^62+4)*4 = 2^64+16 wraps to 16. realloc(ptr, 16) succeeds;
  membuf.c:50 sets max_elems to 2^62+4. The request (2^64+16 bytes)
  could never have succeeded — glibc rejects sizes > PTRDIFF_MAX — so
  the only correct outcome is the documented ENOMEM failure.
  Precisely: the defect triggers when the product wraps while the sum
  does not (a wrapping sum leaves max_elems < num_extra, so the
  documented check still reports failure — probe B, R3). On ILP32 the
  same wrap triggers at num_extra = 2^30+1 for elemsize=4.
- Caller preconditions: none violated — huge-but-unsatisfiable requests
  are exactly what the documented failure semantics exist for.
- Concrete incorrect consequence, observed
  (/tmp/membuf-asan/repro-overflow.c, clang 18 ASan+UBSan):
  ```
  documented check reports SUCCESS: num_space=4611686018427387908
    (0x4000000000000004) num_extra=4611686018427387904
    (0x4000000000000000) errno=0
  max_elems=4611686018427387908 (0x4000000000000004)
  ==ERROR: AddressSanitizer: heap-buffer-overflow
  WRITE of size 4 ... 0 bytes after 16-byte region
  ```
  i.e. errno is not set, membuf_num_space() >= num_extra ("success"
  per the documented check), but the buffer holds 4 ints; a caller
  following the documented protocol (json_out's mkroom pattern:
  prepare, check num_space, then write) heap-overflows on its very
  first writes. With attacker-influenced counts this is a classic
  allocation-size-overflow corruption primitive (the same defect class
  tal guards against with its "allocation size overflow" error).
- Regression test: ccan/membuf/test/run-overflow.c (alarm(10)-bounded,
  6 subtests; memory-safe even when failing — it never writes into the
  bogus buffer). Currently `not ok 4`, `not ok 5`, `not ok 6` in plain
  gcc and ASan builds; must pass after repair.
- Repair direction: detect the overflow before calling expandfn and
  take the documented failure path (state unchanged, errno = ENOMEM),
  e.g. after the doubling adjustment at membuf.c:42-43:
  ```c
  if (num_extra > SIZE_MAX / elemsize - mb->max_elems) {
      errno = ENOMEM;
      return (char *)membuf_elems_(mb, elemsize) - oldstart;
  }
  ```
  (the subtraction covers both the sum wrap and the product wrap).

## F2 — CONFIRMED (FIXED in 4ee105f0): NULL-pointer arithmetic UB in membuf_elems_/membuf_space_ (and the prepare_space delta) on the documented NULL-init idiom — UBSan aborts

- Location: ccan/membuf/membuf.h:86 (membuf_elems_:
  `return mb->elems + mb->start * elemsize;`), membuf.h:133
  (membuf_space_, same pattern), and ccan/membuf/membuf.c:54
  (`return (char *)membuf_elems_(mb, elemsize) - oldstart;` with
  oldstart == NULL when expanding from a NULL buffer).
- Documented contract: membuf.h:48 documents
  `membuf_init(&intp_membuf, NULL, 0, membuf_realloc);` as the
  canonical empty initialization. With elems == NULL and start == 0,
  membuf_elems_ computes NULL + 0 — undefined per C11 6.5.6p8 (the
  pointer operand must point to or one-past an object); the
  subtraction at membuf.c:54 between a live pointer and NULL is
  likewise undefined per 6.5.6p9.
- Reachable path: membuf_init(&mb, NULL, 0, membuf_realloc) followed by
  any of membuf_elems/membuf_space/membuf_add/membuf_prepare_space —
  plain documented API use, no preconditions violated. (The existing
  run.c never NULL-inits, which is why the suite is sanitizer-clean.)
- Concrete incorrect consequence, observed: under clang 18
  ASan+UBSan (`-fno-sanitize-recover=all`), the regression test aborts
  on its third subtest:
  ```
  ccan/membuf/membuf.h:86:19: runtime error: applying zero offset to
  null pointer
  ```
  (/tmp/membuf-asan/probe-disproofs.c scenario C shows the same abort
  on the membuf_prepare_space path.) Any UBSan-instrumented consumer
  (CI, fuzzing) dies on the module's own documented usage, and the UB
  formally licenses optimizer assumptions (NULL+offset folding) — no
  miscompilation was observed in practice: gcc 13.3 -O0/-O2/-Os and
  clang 18 -O2 plain builds all produce correct results (probe C:
  content verified intact). Additionally, when expanding from NULL the
  delta returned at membuf.c:54 is garbage ((size_t) of the new
  allocation's address, e.g. 107448714318496 in probe C) rather than a
  meaningful "offset between old and new locations" — harmless today
  because no valid pointers into a NULL buffer can exist to adjust, but
  it is the same UB site.
- Regression test: ccan/membuf/test/run-null-init.c (alarm(10)-bounded,
  7 subtests). Passes 7/7 in plain gcc/clang builds today; aborts under
  UBSan at membuf.h:86; after repair it must run UBSan-clean.
- Repair direction: guard the arithmetic, e.g. in membuf_elems_/
  membuf_space_ return NULL when mb->elems is NULL (start/end are 0 in
  that state by invariant), and in membuf_prepare_space_ return 0 as
  the delta when oldstart was NULL (no old locations exist to adjust).

## Rejected candidates (disproved)

- R1: membuf_add with a failing expandfn (membuf.h:191-201): proceeds
  to membuf_added_ and, under NDEBUG, would silently corrupt
  bookkeeping — but the header explicitly documents the design
  assumption "We assume expandfn succeeded" (membuf.h:197) and
  membuf_add's own contract "@num ... (must be that much space
  available!)" (membuf.h:178); the supported failure-handling route is
  membuf_prepare_space + the num_space check. In assert-enabled builds
  the failure is caught: probe D aborts with
  `membuf.h:146: membuf_added_: Assertion 'num <= membuf_num_space_(mb)'
  failed`. Documented precondition; rejected.
- R2: heuristic-condition wrap at membuf.c:34
  (`membuf_num_elems_(mb) + num_extra <= mb->max_elems`): with
  num_elems=8, max_elems=16, num_extra=SIZE_MAX-4 the sum wraps to 3
  and the memmove path is wrongly "chosen" — but the memmove is of a
  valid region (8 bytes), data stays intact, and afterwards
  num_space=8 < num_extra so the documented check still reports
  failure. Probe A: `failure correctly reported, data intact=1`.
  No OOB, no false success; rejected.
- R3: sum-only wrap in `mb->max_elems + num_extra` (membuf.c:46,50):
  when the sum wraps but the product does not, max_elems wraps below
  num_extra, so membuf_num_space() < num_extra and the documented
  check still reports failure (probe B: `num_space=3 -> failure
  correctly reported`). Self-correcting; only the product wrap is
  dangerous (F1). Rejected as a separate defect.
- R4: memmove-down path (membuf.c:33-37): heuristic conditions verified
  by case analysis (after expansion num_space >= num_extra in all
  branches: if num_extra >= max_elems, new space = max + num_extra -
  num_elems >= num_extra; else new space >= 3*max/2 > num_extra or
  >= max > num_extra). Probe E: delta=-48 as expected, data intact,
  tail writes to full num_space ASan-clean; run.c's 1000-iteration
  single-element loop exercises it 1412/1412 clean. Rejected.
- R5: realloc move/failure handling (membuf.c:45-52, membuf_realloc):
  on realloc failure the original pointer is preserved and mb->elems
  stays valid (state untouched, errno=ENOMEM); on a moving realloc the
  returned delta correctly reflects the new base (json_out's
  run-move_cb passes; probe C/E deltas verified). Rejected.
- R6: tcon canary usage: MEMBUF(type) via TCON_WRAP, tcon_check_ptr on
  @elems at membuf.h:51, tcon_sizeof for elemsize, tcon_cast_ptr on
  results — all used per tcon's documented contract. Probe-tcon.c:
  `membuf_init(&mb, (char *)p, ...)` on a MEMBUF(int) yields the
  designed "pointer type mismatch in conditional expression" warning;
  correct types and void*/NULL pass silently. Rejected.
- R7: membuf_consume/membuf_added/membuf_unadd with num exceeding the
  population/space: each is guarded by an assert encoding the
  documented precondition (membuf.h:106,139,146,206,213). Violations
  are caller errors; rejected.
- R8: membuf_cleanup then expand: NULL expandfn call is the documented
  behavior ("crash if you try to expand it", membuf.h:221). Rejected.
- R9: negative delta on memmove-down returned as wrapped size_t
  (membuf.c:54): callers cast to ptrdiff_t — implementation-defined,
  identity on every two's-complement target; run-move_cb-style use
  verified correct (probe E delta=-48). Same reasoning as json_out
  audit R4. Rejected.
- R10: membuf_init_ with elems==NULL but max_elems>0, or a custom
  expandfn that shrinks: caller violates the documented init/expandfn
  contracts (membuf.h:43-45, _info example). Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/membuf/test/run-overflow.c — proves F1. Fails 3/6 against
  current code in plain gcc and ASan builds (`not ok 4`, `not ok 5`,
  `not ok 6`: the documented check reports success and errno is unset
  after an impossible growth request); alarm(10)-bounded, memory-safe
  by construction (asserts the check result, never writes into the
  bogus buffer). Must pass after repair.
- ccan/membuf/test/run-null-init.c — proves F2. Passes 7/7 in plain
  gcc -O0/-O2 and clang -O2 builds today; aborts under clang 18
  ASan+UBSan at membuf.h:86 ("applying zero offset to null pointer");
  alarm(10)-bounded. Must run UBSan-clean after repair.
- Both #include ccan/membuf/membuf.c per run-test convention (ccanlint
  only links module objects into api tests).
- ccanlint with both present: 43/47 FAIL, solely because run-overflow
  fails against the current code (tests_pass).

No production files were modified (membuf.h, membuf.c, _info untouched;
verified by git status — the only new paths under ccan/membuf are the
two tests above).
