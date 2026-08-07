# Audit: ccan/crypto/sha256

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: sha256.h (147 lines), sha256.c (308 lines), _info,
ccan/crypto/sha256/test/ (4 run tests). OpenSSL variant
(CCAN_CRYPTO_SHA256_USE_OPENSSL) is commented out and was not
compiled; only the open-coded Bitcoin-derived implementation was
audited (plus the thin sha256_u*/le*/be* wrappers, which are shared).
Deps ccan/compiler (attributes only) and ccan/endian (cpu_to_be*/
be*_to_cpu inlines) treated as given; test dep ccan/str/hex.
config.h here: HAVE_BIG_ENDIAN=0, HAVE_UNALIGNED_ACCESS=1. The
benchmarks/ dir (asm files from upstream Bitcoin) is not built by the
module and was out of scope.

## Mechanical checks

- ccanlint (before auditor additions): **52/56**, every check PASS;
  only tests_coverage partial (+2/6).
- After adding the two auditor regression tests: ccanlint **58/62**,
  every check still PASS (both new tests pass on this LP64 host in
  plain builds; run-length-wrap fails only on ILP32, see F2).
- Existing tests under clang 18 ASan+UBSan (`-fsanitize=address,undefined
  -fno-sanitize=function -fno-sanitize-recover=all`, -g, -I.; each test
  #includes ccan/crypto/sha256/sha256.c directly; run-test-vectors also
  linked ccan/str/hex/hex.c + ccan/str/str.c; all linked ccan/tap/tap.c;
  per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  - run-33-bit-test: **1/1 clean**.
  - run-test-vectors: **5/5 clean** (empty, "abc", 56-byte,
    112-byte vectors + 10^6 x 'a' — all match published values).
  - run-types: **1/1 clean**.
  - run-lotsa-data: **ABORTS** — UBSan "load of misaligned address ...
    for type 'const uint32_t', which requires 4 byte alignment" at
    sha256.c:92:63, reached via sha256.c:197 (see F1). The module's own
    alignment test cannot pass under UBSan.
- Fallback-forced build (fakeconfig config.h with
  HAVE_UNALIGNED_ACCESS 0, /tmp/sha256-asan/fakeconfig, -I before -I.),
  all four tests under ASan+UBSan: **all pass, zero diagnostics** —
  the memcpy path in add() is correct and the UBSan failure is solely
  the unaligned direct-cast path.
- -m32: toolchain works on this host for this module (no errno.h in
  the include chain). All four stock tests build and pass -m32
  (plain, -O1): run-33-bit-test 1/1, run-lotsa-data 63/63,
  run-test-vectors 5/5, run-types 1/1.
- Boundary/differential repro (/tmp/sha256-asan/repro-boundary.c,
  ASan+UBSan): 38 boundary-focused lengths (0..3, 7..9, 31..33,
  52..58, 62..66, 116..122, 126..130, 191..193, 1000 — covering the
  55/56/64-byte finalization edges and multi-block padding), each at
  buffer offsets 0..7, every two-part split point plus three-part
  splits at 1/55/56/64/65: **all digests match a Python hashlib
  reference and chunked updates are self-consistent with one-shot**;
  the only sanitizer output is the F1 misaligned loads.
- 4 GiB differential (/tmp/sha256-asan/repro-4gib-wrap.c, hashing
  2^32 zero bytes in 4 MiB updates, ~10 s per run): LP64 build
  **matches Python hashlib exactly** (8479e439...); -m32 build
  diverges (c08a9f2a...) — see F2. Control at 2^32 - 64 bytes: LP64
  and -m32 agree (8d77f0a5...).
- Reproducers/probes in /tmp/sha256-asan/ (temporary, not committed):
  repro-4gib-wrap.c (wrap64/wrap32), repro-seeded-wrap.c
  (seeded64/seeded32), repro-boundary.c, fakeconfig/config.h,
  m32-* builds.

## Findings

## F1 — REJECTED (maintainer review: HAVE_UNALIGNED_ACCESS means "OK to break the ABI"; the configurator must simply be run with the same sanitizer flags as the build, which sets it to 0. Fix commit a07ba2fc dropped from history): add() casts arbitrarily-aligned caller data to `const uint32_t *` and Transform dereferences it — undefined behavior on the primary API path whenever HAVE_UNALIGNED_ACCESS=1

- Location: ccan/crypto/sha256/sha256.c:194-201, function add:
  ```c
  while (len >= 64) {
      if (alignment_ok(data, sizeof(uint32_t)))
          Transform(ctx->s, (const uint32_t *)data);
  ```
  with the dereference at sha256.c:92 (`w0 = be32_to_cpu(chunk[0])`
  and the 15 following chunk reads in Transform). alignment_ok
  (sha256.c:170-177) returns true unconditionally when
  HAVE_UNALIGNED_ACCESS is 1 — and config.h sets it to 1 on this host
  (and any x86/ARM64 Linux), because the configurator probe
  (tools/configurator/configurator.c:413-421) only checks that the
  *hardware* survives a misaligned int load, not that the C is defined.
- Caller preconditions: none violated. sha256_update/sha256 take
  `const void *p` (sha256.h:112-120, :29-38) and document no alignment
  requirement; passing a misaligned buffer is perfectly legal C for
  the caller. The module's own run-lotsa-data test exists precisely to
  hash buffers at offsets 1..63.
- Reachable path: any update whose current block boundary falls on a
  non-uint32-aligned address — one-shot sha256(&h, buf+1, n>=64), or
  chunked updates where an update starts misaligned with >= 64 bytes
  buffered-free. Concretely: run-lotsa-data hashes `zeroes + i` (936
  bytes) for i in 1..63; offsets 1,2,3 mod 4 take the cast path.
- Concrete incorrect consequence, observed: the load is UB twice over
  — C17 6.3.2.3/7 (pointer conversion to a more-aligned type whose
  requirement is not met) and 6.5/7 (reading char-array storage
  through a uint32_t lvalue violates effective-type rules; the memcpy
  fallback exists precisely to avoid this, but is compiled out whenever
  HAVE_UNALIGNED_ACCESS=1). Observed output, stock test suite, clang 18
  `-fsanitize=undefined` (LP64):
  ```
  ccan/crypto/sha256/sha256.c:92:63: runtime error: load of misaligned
  address 0x5d425fa35401 for type 'const uint32_t' (aka 'const unsigned
  int'), which requires 4 byte alignment
  ```
  run-lotsa-data aborts at subtest 1 of 63; the auditor-added
  run-unaligned-chunks aborts identically. Practical impact: results
  are correct on x86/ARM64 today (verified: digests at all offsets
  match hashlib), but (a) any consumer running UBSan over code that
  hashes unaligned buffers gets an abort from this module — including
  this module's own test suite; (b) the compiler is licensed to assume
  uint32_t alignment of `data` (vectorized/widened loads such as movdqa
  would fault; gcc has performed exactly this transform on similar
  Bitcoin-derived code); (c) strict-aliasing-based optimization may
  reorder the char writes vs uint32_t reads. This is a portability/UB
  defect on the documented primary path, not a wrong-result-on-x86
  defect.
- Regression test: ccan/crypto/sha256/test/run-unaligned-chunks.c
  (alarm(10)-bounded, 8 subtests; deliberately misaligned chunked and
  one-shot updates compared against aligned one-shot references).
  Passes 8/8 in plain LP64 and -m32 builds today (hardware tolerance),
  aborts under ASan+UBSan today at sha256.c:92; after repair it must
  pass in both.
- Repair direction: make the direct-Transform path require actual
  alignment regardless of HAVE_UNALIGNED_ACCESS, e.g. drop the
  `#if HAVE_UNALIGNED_ACCESS` special case in alignment_ok so the
  memcpy fallback is always used for misaligned data
  (`return ((size_t)p % n == 0);`), or always memcpy the block into
  ctx->buf and let the compiler optimize (it recognizes the idiom).
  Keep HAVE_UNALIGNED_ACCESS, if at all, only as a hint that cannot
  bypass C alignment rules.

## F2 — CONFIRMED (FIXED in 1c377ffb): struct sha256_ctx.bytes is size_t, so on ILP32 platforms the byte counter wraps at 2^32 and every message of 4 GiB or more gets a wrong digest

- Location: ccan/crypto/sha256/sha256.h:52 (`size_t bytes;` in
  struct sha256_ctx), consumed at sha256.c:232:
  ```c
  sizedesc = cpu_to_be64((uint64_t)ctx->bytes << 3);
  ```
  The counter is incremented in add() (sha256.c:187, :202, :210) and
  wraps mod 2^32 when size_t is 32-bit.
- Caller preconditions: none violated. SHA-256 (FIPS 180-4) admits
  messages up to 2^64 - 1 bits; 4 GiB is 2^35 bits, an entirely valid
  message, and the header documents no length limit. Streaming
  sha256_update of >= 4 GiB total is the normal way to hash large
  files — precisely what a streaming context exists for.
- Reachable path: sha256_init; sha256_update calls totalling >= 2^32
  bytes; sha256_done. ctx->bytes wraps to (total mod 2^32), so the
  length field emits (total mod 2^32) * 8 instead of total * 8 mod
  2^64. (The padding *position* stays right, since 64 divides 2^32 —
  only the length field is wrong.)
- Concrete incorrect consequence, observed (/tmp/sha256-asan/
  repro-4gib-wrap.c, hashing 2^32 zero bytes via 4 MiB updates):
  ```
  size_t=64 bits: 8479e43911dc45e89f934fe48d01297e16f51d17aa561d4d1c216b1ae0fcddca
  size_t=32 bits: c08a9f2a1f6411dbb2ff8d434e7866203ebcc9146dd041ebe33d05c5b87c1648
  python hashlib: 8479e43911dc45e89f934fe48d01297e16f51d17aa561d4d1c216b1ae0fcddca
  ```
  The -m32 digest is wrong; LP64 matches the reference. Control at
  2^32 - 64 bytes: both builds agree (8d77f0a5...), isolating the wrap
  as the cause. In a digest used for integrity or content addressing,
  a wrong hash for >= 4 GiB inputs on 32-bit builds is a silent
  correctness failure (and cross-platform digest mismatch).
- Independent verification of the regression-test constant: a
  self-contained Python SHA-256 (self-checked against hashlib on the
  same block) with the length field forced to 2^35 bits reproduces the
  LP64 seeded-context digest fc48521c... exactly; the -m32 build
  produces 67b0b9d5... (length field 0 after wrap). See
  /tmp/sha256-asan/repro-seeded-wrap.c.
- Why this is not the "2^61 bytes" suspect: on LP64 the
  `(uint64_t)bytes << 3` shift wraps at 2^61 bytes, but that wrap is
  mod 2^64 and therefore *consistent* with the SHA-256 length field
  (verified live: the 2^32-byte LP64 digest matches hashlib); the
  genuine defect is the ILP32 size_t counter, which wraps 2^29 bits
  too early.
- Regression test: ccan/crypto/sha256/test/run-length-wrap.c
  (alarm(10)-bounded, 1 subtest; seeds ctx.bytes = 2^32 - 64 in the
  style of the existing run-33-bit-test.c, updates 64 known bytes,
  expects the independently-derived digest). Passes LP64 (plain and
  ASan+UBSan) today; **fails 1/1 in a plain -m32 build today**;
  after repair it must pass on both.
- Repair direction: make the counter fixed-width, e.g.
  `uint64_t bytes;` in struct sha256_ctx (this also matches upstream
  Bitcoin's CSHA256, which uses uint64_t). Note SHA256_INIT's
  initializer already zero-fills it, so the public initializer needs
  no change; the struct size changes, so this is an ABI bump for any
  precompiled consumer.

## Rejected candidates (disproved)

- R1: bit-length counter overflow at 2^61 bytes on LP64
  (sha256.c:232): `(uint64_t)bytes << 3` wraps mod 2^64, which is
  exactly the SHA-256 length-field semantics (message length mod 2^64
  bits; and >= 2^64 bits is outside FIPS 180-4 anyway). Buffer position
  bytes % 64 also stays consistent because 64 | 2^64. Verified
  empirically: the LP64 digest of 2^32 zero bytes (which exercises the
  << 3 path past 2^35 bits) matches Python hashlib bit-for-bit. The
  real overflow is F2 on ILP32. Rejected for LP64.
- R2: padding length formula in sha256_done (sha256.c:234),
  `1 + ((128 - 8 - (ctx->bytes % 64) - 1) % 64)`: all-unsigned size_t
  arithmetic, no underflow (128 - 8 - r - 1 >= 56 for r <= 63);
  algebraically equal to the standard `(56 - r) mod 64 + 1` pad for
  every r in 0..63 (for r <= 55: 119 - r - 64 = 55 - r; for r >= 56:
  119 - r itself), landing the total on a block boundary in both
  cases. Empirically confirmed by the boundary differential repro at
  every bytes%64 in 52..66 and 116..130 against hashlib. sizedesc is
  computed before the padding add() calls, so the pre-padding count is
  used, as required. Rejected.
- R3: finalization edge cases (exactly 55/56/64-byte messages and
  chunked equivalents): covered exhaustively by repro-boundary.c —
  lengths 52..58, 62..66, 116..122, 126..130 at offsets 0..7, every
  two-part split point, three-part splits at 1/55/56/64/65; all
  digests match hashlib and are split-invariant. No defect. Rejected.
- R4: endianness of the compression function and output: Transform
  converts every message word with be32_to_cpu (sha256.c:92-107) and
  sha256_done emits cpu_to_be32 per state word (sha256.c:237-238);
  run-types verifies the u*/le*/be* helpers byte-for-byte against a
  raw buffer on both endiannesses (HAVE_BIG_ENDIAN variants compiled
  per config). Only little-endian was executed here, but the code path
  is symmetric through ccan/endian (treated as given). Rejected.
- R5: add() with bufsize + len overflow (sha256.c:184): if len were
  within 63 bytes of SIZE_MAX the sum could wrap and skip the
  flush-buffered-block branch, hashing the buffered prefix out of
  order. Unreachable: it requires a single update of ~2^64 (LP64) or
  ~2^32 (ILP32, exceeding the address space) contiguous bytes. No
  realistic caller; speculative. Rejected.
- R6: sha256_update(ctx, NULL, 0) / sha256(&h, NULL, 0): add() never
  dereferences p when len is 0 (first branch needs len >= 64 -
  bufsize, while needs len >= 64, memcpy is guarded by `if (len)`).
  Traced for bufsize 0 and nonzero; safe. Empty-message digest matches
  the published vector (run-test-vectors subtest 1). Rejected.
- R7: use-after-sha256_done: ctx is invalidated (bytes = (size_t)-1)
  and sha256_update asserts via check_sha256 — but only without
  NDEBUG. Calling update/done on a finished context violates the
  documented "@ctx is *destroyed* by this, and must be reinitialized"
  (sha256.h:127). Documented precondition; rejected.
- R8: alignment_ok casting `const void *` to size_t (sha256.c:175):
  implementation-defined, not UB; universal idiom, and only used as a
  modulus test. Rejected.
- R9: reading ctx->buf.u32 after memcpy into ctx->buf.u8
  (sha256.c:190, :200): union member reinterpretation after a store
  through another member is defined in C (6.5.2.3 footnote; gcc/clang
  document it). buf is a proper union member of the context, correctly
  aligned for uint32_t. Rejected.
- R10: run-33-bit-test.c seeds after_16M_by_64 "produced by actually
  running the code on x86": circular-looking, but the test's final
  digest equals the published SHA-3-program 16777216-repetition
  SHA-256 vector, so the seeded state is externally anchored. Not a
  defect (test artifact note only).
- R11: OpenSSL variant (sha256.c:34-50): not compiled here (macro
  commented out in sha256.h:9); SHA256_Init/Update/Final are
  deprecated in OpenSSL 3 but that path is inactive and unaudited.
  Out of scope; noted for follow-up only.
- R12: -m32 functional equivalence below the wrap: all stock tests
  pass -m32, and the 2^32 - 64 control digest matches LP64, so there
  is no ILP32 defect other than F2. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/crypto/sha256/test/run-unaligned-chunks.c — proves F1.
  Passes 8/8 in plain LP64 and -m32 builds (x86 hardware tolerance);
  aborts under clang 18 ASan+UBSan at sha256.c:92 (load of misaligned
  address for type const uint32_t) on the first chunked misaligned
  update; alarm(10)-bounded. After repairing F1 it must pass in both
  plain and sanitizer builds.
- ccan/crypto/sha256/test/run-length-wrap.c — proves F2. Passes on
  LP64 (plain and ASan+UBSan); fails 1/1 in a plain -m32 build against
  the current code (digest 67b0b9d5... instead of the
  reference-verified fc48521c...); alarm(10)-bounded. After repairing
  F2 it must pass on both.
- ccanlint with both files present: 58/62, all checks PASS (the new
  tests pass on this LP64 host; run-length-wrap only fails on ILP32,
  which ccanlint does not exercise here).

No production files were modified (sha256.h, sha256.c, _info
untouched; verified by git status — the only new paths under
ccan/crypto/sha256 are the two tests above).
