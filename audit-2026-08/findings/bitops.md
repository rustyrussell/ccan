# Audit: ccan/bitops

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: bitops.h (223 lines, static inlines around GCC builtins plus
extern fallback declarations), bitops.c (79 lines, naive loop fallback
implementations compiled only under BITOPS_NEED_*), _info, and
ccan/bitops/test/ only. No dependencies (_info "depends" is empty).
config.h here: all 12 HAVE_BUILTIN_{FFS,CLZ,CTZ,POPCOUNT}{,L,LL} = 1,
so the __builtin inline paths are active on this host; the naive
fallback paths (BITOPS_NEED_*) were exercised via run-selftest.c and an
auditor differential probe.

## Mechanical checks

- ccanlint (before auditor additions): **46/48**, every check PASS.
  Only point deductions: tests_coverage (+5/6; 253 of 257 lines
  covered — the uncovered lines are the never-compiled-here naive
  fallback bodies) and examples_exist (+1/2; bitops.h has no Example:
  section). tests_pass, tests_pass_valgrind, examples_compile,
  module_links, main_header_compiles, hash_if all PASS.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/bitops/bitops.c directly; linked with ccan/tap/tap.c; per-test
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0),
  LP64: **2/2 pass cleanly, zero sanitizer diagnostics** —
  run (632 ok), run-selftest (40968 ok; compares builtin vs naive
  implementations over all one- and two-bit patterns and complements,
  32- and 64-bit). Total 41600 ok.
- -m32: builds and passes (run.c, 632 ok, ASan+UBSan). This also
  exercises the `sizeof(u) == sizeof(long)` branch selection
  (bitops.h:34, :67, :100, :213) on ILP32, where the 64-bit functions
  take the `ll` builtin branch. (The module includes only stdint.h and
  stdlib.h, so the missing-32-bit-asm-headers caveat does not apply.)
- Auditor differential probe /tmp/bitops-asan/probe-diff.c (temporary,
  not committed): builtin vs naive implementations (same double-include
  trick as run-selftest.c) over all edge values (0, all-ones, single
  bits, single clears, low-mask runs and complements, 32- and 64-bit)
  plus 2M random uint64_t values (each also tested inverted and
  truncated to 32 bits), covering ffs/clz/ctz/weight/ls/hs/lc/hc —
  built with clang 18 ASan+UBSan no-recover. Result: **ALL MATCH**,
  zero sanitizer diagnostics.

## Findings

None confirmed. No correctness, security, or portability defect met the
evidence bar.

### Trivial documentation note (not a defect)

- bitops.h:29, bitops_ffs64 comment: "Returns 1 for least significant
  bit, **32** for most significant bit, 0 for no bits set" — should say
  64. Comment typo only; the function itself is correct (verified by
  run.c:16 and the differential probe). ccanlint's examples_exist
  deduction (no Example: in bitops.h) is the related doc gap.

## Rejected candidates (disproved)

- R1: Shift-by-64 / oversized shifts. All shifts in bitops.c use
  `(uint32_t)1 << i` with `i < 32` (bitops.c:12, :32, :66) and
  `(uint64_t)1 << i` with `i < 64` (bitops.c:21, :41, :75); loop bounds
  guarantee the shift counts. In run.c the shifts are bounded by the
  loop limits (i < 32 / i < 64) and the base is unsigned. No
  shift-by-width or larger anywhere. Rejected.
- R2: Signed shifts. Every shift base in module and tests is unsigned
  ((uint32_t)1, (uint64_t)1, 1U, 1ULL, 0xFFFF...ULL). No signed
  left-shift-of-negative or shift-into-sign-bit UB. Rejected.
- R3: `__builtin_ffs(u)` with u > INT_MAX (bitops.h:23): the
  uint32_t→int conversion is implementation-defined (C17 6.3.1.3), not
  UB; GCC/Clang define it as modular wraparound and the builtin counts
  bits in the representation, so results are correct for all inputs on
  every target that provides these builtins (the fallback exists for
  the others). Verified correct for 0x80000000 and all-ones by run.c
  and the probe. Rejected as a portability defect.
- R4: bitops_clz32/clz64/ctz32/ctz64(0) — __builtin_clz/ctz(0) is UB
  in the builtin path when neither CCAN_DEBUG nor CCAN_BITOPS_DEBUG is
  defined. Every one of these functions is documented "(must not be 0)"
  (bitops.h:47, :59, :80, :92) and the DEBUG assert
  (BITOPS_ASSERT_NONZERO, bitops.h:7-12) catches it in debug builds.
  Documented precondition; rejected. Same for ls/hs (documented "must
  not be zero", bitops.h:112, :123, :134, :145) and lc/hc on all-ones
  (documented "must not be 0xFF...", bitops.h:156, :166, :176, :186);
  in debug builds lc/hc are still caught because the assert lives
  inside the ctz/clz they call.
- R5: Naive fallback clz32/clz64 call abort() on 0 (bitops.c:34, :43)
  instead of the builtin path's UB/assert — a behavior difference
  between paths, but only for inputs that violate the documented
  precondition. Rejected.
- R6: Naive fallback ctz32/ctz64(0) without DEBUG returns ffs(0)-1 = -1
  (bitops.c:51, :57) — again only reachable by violating the documented
  nonzero precondition. Rejected.
- R7: `sizeof(u) == sizeof(long)` branch selection is a runtime-constant
  `if`, not preprocessor: both branches must compile, so the guard
  requires HAVE_*_L && HAVE_*_LL together (bitops.h:14, :45, :78,
  :195) — it does. On LP64 the l-variant matches uint64_t; on LLP64/
  ILP32 the ll-variant is selected (sizeof(long)==4). Verified on
  LP64 and -m32 ILP32; both correct. A hypothetical target with
  uint64_t == unsigned long long but sizeof(long)==8 would still be
  correct (same size/representation). Rejected.
- R8: Naive fallback correctness: bitops.c loops return first-set+1 /
  leading-zero count / popcount exactly as documented; the ctz fallback
  delegates to ffs-1 which is exact. Proven exhaustively over edge
  values and 2M random values by the probe (ALL MATCH) and by
  run-selftest over all ≤2-bit patterns. Rejected.
- R9: Macro pitfalls (double evaluation, missing parens, name
  capture): all API entry points are static inline functions or extern
  functions — arguments evaluated exactly once; BITOPS_ASSERT_NONZERO(u)
  appears only inside assert((u) != 0). No macros wrap user arguments.
  Rejected.
- R10: run.c:21/:46 pass `0xFFFFFFFFFFFFFFFFULL << i` to the 32-bit
  functions — deliberate truncation ("Higher bits don't affect
  result"), shift count i < 32 on an unsigned 64-bit base, defined.
  Test artifact, not a defect.
- R11: run-selftest.c never passes 0 to the naive clz/ctz (which
  abort()): all one/two-bit patterns and their complements are nonzero,
  and the explicit zero checks (run-selftest.c:82-85) cover only ffs
  and weight. Consistent with the documented preconditions. Test
  artifact, not a defect.
- R12: Fallback-mode header consistency: when a HAVE_BUILTIN_* triple
  is 0, bitops.h declares the extern prototypes and defines
  BITOPS_NEED_*, and bitops.c compiles exactly the matching naive
  bodies; ls/hs/lc/hc stay static inline and bind to whichever ffs/
  clz/ctz is selected. Partial-availability mixes (e.g. CLZ without
  CTZ) are handled by independent #if blocks. No mismatch path found.
  Rejected.

## Auditor-added test files

None. No defect was confirmed, so no regression test was added to
ccan/bitops/test/. The only auditor artifact is the temporary
differential probe /tmp/bitops-asan/probe-diff.c (described above),
which is not committed.

No production files were modified (bitops.h, bitops.c, _info untouched;
verified by git status — no new or changed paths under ccan/bitops).
