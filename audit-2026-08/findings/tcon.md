# Audit: ccan/tcon

Date: 2026-08-05.  Auditor: kimi-code (per AGENTS.md procedure).
Scope: tcon.h (the entire module — header-only, 368 lines), _info and
ccan/tcon/test/ only.  No runtime dependencies; test dependency
ccan/build_assert already audited (clean).

Mechanical checks:
- ccanlint: 74/74 (full score; includes tests_compile_fail for the 11
  compile_fail-* tests, examples_compile/examples_run for the _info
  example, and the run-* tests).
- Existing run-* tests under clang 18 ASan+UBSan
  (`-fno-sanitize=function`, -g, -I.; each test includes only headers;
  linked with ccan/tap/tap.c; per-test `timeout 60`,
  `ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0`), LP64:
  run-container 12/12 ok, run-wrap 2/2 ok — zero sanitizer diagnostics.
  Total 14 ok.
- -m32 (module includes only <stddef.h>, so 32-bit headers are
  available on this host): run-container 12/12 ok, run-wrap 2/2 ok
  under ASan+UBSan.  Zero diagnostics.
- compile_ok-* and compile_fail-* tests are ccanlint-only (all pass /
  fail as designed per the 74/74 score).
- Reproducers live in /tmp/tcon-asan/repro-*.c (temporary, not
  committed).

Auditor-added test files: none.  The single retained finding is LIKELY
(a silent compile-time check bypass), which by construction cannot be
caught by a runtime TAP test; evidence is the /tmp reproducers below.
No production files modified.

## Findings

## F1 — LIKELY (documented in 8fee253f): a `void *` expression silently bypasses every tcon type check — tcon_check, tcon_check_ptr, and therefore tcon_container_of/tcon_member_of accept any wrong type with zero diagnostics, even -Wall -Wextra -Werror

- Location: ccan/tcon/tcon.h:135 (`tcon_check`: the
  `sizeof((x)->_tcon[0].canary == (expr))` comparison) and tcon.h:150
  (`tcon_check_ptr`: the `sizeof(0 ? (expr) : &(x)->_tcon[0].canary)`
  conditional).  Both are the module's only enforcement points;
  tcon_container_of (tcon.h:328-332) and tcon_member_of
  (tcon.h:357-361) route through tcon_*_check_ptr.
- Mechanism: comparing (or forming a conditional between) a `void *`
  and any object pointer is constraint-conforming C (C17 6.5.9/2,
  6.5.15), so when @expr/@member_ptr has type `void *` (or
  `const void *`) the "type canary" check compiles silently regardless
  of the canary's real type.  The safety feature the module exists for
  ("check the type canary before calling the generic routines", _info;
  "the type of @member_ptr will be verified", tcon.h:322-323) is
  silently absent on exactly the input type generic interfaces produce.
- Reachable path / reproducer (/tmp/tcon-asan/repro-void-bypass.c):
  ```c
  struct outer { int outer_val; struct inner inner; } ovar = { 42, { 7 } };
  /* TCON(TCON_CONTAINER(fi, struct outer, inner)) on info */
  void *wrong = &ovar.outer_val;   /* WRONG member, type erased */
  struct outer *o = tcon_container_of(&info, fi, wrong);
  ```
  Observed: compiles with ZERO diagnostics under
  `clang -Wall -Wextra -Werror` (exit 0); at runtime returns
  `&ovar - 4` (offsetof(inner)=4 subtracted from &outer_val) and
  `o->outer_val` reads garbage (32767) — a wild container pointer with
  no sign anything is wrong.  A second reproducer
  (/tmp/tcon-asan/repro-void-check.c) shows the same silent pass for
  tcon_check and tcon_check_ptr directly (`void *v` against an
  `int *canary`, -Wall -Wextra -Werror clean).
- Control: the same misuse with a *typed* wrong pointer IS caught —
  /tmp/tcon-asan/repro-typed-caught.c (`char *` against `int *canary`)
  warns `comparison of distinct pointer types` at tcon.h:135, and
  ccanlint's compile_fail-container1 proves the typed tcon_container_of
  case.  Only the void*-erased case escapes.
- Preconditions: gray zone — one can argue a `void *` forfeits the type
  information the check needs (same reasoning as container_of F2 and
  typesafe_cb F2 in this audit series).  But nothing in tcon's
  documentation warns about it, the failure is silent, and void*-typed
  member pointers are precisely what generic callback/container APIs
  hand to these macros.  Retained as LIKELY, mirroring the container_of
  audit's classification of the identical hole.
- Consequence: wrong type/member pairings that traveled through a
  `void *` compile silently and compute bad pointers (OOB container
  pointer, type-confused reads/writes) at runtime.
- Repair direction: documentation note on tcon_check, tcon_check_ptr,
  tcon_container_of and tcon_member_of: "the type check is void if the
  checked expression is void *; cast to the real type first".  A code
  fix is not possible with the comparison/conditional-based check
  without also rejecting the documented legitimate use of a `void *`
  *canary* as a wildcard (tcon.h:18, 64).

## Rejected candidates (actively disproved)

- R1: NULL member/container pointer arithmetic UB (the container_of
  audit's F1 analog: `container_of_or_null(NULL,...)` added 0 to NULL
  and clang UBSan diagnosed it).  Disproved: tcon_container_of_
  (tcon.h:334-337) and tcon_member_of_ (tcon.h:362-365) both guard
  explicitly (`member_ptr ? (char *)member_ptr - offset : NULL`), and
  run-container exercises all four NULL paths (lines 53-56) cleanly
  under clang 18 UBSan, LP64 and -m32.  Rejected.
- R2: Double evaluation of macro arguments.  Disproved by expansion:
  in tcon_check/tcon_check_ptr the canary/expr appear only inside
  sizeof (unevaluated) and `x` appears once per conditional arm (only
  one arm evaluated); in tcon_container_of/tcon_member_of, `x` appears
  only inside sizeof/typeof except one evaluated arm, and
  member_ptr/container_ptr is evaluated exactly once in
  tcon_container_of_/tcon_member_of_.  Rejected.
- R3: Canary name collisions between TCON_VALUE's `_value_##canary`,
  TCON_CONTAINER's `_container_/_member_/_offset_##canary` and user
  declarations.  Requires the user to declare canaries with the
  module's reserved prefixes; the naming scheme is visible in the
  header docs and collisions are hard compile errors (duplicate
  member), not silent misbehavior.  Speculative; rejected.
- R4: TCON_VALUE(canary, 0) creates a zero-length array (GNU
  extension) and tcon_value yields 0.  Documented contract requires "a
  positive integer" (tcon.h:173-178); 0 violates it, and negative or
  oversized values are compile errors.  Rejected (documented
  precondition).
- R5: tcon_container_of discards const (const member_ptr yields
  non-const container).  Explicitly documented at tcon.h:323-325.
  Rejected.
- R6: char * arithmetic in tcon_container_of_/tcon_member_of_
  (formally the container_of idiom, `(char *)p ± offset`).  Same
  reasoning as the container_of audit's rejections: the pointer passes
  through char *, which may alias anything, and the converted-back
  pointer is the original object; no sanitizer diagnosis on the
  module's own tests at offset 0 and non-zero offsets.  Rejected.
- R7: `void *` canary compared against a function pointer
  (compile_ok-void.c passes `main` through tcon_check) — a GNU
  extension under strict ISO C.  The module documents "void * will
  allow tcon_check() to pass on any (pointer) type" and the test suite
  blesses the usage on supported compilers; ccanlint (which compiles
  with warnings-as-errors for the module's own flags) passes.
  Rejected.
- R8: TCON as non-last member / anonymous union in TCON_WRAP being
  pre-C11.  Both are documented/visible design points ("It should be
  the last element", tcon.h:13; FAM misuse is a compile error, not
  silent); CCAN targets GCC/Clang extensions throughout.  Rejected.
- R9: _info example correctness.  Verified by reading: with args
  "foo" (argc=2) it prints "Last arg is foo of 1 arguments" — matches
  the expected-output comments; container_add evaluates (p) once
  (sizeof arm unevaluated).  ccanlint examples_compile/examples_run
  pass.  Rejected.
- R10: tcon_member_of on a const container pointer discards const
  through tcon_member_of_'s `void *` parameter.  Unlike F1 this is NOT
  silent: passing `const T *` where `void *` is expected is a
  constraint violation that every supported compiler diagnoses
  (-Wdiscarded-qualifiers, on by default).  Rejected.
