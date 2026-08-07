# Audit: ccan/intmap

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: intmap.h (502 lines), intmap.c (339 lines), _info and
ccan/intmap/test/ only. Deps (_info "depends"): ccan/bitops,
ccan/short_types, ccan/str, ccan/tcon, ccan/typesafe_cb — documented
behavior taken as given (bitops_ls64/hs64 require nonzero args;
tcon_check's expr is inside sizeof and never evaluated).

## Mechanical checks

- ccanlint (before auditor additions): **62/68 PASS**, every check
  passes; the only deduction is _info having "No Example: section".
- After adding the auditor test: ccanlint **54/59 FAIL** — exactly as
  designed, because test/run-signed-firstlast-empty.c fails 2/6
  subtests against the current code (F1).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/intmap/intmap.c directly; linked with ccan/tap/tap.c only;
  per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **6/6 pass cleanly, zero sanitizer diagnostics** — run (40 ok),
  run-after-exhaustive (2048 ok), run-after-fail (2 ok), run-order
  (6002 ok), run-order-smallsize (602 ok), run-signed-int (38 ok).
  Total 8732 ok.
- -m32: unavailable, skipped — intmap.c includes <errno.h>, which
  pulls in /usr/include/linux/errno.h → missing 32-bit
  'asm/errno.h' on this host (anticipated; same as other
  errno.h-including modules).
- Reproducers/probes in /tmp/intmap-asan/ (temporary, not committed):
  repro-uninit-firstlast.c (valgrind), probe-written-value.c,
  probe-exhaustive.c, probe-signed-exhaustive.c, probe-fuzz.c,
  probe-mallocfail.c.

## Findings

## F1 — CONFIRMED (FIXED in 1a1bb440): sintmap_first()/sintmap_last() on an empty map write an uninitialized value to *indexp

- Location: ccan/intmap/intmap.h:465-473 (sintmap_first_) and
  intmap.h:493-501 (sintmap_last_):
  ```c
  static inline void *sintmap_first_(const struct intmap *map,
                                     sintmap_index_t *indexp)
  {
      intmap_index_t i;
      void *ret = intmap_first_(map, &i);
      *indexp = SINTMAP_UNOFF(i);   /* i uninitialized when ret == NULL */
      return ret;
  }
  ```
  When the map is empty, intmap_first_/intmap_last_
  (ccan/intmap/intmap.c:159-166, :291-298) return NULL with
  errno = ENOENT *without* touching *indexp, leaving the local `i`
  indeterminate; SINTMAP_UNOFF(i) is then computed from it and stored
  into the caller's *indexp.
- Reachable path: `sintmap_first(&map, &s)` or `sintmap_last(&map, &s)`
  on any empty SINTMAP — a normal, documented call.
- Caller preconditions: none violated. The documented contract
  (intmap.h:276-280, :353-357) is "Returns NULL if the map is empty,
  otherwise populates *@indexp" — i.e. *indexp is populated only on
  success. The unsigned uintmap_first/uintmap_last indeed leave
  *indexp untouched on failure; the signed wrappers do not.
- Concrete consequences:
  1. Read of an indeterminate value (UB; trips MemorySanitizer and
     valgrind). Observed (valgrind --quiet on
     /tmp/intmap-asan/repro-uninit-firstlast.c):
     ```
     ==1588499== Conditional jump or move depends on uninitialised value(s)
     ==1588499==    at 0x109D7C: main (repro-uninit-firstlast.c:19)
     ==1588499== Conditional jump or move depends on uninitialised value(s)
     ==1588499==    at 0x109DB4: main (repro-uninit-firstlast.c:23)
     ```
  2. The caller's index variable is clobbered with garbage even though
     the call failed. Observed deterministically
     (/tmp/intmap-asan/probe-written-value.c, stack groomed with
     0xDEADBEEFCAFEBABE): 16/16 calls at both -O0 and -O2 leave
     *indexp != sentinel, e.g.
     ```
     first: *indexp clobbered to 6822318947648322238 (0x5eadbeefcafebabe)
     last:  *indexp clobbered to 6822318947648322238 (0x5eadbeefcafebabe)
     ```
     A caller looping `for (v = sintmap_first(m, &i); v; v =
     sintmap_after(m, &i))` is safe (loop body never runs), but any
     caller that inspects the index after a failed call — as the
     module's own test run-signed-int.c:18-29 does around after()
     calls — reads garbage.
  3. Note the sibling wrappers sintmap_after_/sintmap_before_
     (intmap.h:475-491) are NOT affected: they initialize
     `i = SINTMAP_OFF(*indexp)`, so on failure they write back the
     caller's original value (SINTMAP_UNOFF∘SINTMAP_OFF is the
     identity mod 2^64).
- Regression test: ccan/intmap/test/run-signed-firstlast-empty.c
  (alarm(10)-bounded, 6 subtests). Currently fails exactly 2/6
  (`not ok 3 - s == SENTINEL`, `not ok 6 - s == SENTINEL`) in plain
  clang, plain gcc, and clang ASan+UBSan builds; zero sanitizer
  diagnostics. Verified the repair makes it pass 6/6 (patched header
  copy in /tmp/intmap-fix: `if (ret) *indexp = SINTMAP_UNOFF(i);`).
- Repair direction: only populate *indexp on success in both inlines:
  ```c
  void *ret = intmap_first_(map, &i);
  if (ret)
      *indexp = SINTMAP_UNOFF(i);
  return ret;
  ```
  (same for sintmap_last_).

## Rejected candidates (disproved)

- R1: intmap_after_/intmap_before_ prefix-comparison logic
  (intmap.c:190-227, :247-284; the `idx |= 1` "leave critbit in
  place" trick and the idx-vs-prefix subtree comparisons). Disproved
  exhaustively: /tmp/intmap-asan/probe-exhaustive.c checks after() and
  before() against a brute-force model for **all 65536 subsets** of a
  16-value pool (dense clusters + wide gaps) × **all 256 query
  indices** in a uint8_t index space — all pass under ASan+UBSan.
  /tmp/intmap-asan/probe-signed-exhaustive.c repeats this over the
  full int8_t signed space (queries -128..127) — all pass. Together
  with run-after-exhaustive.c (2048 ok) this covers existing and
  non-existing predecessor/successor queries at every critbit level
  representable in 8 bits.
- R2: Insert/delete edge cases (split_node at internal node vs leaf,
  root deletion, parent pointer tracking in intmap_del_). Disproved by
  /tmp/intmap-asan/probe-fuzz.c: 200000 random add/del ops (including
  duplicate adds and deletes of absent keys) over uint8_t space,
  verifying get/first/last/forward/backward iteration against an array
  model every 97 ops — all pass under ASan+UBSan. run-order.c (6002
  ok) and run-order-smallsize.c (602 ok) cover the 64-bit and
  exhaustive-8-bit cases.
- R3: Allocation-failure path in split_node (intmap.c:66-71).
  /tmp/intmap-asan/probe-mallocfail.c (#define malloc interposer):
  failing the split malloc returns false with errno == ENOMEM, leaves
  the map fully intact (existing entries found, no phantom entry,
  first() correct), and a subsequent add with malloc restored succeeds
  — no partial state, no leak under ASan. Not a defect. (Aside:
  test/run-after-fail.c's name suggests malloc-failure coverage, but
  it actually tests uintmap_after after a failed... it simply tests
  after() with a non-existent predecessor; the ENOMEM path was
  previously untested.)
- R4: Signed wrapper ordering at INT64_MIN/INT64_MAX (SINTMAP_OFF adds
  2^63; INT64_MIN → 0, INT64_MAX → UINT64_MAX). Verified by
  run-signed-int.c (38 ok, covers add/get/del/after at both bounds and
  at INT64_MAX-1) and by the R1 signed exhaustive probe.
  sintmap_after at INT64_MAX: SINTMAP_OFF gives UINT64_MAX, +1 wraps
  to 0 → correctly reports no successor (intmap.c:182-184).
- R5: critbit at bit 63 (prefix_mask = `-2ULL << 63` == 0, shift of an
  unsigned — defined; mask 0 is correct since no bits exist above bit
  63, and direction is fully determined by bit 63). Exercised by
  run-signed-int.c (INT64_MIN vs INT64_MAX differ only in bit 63)
  under UBSan — clean. bitops_hs64/ls64 are never called on zero: add
  checks the mask mismatch / index equality before splitting, and
  critbit() reads prefix_and_critbit which always has its marker bit
  set (intmap.c:29-33).
- R6: Macro double evaluation. tcon_check's expr sits inside sizeof
  (never evaluated, ccan/tcon/tcon.h:138-139); indices/values are
  passed as function arguments to intmap_*_ (evaluated once);
  first/after/before/last signed wrappers are static inlines precisely
  "due to multi-evaluation" (intmap.h:464). No defect.
- R7: sintmap_iterate casts the user callback
  `bool (*)(sintmap_index_t, ...)` to `bool (*)(intmap_index_t, ...)`
  via typesafe_cb_cast (intmap.h:438-446) — a call through an
  incompatible function pointer type, UB in theory. Both are 64-bit
  integer types with identical calling convention on all supported
  ABIs; the index value passed (intmap.c:335, `n->u.i - offset`) is
  the exact modular inverse of SINTMAP_OFF. This is the module's
  documented design pattern (and why -fno-sanitize=function is needed
  for CCAN generally). Not an in-practice defect; rejected.
- R8: intmap_iterate_/clear() recursion (intmap.c:309-316, :325-338):
  depth is bounded by the number of critbit levels ≤ 8*sizeof(index)
  = 64 frames — no stack-exhaustion risk regardless of map contents.
- R9: errno on success: get/add do not clear errno on success while
  del/first/last/after/before set errno = 0. Docs only promise
  ENOENT/EEXIST/ENOMEM on the respective failure returns; no contract
  violation. Rejected.
- R10: _info docstring says "ordered map of strings to values" /
  "map of strings as a critbit tree" (and intmap.c:12 "These point to
  strings or nodes") — copy-paste residue from ccan/strmap.
  Documentation nit, not a correctness/security/portability defect;
  noted for the fix-up pass.
- R11: intmap_get_/intmap_del_ skip the prefix comparison during
  descent (FIXME comments at intmap.c:42, :132). Not a defect: the
  final full-index comparison at the leaf (intmap.c:46, :139) is
  sufficient for correctness in a critbit tree; the FIXME is a
  performance idea. Covered by R1/R2 probes.
- R12: uintmap_add with a NULL value aborts via assert (intmap.c:86)
  — NULL values are the documented exclusion ("the (non-NULL) value",
  intmap.h:160; SINTMAP docs intmap.h:50-51). Documented precondition;
  rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/intmap/test/run-signed-firstlast-empty.c — proves F1.
  Currently fails 2/6 in plain clang, plain gcc, and clang ASan+UBSan
  builds (the two `s == SENTINEL` checks); alarm(10)-bounded. After
  repairing F1 it must pass 6/6 (verified against a patched header
  copy in /tmp/intmap-fix).
- ccanlint with this file present: 54/59 FAIL, solely because the new
  test fails against the current code.

No production files were modified (intmap.h, intmap.c, _info
untouched; verified by git status — the only new path under
ccan/intmap is the test above).
