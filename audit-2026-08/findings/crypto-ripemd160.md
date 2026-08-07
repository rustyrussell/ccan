# Audit: ccan/crypto/ripemd160

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: ripemd160.h, ripemd160.c, _info, ccan/crypto/ripemd160/test/
(run-test-vectors.c, run-lotsa-data.c, run-types.c). ~548 LOC:
open-coded RIPEMD-160 translated from Bitcoin Core's
src/crypto/ripemd160.cpp, plus an optional OpenSSL wrapper
(CCAN_CRYPTO_RIPEMD160_USE_OPENSSL, not the default) and u8/u16/u32/
u64/le/be typed-update helpers. Dependencies ccan/compiler (UNUSED/
inline) and ccan/endian (le32_to_cpu, cpu_to_le32/le64, CPU_TO_BE32) —
treated as given (ccan/endian already audited, endian.md); their use
here was checked for correctness. Host config.h: HAVE_BIG_ENDIAN 0,
HAVE_UNALIGNED_ACCESS 1, so alignment_ok() (ripemd160.c:262-269)
always returns true on this host and the memcpy fallback
(ripemd160.c:290-292) is dead code in this configuration.

## Mechanical checks

- ccanlint: **50/53**, every check PASS except tests_coverage
  (+3/6; the uncovered lines are the compiled-out OpenSSL wrapper and
  the HAVE_UNALIGNED_ACCESS==0 memcpy fallback, both unreachable in
  this configuration). tests_pass, tests_pass_without_features,
  tests_pass_valgrind, examples_compile, examples_run all PASS.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/crypto/ripemd160/ripemd160.c directly; linked with
  ccan/tap/tap.c only; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - run-test-vectors: **16/16 clean** (all official RIPEMD-160
    vectors incl. 1,000,000 x 'a', one-shot and streamed).
  - run-types: **1/1 clean**.
  - run-lotsa-data: **aborts** — UBSan alignment diagnostic at
    ripemd160.c:83 (see F1). With only the alignment check disabled
    (`-fno-sanitize=alignment`): **63/63 clean**, zero other
    diagnostics.
- -m32 (clang, ASan+UBSan): builds (no errno.h in this module's
  include closure) and passes — run-test-vectors 16/16,
  run-lotsa-data 63/63, run-types 1/1. The -m32 UBSan build does not
  flag the F1 misaligned load.
- OpenSSL variant (`-DCCAN_CRYPTO_RIPEMD160_USE_OPENSSL -lcrypto`,
  OpenSSL 3.x headers; deprecation warnings only): run-test-vectors
  **16/16 pass** — the RIPEMD160_INIT OpenSSL initializer
  (ripemd160.h:100-105) matches this OpenSSL's RIPEMD160_CTX layout.
- Differential check vs reference (/tmp/ripemd160-asan/repro-diff.c +
  actual.txt, temporary): 323 cases — lengths {0,1,2,3,54,55,56,57,
  63,64,65,118,119,120,127,128,129,200,255,256,300,511,512,513,1000,
  4096} x one-shot at buffer offsets {0,1,2,3,7} (exercises the
  misaligned direct-Transform path), two-way split updates at
  {0,1,55,56,63,64,65,119,120}, and three-way split updates — every
  digest compared against Python hashlib's OpenSSL-backed
  `'ripemd160'`: **323/323 match, zero mismatches**, under
  ASan+UBSan(-alignment) with zero sanitizer diagnostics. This covers
  every padding/finalization boundary (bytes%64 = 55/56/63/0/1) and
  the length-counter encoding.
- Optimization stress: gcc -O2 -fstrict-aliasing and clang -O2
  -fstrict-aliasing builds of all three tests: all pass — no observed
  miscompilation from the F1 aliasing/alignment UB on current
  compilers.
- Reproducers/probes in /tmp/ripemd160-asan/ (temporary, not
  committed): repro-diff.c, actual.txt, o2-* builds, m32-* builds,
  ossl build.

## Findings

## F1 — REJECTED (maintainer review: HAVE_UNALIGNED_ACCESS means "OK to break the ABI"; the configurator must simply be run with the same sanitizer flags as the build, which sets it to 0. Fix commit befab459 dropped from history): add() casts the caller's byte buffer to `const uint32_t *` and Transform() loads through it — misaligned-load UB (UBSan aborts the stock run-lotsa-data) and strict-aliasing UB even when aligned

- Location: ccan/crypto/ripemd160/ripemd160.c:286-293 (add), the cast
  at ripemd160.c:289:
  ```c
  if (alignment_ok(data, sizeof(uint32_t)))
      Transform(ctx->s, (const uint32_t *)data);
  ```
  and the loads at ripemd160.c:83-86 (Transform):
  ```c
  uint32_t w0 = le32_to_cpu(chunk[0]), ...  /* chunk[0..15] */
  ```
- Two distinct UB issues on this path:
  1. Alignment: with HAVE_UNALIGNED_ACCESS == 1 (this host),
     alignment_ok() always returns true (ripemd160.c:262-269), so any
     caller buffer whose address is not 4-aligned is read through a
     `uint32_t` lvalue — a misaligned load, UB per C17 6.3.2.3p7
     regardless of whether the hardware tolerates it. HAVE_UNALIGNED
     _ACCESS is a configurator EXECUTE probe (configurator.c:414-423)
     that only proves the *CPU* performs the access; it says nothing
     about the C abstract machine or what the compiler may assume.
  2. Aliasing: even when `data` happens to be aligned, it points at
     the caller's byte array (effective type e.g. `char[N]` or
     `unsigned char[N]`, or a malloc'd region whose effective type was
     set by byte writes). Reading it through a `uint32_t` lvalue
     violates C17 6.5p7. This half is independent of
     HAVE_UNALIGNED_ACCESS and also applies on platforms where
     alignment_ok() genuinely checked alignment.
- Reachable path: completely ordinary use — the module's own stock
  test does it: run-lotsa-data.c:17 calls
  `ripemd160(&h, zeroes + i, sizeof(zeroes) - 64)` for i = 1..63 over
  a 936-byte message; add() takes the direct-chunk loop (len >= 64)
  and Transform loads `chunk[0]` at zeroes+i for odd i. Any external
  caller hashing >64 bytes from an unaligned pointer (network buffer
  offset, packed struct member) hits the same path.
- Caller preconditions: none violated — ripemd160_update documents
  "@p: pointer to memory, @size: the number of bytes" (ripemd160.h:
  113-121); no alignment precondition is stated anywhere.
- Observed output (clang 18, ASan+UBSan, -fno-sanitize-recover=all,
  stock test run-lotsa-data):
  ```
  ccan/crypto/ripemd160/ripemd160.c:83:31: runtime error: load of misaligned address
  0x...361 for type 'const uint32_t' (aka 'const unsigned int'), which requires 4 byte alignment
  SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior ccan/crypto/ripemd160/ripemd160.c:83:31
  timeout: the monitored command dumped core
  ```
  i.e. the module cannot run its own test suite clean under UBSan in
  its shipped configuration.
- Concrete incorrect consequence: none observed on output — all
  323 differential cases and all stock vectors produce correct
  digests on x86-64 and -m32, and -O2 -fstrict-aliasing builds of
  gcc and clang do not miscompile it. The consequence today is the
  formal UB itself plus the UBSan abort; the risk is a future/edge
  compiler exploiting the alignment or aliasing assumption (e.g.
  vectorized widening of the `chunk[]` loads).
- Why LIKELY rather than CONFIRMED: no wrong result or memory error
  is demonstrable on any supported platform; the defect is real UB in
  production code reached by a stock test, with an observed sanitizer
  abort, but without a demonstrated incorrect hash. Why not REJECTED:
  unlike pure formal-UB candidates, this one has observed tool output
  aborting the module's own regression test, and the fix is free
  (compilers elide the bounce-copy on unaligned-tolerant targets).
- Reproducer: the stock test suffices —
  `clang -fsanitize=undefined -fno-sanitize-recover=all -I. \
     ccan/crypto/ripemd160/test/run-lotsa-data.c ccan/tap/tap.c`
  aborts at iteration i=1. No auditor TAP test was added: the defect
  only manifests as a sanitizer diagnostic, so a plain-build TAP test
  cannot fail and would add noise (see "Auditor-added test files").
- Repair direction: stop forming a `uint32_t *` into caller memory.
  Either (a) in add(), drop the alignment_ok branch and always
  `memcpy(ctx->buf.u8, data, 64); Transform(ctx->s, ctx->buf.u32);`
  (ctx->buf is a union member, so both alignment and effective type
  are fine), or (b) change Transform to take `const unsigned char *`
  and load each word with a memcpy-based little-endian decode (as
  ccan/endian's HAVE_UNALIGNED_ACCESS==0 fallbacks do). GCC/clang
  turn the 64-byte memcpy into direct loads on x86, so there is no
  performance loss and HAVE_UNALIGNED_ACCESS can stop being consulted
  here. Note: ccan/crypto/sha256 has the identical construct
  (sha256.c:196-197 cast + Transform loads) and needs the same fix;
  worth a cross-module follow-up by the orchestrator.

## Rejected candidates (disproved)

- R1 (brief suspect: padding formula): ripemd160_done adds
  `1 + ((119 - (ctx->bytes % 64)) % 64)` pad bytes (ripemd160.c:326).
  For b = bytes%64 in [0,63], (119 - b) % 64 == (55 - b) % 64, so the
  pad length is the standard `((56 - b - 1) mod 64) + 1` in [1,64];
  the 64-byte pad array (ripemd160.c:320) is never over-read, and the
  total (pad + 8-byte length) always lands exactly on a block
  boundary. All boundary lengths (b = 54..57, 62..65, 118..121,
  126..129) verified against hashlib in the 323-case differential run.
  Rejected.
- R2 (brief suspect: length counter): sizedesc =
  cpu_to_le64(ctx->bytes << 3) is computed *before* the pad/length
  add() calls mutate ctx->bytes (ripemd160.c:324 vs 326-328) —
  correct. RIPEMD-160 specifies the length as a little-endian
  bit count mod 2^64; `bytes << 3` wraps exactly as specified.
  Verified by all vectors including 1,000,000-byte and 4096-byte
  cases. Rejected.
- R3 (brief suspect: finalization boundaries / streaming split
  invariance): two-way splits at {0,1,55,56,63,64,65,119,120} and
  three-way splits for 26 lengths all match one-shot hashlib
  digests; add()'s buffer-fill-then-direct-chunks logic
  (ripemd160.c:271-304) is correct, including the bufsize==0 &&
  len>=64 case where the first chunk is needlessly bounced through
  ctx->buf (inefficiency, not a defect). Rejected.
- R4 (brief suspect: endianness): Transform decodes message words
  with le32_to_cpu and the digest is emitted with cpu_to_le32
  (ripemd160.c:330); RIPEMD-160 is little-endian throughout, unlike
  SHA-256. Correct for this LE host by test; correct for BE hosts by
  inspection given ccan/endian's documented behavior (dependency,
  treated as given; BE not testable on this host). The comment at
  ripemd160.c:327 says "big endian" — stale, copied from sha256.c;
  the code is right. Comment nit only, not a code defect. Rejected.
- R5: rol(x, i) shift-by-32/0 UB (ripemd160.c:58): every rotation
  constant in the 160 Round invocations is in [5,15] (verified by
  reading all R11..R52 call sites); i is never 0 or 32. Rejected.
- R6: use of a destroyed context (after ripemd160_done) traps only
  via assert (check_ripemd160, ripemd160.c:25-32) and is silent under
  NDEBUG: the header documents "@ctx is *destroyed* by this, and must
  be reinitialized" (ripemd160.h:128-129) — documented precondition.
  Additionally verified by inspection that even NDEBUG misuse stays
  memory-safe: bytes == -1ULL gives bufsize = 63, and add()'s
  arithmetic neither over-reads the pad array nor overflows buf.
  Rejected.
- R7: ripemd160_update(ctx, NULL, 0) / ripemd160(&h, NULL, 0): add()
  with len 0 never dereferences p (the bufsize+len>=64 branch
  requires len >= 1 since bufsize < 64; the final memcpy is guarded
  by `if (len)`). Rejected.
- R8: `bufsize + len >= 64` overflow (ripemd160.c:276): bufsize <= 63
  (it is bytes % 64), so no size_t overflow is possible. Rejected.
- R9: RIPEMD160_INIT union/struct initializer (ripemd160.h:107-109)
  leaves buf zero-initialized; static-init contexts behave identically
  to ripemd160_init (ripemd160.c:306-310 assigns a RIPEMD160_INIT
  temporary) — exercised by run-test-vectors' streamed path
  (RIPEMD160_INIT) vs one-shot path (ripemd160_init), 16/16. The
  OpenSSL-variant initializer was verified by actually building and
  passing all 16 vectors against OpenSSL 3.x. Rejected.
- R10: double ripemd160_done / ripemd160_done on a fresh context:
  fresh-context done is the empty-message vector (passes); double
  done violates the documented "destroyed" contract (see R6) and
  stays memory-safe anyway. Rejected.
- R11: struct ripemd160 union punning (res->u.u32 written,
  res->u.u8 read, ripemd160.c:329-330; test reads h.u.u8): union
  type-punning is defined in C (C17 6.5.2.3 footnote); no defect.
  Rejected.
- R12: dead memcpy fallback (ripemd160.c:290-292) when
  HAVE_UNALIGNED_ACCESS == 1: unreachable in this configuration,
  correct by inspection where reachable (copies into ctx->buf, then
  Transform on the union member — the shape F1's fix should take
  everywhere). Coverage gap only. Rejected.
- R13: -m32 concerns: module has no word-size-sensitive arithmetic
  (all counters uint64_t, all transforms uint32_t); -m32 ASan+UBSan
  builds pass all three tests. Rejected.

## Auditor-added test files

None. The single retained finding (F1) manifests only as a UBSan
alignment diagnostic; the module's own stock test run-lotsa-data.c
is already the minimal reproducer under
`clang -fsanitize=undefined -fno-sanitize-recover=all`, and a TAP
test cannot fail in a plain build (all digests are correct), so no
temporary test was added to ccan/crypto/ripemd160/test/. All probes
live in /tmp/ripemd160-asan/ (repro-diff.c + actual.txt, the
323-case differential harness against hashlib's reference
implementation).

No production files were modified (ripemd160.h, ripemd160.c, _info
untouched; verified by git status — no new or changed paths under
ccan/crypto/ripemd160).
