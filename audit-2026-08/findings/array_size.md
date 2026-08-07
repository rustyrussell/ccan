# Audit: ccan/array_size

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: array_size.h (26 lines, header-only), _info and
ccan/array_size/test/ only. Sole dependency: ccan/build_assert
(audited separately; BUILD_ASSERT_OR_ZERO here expands to
`sizeof(struct { _Static_assert(...); char c; }) - 1` under
HAVE_STATIC_ASSERT=1, negative-array trick otherwise).

Macro inventory: ARRAY_SIZE (array_size.h:15) and the internal
_array_size_chk (array_size.h:20-24). config.h here:
HAVE_BUILTIN_TYPES_COMPATIBLE_P=1, HAVE_TYPEOF=1,
HAVE_STATIC_ASSERT=1, so the type-checked branch is active.

The guard works by exact-type comparison, not pointer-comparison:
for any pointer p, `typeof(p)` is `T *` and `typeof(&p[0])` is also
`T *` — identical, so `__builtin_types_compatible_p` is true and the
negated _Static_assert fires. For any array, `typeof(arr)` is `T[N]`
and `typeof(&arr[0])` is `T *`, which are never compatible, so the
assert passes. Unlike the comparison-based check_types_match
(check_type audit F1, container_of audit F2), there is no `void` hole:
`types_compatible_p(void *, void *)` is true, so `ARRAY_SIZE(void_ptr)`
is a hard error (verified below). This closes the audit's prime
suspect up front.

## Mechanical checks

- ccanlint (before auditor additions): **38/39**; every check passes
  except "array_size.h: No Example: section" — the example lives in
  _info instead; documentation-completeness nit, not a defect.
- test/run.c (8 subtests) under clang 18.1.3 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function
  -fno-sanitize-recover=all -g -I.`, `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0):
  **passes, rc=0**, all 8 subtests ok.
- Manual compile_fail matrix (gcc 13.3.0 and clang 18.1.3,
  `-c -Wall -Werror -I.`): both compile_fail.c and
  compile_fail-function-param.c compile without FAIL and are rejected
  with -DFAIL under BOTH compilers (rejection observed as the
  _Static_assert "BUILD_ASSERT_OR_ZERO" failure on gcc, and on clang
  additionally the default-on -Wsizeof-pointer-div diagnostic). No
  gcc/clang divergence.
- After adding the auditor test (test/compile_fail-void-ptr.c):
  ccanlint **40/41**, all checks pass except the same docs nit; the
  new test compiles plain and is rejected with -DFAIL on both
  compilers.
- Fallback mode probe (simulated config.h with HAVE_TYPEOF=0 and
  HAVE_BUILTIN_TYPES_COMPATIBLE_P=0, /tmp/array_size-asan/cfg-noft):
  _array_size_chk becomes 0 and ARRAY_SIZE(ptr) silently yields 2
  (8/4); gcc emits one -Wsizeof-pointer-div warning, clang two.
  Documented limitation — see R4.
- Probes in /tmp/array_size-asan/ (temporary, not committed):
  probe-vla.c, probe-vla2.c, probe-void.c, probe-fptr.c,
  probe-misc.c, probe-edge.c, probe-extern.c, probe-register.c,
  probe-fallback.c, cfg-noft/config.h.

## Findings

None. Every candidate was disproved; see below. The guard's
types_compatible_p design is exact-type equality and is airtight for
the array/pointer distinction: an array type `T[N]` is never
compatible with a pointer type `T *`, and any subscriptable
non-array's `&x[0]` has exactly the same type as `x`.

## Rejected candidates (disproved)

- R1: `void *` argument bypasses the guard (the check_type F1 /
  container_of F2 hole). Disproved: probe-void.c is rejected by BOTH
  compilers. gcc 13: `error: division 'sizeof (void *) / sizeof
  (void)' does not compute the number of array elements
  [-Wsizeof-pointer-div]` AND `error: static assertion failed:
  "BUILD_ASSERT_OR_ZERO"`. clang 18: `-Wsizeof-pointer-div` error
  under -Werror, and with -Wno-sizeof-pointer-div the static assert
  still fires: `error: static assertion failed due to requirement
  '!__builtin_types_compatible_p(void *, void *)'`. typeof(void *) ==
  typeof(&vp[0]) == void *, so the assert fires by design.
- R2: Function-pointer argument bypasses the guard. Disproved:
  probe-fptr.c is a loud error on both compilers (`subscripted value
  is pointer to function` — fp[0] itself is invalid for
  void(*)(void)); the guard is not even needed.
- R3: VLA argument breaks compilation (types_compatible_p of a
  variably-modified type may not be an integer constant expression,
  which _Static_assert requires). Disproved: probe-vla.c
  (`int vla[n]; ARRAY_SIZE(vla)`) compiles clean under
  `-Wall -Wextra -Werror` on BOTH compilers and returns 7/42
  correctly at runtime; probe-vla2.c (variably-modified element type,
  `int (*pv)[n]`, ARRAY_SIZE(*pv)) likewise compiles and returns 11.
  Both compilers accept the condition as constant-foldable here.
- R4: Without HAVE_TYPEOF/HAVE_BUILTIN_TYPES_COMPATIBLE_P the check
  is a silent no-op and pointers pass. True (probe-fallback.c:
  ARRAY_SIZE(ptr) yields 2 with at most a -Wsizeof-pointer-div
  warning), but explicitly documented: array_size.h:11-13 — "With
  correct compiler support, such usage will cause a build error";
  the compile_fail tests carry an `#if !HAVE_TYPEOF || ... #error`
  guard acknowledging it, and ccanlint's without-features pass is
  green. Documented limitation; rejected.
- R5: Extern array of unknown bound (`extern int a[]`). Disproved as
  a silent hazard: probe-extern.c is a loud error on both compilers
  (`invalid application of 'sizeof' to incomplete type 'int[]'`),
  and "arrays declared as []" are called out in array_size.h:11.
  Rejected (loud + documented).
- R6: Side effects / double evaluation of the argument. Disproved:
  all occurrences of arr are inside sizeof or typeof (unevaluated for
  non-variably-modified types). probe-edge.c with a side-effecting
  getter behind `int (*pa)[4]` shows evals=1 — the single explicit
  call, ARRAY_SIZE(*pa) adds none.
- R7: Zero-length arrays (GNU extension), multidimensional arrays,
  parenthesized array names, const arrays, string literals, struct
  member arrays, and ICE contexts. All behave correctly
  (probe-misc.c/probe-edge.c, both compilers): zero-length → 0;
  `int[3][5][7]` → 3 (outer dimension, matching run.c's array3);
  ARRAY_SIZE((arr)) works (typeof preserves the array type through
  parentheses); const and member arrays fine; `ARRAY_SIZE("literal!")`
  → 9 (it IS an array; counting the NUL is sizeof semantics);
  enumerator (`enum { N = ARRAY_SIZE(a) }`), array bound
  (`char b[ARRAY_SIZE(a)]`) and case-label use all compile and give
  6 — the sizeof-anonymous-struct form of BUILD_ASSERT_OR_ZERO is a
  valid integer constant expression at file and block scope. Rejected.
- R8: register arrays. `ARRAY_SIZE(register_array)` fails to compile
  on both compilers (`error: address of register variable
  requested` — &arr[0] is invalid), i.e. a false positive on a
  genuine array. Rejected: register-qualified arrays are effectively
  nonexistent in real code (the keyword is a no-op hint compilers
  ignore), the failure is loud with a trivial workaround (drop
  register), and no documented promise covers it.
- R9: `-std=c99 -Wpedantic -Werror` noise: _Static_assert is a C11
  feature, so a C99-strict build of run.c errors in
  build_assert.h:56. This is a config.h-vs-language-standard mismatch
  owned by build_assert/configurator (HAVE_STATIC_ASSERT=1 while
  compiling pre-C11), not an array_size logic defect; ccanlint's
  default-std pass is clean. Rejected (out of module scope).
- R10: Include-guard end comment at array_size.h:26 reads
  `/* CCAN_ALIGNOF_H */` (copy-paste from ccan/alignof). Cosmetic
  comment typo; AGENTS.md excludes style. Noted, rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/array_size/test/compile_fail-void-ptr.c — with -DFAIL,
  `ARRAY_SIZE(vp)` for a `void *` must not compile. It is correctly
  rejected today by gcc and clang (GREEN), pinning the R1 disproof
  in-tree: the void shape is exactly where the comparison-based
  checks in check_type/container_of silently failed, so this guards
  array_size's types_compatible_p guard against regressing to a
  comparison-based form. ccanlint with the file present: 40/41, all
  checks pass except the pre-existing "No Example: section" docs nit.
  Without FAIL it compiles clean on both compilers (verified with
  `-Wall -Werror`).

No production files were modified (array_size.h and _info untouched;
verified by git status — the only new path under ccan/array_size is
the test above).
