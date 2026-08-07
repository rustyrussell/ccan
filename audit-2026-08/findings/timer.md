# Audit: ccan/timer

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: timer.h, timer.c, design.txt, _info and ccan/timer/test/ only.
benchmarks/ excluded (not linked into the module).  Deps ccan/array_size,
ccan/ilog, ccan/time: documented behavior taken as given (timer uses
ilog64/ilog64_nz and the timemono helpers per their contracts; no misuse
found).  ccan/list already audited.

Mechanical checks:
- ccanlint: 61/66.  The only check losing points is tests_coverage
  (+1/6; "1463 of 2280 lines covered").  All other checks pass,
  including tests_pass, tests_pass_valgrind and
  tests_pass_valgrind_noleaks.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  -g, -I.; each test #includes timer.c directly; linked with tap.c,
  list.c, time.c, ilog.c; per-test `timeout 120`), LP64:
  8/8 pass cleanly, zero sanitizer diagnostics:
  run (495 ok), run-add (11666), run-allocator (12), run-corrupt (7),
  run-corrupt2 (1), run-original-corrupt (2), run-expiry (7), run-ff (3).
- Same 8 tests built and run under `-m32` ASan+UBSan (ILP32): all pass
  cleanly, no sanitizer diagnostics (one pre-existing cosmetic -Wformat
  warning in the CCAN_TIMER_DEBUG dump code, timer.c:507 `%lu` vs
  uint64_t — see R6).
- Debug knob: CCAN_TIMER_DEBUG (timers_dump) is exercised by run.c,
  run-corrupt.c, run-corrupt2.c, run-original-corrupt.c, run-allocator.c;
  timers_check() is always compiled and called by every test.
- Config knobs: run.c passes with -DTIMER_LEVEL_BITS=6 (495/495).  With
  -DTIMER_GRANULARITY=1 run.c reports 94 failures — test-harness
  artifact, not a module defect; see R3.
- Differential fuzzer /tmp/timer-asan/fuzz.c (model-checked random
  add/del/expire/earliest, timers_check() after every op, deltas up to
  2^62 grains, base swept to 2^60-1 so levels 0-11 are all exercised):
  3 x 300,000 ops under ASan+UBSan, zero failures, zero diagnostics.
  (First uncapped run independently rediscovered F1.)
- Note: on this host LeakSanitizer's exit-time check can hang; runs were
  done with detect_leaks=0 where relevant (same host issue noted in the
  opt audit).  Sanitizer transcripts captured with the default
  recover-mode UBSan.
- Reproducers live in /tmp/timer-asan/repro-*.c (temporary, not
  committed).

Auditor-added test files (temporary; keep or remove later):
- ccan/timer/test/run-expire-alloc-fail.c — proves F2.  Currently
  crashes: SEGV (plain build, rc=139) / NULL-member-access UBSan report
  + ASan SEGV at timer.c:368 (sanitizer build, rc=134).  alarm(10)
  bounded; no production files modified.
- ccan/timer/test/run-far-level12.c — proves F1.  Currently fails:
  UBSan "shift exponent 65" at timer.c:317, then hangs in
  timers_expire() until alarm(10) fires (rc=142, both plain and
  sanitizer builds).  The test deliberately jumps base 0 -> 2^60 in one
  expire() call: any intermediate expire raises base and masks the hang
  (see F1).

## Findings

## F1 — CONFIRMED (FIXED in f7b21467): timers >= 2^60 grains away hit `1ULL << 65` UB in timer_fast_forward()/add_level(); far timer never promoted, timers_expire() can spin forever

- Location: ccan/timer/timer.c:317 (timer_fast_forward's
  `timers_far_get(timers, &list, timers->base +
  (1ULL << ((level+1)*TIMER_LEVEL_BITS))-1)`) and the identical
  expression at timer.c:173 (add_level).  With the default
  TIMER_LEVEL_BITS=5 and level == 12 the shift count is 65 — undefined
  behavior (C11 6.5.7p3).  A third site with the same root cause is
  timers_check() at timer.c:451-452 (`1ULL << (TIMER_LEVEL_BITS * l)`
  with l == 13), reachable once all 13 levels are allocated.
- Why level 12 is reachable: level_of() (timer.c:66-73) returns
  ilog64(diff/2)/5, which is 12 for diff >= 2^60 (ilog64(2^59) = 60,
  60/5 = 12).  level[12] is a valid array slot
  (struct timers.level[13], timer.h:206), so a timer >= 2^60 grains in
  the future is a legal, representable entry; because level[12] starts
  NULL it goes to the far list (timer.c:81-83).  2^60 grains is ~36.6
  years at TIMER_GRANULARITY=1 (ns) and ~36,600 years at the default
  1000 (us) — beyond real monotonic uptimes, but timer.h documents no
  upper bound on @when/@expire; the values are legal API inputs.
- Reachable path (minimal):
  1. timers_init(&timers, epoch0); timer_addmono(&timers, &t,
     grains_to_time(1ULL << 60)) → far list, timers->first = 2^60.
  2. timers_expire(&timers, grains_to_time(1ULL << 60)): level[0] is
     allocated, first <= now, so timer_fast_forward(timers, 2^60) runs
     with changed = ilog64_nz(2^60) = 61 → level = 12, level[12] NULL
     → UB shift at timer.c:317.  On x86-64 the shift count is masked to
     1, so the far-pull bound becomes base+1 instead of 2^65-1: the far
     timer is NOT pulled.  need_level = 12, so add_level(12) repeats the
     UB at timer.c:173 with the same non-pull.  base becomes 2^60, the
     timer stays in far.
  3. Back in timers_expire(): level[0] bucket is empty, but
     update_first() keeps finding the far timer (first = 2^60 == base,
     <= now), timer_fast_forward(timers, 2^60) is a no-op, the pop
     returns NULL, update_first() returns true again — infinite loop in
     the `do { ... } while (!t && update_first(timers));` loop
     (timer.c:358-371).  The timer never expires.
- The hang needs base + (2^60 - 1) < T (base == 0 in practice): with
  base > 0 the level-11 far-pull bound base + 2^60 - 1 (computed with a
  legal 60-bit shift) reaches the stranded timer and rescues it — the
  UB shift still fires, but expiry completes.  This is why the
  regression test jumps base 0 -> 2^60 in a single timers_expire().
- Preconditions: none violated; documented contracts only forbid moving
  @expire backwards (timer.h:136) and adding a timer twice
  (timer.h:77).
- Concrete consequences: (a) undefined behavior (oversized shift) on
  any fast-forward crossing the 2^60-grain boundary while level 12 is
  unallocated — including with an empty far list, since the bound is
  computed unconditionally; (b) wrong shift result in practice → far
  timer never promoted → it never expires and timers_expire() hangs
  (CPU spin) when called at/past its time from base 0; (c) once level
  12 is allocated, timers_check() hits the third UB shift at
  timer.c:451-452.
- Reproducer: /tmp/timer-asan/repro-level12.c (alarm-bounded).
  Observed under clang 18 ASan+UBSan:
  ```
  added: t.time=1152921504606846976 base=0 first=1152921504606846976 far_empty=0
  ccan/timer/timer.c:317:20: runtime error: shift exponent 65 is too
    large for 64-bit type 'unsigned long long'
  Alarm clock   (rc=142 — hung in timers_expire until alarm(5))
  ```
  (timer.c:173 fires as well; seen in the fuzz transcript.)  Regression
  test ccan/timer/test/run-far-level12.c fails as designed: UBSan
  report at timer.c:317, then SIGALRM from the spin (rc=142, plain and
  sanitizer builds).  Note: test 5 ("not due at the epoch") uses
  expire == base precisely because any larger intermediate expire
  advances base and masks the hang (base + 2^60 - 1 >= 2^60).
- Repair direction: saturate the far-pull bound instead of shifting
  past 63, e.g.
  `unsigned int bits = (level+1)*TIMER_LEVEL_BITS; uint64_t bound =
   bits >= 64 ? -1ULL : timers->base + (1ULL << bits) - 1;`
  at timer.c:173 and timer.c:317 (watch base + bound wraparound: with
  the saturated -1ULL the intended meaning is "pull everything", so
  skip the addition), and guard timers_check()'s past_levels bound at
  timer.c:451-452 the same way (l*TIMER_LEVEL_BITS >= 64 → min 0 /
  whole range).  The level-12 bucket math itself ((time >> 60) & 31,
  timer.c:85) is fine for all uint64_t times.  The same saturation
  covers non-default TIMER_LEVEL_BITS values whose top level needs more
  than 64 bits (e.g. TIMER_LEVEL_BITS=6: top level 10, (10+1)*6 = 66).
  Note the fix must also make add_level(12) actually pull level-12 far
  timers (bound 2^65-1 means "all"), otherwise they stay stranded.

## F2 — CONFIRMED (FIXED in 1127eb30): allocator failure in add_level() during timers_expire() → NULL timer_level dereference at timer.c:368

- Location: ccan/timer/timer.c:352-356 (timers_expire allocates level 0
  on demand) and timer.c:368
  (`t = list_pop(&timers->level[0]->list[off], struct timer, list);`).
- Reachable path:
  1. Caller installs a custom allocator via the documented
     timers_set_allocator() (timer.h:173-174); allocation failure is
     anticipated by the module itself — add_level() checks for NULL and
     returns early (timer.c:163-165).
  2. timer_addmono() with level[0] == NULL places the timer on the far
     list (timer.c:81-83).
  3. timers_expire() with the timer due: add_level(timers, 0) fails and
     leaves timers->level[0] == NULL.  The code proceeds anyway;
     timer_fast_forward() copes with NULL levels (re-adds via the far
     list), but the pop at timer.c:368 dereferences
     timers->level[0]->list[off] — a NULL struct timer_level *.
- Preconditions: none violated.  run-allocator.c exercises the same
  API sequence with a *succeeding* allocator; the failing path is
  untested by the suite.
- Concrete consequence: NULL-pointer member access (offset
  16 + 16*off bytes) — immediate crash (SEGV read, would be a wild
  write in list_pop's unlink) on any OOM at the wrong moment.  With
  real malloc this needs genuine memory pressure (each level is
  512 bytes); with a hook allocator (the documented feature) it is
  trivially reachable.
- Reproducer: /tmp/timer-asan/repro-oom.c (alarm-bounded; allocator
  always fails).  Observed under clang 18 ASan+UBSan:
  ```
  ccan/timer/timer.c:368:7: runtime error: member access within null
    pointer of type 'struct timer_level'
  ccan/timer/timer.c:368:7: runtime error: applying non-zero offset 80
    to null pointer
  AddressSanitizer: SEGV on unknown address 0x000000000050 (READ)
  ```
  Plain clang build: SIGSEGV (rc=139).  Regression test
  ccan/timer/test/run-expire-alloc-fail.c fails as designed (crash
  before test 2).
- Repair direction: after the add_level(timers, 0) call, bail out if
  the allocation failed, e.g. `if (!timers->level[0]) return NULL;` —
  the timers stay on the far list, the structure remains consistent
  (timers_check passes), and a later timers_expire() with memory
  available delivers them.  The regression test encodes exactly this
  contract: expire-under-OOM returns NULL without losing the timer;
  once allocation succeeds the same timer expires normally.

## Rejected candidates (disproved)

- R1: Negative timemono passed to timer_addmono (e.g. tv_sec = -1).
  time_to_grains() (timer.c:38-42) converts tv_sec to uint64_t, so the
  value wraps to ~2^64 and the timer is treated as far-future instead
  of expiring immediately as timer.h:83-84 promises for "when ... is
  before time_mono()".  Reproduced: /tmp/timer-asan/repro-negative.c
  (t.time = 18446744073708551616, timer_earliest reports year ~584K).
  Rejected: a negative struct timemono is outside the type's domain
  (monotonic clock values from time_mono()/timemono_add(); time.h:62);
  the consequence is a never-firing timer, no memory-safety or UB
  issue.  Doc-gap at most.
- R2: Bucket/level arithmetic, cascade completeness and expiry ordering
  below 2^60 grains.  Disproved by construction and testing:
  level_of() gives level l exactly for diff in [2^(5l), 2^(5l+5)-2],
  one full 32-bucket cycle, so add_level()/timer_fast_forward()'s
  far-pull bound base + 2^(5(l+1))-1 covers every timer belonging to
  level l (for l <= 11); base never advances past the true minimum
  timer time (timers_expire fast-forwards to timers->first, and
  timers->first is only ever stale-low after timer_del), so the
  one-bucket-per-level cascade cannot strand timers below base.
  run-add.c checks all (base, diff) pairs up to 2^34 with timers_check;
  the differential fuzzer (900k ops, deltas to 2^62, base to 2^60-1,
  timers_check + model comparison every op) found zero divergence in
  expiry set, expiry order (nondecreasing) and timer_earliest.
- R3: run.c with -DTIMER_GRANULARITY=1 reports 94 failures.  Test
  artifact: run.c:75-91 adds timers at exp and exp+1 *nanoseconds* and
  expects both to expire at the same `earliest` — true only when both
  round to the same grain (granularity 1000).  With granularity 1 the
  +1ns timer is correctly not due.  Verified with
  /tmp/timer-asan/gran1-check.c (granularity 1, timers 1 grain apart
  expire separately and in order, structure empties cleanly; the
  observed hang in that check was this host's LSan exit issue, gone
  with detect_leaks=0).  Module behaves correctly.
- R4: timers_expire() called with now < base (clock stepping backwards,
  e.g. non-monotonic fallback or the OpenBSD/virtualbox case noted at
  timer.c:346-348) returns NULL without touching the structure;
  timers due exactly at base are correctly not expired (base > now
  means not yet due).  Handled per timer.h:136 and commit 48b4ffc3.
- R5: add_level() allocation failure in timer_fast_forward() (non-zero
  levels): pulled timers are re-added via timer_add_raw() after the
  base update and land back on the far list or the correct allocated
  level; need_level only records the lowest NULL level, but higher NULL
  levels merely keep their timers in far until needed.  No state loss;
  the only crash site from OOM is F2's level[0] pop.
- R6: CCAN_TIMER_DEBUG-only cosmetics: dump_bucket_stats() prints the
  empty-bucket newline with printf() instead of fprintf(fp) (timer.c:468)
  and timers_dump() uses %lu for the uint64_t base+i (timer.c:506-507,
  -Wformat warning on -m32).  Debug/dump path only, no defect in the
  data structure.
- R7: timer_del() of a never-added (but timer_init'd) or already
  deleted timer: list_del_init on a self-linked node is a no-op write
  to itself; documented as legal (timer.h:98-100) and covered by run.c.
  Double timer_add of a live node is caught by
  assert(list_node_initted()) (timer.c:107/123) and violates the
  documented precondition (timer.h:77 "initialized or timer_del'd") —
  out of scope.
- R8: timers->firsts[] watermarks going stale-low after timer_del or
  expiry of the minimum: only ever conservative (extra scanning in
  brute_force_first via level_may_beat), never stale-high, because adds
  lower the watermark and first_for_level() resets it to -1ULL only
  when a level scans empty.  Verified by the fuzzer's earliest-vs-model
  checks.
- R9: time_to_grains()/grains_to_time() round-trip and
  TIMER_GRANULARITY not dividing 10^9: conversion is exact for any
  divisor granularity (sec*q + nsec/g with nsec < g*q); overflow of
  tv_sec*q needs tv_sec ~ 2^64/10^6 (~585K years at g=1000) — time.h's
  domain.  timemono_add() overflow in timer_addrel is time.c's
  documented precondition ("must not overflow", time.h:504).
- R10: Re-entrancy: timers_expire() returns one timer at a time with
  the structure consistent between calls; caller callbacks that
  add/delete/free timers between calls are the documented usage pattern
  (timer.h:141 example, _info example) and are covered by
  run-corrupt2.c's add/earliest/expire interleaving and by the fuzzer.
- R11: get_first() fast path scanning only level-0 buckets >= base %
  PER_LEVEL: sound because buckets below the current offset are
  guaranteed empty (cascade invariant, R2) — and level-0 buckets each
  hold exactly one time value (level-0 diff range [0,31] == PER_LEVEL),
  justifying find_first()'s level-0 early break (timer.c:190).

No other findings.  Everything else in the audit plan (wraparound of
the time base, bucket-boundary adds, far-list promotion below level 12,
timers_check/timers_dump bounds, allocator API restore-to-default,
TIMER_LEVEL_BITS=6 knob, -m32) checked out clean under the sanitizer
suite and the model-checked fuzzer.
