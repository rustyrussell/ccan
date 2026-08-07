# Audit: ccan/ptrint

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: ptrint.h (35 lines, header-only, two static inlines), _info,
info, ccan/ptrint/test/run.c only. Deps ccan/build_assert and
ccan/compiler (already audited); testdep ccan/array_size. ptrint uses
them exactly as documented (BUILD_ASSERT, CONST_FUNCTION) — no misuse.

## Mechanical checks

- ccanlint: **37/38**, every check passes. The single lost point is
  examples_exist (+1/2): ptrint.h itself has no `Example:` section
  (the _info example exists, compiles, runs and is relevant).
  Documentation nit, not a defect.
- Existing test under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; run.c does not include a module
  .c — header-only module; linked with ccan/tap/tap.c; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **14/14 ok, zero sanitizer diagnostics**.
- -m32 (ILP32): builds and passes 14/14 under clang ASan+UBSan. The
  module's only size assumption (sizeof(int) <= sizeof(ptrdiff_t)) is
  enforced by its own BUILD_ASSERT (ptrint.h:26).
- Reproducers/probes in /tmp/ptrint-asan/ (temporary, not committed):
  repro-ub.c, repro-ub2.c, repro-roundtrip.c, repro-align.c.

## Findings

None. No CONFIRMED or LIKELY defects retained.

## Rejected candidates (disproved)

- R1: `int2ptr()` computes `(char *)NULL + i` (ptrint.h:32) — pointer
  arithmetic on a null pointer, which is UB per C17 6.5.6p8 (the base
  must point into an array object; C23 legitimizes only null+0).
  Actively tried to produce an observable consequence:
  - clang 18 `-fsanitize=undefined` (which includes pointer-overflow):
    silent on the header's code at -O0 and -O2, for i in
    {-INT_MAX, -4096, -1, 0, 1, 2, 17, INT_MAX, ±2^40}
    (repro-ub.c, repro-roundtrip.c).
  - clang `-fsanitize=pointer-overflow` explicitly: still silent.
    Cause identified with repro-ub2.c: clang *does* flag
    "applying non-zero offset 17 to null pointer" when the null base
    arrives via a runtime variable, but does not instrument arithmetic
    on a null-pointer-*constant* base — and ptrint.h:32 uses the
    constant form, so even sanitizer-instrumented callers see nothing.
  - gcc 13 `-O2 -fsanitize=undefined -fno-sanitize-recover=all`: silent.
  - No miscompilation: clang -O2 and gcc -O2 round-trip all probe
    values bit-exactly, including the truth-value property
    `!int2ptr(v) == !v` (v=0 yields NULL).
  The conversion is the module's entire documented purpose
  (_info: "Encoding integers in pointer values"); every platform CCAN
  targets implements the obvious flat-address semantics, and no
  sanitizer or optimizer on this toolchain turns the strict-UB reading
  into an incorrect result. A reformulation through uintptr_t casts
  would be implementation-defined rather than UB, but that is
  speculative hardening per AGENTS.md. Rejected.
- R2: `ptr2int()` computes `(const char *)p - (const char *)NULL`
  (ptrint.h:27) — pointer subtraction whose operands do not point into
  the same array object, UB per C17 6.5.6p9. Same disproof family as
  R1: silent under clang `-fsanitize=undefined` and under
  `-fsanitize=address,pointer-subtract` (repro-ub.c, repro-ub2.c —
  clang's pointer-subtract check fires on neither the header form nor a
  runtime-null equivalent), no optimizer effect at -O2. Rejected.
- R3: Truncation on LLP64 (Windows): the interface is `ptrdiff_t` in
  both directions, so there is no narrowing conversion anywhere in the
  module. On Win64, ptrdiff_t is pointer-sized (__int64), and the
  header's own BUILD_ASSERT(sizeof(int) <= sizeof(ptrdiff_t))
  (ptrint.h:26) fails the build on any platform where even an `int`
  would not fit. MSVC cannot be compile-tested on this host; supported
  by reasoning plus the -m32 ILP32 pass. Rejected.
- R4: Alignment of packed ints: int2ptr(1) forms a `ptrint_t *` with
  address 1, misaligned for any struct — at worst a C17 6.3.2.3p7
  issue in the char* -> ptrint_t* conversion. Disproved: `ptrint_t`
  is a deliberately incomplete type (ptrint.h:13-16) precisely so the
  pointer can never be dereferenced, and clang with
  `-fsanitize=address,undefined,alignment -fno-sanitize-recover=all`
  round-trips addresses 1, 3, 7, 0x7fffffff silently
  (repro-align.c). Rejected.
- R5: Signedness: ptr2int returns signed ptrdiff_t; negative values
  (-INT_MAX, -1, -(1<<40)) round-trip bit-exactly (repro-roundtrip.c),
  and the documented truth-value property holds for all probe values
  including 0. Rejected.
- R6: CONST_FUNCTION (gcc `const` attribute) on functions taking
  pointer arguments: purity requires only that the result not depend on
  pointed-to memory; both functions depend solely on the pointer
  *value*, which is the permitted case. No miscompilation observed at
  -O2 with either compiler. Rejected.
- R7: ptrint.h:22-25 comment says "we want a warning" but BUILD_ASSERT
  is a compile *error*. Comment wording nit only — ignored per the
  style rule.
- R8: The _info example assigns `ptr2int(opaque)` (ptrdiff_t) to an
  `int`: a caller-side narrowing that only bites if the caller packed a
  value outside int range; the example packs 17, and the API itself is
  lossless at ptrdiff_t width. Rejected.

## Auditor-added test files

None — no defect was retained, so no regression tests were added.

No production files were modified (ptrint.h, _info, info untouched;
verified by `git status` — no new or changed paths under ccan/ptrint).
