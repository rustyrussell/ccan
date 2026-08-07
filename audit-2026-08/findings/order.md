# Audit: ccan/order

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: order.h (76 lines), order.c (71 lines), _info and
ccan/order/test/ (api.c, run-fancy.c, fancy_cmp.h, compile_ok.c,
compile_fail_1.c, compile_fail_2.c). Dependencies: ccan/typesafe_cb and
ccan/ptrint (treated as given; ptrint.h includes <stddef.h>, so the
`offsetof` used by total_order_by_field resolves transitively through
order.h — no missing-include defect). testdepends: array_size, asort.
Note: the suspected "string/pointer order variants" do not exist in
this version of the module — order.c defines only the 16 scalar
families (s8..s64, u8..u64, int, uint, long, ulong, size, ptrdiff,
float, double), each with forward/reverse and noctx wrappers.

## Mechanical checks

- ccanlint: **38/40**, every check PASS. The missing 2 points are
  examples_exist (+0/2: no Example: sections in _info/order.h).
  tests_pass PASSes — but see F1: run-fancy exits 0 despite 2 failing
  subtests, so ccanlint cannot see them.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; linked with ccan/order/order.c,
  ccan/tap/tap.c, ccan/asort/asort.c; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - api: rc=0, **84/84 ok, zero sanitizer diagnostics** (covers
    INT_MIN/INT_MAX, LONG_MIN/LONG_MAX, ±INFINITY, unsigned wraparound
    literals, and the by-field struct path with a char[5] prefix).
  - run-fancy: rc=0 but **16 ok + 2 "not ok"** (subtests 13 and 16) —
    test bug F1; the failures are masked by `exit(0)`.
  - No ASan/UBSan diagnostics from any binary.
- -m32: builds and runs; 77 ok + 7 "not ok", all in the long/ulong
  cases — fully explained as an ILP32 test-data artifact (R6 below),
  not a production defect.
- Reproducers/probes in /tmp/order-asan/ (temporary, not committed):
  repro-fancy.c, repro-nan.c (plain and -DNDEBUG builds),
  repro-reverse-extremes.c, repro-dbl-eval.c.

## Findings

## F1 — CONFIRMED (FIXED in 9df9809e) (test defect): run-fancy.c has two wrong expected values (audit said lines 59 and 64; verified as lines 59 and 63), and `exit(0)` instead of `exit_status()` hides the failures from ccanlint/CI

- Location: ccan/order/test/run-fancy.c:59 and run-fancy.c:64 (wrong
  expectations), run-fancy.c:67 (`exit(0);` masks the failure count).
- Reachable path: simply running the existing test. The comparisons
  involve only test-local code (test/fancy_cmp.h) driven through the
  total_order_cmp macro, so this is a defect in the test, not in
  order.h/order.c — the production code produces the correct answer.
- Analysis: order2 uses ctx2 = { .xcode = 0x1000, .offset = 1 }.
  item1.value == item2.value == 0, so after the xor both keys tie at
  0x1000 and fancy_cmp falls through to the string tie-break
  (fancy_cmp.h:26-29): `strcmp("aaa"+1, "abb"+1)` =
  `strcmp("aa", "bb")` = -1. Therefore
  `total_order_cmp(order2, &item1, &item2)` is -1, but line 59 expects
  1; symmetrically line 64 expects -1 where the result is 1. All 14
  other expectations in the test are consistent with fancy_cmp
  semantics (item3 maps to key 0 and sorts first under ctx2).
- Observed output (clang 18 ASan+UBSan build, and identically in a
  plain gcc build):
  ```
  not ok 13 - total_order_cmp(order2, &item1, &item2) == 1
  not ok 16 - total_order_cmp(order2, &item2, &item1) == -1
  # Looks like you failed 2 tests of 18.
  ```
  Independent minimal reproducer /tmp/order-asan/repro-fancy.c (copy of
  fancy_cmp, no module code):
  ```
  cmp(item1,item2) = -1
  ```
- Concrete incorrect consequence: the module's only test of the
  total_order()/total_order_cmp() macros with a non-trivial context
  permanently fails 2/18 subtests, and because main() ends with
  `exit(0)` (run-fancy.c:67) instead of `return exit_status();`,
  ccanlint tests_pass and tests_pass_valgrind both report PASS — the
  failure is invisible to CI, and any future real regression in these
  macros would also be invisible.
- Repair direction: in run-fancy.c change line 59's expected value to
  -1 and line 64's to 1, and replace `exit(0);` with
  `return exit_status();`. After that the test passes 18/18 and
  ccanlint will catch any recurrence.
- Regression test: none added — the defect and its fix live entirely in
  an existing test file; the corrected run-fancy.c is its own
  regression test (it fails 2/18 today and must pass 18/18 after the
  fix). No production file is involved, so there is nothing else for a
  new TAP test to pin down.

## Rejected candidates (disproved)

- R1: Signed overflow in subtraction-based comparisons (task suspect).
  Disproved by reading: order.c:14-20 compares with relational
  operators only (`*aa < *bb`, `*aa > *bb`) — there is no subtraction
  anywhere in the module. No overflow is possible for any input pair,
  including INT64_MIN vs INT64_MAX (exercised by api.c under UBSan,
  clean).
- R2: Sign flipping on INT_MIN in the _reverse variants (order.c:33,
  `return -_order_##_oname(a, b, ctx);`). _order_##_oname returns only
  -1, 0 or 1 by construction (the equality case is assert-guarded at
  order.c:19), so the negation can never overflow.
  /tmp/order-asan/repro-reverse-extremes.c calls the s64 and int
  reverse comparators with {INT64_MIN, INT64_MAX} and {INT_MIN,
  INT_MAX} under ASan+UBSan no-recover: correct ±1/0 results, zero
  diagnostics. Rejected.
- R3: NaN input makes order_float/order_double an inconsistent
  comparator (both `<` and `>` false, so the else branch returns 0 for
  NaN vs everything, violating transitivity) and aborts in assert
  builds. Observed: /tmp/order-asan/repro-nan.c aborts at
  `order.c:70: _order_float: Assertion '*aa == *bb' failed` in a normal
  build; with -DNDEBUG it prints
  `cmp(NaN,1.0)=0 cmp(NaN,2.0)=0 cmp(1.0,2.0)=-1`. Rejected as a caller
  precondition violation: a total order over floating point cannot
  include NaN, and the implementation's own assert(*aa == *bb)
  (order.c:19) explicitly treats the not-less/not-greater case as
  reachable only for equality, i.e. NaN is out-of-domain and is caught
  loudly in debug builds. Feeding NaN to qsort via these comparators is
  the caller breaking qsort's comparator contract, not a module defect.
- R4: total_order_cmp double-evaluates `_order` (order.h:34-35:
  `(_order).cb((_a), (_b), (_order).ctx)`). Demonstrated:
  /tmp/order-asan/repro-dbl-eval.c passes `ords[i++]` and i is 2 after
  one call (clang even warns -Wunsequenced). Rejected: `_a` and `_b`
  are each evaluated once; only the order object is duplicated, and
  every documented and in-tree use passes a plain struct declared by
  total_order() — a side-effecting order expression contradicts the
  macro's evident intent (same convention as other CCAN container
  macros). No reachable incorrect consequence for conforming callers.
- R5: total_order_by_field (order.h:69-74) stores `_order_##_oname`
  (type int(const void *, const void *, void *)) through a cast into a
  field of type int(*)(const _itype *, const _itype *, ptrint_t *) and
  later calls through it — formally UB per C17 6.3.2.3/8. This is the
  core typesafe-callback idiom shared with the rest of CCAN
  (typesafe_cb exists precisely for this); the representations are
  ABI-identical on every supported platform, and CCAN convention
  disables UBSan's function sanitizer (-fno-sanitize=function).
  Rejected as inherent design, not a defect.
- R6: -m32 api.c failures (7 subtests: "asort by field long", and all
  six ulong cases, api.c:125 and :128). Pure ILP32 test-data artifact:
  on ILP32 LONG_MIN==INT_MIN and LONG_MAX==INT_MAX, so (a) the ulong
  expected-sorted array becomes
  {0,1,10,2^31-1,2^31,2^31-1,2^31,...} — not sorted, so no correct sort
  can reproduce it; (b) the long data collapses to duplicate extreme
  values, and the struct-by-field memcmp then depends on the order of
  equal keys, which asort (unstable) does not guarantee. The bare-long
  qsort/asort subtests (61-64) pass because equal longs compare equal
  byte-wise. Not a production defect; rejected. Noted only as a
  test-portability limitation (api.c data assumes LP64).
- R7: Misaligned or out-of-object field access via
  `(const _type *)((char *)a + offset)` (order.c:11-12). The offset
  comes from offsetof(_itype, _field) in total_order_by_field
  (order.h:73), hence naturally aligned and in-object; api.c exercises
  this with a `char dummy0[5]` prefix before 8-byte members under ASan,
  clean. Handing an arbitrary ctx pointer/offset to the _order_*
  functions directly is a caller precondition (the ctx mechanism is
  deliberately opaque, ptrint_t "should never be dereferenced").
  Rejected.
- R8: ptrint conversions of the offset (ptr2int/int2ptr, order.c:10,27,
  40,45,50): offsets are bounded by PTRDIFF_MAX per the C standard and
  ptrint_t round-trips exactly such values (dependency behavior,
  treated as given). No reachable truncation. Rejected.

## Auditor-added test files

None. The single confirmed finding (F1) is a bug inside an existing
test file; its fix (correct the two expectations, return
exit_status()) makes run-fancy.c itself the regression test — it
currently fails 2/18 and must pass 18/18 after repair. Adding a
duplicate TAP test would pin down nothing about the production code,
which is correct.

No production files were modified (order.h, order.c, _info, and all
existing tests untouched; verified by git status — no new or changed
paths under ccan/order).
