# Audit: ccan/compiler

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: compiler.h (317 lines, header-only, no dependencies), _info and
ccan/compiler/test/ only. config.h here has every HAVE_* gate the header
uses set to 1 (HAVE_ATTRIBUTE_COLD/CONST/DEPRECATED/NONNULL/
RETURNS_NONNULL/SENTINEL/PURE/NORETURN/PRINTF/UNUSED/USED,
HAVE_BUILTIN_CONSTANT_P, HAVE_BUILTIN_CPU_SUPPORTS,
HAVE_WARN_UNUSED_RESULT), so all attribute branches are active.

Macro inventory: COLD (:20), NORETURN (:40), PRINTF_FMT (:58),
CONST_FUNCTION (:74), PURE_FUNCTION (:87), UNNEEDED (:112),
NEEDED (:131/:134), UNUSED (:153), IS_COMPILE_CONSTANT (:204/:207),
WARN_UNUSED_RESULT (:226), WARN_DEPRECATED (:242), NO_NULL_ARGS (:257),
NON_NULL_ARGS (:268), RETURNS_NONNULL (:283), LAST_ARG_NULL (:297),
cpu_supports (:312/:314). Note: the task overview mentioned IDEMPOTENT
and SCANF_FMT — neither exists anywhere in the tree; nothing is missing
relative to _info.

## Mechanical checks

- ccanlint (before auditor additions): **50/50**, every check passes.
- compile_fail-printf.c verified manually (gcc 13.3.0, clang 18.1.3,
  `-c -Wall -Werror -I.`): compiles clean without FAIL; with -DFAIL both
  compilers reject it for the intended reason — gcc: "format '%p'
  expects argument of type 'void *', but argument 3 has type
  'unsigned int' [-Wformat=]"; clang: "format specifies type 'void *'
  but the argument has type 'unsigned int'". This also proves the
  PRINTF_FMT(2,3) indexes are interpreted 1-based as documented
  (see R2).
- test/run-is_compile_constant.c (2 subtests) under clang 18
  ASan+UBSan (`-fsanitize=address,undefined -fno-sanitize=function
  -fno-sanitize-recover=all -g -I.`, `timeout 60`): **passes, rc=0**
  (header-only module; the only runtime semantics are in
  IS_COMPILE_CONSTANT, and both branches are covered by the test's
  `#if HAVE_BUILTIN_CONSTANT_P`).
- After adding the auditor regression test (test/run-predefine-const.c):
  ccanlint **40/52 FAIL**, sole failing check tests_compile with
  "error: expected ';' before 'int'" at the PURE_FUNCTION use — this is
  the F1 defect reproduced in-tree, by design (red until repaired).
- Functional attribute probes in /tmp/compiler-asan/ (temporary, not
  committed): probe-all-attrs.c (every macro used, gcc+clang
  `-Wall -Wextra -Werror` clean), probe-nonnull-warn.c, probe-sentinel.c,
  probe-warnunused.c, probe-noreturn-def.c, probe-predef-const.c,
  probe-predef-pure.c, probe-redef.c, probe-redef2.c, probe-cpp.cc,
  probe-docexample.c.

## Findings

## F1 — LIKELY (FIXED in 093cfeb0): PURE_FUNCTION definition is nested inside the `#ifndef CONST_FUNCTION` guard — predefining CONST_FUNCTION leaves PURE_FUNCTION undefined and breaks the build

- Location: ccan/compiler/compiler.h:65-92. Structure:
  ```c
  #ifndef CONST_FUNCTION        /* :65 */
  #if HAVE_ATTRIBUTE_CONST      /* :66 */
  #define CONST_FUNCTION ...    /* :74 */
  #else
  #define CONST_FUNCTION        /* :76 */
  #endif                        /* :77, closes :66 */

  #ifndef PURE_FUNCTION         /* :79  -- still inside :65 */
  ...
  #endif                        /* :91, closes :79 */
  #endif                        /* :92, closes :65 */
  ```
  The entire PURE_FUNCTION block (:79-91) sits between the CONST_FUNCTION
  guard (:65) and its `#endif` (:92), instead of after it. Every other
  guarded macro in the file (COLD, NORETURN, PRINTF_FMT, UNNEEDED,
  NEEDED, UNUSED, IS_COMPILE_CONSTANT, WARN_UNUSED_RESULT) is a
  self-contained `#ifndef X ... #endif` unit.
- Reachable usage / documented expectation: the `#ifndef X` guards exist
  for exactly one purpose — to let a consumer pre-define its own X
  before including compiler.h. A consumer that defines CONST_FUNCTION
  (e.g. to a compiler-specific spelling) and then uses PURE_FUNCTION —
  documented in the same header — gets no PURE_FUNCTION definition at
  all. Nothing anywhere suggests the two macros are meant to be coupled.
- Reproducer (/tmp/compiler-asan/probe-predef-const.c):
  ```c
  #define CONST_FUNCTION
  #include <ccan/compiler/compiler.h>
  static PURE_FUNCTION int f(int x) { return x; }
  int main(void) { return f(0); }
  ```
  Observed output — gcc: "error: expected ';' before 'int'" at the
  PURE_FUNCTION use; clang 18: "error: unknown type name
  'PURE_FUNCTION'". Control (/tmp/compiler-asan/probe-predef-pure.c):
  predefining PURE_FUNCTION alone works fine under
  `gcc -Wall -Werror`, confirming the coupling is one-directional and
  accidental.
- In-tree reproducer: the auditor-added
  ccan/compiler/test/run-predefine-const.c fails tests_compile under
  ccanlint today for exactly this reason.
- Consequence: loud build failure (not silent miscompilation) for the
  guard's intended use case; also silently *loses* the PURE_FUNCTION
  definition for any config where CONST_FUNCTION happens to be
  pre-defined by another header.
- LIKELY rather than CONFIRMED: pre-defining these macros is not
  explicitly documented in _info or the header comments; the intent is
  inferred from the guard structure (which is otherwise purposeless).
  The defect in the preprocessor structure itself is certain; only the
  supported-usage status is inferred.
- Repair direction: move the closing `#endif` of the CONST_FUNCTION
  guard from :92 to :78 (i.e. end the guard right after the
  CONST_FUNCTION `#if/#else/#endif`, making the PURE_FUNCTION block a
  top-level unit like its siblings). Verified equivalent shape compiles
  in the probes; the regression test must then pass ccanlint (back to
  full score).

## F2 — LIKELY (FIXED in 5c6469da): six macros lack the `#ifndef` override guard their ten siblings have — predefining them yields macro-redefinition warnings (breaking -Werror builds)

- Location: compiler.h — WARN_DEPRECATED (:233-245), NO_NULL_ARGS /
  NON_NULL_ARGS (:248-272), RETURNS_NONNULL (:274-286),
  LAST_ARG_NULL (:288-300), cpu_supports (:302-315) are all
  `#if HAVE_* ... #define X ... #else #define X-empty #endif` with no
  enclosing `#ifndef X`. The ten other macros all have the guard.
- Reproducer (/tmp/compiler-asan/probe-redef.c):
  ```c
  #define WARN_DEPRECATED
  #include <ccan/compiler/compiler.h>
  int main(void) { return 0; }
  ```
  Observed output — gcc: `compiler.h:242: warning: "WARN_DEPRECATED"
  redefined`; clang: "warning: 'WARN_DEPRECATED' macro redefined
  [-Wmacro-redefined]". Identical for NO_NULL_ARGS (probe-redef2.c,
  `compiler.h:257: warning: "NO_NULL_ARGS" redefined`). Under -Werror
  (or clang's default-on -Wmacro-redefined in strict builds) this is a
  build failure; either way the consumer's override is silently
  *ignored* (the header's definition wins), which is the opposite of
  what the guard pattern provides elsewhere.
- Consequence: inconsistent override behavior within one header;
  a consumer who relies on the (evidently intended) predefine hook gets
  warnings and no effect for these six macros.
- LIKELY for the same reason as F1 (guard purpose inferred, not
  documented); the behavioral difference itself is directly observed.
- Repair direction: wrap each of the six blocks in `#ifndef X ... #endif`
  (for NON_NULL_ARGS, guard with `#ifndef NON_NULL_ARGS` around both
  branches; note NO_NULL_ARGS and NON_NULL_ARGS share one HAVE_ gate and
  would need separate guards).

## F3 — CONFIRMED (FIXED in 3e034139) (documentation): the IS_COMPILE_CONSTANT doc example does not compile as written — `IS_COMPILE_CONSTANT(greek)` uses the enum type name where the macro parameter `g` is meant

- Location: compiler.h:201-202:
  ```c
   *	#define greek_name(g)						\
   *		 (IS_COMPILE_CONSTANT(greek) ? _greek_name(g) : greek_name(g))
  ```
  In the example's own context, `greek` is only `enum greek` (a type)
  and the local parameter of the out-of-line/inline helpers — no object
  named `greek` is in scope at the macro's use site.
- Reproducer (/tmp/compiler-asan/probe-docexample.c): the doc example
  transcribed verbatim. Observed output — gcc:
  "error: 'greek' undeclared (first use in this function)" at the
  IS_COMPILE_CONSTANT call (clang likewise). Replacing `greek` with `g`
  makes it compile and behave as documented.
- Consequence: anyone copy-pasting the module's flagship example for
  IS_COMPILE_CONSTANT gets an immediate compile error and must diagnose
  the typo themselves. Documentation-only; no effect on compiled code.
- Repair direction: change `IS_COMPILE_CONSTANT(greek)` to
  `IS_COMPILE_CONSTANT(g)` in the comment at compiler.h:202.

## Rejected candidates (disproved)

- R1: Misspelled attribute names that gcc/clang silently ignore.
  Disproved two ways. (a) probe-all-attrs.c uses every macro in the
  header and compiles clean under both `gcc` and `clang`
  `-c -Wall -Wextra -Werror`; unknown/ misspelled `__attribute__`
  names produce -Wattributes warnings (enabled by default in gcc), so
  any typo would have been caught. (b) Functional probes show each
  attribute doing its job on both compilers: -Wnonnull fires for NULL
  literals passed to NO_NULL_ARGS and NON_NULL_ARGS(1) functions
  (probe-nonnull-warn.c); -Wsentinel fires for a missing NULL
  terminator (probe-sentinel.c, 2 warnings each compiler);
  -Wunused-result fires for WARN_UNUSED_RESULT (probe-warnunused.c);
  "function declared 'noreturn' does return" fires for a NORETURN
  function that falls off the end (probe-noreturn-def.c);
  -Wdeprecated-declarations fires for WARN_DEPRECATED (first
  probe-all-attrs run). All spellings (`cold`, `noreturn`,
  `format(__printf__,...)`, `const`, `pure`, `unused`, `used`,
  `deprecated`, `nonnull`, `returns_nonnull`, `sentinel`) are correct.
- R2: PRINTF_FMT/SCANF_FMT format-arg indexes off by one (would corrupt
  format checking tree-wide). Disproved: compile_fail-printf.c uses
  PRINTF_FMT(2,3) on `my_printf(int x, const char *fmt, ...)` and both
  compilers diagnose the bad `%p`-vs-unsigned-int call at "argument 3"
  — exactly the documented 1-based (fmt, first-vararg) numbering.
  Also NON_NULL_ARGS verified 1-based: NON_NULL_ARGS(1) warns on a NULL
  first argument, NON_NULL_ARGS(2) makes the compiler treat the second
  parameter as nonnull (probe-all-attrs first run: -Wnonnull-compare /
  -Wpointer-bool-conversion inside the function body). No SCANF_FMT
  macro exists in this header.
- R3: HAVE_* fallback definitions silently change semantics. Reviewed
  each: IS_COMPILE_CONSTANT falls back to literal 0 with an explicit
  comment "If we don't know, assume it's not" and the run test covers
  both branches; cpu_supports falls back to 0 and is documented
  "currently only works on glibc platforms"; all attribute fallbacks
  are empty defines, which only lose warnings/optimization hints, never
  change program semantics (attributes in this header are all
  hint/warning-class; CONST_FUNCTION/PURE_FUNCTION misuse is a caller
  contract issue, not a fallback issue). Rejected.
- R4: NEEDED's !HAVE_ATTRIBUTE_USED fallback
  (`#define NEEDED __attribute__((__unused__))`, :134) does not force
  emission of an unreferenced static, so it does not deliver NEEDED's
  documented "must exist even if it (seems) unused". True, but
  unreachable on every compiler CCAN targets: gcc has had `used` since
  3.1, clang always; config.h here has HAVE_ATTRIBUTE_USED=1. The
  accompanying comment ("Before used, unused functions and vars were
  always emitted") shows the fallback deliberately documents the
  limitation. Rejected as unreachable-on-supported-compilers.
- R5: C++ incompatibility. Disproved: probe-cpp.cc (PRINTF_FMT,
  CONST_FUNCTION, IS_COMPILE_CONSTANT in C++) compiles clean under both
  `g++` and `clang++` `-Wall -Wextra -Werror`. The header is pure
  macros; no extern "C" needed. Rejected.
- R6: CONST_FUNCTION / PURE_FUNCTION doc wording ("depends only on its
  argument", "it's return value" grammar). Wording nits only; the
  attribute contracts themselves are stated correctly (const implies no
  globals/pointer derefs, pure allows globals). Style; rejected.
- R7: Attribute placement (statement vs declaration position). All doc
  examples place the attribute in valid declaration positions; every
  position used in-tree and in the probes compiles -Wall -Wextra -Werror
  clean and the attributes provably take effect (R1b). Rejected.
- R8: IS_COMPILE_CONSTANT fallback branch making the doc-example macro
  recurse infinitely (`greek_name(g)` expanding itself). Disproved:
  C macro self-reference is not re-expanded (blue paint), so the false
  branch refers to the out-of-line function as documented. Moot given
  F3's typo. Rejected.
- R9: `#ifndef UNUSED` sitting inside `#if HAVE_ATTRIBUTE_UNUSED`
  (:94/:138) could leave UNUSED undefined in some configuration.
  Disproved by reading: the `#else` branch (:155-165) defines all three
  of UNNEEDED/NEEDED/UNUSED empty, each under its own `#ifndef`.
  All four combinations reachable. Rejected.
- R10: IS_COMPILE_CONSTANT evaluated twice / side effects. The macro
  expands its argument once inside __builtin_constant_p (unevaluated
  context for the expression's side effects is not guaranteed, but
  __builtin_constant_p does not evaluate its argument at runtime; gcc
  documents it as a compile-time query) and the fallback expands to
  literal 0 without referencing expr — expr is never evaluated in either
  branch, which matches the documented "does the compiler know the
  value" query semantics. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/compiler/test/run-predefine-const.c — regression test for F1:
  pre-defines CONST_FUNCTION, then uses PURE_FUNCTION (1 TAP subtest).
  Currently RED: fails to compile ("expected ';' before 'int'" /
  "unknown type name 'PURE_FUNCTION'"), taking ccanlint from 50/50 to
  40/52 (tests_compile FAIL; dependent run/coverage checks cascade).
  After the F1 repair (close the CONST_FUNCTION guard before the
  PURE_FUNCTION block) it must compile and pass, restoring full score.
  No runtime/UBSan component — the defect is purely compile-time.

No production files were modified (compiler.h and _info untouched;
verified by git status — the only new path under ccan/compiler is the
test above).
