# Audit: ccan/endian

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: endian.h (363 lines, header-only), _info and ccan/endian/test/
only. No dependencies ("depends" empty in _info). Host config.h:
HAVE_LITTLE_ENDIAN=1, HAVE_BIG_ENDIAN=0, HAVE_BYTESWAP_H=1,
HAVE_BSWAP_64=1, so the default build uses glibc's bswap_16/32/64 and
the LE identity conversions; the ccan fallbacks and the BE branches were
exercised via config-override probes (below).

Module shape: pure value arithmetic. BSWAP_16/32/64 constant macros
(endian.h:18-58), optional inline bswap_* fallbacks (:71-103),
leint/beint typedefs with sparse annotations (:139-144), CPU_TO_LE/BE
and LE/BE_TO_CPU macro pairs selected by HAVE_LITTLE/BIG_ENDIAN
(:146-236), and 12 static inline converters (:243-345). There are NO
pointer dereferences, loads, stores, or type-punning casts anywhere in
the module — no memory access of any kind.

## Mechanical checks

- ccanlint (before any auditor additions): **52/52**, all checks pass.
- test/run.c (48 subtests) under clang 18.1.3 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function`, -g, -I.,
  linked with ccan/tap/tap.c; ASAN_OPTIONS=abort_on_error=1:symbolize=0:
  detect_leaks=0, timeout 60): **48/48 pass, rc=0, zero sanitizer
  diagnostics**.
- test/run.c under the forced-fallback config (HAVE_BYTESWAP_H=0,
  HAVE_BSWAP_64=0 via /tmp/endian-asan/cfg-nobswap/config.h — exercises
  ccan's inline bswap_16/32/64 at endian.h:71-103 instead of glibc's):
  48/48 pass, rc=0, UBSan-clean.
- test/run.c under gcc 13 ASan+UBSan: 48/48 pass, rc=0.
- test/run.c under clang -m32 ASan+UBSan (this host has working 32-bit
  headers for this include set): 48/48 pass, rc=0.
- test/compile_ok-constant.c (BSWAP_* in static initializers): compiles
  and runs clean under clang ASan+UBSan, rc=0.
- Value-level fuzz probes in /tmp/endian-asan/ (temporary, not
  committed): probe-fallback.c, probe-be.c, probe-sysendian.c,
  probe-dbleval.c.

## Findings

None. No CONFIRMED or LIKELY defects retained.

## Rejected candidates (disproved)

- R1: Shift overflow / UB in the 64-bit BSWAP path (endian.h:50-58).
  Disproved: every operand is cast to uint64_t before masking and the
  shifts (8..56) are below the 64-bit width, so C17 6.5.7/4 is
  satisfied; the result always fits. probe-fallback.c ran BSWAP_64,
  bswap_64, BSWAP_32/16 and all round trips against a byte-loop
  reference over 12 edge values (0, 1, 0xff..0xffff, 2^32, 2^63,
  UINT64_MAX, 0x0011223344556677, 0xdeadbeefcafebabe) plus 200,000
  xorshift64* pseudo-random values under clang UBSan: fails=0, zero
  sanitizer diagnostics. Same for the 32/16-bit paths (uint32_t shifts
  <=24; uint16_t promotes to int, `<< 8` reaches at most 0xff00, well
  inside INT_MAX).
- R2: Strict-aliasing violation in the byte-swap fallback. Disproved by
  reading: the !HAVE_BYTESWAP_H/!HAVE_BSWAP_64 fallbacks
  (endian.h:71-103) are pure shift/mask arithmetic on by-value integer
  arguments — no pointer casts, no type punning. There is nothing to
  alias. Verified UBSan-clean (-fsanitize=undefined includes no
  aliasing check, but no questionable cast exists to begin with) in
  probe-fallback.c.
- R3: Unaligned access. Disproved by reading: the module never accesses
  memory; leint64_t/beint64_t etc are plain integer typedefs
  (endian.h:139-144). Any unaligned load/store would be caller code
  casting buffers, outside this module's contract and reach.
- R4: Big-endian correctness (the HAVE_BIG_ENDIAN branches,
  endian.h:183-190 and :192-236, are never compiled on this host).
  Probed by simulation: /tmp/endian-asan/cfg-be/config.h claims
  HAVE_BIG_ENDIAN=1/HAVE_LITTLE_ENDIAN=0 on the LE host, and probe-be.c
  checks all 12 conversion directions value-wise against byte-loop
  references (under a BE config, cpu_to_leN must equal bswapN and
  cpu_to_beN must be identity) over the same edge+200k random value
  set, plus all round trips: fails=0, rc=0, UBSan-clean. The BE
  branches reuse the same BSWAP_* macros validated in R1. (The module's
  own run.c cannot be reused for this since its union-byte assertions
  describe true memory order; the value-level probe is the correct
  equivalent.) Note run.c:59-103 independently confirms the true-LE
  memory layout on this host.
- R5: Constant-fold vs runtime path divergence. Disproved: both paths
  expand the same BSWAP_* expressions (CPU_TO_BE32(native) is
  `(beint32_t)BSWAP_32(native)` and cpu_to_be32() just calls the macro,
  endian.h:230-235/:303-310); compile_ok-constant.c proves the macros
  are valid in static initializers, and probe-fallback.c checks BSWAP_*
  and bswap_* against the same reference on identical inputs (fails=0).
- R6: Double evaluation of BSWAP_16/32/64 (arg textually repeated
  2/4/8 times). probe-dbleval.c confirms evals=2/4/8 for
  BSWAP_16/32/64(next()) with a side-effecting argument. But the
  documented contract is "@val: constant value whose bytes to swap" and
  "Designed to be usable in constant-requiring initializers"
  (endian.h:9-11, 25-27, 42-44); runtime conversions are provided as
  the single-evaluating inline functions bswap_16/32/64 and
  cpu_to_*/**_to_cpu (probe-dbleval.c: bswap_64 evals=1, cpu_to_be64
  evals=1). Handing a side-effecting expression to BSWAP_* violates the
  documented precondition. Rejected per AGENTS.md.
- R7: Reserved-identifier/redefinition clash from endian.h:106-127
  defining __LITTLE_ENDIAN/__BIG_ENDIAN unconditionally and __BYTE_ORDER
  conditionally. Disproved: probe-sysendian.c includes the system
  <endian.h> first, then ccan/endian/endian.h, under
  `clang -Wall -Wextra -Werror` — compiles clean (glibc's definitions
  are token-identical, so the redefinitions are benign per C17 6.10.4/2),
  __BYTE_ORDER == __LITTLE_ENDIAN, conversions work. A genuinely
  conflicting pre-definition is caught loudly by the #error directives
  at endian.h:118-126, not silently. No incorrect consequence reachable.
- R8: BSWAP_16's result type is int (integer promotion), risking sign
  issues in `char buf[BSWAP_16(0xFF00)]` or narrowing. Disproved: the
  value range is 0..0xffff (both halves are masked before shifting), so
  the int result is never negative; conversion back to uint16_t in
  bswap_16/CPU_TO_LE16 is exact. compile_ok-constant.c uses exactly the
  0xFF00 case in a static array size and compiles clean.
- R9: glibc bswap_64 presence assumed when HAVE_BYTESWAP_H=1. Reading:
  bswap_64 has its own gate (HAVE_BSWAP_64, endian.h:90) precisely
  because some byteswap.h variants lack it; when absent, ccan's inline
  fallback (validated in R1/R2) is used and cannot clash with a system
  definition because the gate implies none exists. Rejected.
- R10: Union type-punning in test/run.c. C11 explicitly permits reading
  a union member other than the last stored (6.5.2.3/3 + footnote);
  also test code, not production. Rejected.

## Auditor-added test files

None. No defect survived disproof, so no TAP regression test was added;
the module's test/ directory is untouched. All probes live in
/tmp/endian-asan/ (temporary).

No production files were modified (endian.h and _info untouched).
