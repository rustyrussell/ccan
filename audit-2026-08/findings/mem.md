# Audit: ccan/mem

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: mem.h (295 lines, mostly static inlines), mem.c (128 lines),
_info, ccan/mem/test/ and ccan/mem/bench/ only. Sole dependency:
ccan/compiler (PURE/CONST_FUNCTION attributes); treated as given.
config.h here: HAVE_MEMMEM=1, HAVE_MEMRCHR=1, HAVE_TYPEOF=1,
HAVE_VALGRIND_MEMCHECK_H=1 — so on this host the memmem/memrchr
fallbacks (mem.c:9-43) are compiled out; they were audited by forcing
HAVE_MEMMEM 0 / HAVE_MEMRCHR 0 via a shim config.h
(/tmp/mem-asan/fakeconfig/config.h, -I before -I.) and by direct
reproducers. memcchr/mempbrkm/memeqzero/memswap/memtaint and all
inlines are active on this host and were exercised directly.

## Mechanical checks

- ccanlint (before auditor additions): **60/61**, every check passes
  except examples_exist (+1/2, no Example: section in _info).
  tests_pass, tests_pass_valgrind, examples_compile all PASS.
- After adding the two auditor regression tests: ccanlint **51/59
  FAIL** — exactly as designed: run-memcchr-range.c fails 3/6 and
  run-memrchr-fallback.c fails 3/5 against the current code
  (tests_pass +2/4, tests_pass_without_features +2/4, dependent checks
  skipped). tests_compile passes for both new files (run tests must
  #include the module .c themselves — ccanlint only links module
  objects into api tests, tests_compile.c:146).
- Existing tests under clang 18.1.3 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; linked with ccan/mem/mem.c and
  ccan/tap/tap.c; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - api.c: **aborts at 54/65** — UBSan array-bounds on the *test's own*
    UB at api.c:99 (`haystack1 - 1`, forming a pointer before the
    array; see R8). Zero failures in the 54 completed subtests. With
    that one expression neutralized via uintptr_t in a /tmp copy
    (api-noub.c, production untouched): **65/65 clean**, zero
    sanitizer diagnostics.
  - api-memcheck.c: **4/4 clean**.
- Fallback-forced build (fakeconfig HAVE_MEMMEM 0, HAVE_MEMRCHR 0,
  compiling the module's own memmem/memrchr): api-noub **65/65 clean**
  under ASan+UBSan. So the fallback memmem handles all api.c edge
  cases (needlelen > haystacklen, embedded NULs, etc.) correctly.
- -m32: builds and passes (api.c 65/65, ASan+UBSan; the api.c:99 UB is
  not flagged by -m32 clang). No word-size-sensitive arithmetic in the
  module.
- Differential fuzzer (/tmp/mem-asan/repro-diff-fuzz.c, 200k random
  iterations, ASan+UBSan, fallback memmem/memrchr active): memmem,
  mempbrkm, memeqzero **zero mismatches** vs brute-force oracles
  (needlelen 0, haystacklen 0, needle longer than haystack, high-bit
  bytes all covered). Only the memrchr (F2) and memcchr (F1)
  c-conversion mismatches fired.
- Reproducers/probes in /tmp/mem-asan/ (temporary, not committed):
  repro-memcchr-sign.c, repro-memrchr-c.c, repro-diff-fuzz.c,
  repro-overlaps-empty.c, plain-glibc.c, glibc-oracle.c, api-noub.c,
  fakeconfig/config.h.

## Findings

## F1 — CONFIRMED (FIXED in 351072ac): memcchr() compares a signed char against int c, so bytes 0x80..0xFF never match c values 128..255 (and c > 255 never matches), violating its documented "complement of memchr()" contract

- Location: ccan/mem/mem.c:57-67, concretely mem.c:63:
  ```c
  char const *p = data;
  ...
  if (p[i] != c)
  ```
  `p[i]` is `char` — signed on x86/x86-64 and ARM-default Linux — and
  is sign-extended to int for the comparison; `c` is used unconverted.
- Documented contract: mem.h:70-79 — "memcchr - scan memory until a
  character does _not_ match @c ... The complement of memchr().
  Returns a pointer to the first character which is _not_ @c."
  memchr (C17 7.24.5.1/2) converts c to unsigned char; the complement
  must therefore treat byte value (unsigned char)c as matching.
- Reachable path: any call memcchr(data, c, len) where the buffer
  contains byte value (unsigned char)c and c is outside the signed-
  char range of that byte: c in 128..255 with high-bit bytes present
  (e.g. scanning UTF-8/binary data for a byte value held in an int,
  the natural type produced by getc()/byte arithmetic), or c > 255
  (memchr semantics: c converts mod 256). Example:
  memcchr("\x80\x80x", 0x80, 3) must return buf+2 ('x' is the first
  byte not equal to 0x80); it returns buf+0, because
  (int)(char)0x80 == -128 != 0x80.
- Caller preconditions: none violated — c is any int, as for memchr;
  the data pointer/length are valid.
- Observed output (/tmp/mem-asan/repro-memcchr-sign.c, plain and
  ASan+UBSan builds; also the differential fuzzer, e.g.
  "memcchr mismatch iter=361 hl=26 c=256"):
  ```
  memcchr(buf, 0x80, 3)    = buf+0, expect buf+2   (MISMATCH)
  memchr (buf, 0x80, 3)    = buf+0                 (libc memchr agrees byte 0x80 matches c=0x80)
  memcchr(buf_ff, 255, 2)  = buf_ff+0, expect +1   (MISMATCH)
  memcchr(buf_ff, -1, 2)   = buf_ff+1              (negative c works only by sign-extension accident)
  MISMATCH (2), rc=2
  ```
- Concrete consequence: a caller using memcchr to skip a run of a
  high-bit byte (e.g. trimming 0xFF padding, c held as int 255) gets a
  pointer to the very byte it asked to skip — a wrong result from a
  valid call. Unlike the fallback-only F2, this function is compiled
  unconditionally and is broken on this host today.
- Regression test: ccan/mem/test/run-memcchr-range.c (alarm(10)-
  bounded, 6 subtests). Currently fails 3/6 (c=0x80, c=255, c=0x100);
  must pass after repair.
- Repair direction: convert both sides to unsigned char at mem.c:63,
  e.g. `if ((unsigned char)p[i] != (unsigned char)c)` (or make p an
  `unsigned char const *`).

## F2 — CONFIRMED (FIXED in dde9474c): fallback memrchr() (HAVE_MEMRCHR == 0) does not convert c to unsigned char; any c outside 0..255 never matches, where the real memrchr finds the byte

- Location: ccan/mem/mem.c:30-43, concretely mem.c:36:
  ```c
  if (p[n-1] == c)
  ```
  `p[n-1]` is unsigned char promoted to int (range 0..255); `c` is the
  unconverted int argument.
- Documented contract: the module exists to "implement some string.h
  mem*() functions if they're not already available in the C library"
  (_info). memrchr is documented (glibc/POSIX, and the module's own
  declaration comment in mem.h:17-20 provides it as a drop-in) as
  "like memchr, searching backward"; memchr converts c to unsigned
  char (C17 7.24.5.1/2). glibc 2.39 behavior verified directly
  (/tmp/mem-asan/plain-glibc.c): memrchr(buf, 0x1FF, 3) == buf+1,
  memrchr(buf, -1, 3) == buf+1.
- Reachable path: on a libc without memrchr (the exact platforms the
  fallback exists for), any call memrchr(s, c, n) with c outside
  0..255 and byte (unsigned char)c present in s — e.g. c = -1 from a
  caller passing a signed char variable holding 0xFF, or c with high
  bits set. The fallback returns NULL instead of the match.
- Caller preconditions: none violated — c is any int per the standard
  memchr-style contract.
- Observed output (/tmp/mem-asan/repro-memrchr-c.c, fallback forced
  via fakeconfig, oracle = documented conversion semantics; identical
  results in plain gcc and clang ASan+UBSan builds):
  ```
  c=0x1FF: fallback=(nil) oracle=buf+1 (expect buf+1)
  c=-1:    fallback=(nil) oracle=buf+1 (expect buf+1)
  c=0x141: fallback=(nil) oracle=buf+0 (expect buf+0)
  MISMATCH (3), rc=3
  ```
  (An earlier attempt linking glibc as the oracle silently called the
  fallback back — the module's memrchr interposes the libc symbol when
  linked into the same binary; noted as an integration hazard of the
  fallback, not separately reported.)
- Concrete consequence: a reverse byte search silently misses an
  existing match (returns NULL) on fallback platforms — e.g. a
  parser scanning backwards for a separator passed as a signed char.
- Regression test: ccan/mem/test/run-memrchr-fallback.c (alarm(10)-
  bounded, 5 subtests). It forces HAVE_MEMRCHR 0 and #includes
  ccan/mem/mem.c so the fallback is tested on any host. Currently
  fails 3/5 (c=-1, c=0x1FF, c=0x141); must pass after repair.
- Repair direction: convert at mem.c:36:
  `if (p[n-1] == (unsigned char)c)`.

## F3 — LIKELY (documented in f5b48d0f): memoverlaps() reports an empty range as overlapping any range that straddles its address

- Location: ccan/mem/mem.h:223-231:
  ```c
  return (a < (b + bl)) && (b < (a + al));
  ```
  With al == 0 this reduces to `a < b + bl && b < a` — true whenever a
  lies strictly inside [b, b+bl). An empty range overlaps nothing
  under half-open-interval set semantics, so the answer is a false
  positive.
- Observed output (/tmp/mem-asan/repro-overlaps-empty.c):
  ```
  memoverlaps(buf+4, 0, buf, 8) = 1 (set semantics: 0)
  memoverlaps(buf+4, 0, buf+4, 0) = 0
  ```
- Why LIKELY rather than CONFIRMED: the doc (mem.h:216-222) does not
  define empty-range behavior, and there is no in-module consequence —
  the only internal caller, memswap's assert (mem.c:77), queries
  memoverlaps(a, n, b, n) with equal lengths, where the quirk cannot
  fire for n == 0 (both-empty reduces to a < b && b < a, false) and
  the false positive for n > 0 requires the ranges to actually
  overlap. A caller checking overlap before a zero-length operation
  would merely take a needless conservative path. The defect is a real
  logic error in the boundary semantics but with weak demonstrated
  impact.
- Repair direction: reject empty ranges up front, e.g.
  `return al != 0 && bl != 0 && (a < (b + bl)) && (b < (a + al));`
  — or document the current boundary semantics.

## Rejected candidates (disproved)

- R1: memmem fallback forms a pointer beyond one-past-the-end
  (mem.c:20-22): when needlelen == haystacklen and the first position
  mismatches, `p + needlelen` computes haystack + haystacklen + 1 —
  pointer arithmetic past the one-past-end pointer, formally UB
  (C17 6.5.6/8). Actively tried: 200k-iteration differential fuzz
  under ASan+UBSan (including nl == hl) — no diagnostic, no wrong
  result; the pointer is only compared, never dereferenced, and
  address-space wrap would require a buffer ending within
  haystacklen bytes of SIZE_MAX. No reachable incorrect consequence;
  rejected as speculative.
- R2: memmem(NULL, 0, needle, 0) — NULL + 0 arithmetic in the fallback
  loop condition. Null/0 buffers are an implied-precondition edge
  (glibc happens to tolerate it); rejected.
- R3: memoverlaps relational comparison of pointers to distinct
  objects (mem.h:230) and potential address wrap in `b + bl` —
  formally UB, but the universal flat-address-space idiom; wrap
  requires a SIZE_MAX-scale length (no real buffer). No observed
  consequence; rejected.
- R4: memswap overlap only checked by assert (mem.c:77), absent under
  NDEBUG — overlap is a documented precondition: "Undefined results
  if the two memory regions overlap" (mem.h:239). Rejected.
- R5: memtaint(NULL, 0) calls memcpy(NULL, tainter, 0) (mem.c:123) —
  formally UB, universally benign, precondition edge; rejected.
- R6: memeqzero self-overlapping memcmp (mem.c:108) — memcmp imposes
  no overlap restriction (unlike memcpy); the first-16-bytes loop
  guarantees length accounting is exact (verified by reasoning over
  len 0..17 and by the fuzzer over len 0..32). Correct; rejected.
- R7: memcchr with negative c (e.g. -1) — works by sign-extension
  accident ((int)(char)0xFF == -1), matching the conversion semantics
  for that one case; not a defect. The genuinely broken ranges are F1.
- R8: api.c:99-100 forms `haystack1 - 1` (pointer before the array) to
  test memoverlaps — the test's own UB; it aborts the stock test
  suite under UBSan array-bounds (observed, 54/65 before abort), so
  the module currently cannot demonstrate a clean sanitizer run.
  Test artifact, not a production defect; production files untouched
  (the neutralized copy api-noub.c lives in /tmp only). Worth a
  follow-up fix by the maintainer (e.g. compute the pointer via
  uintptr_t, or point into a larger enclosing buffer).
- R9 (from the suspect list): word-at-a-time alignment issues — none
  exist. The module does no word-at-a-time access: memtaint writes via
  16-byte memcpy (alignment-safe), memeqzero compares with memcmp, all
  other functions are byte loops or thin wrappers over memcmp/strlen.
- R10: len arithmetic in memends/memstarts/memeq inlines — the guards
  are exactly right: memends requires s_len >= suffix_len before
  computing s + s_len - suffix_len (mem.h:195), memstarts rejects
  prefix_len > data_len (mem.h:128), memeq short-circuits al == 0
  before memcmp (mem.h:107). Empty suffix/prefix return true, matching
  api.c expectations. No underflow or OOB; rejected.
- R11: HAVE_MEMMEM/HAVE_MEMRCHR gating (mem.h:11-20 vs mem.c:9-43) is
  consistent, and the fallback declarations match the definitions.
  Verified by compiling both configurations. No defect.
- R12: memmem fallback needlelen == 0 returns haystack, matching glibc
  semantics (verified in the differential fuzz, nl == 0 cases clean).
  memcmp(NULL, NULL, 0) corner subsumed by R2. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/mem/test/run-memcchr-range.c — proves F1. Fails 3/6 against the
  current code in plain builds (memcchr returns the byte it was asked
  to skip for c = 0x80, 255, 0x100); alarm(10)-bounded. Must pass
  after repair. #includes ccan/mem/mem.c per run-test convention
  (ccanlint does not link module objects into run tests).
- ccan/mem/test/run-memrchr-fallback.c — proves F2. Forces
  HAVE_MEMRCHR 0 and #includes ccan/mem/mem.c so the fallback is
  exercised on any host; fails 3/5 today (c = -1, 0x1FF, 0x141);
  alarm(10)-bounded. Must pass after repair.
- ccanlint with both files present: 51/59 FAIL, solely because these
  two tests fail against the current code (tests_pass and
  tests_pass_without_features +2/4 each; dependent checks skipped).

No production files were modified (mem.h, mem.c, _info untouched;
verified by git status — the only new paths under ccan/mem are the
two tests above).
