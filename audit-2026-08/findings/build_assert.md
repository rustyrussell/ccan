# Audit: ccan/build_assert

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: build_assert.h (40 lines, header-only, no dependencies), _info
and ccan/build_assert/test/ only.

Macro inventory: BUILD_ASSERT (build_assert.h:22-23, statement form,
`do { (void) sizeof(char [1 - 2*!(cond)]); } while(0)`) and
BUILD_ASSERT_OR_ZERO (:37-38, expression form,
`(sizeof(char [1 - 2*!(cond)]) - 1)`). No generated identifiers, no
_Static_assert, no config.h feature gates — the negative-array-size
trick is the only implementation on all compilers.

## Mechanical checks

- ccanlint (before auditor additions): **36/36**, every check passes.
- Manual compile_fail matrix (gcc 13.3.0 and clang 18.1.3,
  `-c -Wall -Werror -I.`): compile_fail.c, compile_fail-expr.c and
  compile_ok.c all compile without FAIL; with -DFAIL both compile_fail
  tests are rejected under BOTH compilers for exactly the intended
  reason — the assertion firing: gcc "error: size of unnamed array is
  negative" (build_assert.h:23 / :38), clang "error: array size is
  negative". No gcc/clang divergence; no syntax-quirk failures.
- test/run-BUILD_ASSERT_OR_ZERO.c (1 subtest) under clang 18
  ASan+UBSan (`-fsanitize=address,undefined -fno-sanitize=function
  -fno-sanitize-recover=all -g -I.`, `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0):
  **passes, rc=0**.
- After adding the two auditor regression tests
  (test/compile_fail-nonconst.c, test/compile_fail-expr-nonconst.c):
  ccanlint **28/40 FAIL**, sole failing check tests_compile —
  "compile_fail-expr-nonconst.c: Compiled successfully with -DFAIL?".
  This is the F1 defect reproduced in-tree, by design (red until
  repaired). compile_fail-nonconst.c likewise compiles with -DFAIL
  today (verified manually under both compilers with
  -Wall -Wextra -Werror).
- Probes in /tmp/build_assert-asan/ (temporary, not committed):
  probe-nonconst.c, probe-nonconst-expr.c, probe-struct.c,
  probe-struct-fail.c, probe-instruct.c, probe-twice.c,
  probe-sideeffect.c, probe-const-nosideeffect.c, probe-float.c,
  probe-type.c, probe-divzero.c, probe-cpp.cc, probe-repair.c,
  probe-repair-nonconst.c, probe-repair-false.c.

## Findings

## F1 — CONFIRMED (FIXED in 3854bb65 + 5b2773b2): a non-constant (or compiler-un-evaluable) condition does NOT fail compilation — both macros silently no-op, and a false-at-runtime condition is a negative VLA bound (UB) making BUILD_ASSERT_OR_ZERO return garbage instead of 0

- Location: ccan/build_assert/build_assert.h:22-23 (BUILD_ASSERT) and
  :37-38 (BUILD_ASSERT_OR_ZERO). The bound `1 - 2*!(cond)` is only a
  *constant* expression when the compiler can fold `cond`. When it
  cannot, `char [1 - 2*!(cond)]` is a variable length array, which
  C99+ permits in block scope — so compilation succeeds.
- Documented expectation: build_assert.h:9-10 — "Your compile will
  fail if the condition isn't true, **or can't be evaluated by the
  compiler**." (identical sentence at :29-30 for
  BUILD_ASSERT_OR_ZERO). The header explicitly specifies the behavior
  for non-evaluable conditions: compile failure. The implementation
  delivers the opposite.
- Reachable usage: any caller passing a condition the compiler cannot
  constant-fold (a runtime variable, or an un-evaluable constant such
  as 1/0). All 40+ current in-tree callers pass compile-time constants
  (grep-verified: sizeof comparisons, enum/builtin queries,
  `BUILD_ASSERT_OR_ZERO(0)` stubs in cpuid.h), so nothing in-tree is
  broken today — the hole is latent in the documented contract.
- Reproducers (/tmp/build_assert-asan/probe-nonconst.c):
  ```c
  int main(int argc, char **argv)
  {
  	int x = argc;               /* runtime value */
  	BUILD_ASSERT(x == 0);       /* false at runtime */
  	printf("BUILD_ASSERT did not fire, x=%d\n", x);
  	return 0;
  }
  ```
  and probe-nonconst-expr.c (same with
  `BUILD_ASSERT_OR_ZERO(x == 0)`). In-tree reproducers: the
  auditor-added test/compile_fail-nonconst.c and
  test/compile_fail-expr-nonconst.c, which must NOT compile with
  -DFAIL but do today (taking ccanlint from 36/36 to 28/40,
  tests_compile FAIL).
- Observed output:
  - gcc 13.3.0 AND clang 18.1.3, default flags **and**
    `-Wall -Wextra -Werror`: both probes compile with ZERO diagnostics
    ("COMPILED-CLEAN" x4).
  - Runtime (uninstrumented): "BUILD_ASSERT did not fire, x=1", rc=0 —
    the assertion never fires anywhere. The expr form prints
    `OR_ZERO=18446744073709551614` (gcc) / `OR_ZERO=4294967294`
    (clang) — garbage, not the documented "value is 0".
  - clang 18 `-fsanitize=undefined -fno-sanitize-recover=all`:
    ```
    probe-nonconst.c:10:2: runtime error: variable length array bound
        evaluates to non-positive value -1
    SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior (rc=1)
    ```
    (identical for the expr form at probe-nonconst-expr.c:9:24). A
    false non-constant condition is a negative VLA bound — undefined
    behavior (C11 6.7.6.2/5).
  - Un-evaluable constant sub-case (probe-divzero.c,
    `BUILD_ASSERT(1/0 == 0)`): gcc accepts it with NO diagnostic even
    with -Wall; clang warns (-Wdivision-by-zero) but compiles; the
    assertion never fires.
- Side-effect color (probe-sideeffect.c): on the non-constant path the
  VLA bound is evaluated at runtime — clang evaluates `f()` once, gcc
  zero times — so even evaluation is inconsistent across compilers.
  (Constant conditions are inside sizeof, unevaluated; see R3.)
- Consequence: a caller who believes they have a build-time check has
  nothing — silent acceptance of an unchecked assumption, the exact
  failure mode this module exists to prevent; plus silent UB and a
  garbage "zero" when the runtime value happens to be false.
- Repair direction (validated in probe-repair.c /
  probe-repair-nonconst.c / probe-repair-false.c under both compilers,
  `-Wall -Wextra -Werror`): gate on C11 and use _Static_assert, keeping
  the array trick as the pre-C11 fallback (with the limitation
  documented):
  ```c
  #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  #define BUILD_ASSERT(cond) _Static_assert(cond, "BUILD_ASSERT failed")
  #define BUILD_ASSERT_OR_ZERO(cond) \
  	(sizeof(struct { _Static_assert(cond, "BUILD_ASSERT_OR_ZERO failed"); \
  			 char c; }) - 1)
  #else
  	... existing definitions ...
  #endif
  ```
  Verified: all valid uses still compile clean (function scope, struct
  array bound, file-scope array bound, enum, static initializer,
  multiple uses per line); a constant-false condition still fails with
  a *better* message ("static assertion failed"); a non-constant
  condition is now a hard error on both compilers ("expression in
  static assertion is not constant" / "static assertion expression is
  not an integral constant expression"). One caveat verified in the
  probe: _Static_assert is a declaration, not a statement, so the
  C11 form cannot be the body of a braceless `if` (`if (x)
  BUILD_ASSERT(c);` compiles today, needs braces after the repair) —
  worth a doc note alongside the fix.

## Rejected candidates (disproved)

- R1: Identifier collisions / __LINE__-pasting robustness (two asserts
  on one line, macro stacking). Disproved: the macros generate no
  identifiers at all (unnamed array type inside sizeof). probe-twice.c
  — three BUILD_ASSERTs on one line, nested wrapper macros doubling
  the assert, both arms of an if/else — compiles
  `-Wall -Wextra -Werror` clean on gcc and clang. Rejected.
- R2: Assertion silently not firing inside a struct definition.
  Disproved both ways (probe-struct-fail.c, probe-instruct.c):
  BUILD_ASSERT_OR_ZERO as a struct array bound with a false condition
  errors loudly ("size of unnamed array is negative" gcc / "array size
  is negative" clang); a BUILD_ASSERT statement inside a struct body is
  a loud syntax error on both compilers. No silent path. Rejected.
- R3: Side effects in cond (evaluated?). Disproved for the documented
  case: cond appears exactly once per expansion, inside sizeof —
  unevaluated for constant conditions (probe-const-nosideeffect.c
  clean; a genuinely constant condition cannot contain side effects).
  The inconsistent evaluation on the non-constant path is F1
  territory, noted there. Rejected as a separate finding.
- R4: Floating-point condition mishandled. Disproved: `BUILD_ASSERT(0.5)`
  is true (`!0.5 == 0`) on both compilers, a false float (0.0) fails
  compilation as intended; clang additionally emits a loud, useful
  -Wliteral-conversion for the double->_Bool conversion. gcc's
  -Wunused-value error in probe-float.c was the probe's own misuse
  (BUILD_ASSERT_OR_ZERO as a bare statement). Rejected.
- R5: BUILD_ASSERT_OR_ZERO's result type is size_t, not int.
  probe-type.c: the documented "its value is 0" holds; the unsigned
  type only surprises in off-contract arithmetic
  (`BUILD_ASSERT_OR_ZERO(1) - 1 > 0` evaluates true via unsigned
  wraparound). In the documented usage (pointer offset arithmetic) it
  is harmless; the Linux kernel's BUILD_BUG_ON_ZERO has the same
  size_t shape. No concrete incorrect consequence in documented usage.
  Rejected.
- R6: _Static_assert vs array-trick dispatch broken / fallback not
  working on claimed compilers. Disproved: there is no dispatch — the
  array trick is universal, and it demonstrably fires on constant-false
  conditions under both gcc and clang (the compile_fail matrix). The
  non-constant hole is F1, not a dispatch defect. Rejected.
- R7: File-scope / static-initializer use broken. Disproved:
  probe-struct.c (file-scope array bound, enum constant, static
  initializer, struct array bound, bitfield width — all via
  BUILD_ASSERT_OR_ZERO) compiles `-Wall -Wextra -Werror` clean and
  runs correctly on both compilers; BUILD_ASSERT at file scope is a
  loud syntax error matching its documentation ("This can only be used
  within a function", build_assert.h:11). Rejected.
- R8: Double evaluation of cond. Disproved by reading: cond appears
  exactly once in each macro body. Rejected.
- R9: C++ mode. CCAN is a C project (C++ use undocumented); noted only
  that in C++ mode the F1 hole is partially loud — clang++ diagnoses
  the VLA (-Wvla-cxx-extension) while g++ accepts it as a silent
  extension (probe-cpp.cc). Rejected as a separate finding.
- R10: Use inside `#if`. sizeof is not permitted in preprocessor
  conditionals, so misuse is a loud preprocessing error; the usage is
  undocumented. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/build_assert/test/compile_fail-nonconst.c — regression test for
  F1, statement form: `BUILD_ASSERT(argc == 0)` under -DFAIL must not
  compile (the header documents compile failure for non-evaluable
  conditions). Currently RED: it compiles silently today under gcc and
  clang, even `-Wall -Wextra -Werror`; after the F1 repair the -DFAIL
  build must be rejected.
- ccan/build_assert/test/compile_fail-expr-nonconst.c — same for the
  expression form (`BUILD_ASSERT_OR_ZERO(argc == 0)`); also documents
  that the false-at-runtime path currently yields garbage/UB instead
  of 0. Currently RED (this is the file ccanlint names in its
  tests_compile failure).
- ccanlint with both files present: 28/40 FAIL (tests_compile); both
  files compile cleanly without FAIL under both compilers, so only the
  -DFAIL leg is red. After repair, full score must be restored.

No production files were modified (build_assert.h and _info untouched;
verified by git status — the only new paths under ccan/build_assert
are the two tests above).
