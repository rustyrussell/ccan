# Audit: ccan/typesafe_cb

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: typesafe_cb.h (134 lines, header-only), _info and
ccan/typesafe_cb/test/ only. No dependencies. This module is a
compile-time checking facility, so "defect" here mostly means "the
check can be silently bypassed or the tests don't test what they
claim"; runtime defects are limited to the test suite itself.

Note on macro inventory: the prompt anticipated typesafe_cb_def,
typesafe_cb_exact, cast_if_type and cast_if_any. Those were removed in
b0fa019a ("typesafe_cb: simplify, preserve namespace"). The current
header defines exactly five macros: typesafe_cb_cast (typesafe_cb.h:32),
typesafe_cb_cast3 (:61), typesafe_cb (:86), typesafe_cb_preargs (:108),
typesafe_cb_postargs (:130). Stale references remain: the test file
name compile_fail-cast_if_type-promotable.c (it actually tests
typesafe_cb_cast) and the comment "should be OK for normal and _def"
in compile_ok-typesafe_cb-NULL.c:3.

## Mechanical checks

- ccanlint (before auditor additions): 53/53. Every check passes;
  tests_compile PASS (+8/8), and -vvv output confirms each
  compile_fail-* test is built both plain and with -DFAIL
  (rejection required). Compiler used: config.h `cc` with
  `-g3 -ggdb -Wall -Wstrict-prototypes -Wold-style-definition -Wundef
  -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith
  -Wwrite-strings -Wshadow=local`.
- After adding the auditor test
  (test/compile_fail-typesafe_cb-int-fixed.c): ccanlint 55/55, all
  checks still pass (the new test is picked up automatically and
  rejected with -DFAIL as required).
- Manual compile matrix (gcc 13.3 and clang 18.1.3, `-c -Wall -Werror
  -I.`): all 4 compile_ok tests compile, all 7 compile_fail tests are
  rejected with -DFAIL and compile without it, under BOTH compilers.
  No gcc/clang divergence like the io run-30 case. Without -Werror,
  gcc accepts several compile_fail cases with warnings only; clang 18
  rejects most of them by default (-Wincompatible-function-pointer-types
  and -Wint-conversion are errors in clang 16+). See "documentation-
  acknowledged sharp edges" below — warning-level protection is the
  documented design.
- test/run.c (9 subtests) under clang 18 ASan+UBSan
  (`-fno-sanitize=function`, `-fno-sanitize-recover=all`, -g, -I.),
  LP64: passes cleanly, rc=0, zero sanitizer diagnostics.
- Same under gcc 13 ASan+UBSan: passes cleanly, rc=0.
- Same under clang WITH `-fsanitize=function` (diagnostic data for the
  UB section below): fires on the module's own test, as designed-in:
  ```
  ccan/typesafe_cb/test/run.c:19:2: runtime error: call to function
  (unknown) through pointer to incorrect function type 'void (*)(void *)'
  SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior ... run.c:19:2
  Aborted (rc=134)
  ```
  run.c:19 is `fn(arg)` inside `_callback_onearg` — i.e. the example
  usage calls a `void (char *)` function through a `void (*)(void *)`
  lvalue. This is the module's core purpose, exercised exactly as
  documented; it is UB per C11 6.3.2.3/8 (see UB section).
- Reproducers/probes live in /tmp/typesafe_cb-asan/ (temporary, not
  committed): probe-eval.c, probe-const.c, probe-qual.c, probe-intfix.c,
  probe-noproto.c / probe-one.c, probe-zero.c.

## Findings

## F1 — CONFIRMED (FIXED in 46acc9b6) (test defect): compile_fail-typesafe_cb-int.c was never updated for the 4-arg typesafe_cb API — it "passes" via a preprocessor arity error and exercises none of the type checking it was written for

- Location: ccan/typesafe_cb/test/compile_fail-typesafe_cb-int.c:12:
  ```
  #define callback(fn, arg)					\
  	_callback(typesafe_cb(void, (fn), (arg)), (arg))
  ```
  typesafe_cb has taken 4 arguments (rtype, atype, fn, arg) since
  b0fa019a ("typesafe_cb: simplify, preserve namespace"); the pre-
  b0fa019a API was 3-arg (verified: `git show b0fa019a^:.../typesafe_cb.h`
  line 89, `#define typesafe_cb(rtype, fn, arg)`). Every other test was
  converted; this one was left with the 3-arg call.
- Observed behavior (both compilers, `-c -Wall -Werror -DFAIL`):
  - gcc: `compile_fail-typesafe_cb-int.c:25:34: error: macro
    "typesafe_cb" requires 4 arguments, but only 3 given`
  - clang: `compile_fail-typesafe_cb-int.c:25:2: error: too few
    arguments provided to function-like macro invocation`
  ccanlint's compile_fail logic only requires *some* compilation
  failure, so tests_compile reports PASS and nothing notices that the
  intended scenario — a callback whose `arg` is an int, which per the
  test's own comment "fails due to arg, not due to cast" — is never
  compiled at all (the FAIL body dies in the preprocessor).
- Consequence: the suite silently lost coverage of the "non-pointer
  arg to a pointer-expecting callback registration" case. If a change
  to typesafe_cb broke that path (e.g. made the cast unconditional),
  no test would fail.
- Reproducer / repair validation: auditor-added
  ccan/typesafe_cb/test/compile_fail-typesafe_cb-int-fixed.c is the
  same test modernized to the 4-arg API
  (`typesafe_cb(void, void *, (fn), (arg))`). With -DFAIL it is
  rejected for the *intended* reason on both compilers:
  - gcc: `error: passing argument 2 of '_callback' makes pointer from
    integer without a cast [-Werror=int-conversion]`
  - clang: `error: incompatible integer to pointer conversion passing
    'int' to parameter of type 'void *' [-Wint-conversion]`
  Without FAIL it compiles cleanly on both. ccanlint with the new file
  present: 55/55, all pass.
- Repair direction: replace the body of compile_fail-typesafe_cb-int.c
  with the auditor-added file (or delete the stale file and rename the
  new one). No production change needed; the header is not at fault.

## F2 — LIKELY (documented in 2b988863): a callback declared without a prototype (`void cb()`) bypasses all checking silently — no diagnostic from gcc -Wall or clang -Wall, then is called through the wrong signature

- Location: the design, not a line — typesafe_cb.h:32-36 correctly
  *declines* to cast (typeof of an unprototyped `void (*)()` is not
  compatible with the oktype), leaving the expression alone per the
  documented fallback ("otherwise left alone", typesafe_cb.h:16). The
  safety net is then "the compiler will diagnose the conversion at the
  registration call" — but assigning `void (*)()` to a
  `void (*)(void *)` parameter is accepted with ZERO diagnostic by both
  gcc 13.3 -Wall and clang 18 -Wall (neither -Wincompatible-pointer-types
  nor -Wincompatible-function-pointer-types fires; only
  -Wstrict-prototypes, not in -Wall, hints at it).
- Reachable path / reproducer (/tmp/typesafe_cb-asan/probe-one.c):
  ```
  static void _register(void (*cb)(void *), void *a) { (void)a; cb(a); }
  #define register_cb(cb, arg) \
  	_register(typesafe_cb(void, void *, (cb), (arg)), (arg))
  static void cb_noproto() { }        /* no prototype */
  register_cb(cb_noproto, str);       /* compiles silently, both compilers */
  ```
  Control in the same file (`static void cb_int(int x)` registered the
  same way) IS diagnosed by both compilers, proving the check works
  for prototyped callbacks. So an unprototyped callback slips past
  both layers (the macro and the implicit-conversion diagnostic).
- Consequence: a completely unchecked registration — the exact
  scenario the module exists to prevent ("We really want
  register_callback() to accept only the exactly correct function type
  to match the argument, or a function which takes a void *", _info).
  The subsequent call through `void (*)(void *)` is UB (6.3.2.3/8)
  with no compile-time sign.
- LIKELY rather than CONFIRMED because the caller must use an
  obsolescent (C23-removed) non-prototype declaration, which is
  arguably outside the module's documented contract, and no in-tree
  user does this.
- Repair direction (for the rewrite): nothing in
  __builtin_types_compatible_p or _Generic can see "unspecified
  parameters", so this hole is inherited by any typeof-based redesign;
  worth an explicit documentation note ("callbacks must have
  prototypes") rather than code.

## UB characterization (for the planned non-UB rewrite)

Per-macro, where exactly the undefined behavior lives. Terms:
"UB-always" = undefined the moment the construct is used;
"UB-at-call" = defined conversions, UB only when the produced pointer
is used to call the function; "non-portable" = not ISO C but not UB.

### typesafe_cb_cast(desttype, oktype, expr) — typesafe_cb.h:32-36

Expansion (when HAVE_TYPEOF && HAVE_BUILTIN_CHOOSE_EXPR &&
HAVE_BUILTIN_TYPES_COMPATIBLE_P, all 1 in config.h, probed by
tools/configurator):
```
__builtin_choose_expr(
	__builtin_types_compatible_p(__typeof__(0?(expr):(expr)), oktype),
	(desttype)(expr), (expr))
```

- `__typeof__`, `__builtin_choose_expr`,
  `__builtin_types_compatible_p`: GNU extensions — non-portable, not
  UB. Config-gated; without them the macro degrades to
  `((desttype)(expr))` with no checking (documented, typesafe_cb.h:20-24;
  every compile_fail test carries the matching `#error "Unfortunately
  we don't fail if typesafe_cb_cast is a noop."`). Supported
  compilers: gcc and clang only, in practice.
- `__typeof__` operand is unevaluated; `__builtin_choose_expr`
  evaluates only the selected arm. Verified single evaluation of expr
  in both the match and no-match paths (probe-eval.c: side-effecting
  expr, `evals=2` for two invocations). No double evaluation.
  Exception: typeof of a variably-modified type evaluates array-size
  expressions — unreachable here (the conditional decays arrays to
  pointers before typeof can see a VLA).
- The `0?(expr):(expr)` trick: the conditional operator with identical
  operands forces lvalue-to-rvalue, array-to-pointer and
  function-to-pointer conversions and drops top-level scalar
  qualifiers, so types_compatible_p compares the type the value will
  actually have at the call/assignment. Verified (probe-qual.c, clean
  under gcc/clang -Wall -Wextra): `const unsigned long` matches oktype
  `unsigned long`; `char[8]` matches oktype `char *`. Consequences for
  a rewrite: exactness applies AFTER decay/qualifier-drop — pointed-to
  qualifiers remain significant (`const char *` vs `char *` do not
  match; probe-const.c shows the mismatch is caught).
- `(desttype)(expr)` with function-pointer desttype: converting a
  function pointer to a different function pointer type is DEFINED
  (C11 6.3.2.3/8: "A pointer to a function of one type may be
  converted to a pointer to a function of another type and back
  again"). The cast itself is never UB. The same sentence continues:
  "if a converted pointer is used to call a function whose type is not
  compatible with the referenced type, the behavior is undefined" —
  that is where the UB lives (see typesafe_cb below).
- `(desttype)(expr)` with object desttype (the documented
  `set_some_value` use, desttype `void *`): object-pointer <-> void *
  conversions are fully defined (6.3.2.3/1). No UB anywhere in this
  usage.
- Not usable in ISO constant expressions, but __builtin_choose_expr is
  accepted in static initializers by gcc/clang — the documented "can
  be used in static initializers" (typesafe_cb.h:18) is verified by
  run.c:68-90 (three file-scope initializers through all three
  callback macros). Non-portable but works on both compilers.
- Sharp edge observed: gcc emits diagnostics from the UNCHOSEN arm of
  __builtin_choose_expr (in compile_fail-typesafe_cb_cast.c gcc's
  error is `typesafe_cb.h:36:17: cast to pointer from integer of
  different size` — the cast arm, which is dead because the condition
  is false). Both arms must also parse. A rewrite must expect
  diagnostic leakage from dead arms.

### typesafe_cb(rtype, atype, fn, arg) — typesafe_cb.h:86-89

Just typesafe_cb_cast with desttype `rtype (*)(atype)` and oktype
`rtype (*)(__typeof__(arg))`. Evaluation: fn is evaluated at most once
(only the chosen arm); arg is only typeof'd, never evaluated by the
macro (the surrounding registration macro evaluates it separately).

UB profile — the heart of the matter:

- If fn's decayed type exactly equals `rtype (*)(typeof(arg))`, it is
  cast to `rtype (*)(atype)` and stored. In every real use atype is
  `void *` (or another common type) while fn takes the concrete
  argument pointer type: the function types are NOT compatible
  (6.2.7, 6.7.6.3/15 — `char *` and `void *` are distinct,
  incompatible types). The registration function then calls the
  callback through its declared parameter type — **UB-at-call per
  C11 6.3.2.3/8, every time, by design**. This is not a corner case;
  it is the module's documented happy path, confirmed by
  -fsanitize=function firing on run.c:19 (transcript above).
- Mitigating facts (why it works everywhere CCAN runs): C11 6.2.5/28
  guarantees void * and char * have the same representation and
  alignment; for other object pointers (struct foo *) even
  representation equality is only universal-in-practice, not
  guaranteed. The argument value itself travels as void * (defined),
  and on all targeted ABIs (LP64/LLP64, SysV, Windows) the call is
  bit-compatible. So: formally UB, universally benign, flagged by
  UBSan's function sanitizer and by any future strict implementation.
- If fn does NOT match, it is left alone and safety depends entirely
  on the compiler diagnosing the conversion at the registration call.
  Silence cases observed: (a) gcc without -Werror/-Wall — warning
  only, compiles; (b) unprototyped callbacks — zero diagnostic
  (F2); (c) void * <-> function pointer (GNU extension, silent without
  -Wpedantic) — deliberately relied upon by
  compile_ok-typesafe_cb-NULL.c (NULL as callback).
- Return-type and extra-argument mismatches are covered: return type
  is part of function-type compatibility, so a wrong rtype fails to
  match and produces the fallback diagnostic.

### typesafe_cb_preargs / typesafe_cb_postargs — typesafe_cb.h:108-111, 130-133

Same construction with __VA_ARGS__ spliced before/after atype.
Identical UB profile (UB-at-call through the incompatible common
type). Additional sharp edges:

- Only the atype position is checked against typeof(arg); the
  pre/post argument types are checked only indirectly, via the
  fallback conversion diagnostic against the registration function's
  parameter type. Correct but worth restating in a rewrite.
- __VA_ARGS__ must be non-empty: with zero extra types the expansion
  contains `rtype (*)(, atype)` — a hard syntax error, not a graceful
  failure. Acceptable (these macros exist precisely for ≥1 extra arg)
  but a rewrite using _Generic would face the same variadic-empty
  problem.

### typesafe_cb_cast3 — typesafe_cb.h:61-65

Nested typesafe_cb_cast; innermost match wins, at most one cast is
applied (all to the same desttype, so idempotent). Same UB profile as
typesafe_cb_cast: defined for object-pointer desttypes, UB-at-call
only if desttype were an incompatible function pointer type (not an
in-tree use).

### What is NOT UB

- __builtin_types_compatible_p performs no runtime comparison —
  "comparisons of function pointers with different types" never occur;
  the check is a pure compile-time predicate. No UB.
- The 0?(expr):(expr) trick: defined behavior, canonicalizes types;
  not UB (semantics detailed above).
- Static-initializer use: GNU extension acceptance, not UB.

### Documentation-acknowledged sharp edges

- typesafe_cb.h:20-24: "This is merely useful for warnings" — the
  protection is diagnostic-level. With plain `gcc -c` (no -Werror)
  several compile_fail cases compile with warnings (observed in the
  mechanical matrix); clang 16+ makes most of them default errors.
- _info: "On compilers which don't support the extensions
  typesafe_cb_cast() and friend become an unconditional cast, so your
  code will compile but you won't get type checking."
- typesafe_cb.h:79: "It is assumed that @arg is of pointer type."

### Implications for a non-UB rewrite

1. The checking machinery (typeof + types_compatible_p + choose_expr)
   is NOT the UB. It can be replaced portably with C11 _Generic:
   `#define tsc(desttype, oktype, expr) _Generic((expr), oktype:
   (desttype)(expr), default: (expr))`. The controlling expression of
   _Generic undergoes the same lvalue/array/function conversions
   (C11 6.5.1.1p3) that the 0?():() trick forces, so the
   decay/qualifier semantics verified in probe-qual.c carry over.
   Caveats: config.h currently has NO _Generic probe (grep:
   configurator has no HAVE_GENERIC) — one would be needed, or a
   C11 minimum; all association arms must still parse/type-check,
   same as choose_expr's dead-arm diagnostics.
2. The UB is exclusively at the CALL SITE, inside the user's
   registration function that stores the callback as
   `void (*)(void *)` and later calls it. No change to the header's
   *expression* can remove that; the UB is in the pattern. Strictly
   conforming alternatives:
   - Trampoline: the macro (or a companion DEF macro) emits a real
     wrapper `static void tramp(void *p) { real_cb((struct foo *)p); }`
     — the object-pointer conversion is defined (6.3.2.3/1) and the
     call goes through the correct type. Costs: needs a unique symbol
     (no nested functions in ISO C; __COUNTER__/__LINE__ or explicit
     name argument), and file-scope emission breaks the current
     "usable in static initializers / pure expression" property —
     registration would need a separate declaration-site macro.
   - Type-preserving storage: _Generic-based registration over a fixed
     set of callback types stored in a tagged union, called through
     the matching member. Costs: closed set of types; union member
     punning of function pointers is implementation-defined territory
     (fine on gcc/clang, which is all CCAN targets).
   - Keep the cast but require exact signatures (drop the void *
     common-type pattern): registration takes the true type; generic
     dispatch via _Generic selects the registration function. This is
     the cleanest: no function-pointer conversion at all.
3. Compiler baseline: the codebase targets gcc/clang with GNU
   extensions; config.h gates HAVE_TYPEOF, HAVE_BUILTIN_CHOOSE_EXPR,
   HAVE_BUILTIN_TYPES_COMPATIBLE_P (all 1 here). A _Generic rewrite
   keeps full functionality on gcc ≥ 4.9 / clang ≥ 3.x in C11 mode and
   can fall back to the unconditional cast elsewhere — same graceful
   degradation story as today. C23 would standardize typeof itself.
4. _Static_assert can deliver hard errors instead of warnings, but it
   is a declaration, not an expression: it cannot sit inside an
   argument list, so adopting it sacrifices either the expression form
   or the static-initializer use. The current "leave alone and let the
   conversion diagnose" degradation is the better default; an
   additional opt-in STRICT variant (statement-expression +
   _Static_assert, GNU-only) could give -Werror-free hard errors.
5. Any rewrite must preserve these verified semantics: single
   evaluation of fn/arg (probe-eval), top-level qualifier drop and
   array/function decay in the comparison (probe-qual), pointed-to
   const significance (probe-const), static-initializer usability
   (run.c:68-90), and NULL-as-callback acceptance
   (compile_ok-typesafe_cb-NULL.c). F2 (unprototyped callbacks) is
   undetectable by type predicates and should be documented.

## Rejected candidates (disproved)

- R1: Double evaluation of fn/expr through the two textual arms of
  __builtin_choose_expr. Disproved: only the chosen arm is evaluated;
  probe-eval.c prints evals=2 for two invocations (one match, one
  no-match). typeof's operand is unevaluated; arg is never evaluated
  by the macro at all.
- R2: const-stripping silently accepts qualifier-unsafe callbacks.
  Disproved: pointed-to qualifiers survive the 0?():() canonicalization;
  a `void cb(char *)` callback with a `const char *` arg is not cast
  and is diagnosed by both compilers (probe-const.c). Only TOP-LEVEL
  scalar qualifiers are dropped (const unsigned long matches unsigned
  long — probe-qual.c), which is correct by-value semantics.
- R3: Signedness/promotion slips (bool vs long, int vs unsigned long).
  Disproved by the module's own compile_fail-cast_if_type-promotable.c
  (bool vs long rejected, both compilers) and compile_fail-
  typesafe_cb_cast.c (int vs unsigned long rejected). types_compatible_p
  is exact.
- R4: Literal `0` as arg silently matches an int-taking callback:
  `callback(my_callback /* void(int) */, 0)` compiles with ZERO
  diagnostics on gcc -Wall and clang -Wall and "works" (prints
  got 0) — typeof(0) is int, so the cast to void (*)(void *)
  succeeds and 0 converts cleanly to void *. Formally UB-at-call,
  silent, ABI-benign (probe-zero.c); clang -fsanitize=undefined does
  fire at runtime: `probe-zero.c:3:59: runtime error: call to function
  my_callback through pointer to incorrect function type 'void
  (*)(void *)'`. Rejected as a documented-
  precondition violation: typesafe_cb.h:79 "It is assumed that @arg is
  of pointer type"; a literal 0 has type int. Worth a doc line in the
  rewrite (NULL the macro is fine; literal 0 is not).
- R5: Function-vs-object pointer confusion in typesafe_cb_cast:
  passing a function pointer where oktype is an object type (or vice
  versa) never matches, and the fallback conversion void * <->
  function pointer is diagnosed with -Wpedantic and rejected under the
  ccanlint flag set where applicable. The deliberate exception is NULL
  (void *) as a callback, accepted silently as a GNU extension and
  blessed by compile_ok-typesafe_cb-NULL.c. Not a defect.
- R6: Wrong pre/post arg types or wrong rtype are unchecked. Disproved:
  return type and all parameter types are part of function-type
  compatibility, so any mismatch fails the types_compatible_p test and
  falls to the conversion diagnostic; compile_fail-typesafe_cb_preargs.c
  and _postargs.c cover exactly this and are rejected by both compilers.
- R7: Empty __VA_ARGS__ to preargs/postargs produces a syntax error
  rather than a clean diagnostic. Real, but the macros are documented
  "for callbacks that take other arguments" — calling them with none
  is a usage error, and the failure is loud (syntax error), not
  silent. Rejected.
- R8: __typeof__ evaluating VLA sizes (side effects in variably-
  modified types). Unreachable: the conditional operator decays array
  types to pointers, so typeof never sees a variably-modified type
  from the documented pointer-callback usage. Speculative. Rejected.
- R9: Static-initializer use being non-constant. Disproved: run.c:68-90
  initializes three file-scope structs through all three macros; both
  compilers accept. GNU extension semantics, works as documented.
- R10: gcc diagnosing the dead arm of __builtin_choose_expr
  (compile_fail-typesafe_cb_cast.c errors at the cast arm, not the
  call). Both arms must parse and can warn; the test still fails to
  compile, which is all compile_fail requires. Cosmetic. Rejected
  (noted in the UB section as a rewrite consideration).

## Auditor-added test files (temporary; keep or remove later)

- ccan/typesafe_cb/test/compile_fail-typesafe_cb-int-fixed.c —
  proves F1 and validates its repair: the modernized 4-arg version of
  the stale test, rejected with -DFAIL for the intended reason (int
  arg to void * parameter) under both gcc and clang, compiling
  cleanly without FAIL. ccanlint including this file: 55/55, all
  checks pass. Intended to REPLACE the stale
  compile_fail-typesafe_cb-int.c after review (that edit touches an
  existing test file, so it is left to the orchestrator).

No production files were modified (typesafe_cb.h and _info untouched;
verified by git status).
