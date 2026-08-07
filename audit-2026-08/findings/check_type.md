# Audit: ccan/check_type

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: check_type.h (64 lines, header-only), _info and
ccan/check_type/test/ only. Dependency: ccan/build_assert
(!HAVE_TYPEOF path only; audited separately — BUILD_ASSERT_OR_ZERO now
uses _Static_assert under HAVE_STATIC_ASSERT, negative-array trick
otherwise). config.h here: HAVE_TYPEOF=1, HAVE_STATIC_ASSERT=1,
HAVE_BUILTIN_TYPES_COMPATIBLE_P=1, so the typeof branch is active.

Macro inventory: check_type (check_type.h:49-50 typeof path, :57-58
sizeof fallback) and check_types_match (:52-53 typeof, :60-61
fallback). Both documented to "issue a warning or build failure" on
mismatch, to evaluate to 0, and to not evaluate their expression
arguments.

## Mechanical checks

- ccanlint (before auditor additions): **40/41**; every check passes
  except "_info: No Example: section" (documentation completeness, not
  a correctness defect).
- test/run.c (9 subtests) under clang 18.1.3 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function
  -fno-sanitize-recover=all -g -I.`, `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0):
  **passes, rc=0**. Includes the no-side-effect subtests (x++/y++
  inside both macros).
- Manual compile matrix (gcc 13.3.0 and clang 18.1.3, `-I.`):
  - plain mismatch (int vs char, int vs unsigned char) is a **warning
    by default** on both compilers ("comparison of distinct pointer
    types"), an error under -Werror/-pedantic-errors, and **fully
    silent under -w** (probe-mismatch.c). Matches the documented
    "warning or build failure" contract.
  - !HAVE_TYPEOF fallback (simulated via a patched config.h,
    /tmp/check_type-asan/cfg-sa1 and cfg-sa0): different-size mismatch
    is a **hard compile error** in BOTH static-assert modes (gcc:
    "static assertion failed"/"size of unnamed array is negative";
    clang: equivalents), same-size different-type silently passes
    (documented false negative), exact match passes (probe-fallback.c,
    8/8 configurations as expected). The build_assert rework did not
    break this path.
- After adding the two auditor regression tests
  (test/compile_fail-check_type_void.c,
  test/compile_fail-check_types_match_void.c): ccanlint **26/41
  FAIL**, sole failing check tests_compile —
  "compile_fail-check_type_void.c: Compiled successfully with -DFAIL?"
  (the loop in tools/ccanlint/tests/tests_compile.c:170-189 returns at
  the first red file; the second test is equally red, verified
  manually). This is the F1 defect reproduced in-tree, by design.
- Probes in /tmp/check_type-asan/ (temporary, not committed):
  probe-mismatch.c, probe-const.c, probe-void.c, probe-void2.c,
  probe-fallback.c, probe-vla.c, probe-vla2.c, probe-funcptr.c,
  probe-funcptr2.c, probe-repair.c, cfg-sa1/config.h, cfg-sa0/config.h.

## Findings

## F1 — CONFIRMED (FIXED in 28f7c3fe): an expression of type `void` (or a `void` type argument) silently bypasses the check entirely — zero diagnostics at any warning level, on both gcc and clang

- Location: ccan/check_type/check_type.h:50
  (`((typeof(expr) *)0 != (type *)0)`) and :53 (same shape for
  check_types_match). When typeof(expr) is `void` — e.g. expr is
  `*vp` for a `void *vp` — the comparison becomes
  `(void *)0 != (int *)0`. Comparing a pointer-to-void with a pointer
  to an object type is constraint-*conforming* (C17 6.5.9/2: "one
  operand is a pointer to an object type and the other is a pointer to
  a qualified or unqualified version of void"), so neither compiler
  emits anything, even at `-Wall -Wextra -Werror`. The symmetric case
  `check_type(x, void)` (type argument is void) is equally silent.
- Documented expectation: check_type.h:7 — "issue a warning or build
  failure if type is not correct"; :27 — "issue a warning or build
  failure if types are not same". No exception for void is documented.
  void vs int is unambiguously "not correct"/"not same", yet nothing
  is issued. This is the only type shape with ZERO diagnostics: every
  other mismatch at least warns by default (and ccanlint's
  tests_compile even counts a -DFAIL warning as a pass, per
  tools/ccanlint/tests/tests_compile.c:169 "For historical reasons,
  'fail' often means 'gives warnings'" — the void shape produces not
  even a warning).
- Reachable path: this is not an exotic direct call. container_of
  expands `check_types_match(*(member_ptr), ((containing_type
  *)0)->member)` (container_of.h:37,:73), so any
  `container_of(void_ptr, type, member)` lands exactly here — the
  container_of audit (F2 there) demonstrated a wrong member pairing
  through a `void *` compiling with zero diagnostics under
  `gcc/clang -Wall -Wextra -Werror` and returning a pointer 4 bytes
  into the wrong object at runtime. check_type is the root cause;
  the hole lives in this module's comparison-based design.
- Reproducer: /tmp/check_type-asan/probe-void2.c —
  `check_type(*vp, int)`, `check_types_match(*vp, argc)`,
  `check_type(argc, void)` — compiles with **zero** diagnostics, rc=0,
  under `gcc -c -Wall -Wextra -Werror -pedantic-errors` and
  `clang -c -Wall -Wextra -Werror` (clang -pedantic-errors complains
  only about typeof being a language extension, unrelated). In-tree:
  the auditor-added test/compile_fail-check_type_void.c and
  test/compile_fail-check_types_match_void.c compile cleanly with
  -DFAIL under both compilers (verified with
  `-Wall -Wextra -Werror`, rc=0 in all four combinations) — they are
  RED regression tests.
- Disproof attempts that failed (i.e. the finding stands):
  - "void * as the expression also slips through" — NO:
    `check_types_match(vp, &x)` with `void *vp` compares
    `void **` vs `int **`, a distinct-pointer-types violation that IS
    diagnosed (probe-void.c). Only typeof(expr) == void itself slips.
  - "the fallback has the same hole" — NO: without typeof,
    `check_type(*vp, int)` is `sizeof(void) == sizeof(int)` → 1 != 4 →
    BUILD_ASSERT_OR_ZERO fires (hard error, verified in both
    HAVE_STATIC_ASSERT modes). The fallback accidentally catches the
    void case; the "better" typeof path does not.
  - "-pedantic-errors catches it" — NO (see above; only the typeof
    extension itself is diagnosed, not the mismatch).
- Consequence: the module's sole purpose is silent for exactly the
  input type that generic interfaces (void * callbacks, container
  APIs) produce; downstream (container_of) silently computes wrong
  pointers.
- Repair direction (validated in /tmp/check_type-asan/probe-repair.c):
  gate on HAVE_BUILTIN_TYPES_COMPATIBLE_P (=1 in this config.h) and
  assert instead of compare:
  ```c
  #define check_type(expr, type) \
	  BUILD_ASSERT_OR_ZERO(__builtin_types_compatible_p(typeof(expr), type))
  ```
  Verified on gcc AND clang, even under `-w`: hard error for
  void-vs-int, int-vs-char and int-vs-unsigned-int; clean pass for
  exact match, top-level-const match (types_compatible_p ignores
  top-level qualifiers, same as today's effective semantics), and
  void-vs-void. This upgrades warning → error, which the contract
  ("warning or build failure") permits; keeping the comparison form
  for !HAVE_BUILTIN_TYPES_COMPATIBLE_P compilers preserves the old
  behavior there. A documentation note ("no check is performed when
  either side is void") is the minimal alternative.

## Rejected candidates (disproved)

- R1: The comparison `((typeof(expr)*)0 != (type*)0)` is a runtime
  expression, so maybe it is not reliably diagnosed at compile time.
  Disproved: comparing pointers to incompatible object types is a
  constraint violation (C17 6.5.9/2), and both gcc and clang diagnose
  every non-void mismatch by default (probe-mismatch.c), error under
  -Werror/-pedantic-errors. Matches the documented "warning or build
  failure". Rejected.
- R2: `-w` suppresses the warning, so the check silently no-ops. True
  (probe-mismatch.c, both compilers), but the contract promises "a
  warning or build failure" — a warning it is, and -w is an explicit
  global request to suppress all warnings. Every warning-based check
  in C behaves this way. Rejected (documented design; F1's repair
  direction would additionally make the typeof path -w-proof).
- R3: const/volatile qualification: check_type(const int, int) and
  check_types_match(const int, int) pass silently
  (probe-const.c, clean under -Wall -Wextra -Werror on both
  compilers). Not a defect: for an expression (not a parenthesized
  type name), typeof yields the unqualified type — top-level
  qualifiers are not part of an expression's type — so this matches
  both C23 typeof semantics and user expectation (checking a const
  variable against `int` must pass). Pointed-to qualifiers ARE
  checked: const int * vs int * is diagnosed (void** vs int** in
  probe-void.c, and container_of R4 previously). Rejected.
- R4: !HAVE_TYPEOF fallback broken by the build_assert rework
  (_Static_assert inside a struct in BUILD_ASSERT_OR_ZERO). Disproved:
  probe-fallback.c under cfg-sa1 (HAVE_STATIC_ASSERT=1) and cfg-sa0
  (=0), gcc and clang: exact/same-size pass clean under
  -Wall -Wextra -Werror, different-size mismatch is a hard error in
  all four configurations. Rejected.
- R5: Side effects in expr. Disproved by the module's own run.c
  subtests 6-9 (x++/y++ unevaluated, verified under ASan+UBSan too)
  and by the typeof/sizeof operands being unevaluated. Rejected.
- R6: Variably-modified (VLA) types: typeof of a VM-typed operand is
  evaluated per C11 6.7.6.2/5, potentially breaking "(not evaluated)"
  or dereferencing. Disproved in practice: probe-vla2.c with NULL
  `int (*)[n]` pointers runs clean on both compilers (the evaluation
  computes the bound, no dereference); the only observable residue is
  that `int[n]` vs `int[m]` (n!=m) compares compatible and is silently
  accepted — an exotic false negative in territory the comparison
  idiom cannot reach, with no plausible real-world caller. Rejected
  (noted for completeness).
- R7: Array or parenthesized-function-pointer types as the `type`
  argument (check_type(arr, int[4]), check_type(fp, void (*)(int)))
  are syntax errors ((int[4] *)0 / (void (*)(int) *)0 do not parse;
  probe-vla.c, probe-funcptr.c). Loud failure with a trivial
  workaround (typedef; probe-funcptr2.c passes -Wall -Wextra -Werror
  on both compilers), and function-pointer-vs-void* mismatches are
  diagnosed normally. Not a silent defect; at most a doc nit.
  Rejected.
- R8: Same-size different-type false negative in the fallback (int vs
  unsigned int passes). Explicitly documented twice: check_type.h:17-18
  "a less complete check", _info "they have to use sizeof() which can
  only distiguish between types of different size"; the existing
  compile_fail-check_type_unsigned.c even #errors out the fallback
  case. Rejected (documented limitation).
- R9: check_types_match(function_pointer, void *) slips through like
  void. Disproved: pointer-to-function vs pointer-to-void is a
  constraint violation, diagnosed by both compilers
  (probe-funcptr.c FAIL_VS_VOIDP). Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/check_type/test/compile_fail-check_type_void.c — with -DFAIL,
  `check_type(*vp, int)` (void expression vs int) must not compile.
  Currently compiles silently under gcc and clang (even
  -Wall -Wextra -Werror): RED regression test for F1. Without FAIL it
  is clean. In the !HAVE_TYPEOF fallback it already fails correctly
  (verified under cfg-sa1/cfg-sa0), so it stays green in ccanlint's
  without-features pass.
- ccan/check_type/test/compile_fail-check_types_match_void.c — same
  for `check_types_match(*vp, argc)`. Equally red.
- ccanlint with both files present: 26/41, sole failing check
  tests_compile (reports the first red file; both verified red
  manually). After an F1 repair both must compile-fail with -DFAIL for
  the intended reason and ccanlint should return to 40/41.

No production files were modified (check_type.h and _info untouched;
verified by git status — the only new paths under ccan/check_type are
the two tests above).
