# Audit: ccan/time

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: time.h (812 lines, mostly static inlines), time.c (138 lines),
_info and ccan/time/test/ only. No dependencies (_info "depends" is
empty; optional librt link only). config.h here: HAVE_CLOCK_GETTIME=1,
HAVE_STRUCT_TIMESPEC=1, so TIME_HAVE_MONOTONIC=1 and the
clock_gettime(CLOCK_MONOTONIC) path is active; the gettimeofday and
non-monotonic fallbacks were reviewed by reading only.

## Mechanical checks

- ccanlint (before auditor additions): **77/81**, every check passes
  except tests_coverage (+2/6). tests_pass, tests_pass_valgrind,
  examples_compile all PASS.
- After adding the two auditor tests: ccanlint **73/78 FAIL** — exactly
  as designed, because test/run-divide-roundup.c fails 3/3 subtests
  against the current code (F1). run-multiply-overflow.c passes in
  plain builds (it only aborts under UBSan, F2).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/time/time.c directly; linked with ccan/tap/tap.c; per-test
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0),
  LP64: **3/3 pass cleanly, zero sanitizer diagnostics** —
  run (66 ok), run-check (69 ok), run-monotonic (10 ok). Total 145 ok.
- -m32: builds and passes (run.c, 66 ok, ASan+UBSan); the module has no
  word-size-sensitive arithmetic beyond what uint64_t covers.
- Reproducers/probes in /tmp/time-asan/ (temporary, not committed):
  repro-divide-roundup.c, repro-divide-conseq.c, repro-divide-debug.c,
  repro-mult-overflow.c, repro-check-negnsec.c.

## Findings

## F1 — CONFIRMED (FIXED in dc847d88): time_divide() double path rounds tv_nsec up to exactly 1000000000, returning a malformed timerel from well-formed inputs (DEBUG builds abort on time_divide's own result)

- Location: ccan/time/time.c:57-64, concretely time.c:59-60:
  ```c
  double nsec = rem * 1000000000.0 + t.ts.tv_nsec;
  res.ts.tv_nsec = nsec / div;
  ```
  taken whenever `rem = t.ts.tv_sec % div` has any bit at or above
  2^30 (time.c:57).
- Reachable path: any call `time_divide(t, div)` with `div > 2^30` and
  `t.ts.tv_sec % div == div - 1` and `t.ts.tv_nsec` close to 999999999.
  The true sub-second quotient is then `1e9 - 1/div`, which is within
  half a double-ulp of 1e9 (ulp near 1e9 is ~1.2e-7, and 1/div < 1e-9),
  so the double division rounds UP to exactly 1000000000.0 and
  `res.ts.tv_nsec = 1000000000`. The integer path cannot produce this
  (exact floor of at most `div*1e9 - 1` over `div`).
- Caller preconditions: none violated — t is a well-formed timerel
  (tv_sec >= 0, 0 <= tv_nsec < 1e9), div nonzero. The module's own
  invariant (and the DEBUG TIMEREL_CHECK on every API boundary)
  requires every returned timerel to be well-formed.
- Observed output (/tmp/time-asan/repro-divide-roundup.c, first of a
  dense family — every div in [2^30+1, 2^30+100000) with
  tv_sec = div-1, tv_nsec = 999999999 misbehaves):
  ```
  div=1073741825 -> 0.1000000000 (MALFORMED)
  div=1073741826 -> 0.1000000000 (MALFORMED)
  ...
  ```
- Concrete consequences (/tmp/time-asan/repro-divide-conseq.c, plain
  build, rc=3):
  - Comparisons break: the result holds exactly 1 second worth of ns,
    yet `time_less(r, time_from_sec(1))` is true and
    `timerel_eq(r, time_from_sec(1))` is false — the comparison
    inlines (time.h:159-167, 213-221, 341-345) assume normalized
    inputs and compare tv_sec first.
  - `time_to_msec(r)` returns 1000 where the true quotient truncates
    to 999.
  - In DEBUG builds the defect is fatal: time_divide aborts on its own
    result at the TIMEREL_CHECK(res) on return (time.c:65).
    /tmp/time-asan/repro-divide-debug.c:
    ```
    ./ccan/time/time.c:65 (res) : malformed time 0.1000000000
    Aborted (core dumped)
    ```
    i.e. under DEBUG a fully valid time_divide call aborts the program.
- Regression test: ccan/time/test/run-divide-roundup.c (alarm(10)-
  bounded, 3 subtests). Currently fails 3/3 in a plain build; after
  repair it must pass.
- Repair direction: normalize or clamp the double-path result, e.g.
  after time.c:60 add
  `if (res.ts.tv_nsec >= 1000000000) { res.ts.tv_nsec -= 1000000000; res.ts.tv_sec++; }`
  (round-up lands on the correct {sec+1, 0}); clamping to 999999999 is
  equally acceptable given the fp path is already documented-in-spirit
  as approximate ("FIXME: fp is cheating!", time.c:58).

## F2 — LIKELY (FIXED in aa19e719): time_multiply() double path performs an out-of-range double->time_t conversion (UB) when the product exceeds ~INT64_MAX seconds; overflow precondition undocumented for this function

- Location: ccan/time/time.c:73-78, concretely time.c:77:
  ```c
  double nsec = (double)t.ts.tv_nsec * mult;
  res.ts.tv_sec = nsec / 1000000000.0;
  ```
  taken when `mult >= 2^30` (time.c:73).
- Reachable path: `time_multiply(t, mult)` with
  `t.ts.tv_nsec * mult > ~9.2e27` (e.g. t = {0, 999999999},
  mult = UINT64_MAX). Then nsec/1e9 > INT64_MAX and the
  floating-to-integer conversion is undefined behavior (C17 6.3.1.4/1).
- Observed output (/tmp/time-asan/repro-mult-overflow.c, clang 18
  ASan+UBSan no-recover):
  ```
  ccan/time/time.c:77:19: runtime error: 1.84467e+19 is outside the
  range of representable values of type 'long'
  Aborted (rc=134)
  ```
- Why LIKELY rather than CONFIRMED: the true product (~1.8e19 seconds)
  genuinely does not fit in a timerel, so the caller is asking for an
  unrepresentable result — an inferred-precondition violation. But
  unlike timeabs_add/timemono_add/timerel_add, whose docs explicitly
  say "The times must not overflow, or the results are undefined"
  (time.h:482, :504, :526), time_multiply's documentation
  (time.h:568-577) states no overflow precondition at all, and the
  failure mode is UB (trips UBSan CI) rather than a wrong value. The
  related `res.ts.tv_sec += t.ts.tv_sec * mult` overflow (time.c:85)
  is defined unsigned wraparound producing a silently negative tv_sec —
  same undocumented-precondition family, no UB.
- Regression test: ccan/time/test/run-multiply-overflow.c — passes in
  plain builds, aborts under UBSan today; after repair it must run
  UBSan-clean.
- Repair direction: either document the overflow precondition on
  time_multiply (matching the add functions), or guard the conversion
  (e.g. compute in unsigned 64-bit with explicit wrap/saturation when
  the double exceeds the time_t range). The DEBUG TIMEREL_CHECK cannot
  catch this — the UB happens before the check runs.

## Rejected candidates (disproved)

- R1: Signed overflow in time_to_msec/usec/nsec (time.h:614, :636,
  :658). The multiplications are done in uint64_t (the cast is applied
  to tv_sec before multiplying), so overflow is defined wraparound and
  needs tv_sec > ~1.8e16 seconds (~590 million years) for msec. A
  negative tv_sec wraps, but a negative timerel violates the module's
  documented non-negativity invariant (timerel_check exists precisely
  to reject it). No reachable incorrect consequence; rejected.
- R2: time_check_ does not normalize a negative tv_nsec
  (time.c:89-114): {5, -500000000} is returned as-is with no warning,
  and {-5, -1} is "normalized" to {0, -1} — still malformed
  (/tmp/time-asan/repro-check-negnsec.c). Rejected as outside the
  documented scope: the docs (time.h:85-89 etc.) define the check as
  exactly "isn't negative and doesn't have a tv_nsec >= 1000000000",
  and no module-internal path produces a negative tv_nsec from valid
  inputs (time_sub_'s borrow at time.h:353-357 is correct for
  well-formed inputs). Noted for completeness.
- R3: time_from_sec(1ULL<<63) wraps tv_sec to INT64_MIN silently in
  non-DEBUG builds (repro-check-negnsec.c). The value is
  unrepresentable in time_t — inferred precondition, same family as
  F2's overflow but defined behavior. Rejected.
- R4: time_divide(t, 0) — division by zero at time.c:45-46. Dividing
  by zero is an implied caller precondition for any divide API;
  rejected.
- R5: time_sub_/time_between/timemono_between with recent < old
  produce a negative timerel, silently returned in non-DEBUG builds.
  The docs state "@a: the larger time" / "@recent: the larger time"
  (time.h:364-365, :379-380, :394-395) — documented precondition;
  DEBUG builds abort. Rejected.
- R6: timeabs_sub/timemono_sub producing a negative absolute time
  (subtracting a larger rel). Same reasoning as R5 — implied
  precondition, DEBUG-checked. Rejected.
- R7: Double-precision accuracy loss in the fp paths of time_divide /
  time_multiply ("FIXME: fp is cheating!", time.c:58, :74). Bounded to
  a few hundred ns for in-range results; the module's own run.c
  explicitly allows either rounding ("Allow us to round either way",
  run.c:151). Only the round-up-to-1e9 case produces an out-of-range
  result — that is F1. Rejected otherwise.
- R8: TIME_HAVE_MONOTONIC gating (time.h:72-76, time.c:31-35): the
  macro is defined from HAVE_CLOCK_GETTIME && defined(CLOCK_MONOTONIC)
  and time.c uses it consistently; the fallback (time_mono aliases
  time_now) is explicitly documented (time.h:67-70). No defect.
- R9: timemono vs timeabs confusion: the wrapper structs are distinct
  C types, so mixing them is a compile error — the design the module
  exists for. The shared time_greater_/time_less_/time_sub_/time_add_
  helpers take bare struct timespec and are type-agnostic by design.
  No reachable confusion; rejected.
- R10: Comparison inlines on non-normalized inputs (tv_nsec >= 1e9)
  mis-order. Only reachable via F1 or by handing in a malformed struct
  (precondition violation); F1 covers the module-internal source.
- R11: timespec_to_timeval truncation (time.h:734) and
  timeval_to_timespec scaling (time.h:773): exact for valid inputs
  (0 <= tv_usec < 1e6); negative/huge fields are caller-constructed
  malformed input. Rejected.
- R12: run-check.c:213 expects the odd string "8.1000000002" — that is
  the implementation's %li.%09li format applied to tv_nsec=1000000002
  on the abort path; the test matches the implementation and passes.
  Test artifact, not a production defect.
- R13: -m32 concerns: none — run.c builds and passes 66/66 under
  clang -m32 ASan+UBSan; all scaling arithmetic is uint64_t/double.

## Auditor-added test files (temporary; keep or remove later)

- ccan/time/test/run-divide-roundup.c — proves F1. Currently fails
  3/3 in a plain build (`not ok 1..3`: r.ts.tv_nsec == 1000000000);
  alarm(10)-bounded. After repairing F1 it must pass.
- ccan/time/test/run-multiply-overflow.c — covers F2. Passes in plain
  builds; aborts under clang UBSan (float-cast-overflow at time.c:77)
  today; after repairing F2 it must run UBSan-clean.
- ccanlint with both files present: 73/78 FAIL, solely because
  run-divide-roundup.c fails against the current code.

No production files were modified (time.h, time.c, _info untouched;
verified by git status — the only new paths under ccan/time are the
two tests above).
