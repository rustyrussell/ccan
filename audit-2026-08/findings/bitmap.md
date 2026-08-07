# Audit: ccan/bitmap

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: bitmap.h (245 lines, macros + static inlines), bitmap.c (125
lines: bitmap_zero_range, bitmap_fill_range, bitmap_ffs, bitmap_clz),
_info and ccan/bitmap/test/ only. Dep ccan/endian (cpu_to_be32/64 via
ENDIAN_CAST) used per its documented contract; testdeps ccan/array_size,
ccan/foreach. config.h: HAVE_BUILTIN_CLZL=1, so the __builtin_clzl path
is active; the fallback loop in bitmap_clz was exercised separately by
compiling run-ffs against a copy of bitmap.c with the #if forced to 0
(44832/44832 subtests pass, see below).

The header documents no per-function contracts (no doc comments, no
Example: sections in _info/bitmap.h — ccanlint examples_exist +0/2), so
caller preconditions below are inferred from the API shape: bit and
range indices must lie within the nbits the bitmap was created with,
and nbits must describe a bitmap the caller can actually store.

## Mechanical checks

- ccanlint (before auditor additions): **45/52**, every check passes
  except tests_coverage (+1/6) and examples_exist (+0/2). tests_pass,
  tests_pass_valgrind all PASS.
- After adding the auditor test: ccanlint **39/46 FAIL** — exactly as
  designed, because test/run-sizeof-overflow.c fails 2/4 subtests
  against the current code (F1).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/bitmap/bitmap.c directly; linked with ccan/tap/tap.c only;
  per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **4/4 pass cleanly, zero sanitizer diagnostics** — run (378 ok),
  run-ffs (44832 ok), run-ranges (72989 ok, exhaustive over all ranges
  for sizes 1..256), run-alloc (4494 ok, all alloc/realloc size pairs).
  Total 122693 ok.
- Fallback clz path: run-ffs rebuilt against bitmap-fallback.c
  (HAVE_BUILTIN_CLZL forced to 0): 44832 ok, clean under ASan+UBSan.
- -m32: skipped; fails on missing 32-bit system headers via errno.h
  (known host limitation, same as earlier modules).
- Differential fuzzing (/tmp/bitmap-asan/repro-fuzz.c, temporary):
  200000 iterations on random 1..300-bit bitmaps comparing
  set/clear/test, zero_range/fill_range (random ranges biased to word
  boundaries and empty ranges n==m), bitmap_ffs over the same ranges,
  empty/full/equal/intersects/subset, and/or/xor/andnot/complement
  against a per-bit reference model, under ASan+UBSan: **clean**, no
  mismatches, no sanitizer diagnostics.
- Reproducers/probes in /tmp/bitmap-asan/ (temporary, not committed):
  repro-sizeof-overflow.c, repro-alloc-overflow-write.c, repro-fuzz.c,
  run-ffs-fallback.c + bitmap-fallback.c.

## Findings

## F1 — LIKELY (FIXED in ab052a14): BITMAP_NWORDS/bitmap_sizeof overflow for nbits within 63 of ULONG_MAX — bitmap_alloc() returns a non-NULL 0-byte allocation, first use writes out of bounds

- Location: ccan/bitmap/bitmap.h:15-16 (BITMAP_NWORDS) and
  bitmap.h:31-34 (bitmap_sizeof), feeding bitmap_alloc (bitmap.h:198-201),
  bitmap_alloc0/1, bitmap_realloc0/1, bitmap_zero/fill/copy:
  ```c
  #define BITMAP_NWORDS(_n)   (((_n) + BITMAP_WORD_BITS - 1) / BITMAP_WORD_BITS)
  ```
- Reachable path: any call with `nbits > ULONG_MAX - (BITMAP_WORD_BITS - 1)`
  (on LP64, nbits in [0xffffffffffffffc1, ULONG_MAX] — the top 63
  values). The addition wraps, so BITMAP_NWORDS and bitmap_sizeof
  return 0 (or a tiny value) instead of ~2^58 words / ~2^61 bytes.
- Preconditions: none documented anywhere for these functions. The
  inferred contract of bitmap_alloc(nbits) is "allocate storage for
  nbits bits, or return NULL on failure"; a request that cannot be
  satisfied must fail, not succeed with a 0-byte buffer.
- Concrete incorrect consequence, observed
  (/tmp/bitmap-asan/repro-sizeof-overflow.c, LP64):
  ```
  nbits=0xffffffffffffffff bitmap_sizeof=0        (true: 2^61 bytes)
  nbits=0xffffffffffffffc1 bitmap_sizeof=0        (true: 2^61 bytes)
  nbits=0xffffffffffffffc0 bitmap_sizeof=0x1ffffffffffffff8  (no wrap)
  bitmap_alloc0(ULONG_MAX) = 0x5020...10 (malloc of 0 bytes)
  ```
  and the follow-through (/tmp/bitmap-asan/repro-alloc-overflow-write.c,
  clang 18 ASan): bitmap_alloc(ULONG_MAX) returns non-NULL, then
  `bitmap_set_bit(b, 0)` does an 8-byte read-modify-write on the 1-byte
  region ASan gives malloc(0):
  ```
  ==ERROR: AddressSanitizer: heap-buffer-overflow
  READ of size 8
  0x502000000011 is located 0 bytes after 1-byte region
  ```
  Unsigned wrap is defined behavior, so UBSan stays silent and plain
  builds fail the same way (regression test fails 2/4 with plain gcc).
- Why LIKELY rather than CONFIRMED (same reasoning as time.md F2): the
  wrap only triggers for nbits whose true allocation is >= 2^61 - 8
  bytes — no caller can actually back such a bitmap with memory, so no
  realistic workload reaches it, and the worst realistic outcome is the
  wrong failure mode (non-NULL + OOB on first use instead of a clean
  NULL). But the overflow is mechanical, needs no precondition
  violation by the (undocumented) API, and the fix is trivial.
- Regression test: ccan/bitmap/test/run-sizeof-overflow.c
  (alarm(10)-bounded, 4 subtests; reference formula divides first, then
  rounds up, so it never overflows and its byte count still fits size_t
  on both LP64 and ILP32). Currently fails subtests 1-2 in plain and
  ASan builds; subtests 3-4 pin the largest non-wrapping value
  (ULONG_MAX - 63) so the boundary must keep passing after repair.
- Repair direction: compute the round-up without an overflowing
  addition, e.g.
  `#define BITMAP_NWORDS(_n) (((_n) / BITMAP_WORD_BITS) + (((_n) % BITMAP_WORD_BITS) != 0))`
  On LP64 this peaks at 2^58 words / 2^61 bytes (fits size_t, malloc
  then correctly returns NULL); on ILP32 it peaks at 2^27 words / 2^29
  bytes (also fits).

## Rejected candidates (disproved)

- R1: Shift-by-word-size in BITMAP_WORDBIT (bitmap.h:45-46),
  BITMAP_TAILBITS (bitmap.h:56-57), headmask/tailmask in bitmap.c:16-17,
  41-42, 84-85, and bitmap_clz's fallback mask (bitmap.c:68). Every
  shift count is `x % BITMAP_WORD_BITS` or
  `BITMAP_WORD_BITS - (x % BITMAP_WORD_BITS) - 1`, both bounded to
  [0, BITMAP_WORD_BITS-1]; BITMAP_TAILBITS with nbits % WORD == 0 is a
  shift by 0 (yielding 0) and is additionally only evaluated under
  BITMAP_HASTAIL guards in bitmap_equal/full/empty. No shift-by-W
  exists. UBSan (shift checks enabled) clean over 122693 existing
  subtests + 200000 fuzz iterations. Rejected.
- R2: bitmap_ffs on empty and degenerate ranges (bitmap.c:79-125).
  Traced: for n == m, either an == am == n (aligned; all branches
  skipped, returns m) or am < an (unaligned; headmask & tailmask
  selects the empty bit set n%W..m%W-1, w == 0, returns m) — and the
  word read BITMAP_WORD(b, n) is the valid last word whenever n ==
  m == nbits with nbits % W != 0. In the am < an branch
  BIT_ALIGN_DOWN(m) == BIT_ALIGN_DOWN(n) always holds (m < (n/W+1)*W
  implies same word), so `am + bitmap_clz(w)` is the correct word base.
  Covered by run-ffs (44832 ok) and the fuzz, which includes empty
  ranges and word-edge ranges. Rejected.
- R3: bitmap_clz fallback loops forever on w == 0 (bitmap.c:70-73).
  All four call sites guard on w != 0 (bitmap.c:94, 102, 109, 120).
  Fallback path verified by forced-#if build of run-ffs (44832 ok).
  Rejected.
- R4: bitmap_zero_range/fill_range with m < n in NDEBUG builds
  (bitmap.c:19, 44). The assert(m >= n) documents the precondition; in
  the single-word case the mask is empty so it is a no-op anyway.
  Violation of a documented precondition; rejected.
- R5: Macro multiple evaluation: BITMAP_NWORDS, BITMAP_HEADWORDS,
  BITMAP_TAILBITS and BITMAP_TAIL evaluate their argument 2-4 times.
  These are macro primitives whose arguments are sizes/indices by
  convention (same style as the rest of CCAN); no self-modifying
  argument appears anywhere in-tree. Style; rejected.
- R6: bitmap_realloc0/bitmap_realloc1 overwrite `b` with realloc's
  return (bitmap.h:226, 237), leaking the original allocation if
  realloc fails. Standard realloc-style API caveat, caller-visible by
  design; rejected.
- R7: bitmap_bswap has no return for unsigned long widths other than
  32/64 (bitmap.h:36-42). No supported platform has another width;
  ccanlint's warning-enabled builds are clean. Speculative portability;
  rejected.
- R8: Endianness of the byte-swapped big-endian bit numbering
  (BITMAP_WORDBIT, bitmap_bswap). Internal consistency of
  set/test/ranges/ffs/binops is endian-independent and fully covered by
  the fuzz; only LE hardware was testable on this host, but no
  host-order-dependent path exists outside bitmap_bswap itself.
  Rejected.
- R9: nbits == 0 uses: bitmap_zero/fill/copy memmove 0 bytes;
  BITMAP_DECLARE(name, 0) creates a zero-length array (GNU extension,
  compiles cleanly here); bitmap_empty/full/equal(…, 0) return
  true/true/true and bitmap_ffs(b, 0, 0) returns 0 without touching
  memory. Consistent; rejected.
- R10: bitmap_intersects/subset tail masking (bitmap.h:143-145,
  158-160): `~BITMAP_TAIL(...)` also sets bits outside the tail, but it
  is ANDed with BITMAP_TAIL of the other operand, which is masked to
  the tail bits, so out-of-range bits cannot affect the result.
  Verified by fuzz. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/bitmap/test/run-sizeof-overflow.c — proves F1. Currently fails
  2/4 in both plain gcc and clang ASan builds (BITMAP_NWORDS and
  bitmap_sizeof wrap to 0 at ULONG_MAX); alarm(10)-bounded. After
  repairing F1 it must pass 4/4 (subtests 3-4 pin the non-wrapping
  boundary).
- ccanlint with this file present: 39/46 FAIL, solely because
  run-sizeof-overflow.c fails against the current code.

No production files were modified (bitmap.h, bitmap.c, _info untouched;
verified by git status — the only new path under ccan/bitmap is the
test above).
