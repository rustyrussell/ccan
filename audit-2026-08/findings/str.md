# Audit: ccan/str

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: str.h, str.c, str_debug.h, debug.c and ccan/str/test/ only.
Submodules ccan/str/base32 and ccan/str/hex are separate modules, not
audited here; str proper is not entangled with them (nothing in
str.h/str.c/debug.c references them).

Mechanical checks:
- ccanlint: 90/91.  The single lost point is
  objects_build_with_stringchecks: ccanlint prepends
  `#define CCAN_STR_DEBUG 1` + `#include <ccan/str/str.h>` to each .c
  file and compiles; for debug.c this makes its str_is*() wrappers call
  themselves through str.h's macro overrides, and gcc 13 emits
  -Winfinite-recursion warnings.  That check sets `score->pass = true`
  unconditionally ("We don't fail ccanlint for this"), so nothing else
  fails.  See R1 below for why this is not a reachable defect.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`):
  all pass, in both normal and CCAN_STR_DEBUG builds:
  - test/run.c: 12021/12021 ok (normal and -DCCAN_STR_DEBUG, the latter
    linked with test/debug.c).
  - test/run-STR_MAX_CHARS.c: 13/13 ok (normal and -DCCAN_STR_DEBUG).
  - No ASan/UBSan diagnostics in any log.
- compile_fail-* / compile_ok-* tests are ccanlint-only (they exercise
  the CCAN_STR_DEBUG compile-time checks); not run standalone.
- Reproducers live in /tmp/str-asan/repro-*.c (temporary, not committed).

Auditor-added test files (temporary; keep or remove later):
- ccan/str/test/run-strcount-empty-needle.c — proves F1 (currently
  fails: killed by SIGALRM after alarm(10)).

## Findings

## F1 — CONFIRMED (FIXED in c0c9797f): strcount(haystack, "") never terminates

- Location: ccan/str/str.c:4-13, `strcount()`:
  `nlen = strlen(needle)` is 0 for an empty needle;
  `strstr(haystack, "")` always returns `haystack` (C11 7.24.5.7:
  "If s2 points to a string with zero length, the function returns s1"),
  and `haystack += nlen` adds 0, so the loop condition is permanently
  true and the loop never makes progress.  `i` also wraps around size_t
  after 2^64 iterations, but the non-termination is the primary defect.
- Reachable path: any caller invoking `strcount(s, "")` — e.g. counting
  occurrences of a user-supplied or computed substring that happens to
  be empty (`strcount(line, sep)` where `sep` came from config/argv).
- Preconditions: str.h:64-67 documents only "@haystack: a C string,
  @needle: a substring".  The empty string is a valid C string and a
  substring of every string; nothing documents a non-empty-needle
  requirement.  No EINVAL-style contract exists.
- Concrete consequence: infinite loop (100% CPU, hangs the caller);
  denial of service in any server/tool that counts occurrences of an
  externally supplied substring.
- Reproducer: /tmp/str-asan/repro-strcount-empty.c —
  `timeout 5` kills it (exit 124) in both the normal build and the
  -DCCAN_STR_DEBUG build (the str_strstr() debug wrapper has identical
  empty-needle semantics).  Regression test:
  ccan/str/test/run-strcount-empty-needle.c fails as designed
  (terminated by SIGALRM, exit 142).
- Observed output: reproducer prints nothing and is killed by timeout;
  regression test: "Alarm clock", exit 142, under clang 18
  ASan+UBSan (sanitizers themselves report nothing — it is a pure
  liveness defect).
- Repair direction: handle the empty needle up front, e.g.
  `if (nlen == 0) return 0;` (the regression test asserts 0; returning
  strlen(haystack)+1, "between every character", would be the other
  defensible semantic — either way it must return).  Alternatively
  document a non-empty-needle precondition, but a two-line guard is
  strictly safer and matches the function's grab-bag purpose.
- Fix applied (c0c9797f): `if (nlen == 0) return 0;` guard added at
  str.c:8-9; regression test ccan/str/test/run-strcount-empty-needle.c
  committed alongside and now passes; existing tests (run.c 12021/12021)
  and ccanlint unaffected.

## Rejected candidates (disproved)

- R1: debug.c infinite recursion under CCAN_STR_DEBUG (the ccanlint
  stringchecks warnings).  Disproved as a reachable defect: debug.c
  deliberately includes only <ccan/str/str_debug.h>, never str.h, so in
  every supported build (separate TU, or the test/debug.c include
  pattern whose comment documents exactly this constraint) the is*()
  identifiers in debug.c resolve to the real ctype.h functions.
  Recursion requires a single TU that includes str.h *and* compiles
  debug.c's body, which only ccanlint's synthetic check does; that
  check explicitly declines to fail the module over it
  (objects_build_with_stringchecks: `score->pass = true`).
- R2: strstarts(str, prefix) with prefix longer than str reads out of
  bounds.  Disproved: strncmp() stops at str's NUL (it compares at most
  strlen(str)+1 bytes of str), and strlen(prefix) reads only the
  prefix.  Verified under ASan with strstarts("ab", "abcdef") placed at
  a heap boundary — clean (/tmp/str-asan/repro-edge.c).
- R3: STR_MAX_CHARS too small for some type.  Disproved exhaustively
  for 8- and 16-bit ints (every value, decimal) and at the 32/64-bit
  extremes plus 0x-hex and %p renderings
  (/tmp/str-asan/repro-maxchars-bound.c, ASan stack-buffer checks
  enabled): the formula ((sizeof*CHAR_BIT + 8)/9*3 + 2) always leaves
  room for sign, "0x" and NUL.
- R4: cis*() wrappers pass a plain char to is*().  Disproved: every
  wrapper casts to (unsigned char) first (str.h:110-163), so negative
  char values are converted to their unsigned value as C11 7.4.1
  requires.  Exercised for all char values -128..127 under UBSan —
  clean.  Locale dependence of the result is standard ctype.h behavior,
  not a defect.
- R5: CCAN_STR_DEBUG is*()/str*() macros double-evaluate their
  arguments.  Disproved: in str_check_arg_ (str.h:188-191) the argument
  appears once in evaluated context; the typeof() occurrence is
  unevaluated.  In the strstr/strchr/strrchr overrides (str.h:219-224)
  each argument is evaluated exactly once (the typeof((haystack)+0)
  occurrence is unevaluated).
- R6: strstarts() evaluates @prefix twice (strncmp argument and
  strlen(prefix)).  True, but it is a function-like macro documented as
  such; multi-evaluation of arguments is the standard C macro idiom
  (cf. streq, max, etc.) and only bites side-effecting arguments, which
  is a caller precondition.  Not a defect per AGENTS.md's scope.
- R7: debug.c assert `i >= -1 && i < 256` assumes EOF == -1.  C11 only
  requires EOF to be a negative int.  No known platform (glibc, musl,
  BSD, MSVCRT) uses EOF != -1; purely theoretical portability, rejected
  as speculative.
- R8: strcount() counter `i` overflowing size_t.  Unreachable for
  non-empty needles: occurrences are bounded by strlen(haystack) which
  is itself a size_t, so i cannot wrap.  (Empty needle: see F1 — it
  hangs long before wrap matters.)
- R9: strends() reads before `str` when postfix is longer.  Disproved:
  str.h:45-46 returns false when strlen(str) < strlen(postfix) before
  any pointer arithmetic; the str + strlen(str) - strlen(postfix)
  expression is only formed when the length check passed.  Covered by
  /tmp/str-asan/repro-edge.c (incl. "" cases) under ASan — clean.

## Observation (not a module defect)

- ccan/str/test/run.c:34-37 allocates strlen(i)+strlen(j)+1 bytes but
  passes strlen(i)+strlen(j) as the snprintf size, truncating the last
  character of every generated string ("farbar" -> "farba").  Harmless
  to the test (all comparisons use the consistently-truncated strings
  and the buffer stays NUL-terminated — ASan-clean), but it means the
  test never exercises the exact strings it appears to.  Test file, not
  production; not modified per AGENTS.md.
