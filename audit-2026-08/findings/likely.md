# Audit: ccan/likely

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: likely.h (111 lines), likely.c (138 lines, all inside
`#ifdef CCAN_LIKELY_DEBUG`), _info, and ccan/likely/test/ (run.c,
run-debug.c). Dependencies under CCAN_LIKELY_DEBUG: ccan/str,
ccan/htable, ccan/hash (already-audited or taken as given; only misuse
flagged). Non-debug build is two `__builtin_expect` macros and has no
dependencies (config.h: HAVE_BUILTIN_EXPECT=1, so the builtin branch
is active).

The four prime suspects from the audit order were: stats buffer
overflow, double-counting, fork/thread safety of counters, and format
handling in the report. Three of the four were disproved (see Rejected
candidates R1, R2, R3); the real defect found is a NULL dereference in
likely_stats() on the "nothing meets the criteria" path when the
percent argument is >= 200 (F1).

## Mechanical checks

- ccanlint (before auditor additions): **51/51**, every check passes.
- After adding the auditor regression test
  (test/run-stats-empty-debug.c): ccanlint **42/47 FAIL** — the only
  failure is the new test segfaulting in tests_pass
  (`Segmentation fault (core dumped)`), which is the point of the
  regression test: it proves F1 pre-fix and must pass post-fix.
  run.c and run-debug.c still pass within the same run.
- All run-* tests under clang 18.1.3 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function -g -I.`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0,
  per-test `timeout 60`):
  - test/run.c: **4/4 pass, rc=0**, no sanitizer diagnostics.
  - test/run-debug.c: **14/14 pass, rc=0**, no sanitizer diagnostics.
  - test/run-stats-empty-debug.c (auditor-added): **aborts** on
    subtest 1 — UBSan "member access within null pointer of type
    'struct trace'" at ccan/likely/likely.c:105, then ASan SEGV on
    address 0x0. See F1.
- Same new test under plain `gcc -g -I.` (no sanitizers):
  **SIGSEGV, rc=139** — a real crash, not a sanitizer artifact.
- Probes/reproducers in /tmp/likely-asan/ (temporary, not committed):
  repro-percent.c, probe-many.c, probe-format.c. probe-many also run
  with detect_leaks=1: clean.

## Findings

## F1 — LIKELY (FIXED in 8cfa5f8c): likely_stats() dereferences NULL (SEGV) when no entry meets the criteria and percent >= 200, instead of returning NULL as documented

- Location: ccan/likely/likely.c:89-105, function likely_stats():
  ```c
  worst = NULL;
  worst_ratio = 2;
  for (t = thash_first(&htable, &i); t; t = thash_next(&htable, &i)) {
      if (t->count >= min_hits) { ... }
  }
  if (worst_ratio * 100 > percent)
      return NULL;

  maxlen = strlen(worst->condstr) + ...   /* line 105: worst == NULL */
  ```
  When no trace entry has `count >= min_hits` (including the empty-
  table case), `worst` stays NULL and `worst_ratio` stays 2.0. The
  guard at line 102, `worst_ratio * 100 > percent`, is `200.0 >
  percent` — false for any `percent >= 200` — so execution falls
  through to `strlen(worst->condstr)` with `worst == NULL`.
- Documented caller expectation: likely.h:70-101 documents @percent
  only as "maximum percentage correct" (no range restriction) and
  states: "It returns NULL when nothing meets those criteria." An
  empty table, or all entries below @min_hits, is exactly "nothing
  meets those criteria", so NULL is the documented result.
- Reachable path / reproducer (/tmp/likely-asan/repro-percent.c and
  the committed regression test ccan/likely/test/run-stats-empty-debug.c):
  ```c
  bad = likely_stats(0, 200);   /* empty table */
  ```
  or, with entries that never reach min_hits, `likely_stats(4, 200)`
  after two traced calls. Both crash at likely.c:105. The boundary is
  exact: percent=199 is safe (200.0 > 199 returns NULL), percent=200
  crashes. With a *qualifying* entry present, percent=200 works
  correctly (verified in probe-format.c and subtest 3 of the
  regression test).
- Observed output (clang 18 ASan+UBSan, run-stats-empty-debug):
  ```
  1..3
  ccan/likely/likely.c:105:25: runtime error: member access within null pointer of type 'struct trace'
  ccan/likely/likely.c:105:25: runtime error: load of null pointer of type 'const char *'
  AddressSanitizer:DEADLYSIGNAL
  ==...==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
  ```
  Plain gcc build: `Segmentation fault (core dumped)`, rc=139.
- Consequence: a single call to the module's documented reporting
  function crashes the process (NULL read → SIGSEGV) instead of
  returning NULL, whenever the caller sweeps the table with a percent
  threshold >= 200 and no branch currently qualifies.
- LIKELY rather than CONFIRMED: the crash is unambiguous and fully
  reproduced, but it requires `percent >= 200`, and "maximum
  percentage correct" semantically caps at 100 — every in-tree and
  documented caller uses <= 100 (the header example uses 95; percent
  101-199 is odd but harmless, >= 200 crashes). Nothing in the text
  forbids larger values, and the documented NULL-return sentence
  arguably covers this case, so it is reported rather than ignored as
  a precondition violation; the classification reflects the
  degenerate-argument reachability.
- Repair direction: after the scan loop, treat "no qualifying entry"
  independently of the ratio check, e.g.
  ```c
  if (!worst)
      return NULL;
  if (worst_ratio * 100 > percent)
      return NULL;
  ```
  One added line; no behavior change for any currently working call
  (when `worst != NULL`, `worst_ratio <= 1`, so percent >= 200 already
  reports it). Post-fix the regression test must pass 3/3 and run
  UBSan-clean.

## Rejected candidates (disproved)

- R1 (prime suspect): stats report buffer overflow in likely_stats()
  (likely.c:105-110). Disproved arithmetically and empirically. Output
  length = len(file) + len(condstr) + len("un") + digits(line) +
  digits(pct<=100) + digits(right) + digits(count) + 24 literal chars
  + NUL. Budget beyond the two strings is `sizeof(long)*8 +
  sizeof(fmt)` = 64+48 = 112 on LP64 (need at most
  2+10+3+20+20+24+1 = 80) and 32+48 = 80 on ILP32 (need at most
  2+10+3+10+10+24+1 = 60). Comfortable margin on both, and
  `snprintf(ret, maxlen - 1, ...)` bounds the write regardless (the
  size argument is one byte *conservative*, never overflowing).
  probe-format.c (156-char stringified condition, 100000 hits, percent
  200 with a qualifying entry) produced the complete, untruncated
  report ending in `)` under ASan, rc=0. Rejected.
- R2 (prime suspect): double-counting in _likely_trace(). Each macro
  expansion calls _likely_trace once with `!!(cond)` already computed;
  count++ happens exactly once per call and right++ only when
  cond==expect. run-debug.c's exact-count expectations (1/3, 2/3, 3/4,
  4/4) all pass; probe-many.c created 60 distinct entries and drained
  exactly 60 reports (n back to 0), ASan+LSan clean. Short-circuit
  non-evaluation (e.g. `likely(a) && unlikely(b)` skipping the second
  trace when a is false) is correct tracing semantics, not a defect.
  Rejected.
- R3 (prime suspect): fork/thread safety of the counters. True that
  the global htable and count/right fields have no synchronization —
  concurrent likely() calls from multiple threads under
  CCAN_LIKELY_DEBUG are a data race with lost updates (and
  thash_add/get are not thread-safe either). But neither the header
  nor _info documents any thread-safety contract, the facility is an
  opt-in debug/tracing mode, and fork is harmless (each process gets
  its own copy of the table; counters stay consistent per-process).
  Speculative hardening per AGENTS.md. Rejected.
- R4: division by zero in right_ratio() (likely.c:77,
  `t->right / t->count`). Every entry in the table has count >= 1:
  add_trace() inserts the entry and _likely_trace() increments
  p->count immediately after, in the same call, before any
  likely_stats() can observe it. Rejected.
- R5: unchecked malloc in add_trace() (likely.c:51-52) — NULL deref
  on OOM. CCAN-wide convention is not to check malloc; speculative
  hardening. Rejected.
- R6: iterate-after-delete in likely_stats_reset() (likely.c:129-134,
  thash_next after thash_del/free). htable_del_ (ccan/htable/htable.c:429)
  never rehashes — rehash_table is only called on the add path
  (htable.c:417) — so iterator offsets stay valid; and even a skipped
  entry could not leak or be lost, because the outer `while
  ((t = thash_first(...)))` re-scans from the start until the table is
  empty. probe-many.c: 60 entries, likely_stats_reset(), table empty
  afterwards, ASan+LSan clean. Rejected.
- R7: macro double evaluation of cond. Non-debug: `__builtin_expect(!!(cond), 1)`
  evaluates cond once. Debug: cond is evaluated once as a function
  argument to _likely_trace; stringify(cond) is unevaluated. Rejected
  by reading.
- R8: report percentage truncation, `(unsigned)(worst_ratio * 100)`
  rounds 99.9% down to "99%" (likely.c:113). Matches the module's own
  test expectations ("correct 33%" for 1/3); cosmetic, not a
  correctness defect. Rejected.
- R9: hash seed `trace->line + trace->expect` (likely.c:20) can wrap
  unsigned int. Harmless for a hash input; equality is decided by
  trace_eq on all four fields. Rejected.
- R10: dedup relying on string-literal pointer equality in trace_eq()
  (likely.c:25-26, `t1->condstr == t2->condstr`). All calls from one
  expansion site pass the same literal, hence the same pointer; two
  sites never compare equal on file/line anyway. Rejected.
- R11: off-by-one in `snprintf(ret, maxlen - 1, ...)` (likely.c:110) —
  writes at most maxlen-1 bytes into a maxlen buffer, wasting one
  byte; verified complete output in probe-format.c. Rejected (folded
  into R1's analysis).

## Auditor-added test files (temporary; keep or remove later)

- ccan/likely/test/run-stats-empty-debug.c — 3 TAP subtests,
  alarm(10)-bounded: (1) likely_stats(0, 200) on an empty table must
  return NULL; (2) likely_stats(4, 200) with only below-min_hits
  entries must return NULL; (3) likely_stats(2, 200) with a qualifying
  entry must still report it ("correct 0% (0/2)"). Pre-fix it SIGSEGVs
  on subtest 1 (plain gcc rc=139; ASan/UBSan abort at likely.c:105),
  proving F1; post-fix it must pass 3/3 UBSan-clean. ccanlint with
  this file present: 42/47, sole failure is this test in tests_pass
  (expected until F1 is repaired).

No production files were modified (likely.h, likely.c, _info
untouched; verified by git status — the only new path under
ccan/likely is the test above).
