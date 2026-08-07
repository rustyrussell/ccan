# Audit: ccan/container_of

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: container_of.h (145 lines, header-only), _info and
ccan/container_of/test/ only. Sole dependency: ccan/check_type
(unaudited; documented behavior taken as given, misuse flagged only).

Macro inventory: container_of (container_of.h:33), container_of_or_null
(:69, with inline helper container_of_or_null_ at :65), container_off
(:99, plain offsetof), container_of_var (:119/:122, typeof-gated),
container_off_var (:138/:141, typeof-gated). config.h here:
HAVE_TYPEOF=1, HAVE_BUILTIN_CHOOSE_EXPR=1,
HAVE_BUILTIN_TYPES_COMPATIBLE_P=1, so all typeof branches are active.

Prime suspect cleared up front: the implementation does NOT compute the
offset via `((type *)0)->member` — container_off is plain offsetof
(container_of.h:100). The null-base expression
`((containing_type *)0)->member` appears only as an operand of
check_types_match, i.e. inside typeof (unevaluated) on this
configuration, or inside sizeof (unevaluated) in the !HAVE_TYPEOF
fallback. It is never evaluated; no null dereference, no UB from it.

## Mechanical checks

- ccanlint (before auditor additions): **50/50**, every check passes;
  tests_compile PASS, each compile_fail-* test built both plain and
  with -DFAIL.
- After adding the auditor test (test/run-or-null.c): ccanlint **53/53**,
  all checks pass (the new run test is picked up automatically).
- Manual compile_fail matrix (gcc 13.3.0 and clang 18.1.3,
  `-c -Wall -Werror -I.`): all three compile_fail tests compile without
  FAIL and are rejected with -DFAIL under BOTH compilers, each for its
  intended reason:
  - compile_fail-bad-type.c:17 — `assignment to 'char *' from
    incompatible pointer type 'struct foo *'` (the test's documented
    point: the macro's result type, not the member check).
  - compile_fail-types.c:16 — distinct-pointer-types comparison inside
    check_types_match (int vs char member), both compilers.
  - compile_fail-var-types.c:16 — same via container_of_var's typeof
    branch. No gcc/clang divergence.
- test/run.c (12 subtests) under clang 18 UBSan+ASan
  (`-fno-sanitize=function`, `-fno-sanitize-recover=all`, -g, -I.,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0):
  **FAILS — rc=134**, two UBSan diagnostics at run.c:20 and run.c:21
  (`runtime error: applying zero offset to null pointer`). See F1.
- Same under gcc 13 ASan+UBSan: passes cleanly, rc=0 (gcc UBSan has no
  null-pointer-plus-zero check).
- Reproducers/probes in /tmp/container_of-asan/ (temporary, not
  committed): probe-eval.c, probe-void.c, probe-const.c, probe-array.c,
  probe-bitfield.c, probe-union.c, probe-static-init.c, probe-repair.c,
  probe-nullfix.c.

## Findings

## F1 — CONFIRMED (FIXED in 01fbf784): container_of_or_null(NULL, ...) — the documented happy path — adds 0 to the NULL result; formally UB, diagnosed by clang UBSan on the module's own test

- Location: ccan/container_of/container_of.h:69-73:
  ```c
  #define container_of_or_null(member_ptr, containing_type, member)	\
  	((containing_type *)						\
  	 container_of_or_null_(member_ptr,				\
  			       container_off(containing_type, member))	\
  	 + check_types_match(*(member_ptr), ((containing_type *)0)->member))
  ```
  The inline helper (container_of.h:65-68) correctly guards NULL before
  subtracting the offset — but the `+ check_types_match(...)` (always 0
  for valid pairings) is applied to the helper's result *after* the
  guard, i.e. it computes `NULL + 0` whenever member_ptr is NULL.
- Documented caller expectation: container_of.h:46-48 — "unless it is
  given NULL, in which case it also returns NULL". Passing NULL is the
  macro's advertised second purpose, not a precondition violation.
- Why it is UB: C17 6.5.6/8 — pointer-plus-integer requires both the
  pointer operand and the result to point into (or one past) the same
  array object; a null pointer points to no object, so `NULL + 0` is
  undefined. (C++ exempts +0 on null; ISO C does not, and clang's UBSan
  implements the C rule.)
- Observed output (module's own test, run.c:20-21, clang 18
  -fsanitize=undefined, no-recover):
  ```
  ccan/container_of/test/run.c:20:2: runtime error: applying zero offset to null pointer
  SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior ccan/container_of/test/run.c:20:2
  Aborted (rc=134)
  ```
  With recovery enabled, run.c:21 fires identically (member at non-zero
  offset, `(char *)NULL`); both NULL paths are affected regardless of
  the member's offset. gcc 13 UBSan does not diagnose it (no such
  check); generated code is universally benign (add immediate 0).
- Reproducer: the module's own run.c suffices; the auditor-added
  test/run-or-null.c isolates the same paths (2 UBSan diagnostics
  today, both in the NULL legs).
- Consequence: any project running CCAN code under clang UBSan with
  abort-on-error (exactly what the AGENTS.md audit procedure, and many
  CI setups, do) trips on a fully documented, correct usage of this
  macro. It is a false-positive-generator that trains users to disable
  or ignore the sanitizer.
- Repair direction (validated in /tmp/container_of-asan/probe-repair.c
  and probe-nullfix.c): keep the type check but stop adding it to the
  result pointer, e.g.
  ```c
  #define container_of_or_null(member_ptr, containing_type, member)	\
  	((containing_type *)						\
  	 ((void)check_types_match(*(member_ptr),			\
  				  ((containing_type *)0)->member),	\
  	  container_of_or_null_(member_ptr,				\
  				container_off(containing_type, member))))
  ```
  Verified: correct pairings still compile clean under
  `gcc/clang -Wall -Wextra -Werror`; a wrong pairing (int * vs char
  member) is still diagnosed by both compilers ("comparison of distinct
  pointer types"); the NULL path is UBSan-clean (rc=0). Note the
  `sizeof(check_types_match(...)), ...` comma variant is NOT suitable:
  gcc -Wextra -Werror rejects it (-Wunused-value). The plain
  container_of (container_of.h:33-37) has the same `+ check` shape but
  its result is only null-reachable when the caller already violated
  the documented precondition (member_ptr must point to a member), so
  no change is required there — though applying the same comma form for
  symmetry would be harmless.

## F2 — LIKELY (root cause FIXED in check_type 28f7c3fe): a `void *` member pointer silently bypasses the type check entirely — any wrong member pairing is accepted with zero diagnostics, even -Wall -Wextra -Werror, and yields a silently wrong pointer

- Location: the check at container_of.h:37 (and :73),
  `check_types_match(*(member_ptr), ((containing_type *)0)->member)`.
  When member_ptr is `void *` (or `const void *`), typeof(*member_ptr)
  is void, and the check becomes `(void *)0 != (member_type *)0` —
  comparing a void pointer with an object pointer is *constraint-
  conforming* C (C17 6.5.9/2), so check_types_match neither warns nor
  errors; it quietly evaluates to 0. The safety feature the module
  exists for ("a convenient and fairly type-safe way", _info) is
  silently absent on exactly the input type generic interfaces produce.
- Reachable path / reproducer (/tmp/container_of-asan/probe-void.c):
  ```c
  static struct foo *via_void(void *vp)
  { return container_of(vp, struct foo, a); }   /* wrong member: a, not b */
  ...
  via_void(&foo.b);   /* &foo.b is char *, handed around as void * */
  ```
  Compiles with ZERO diagnostics under both
  `gcc -c -Wall -Wextra -Werror` and `clang -c -Wall -Wextra -Werror`
  (transcript: COMPILED-CLEAN twice); at runtime returns `foo+4` — a
  struct foo * pointing 4 bytes into the object, with no sign anything
  is wrong. The same call with a typed `char *` member pointer IS
  caught by the check (that is compile_fail-types.c's scenario).
- Consequence: the most common real-world shape of this bug — a member
  pointer that traveled through a `void *` callback argument or generic
  container API — is precisely the one where a wrong
  type/member pairing compiles silently and computes a bad pointer.
- LIKELY rather than CONFIRMED: the documented contract says
  "@member_ptr: pointer to the structure member", and one can argue a
  void * forfeits the type information the check needs (a documented-
  precondition gray zone, same reasoning as typesafe_cb F2's
  unprototyped-callback hole). It is also a hole inherited from
  check_types_match's comparison-based design rather than a logic error
  in container_of itself. But nothing in container_of's documentation
  warns about it, and the failure is silent, hence retained as LIKELY.
- Repair direction: documentation note ("the type check is void if
  @member_ptr is void *; cast to the member's real pointer type
  first"). A code fix is not possible with the comparison-based check;
  even a typeof/types_compatible_p-based check cannot reject void *
  without also rejecting the legitimate use of void * for the *first*
  member... in fact it could: `void *` never legitimately matches a
  member type, so a HAVE_BUILTIN_TYPES_COMPATIBLE_P-gated BUILD_ASSERT
  that typeof(*(member_ptr)) is not void would close the hole with no
  false positives.

## Rejected candidates (disproved)

- R1: UB from `((containing_type *)0)->member` (null dereference to
  compute the offset). Disproved by reading: the offset is offsetof
  (container_of.h:100); the null-base expression occurs only inside
  typeof (HAVE_TYPEOF, unevaluated) or sizeof (fallback, unevaluated).
  Never evaluated, no UB.
- R2: Double evaluation of member_ptr. Disproved by probe-eval.c with a
  side-effecting getter: evals=1 for container_of, container_of_or_null
  (both NULL and non-NULL), and container_of_var, under both gcc and
  clang. member_ptr appears twice textually but the second occurrence is
  inside typeof/sizeof (unevaluated); container_of_or_null_ receives it
  as a function argument (evaluated once) and its own two uses are of
  the parameter.
- R3: container_of(NULL, type, member) is UB ((char *)NULL - 0, then
  +0). True, but passing NULL to plain container_of violates the
  documented contract — "pointer to the structure member" — and
  container_of_or_null exists precisely for nullable pointers.
  Documented-precondition violation; rejected (F1 covers the macro
  whose contract *does* promise NULL handling).
- R4: const-qualified member pointer silently accepted / constness
  dropped. Disproved: both compilers diagnose it —
  "comparison of distinct pointer types" (const int * vs int *) for
  container_of, plus -Wdiscarded-qualifiers /
  -Wincompatible-pointer-types-discards-qualifiers on the void *
  argument of container_of_or_null_ for container_of_or_null
  (probe-const.c). Warning-level enforcement is check_type's documented
  design ("issue a warning or build failure").
- R5: Array member used via pointer-to-first-element
  (container_of(&s.arr[0], struct s, arr)). Produces a spurious
  distinct-pointer-types warning (int * vs int (*)[4]) — but the
  computed pointer is correct (probe-array.c: p==&s, both compilers),
  and the documented argument is "pointer to the structure member",
  i.e. &s.arr. No incorrect behavior; rejected (cosmetic noise for an
  off-contract call shape).
- R6: Bitfield member. `&s.bf` is a hard compile error before the macro
  is ever expanded (probe-bitfield.c: "cannot take address of
  bit-field"), and offsetof on a bitfield is likewise a constraint
  violation. Loud failure, not silent; rejected.
- R7: Union members and nested member designators. Work correctly:
  probe-union.c passes -Wall -Wextra -Werror on both compilers and
  container_of(&s.u, struct s, u) recovers the container;
  container_off(struct s, u.i) equals container_off(struct s, u)
  (offset 0 within the union), as expected. Rejected.
- R8: Strict-aliasing/alignment violation on the cast back to
  containing_type. Disproved: the pointer passes through char *, which
  may alias anything; converting char * back to the struct type is
  defined when the address actually designates a struct of that type
  (the documented precondition), and alignment is preserved because the
  address genuinely is the struct's address. No reachable violation.
- R9: container_off arithmetic overflow / size_t vs ptrdiff_t. offsetof
  yields size_t; pointer-minus-size_t converts per the usual rules and
  the result is in bounds for any valid member pointer. No reachable
  incorrect consequence; speculative. Rejected.
- R10: !HAVE_TYPEOF fallback branches (container_of.h:122-125 returns
  void * with no type check; :141-143 container_off_var computes
  `&(var)->member - (char *)(var)`, evaluating var twice and requiring
  a non-NULL var, with ptrdiff_t result vs container_off's size_t).
  All true, but unreachable on every compiler CCAN targets
  (config.h HAVE_TYPEOF=1; gcc/clang only), and container_off_var's
  documentation already requires "a pointer to a container structure".
  Rejected as unreachable-on-supported-compilers; noted for completeness.
- R11: Static-initializer use of container_of. Undocumented, but
  probe-static-init.c shows both gcc and clang accept
  `static struct s *gp = container_of(&gs.y, struct s, y);` at file
  scope (GNU address-constant arithmetic extension). Works; not a
  defect. Rejected.
- R12: compile_fail-bad-type.c exercises the assignment type mismatch
  rather than the member check. Verified it fails with -DFAIL for
  exactly its documented reason ("p is a char *, but this gives a
  struct foo *" → incompatible-pointer-types error) on both compilers,
  and compiles without FAIL. It tests what it claims to test. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/container_of/test/run-or-null.c — 6 TAP subtests covering
  container_of_or_null with NULL and non-NULL member pointers at both
  offset 0 and non-zero offset, plus container_of on the same members.
  Functionally passes today; under clang UBSan it currently reports the
  2 NULL-leg diagnostics of F1, so it serves as the regression test for
  the repair (after fixing F1 it must run UBSan-clean). ccanlint with
  this file present: 53/53, all checks pass.

No production files were modified (container_of.h and _info untouched;
verified by git status — the only new path under ccan/container_of is
the test above).
