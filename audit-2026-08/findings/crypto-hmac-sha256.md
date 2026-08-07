# Audit: ccan/crypto/hmac_sha256

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: hmac_sha256.h (86 lines), hmac_sha256.c (161 lines, of which
lines 105-160 are `#if 0`-style dead `#else` code), _info,
ccan/crypto/hmac_sha256/test/api-rfc4231.c. Sole dependency:
ccan/crypto/sha256 (testdepends: ccan/str/hex). sha256 was read in
full to verify the two contract points hmac_sha256 relies on:
sha256_update copies its input (sha256.c add(), sha256.c:179-212 —
data is memcpy'd into ctx->buf or consumed by Transform; no pointer
retained), and sha256_update(ctx, NULL, 0) is a no-op (all three
branches of add() skip len == 0).

Brief suspects were: key longer than block size handling, ipad/opad
construction, key length 0; verification against RFC 4231 test vectors.

## Mechanical checks

- ccanlint: **39/39**, all checks pass (tests_pass,
  tests_pass_valgrind, examples_compile, examples_run all PASS).
- Existing test api-rfc4231.c under clang 18.1.3 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function
  -fno-sanitize-recover=all -g -I.`, linked with hmac_sha256.c,
  sha256.c, str/hex/hex.c, str/str.c, tap/tap.c; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **12/12 ok, rc=0, zero sanitizer diagnostics** — RFC 4231 test cases
  1-4, 6, 7, each via the one-shot and the split-update APIs.
- Same build with `-m32`: **12/12 ok, rc=0** under ASan+UBSan (this
  module's include chain does not hit the missing 32-bit errno.h
  headers). No word-size-sensitive arithmetic in the module.
- Edge-case reproducer (/tmp/hmac-asan/repro-edge.c, temporary):
  empty key (NULL, 0) with empty data and with "abc", key exactly 64
  bytes, key 65 bytes, incremental update with zero-length and empty
  updates interleaved — all match Python3 hmac/hashlib reference
  values (vectors generated with python3, not hand-written; two of my
  initial hand-written guesses were wrong and were corrected against
  the python output before use).
- Differential fuzzer (/tmp/hmac-asan/repro-diff.c, temporary,
  ASan+UBSan): 5000 pseudo-random cases, key and data lengths drawn
  from {0,1,2,31,32,33,63,64,65,66,127,128,129,200} exhaustively for
  the first 28 cases and randomly (0..200) thereafter; each case also
  cross-checks the incremental API with a random split point against
  the one-shot result internally. Output verified line-by-line against
  python3 hmac.new(key, data, sha256): **5000/5000 match, 0
  incremental mismatches, zero sanitizer diagnostics**.
- Reproducers/probes in /tmp/hmac-asan/ (temporary, not committed):
  repro-edge.c, repro-diff.c, cases.txt.

## Findings

None. All three brief suspects were verified correct and every other
candidate was disproved; see below. The implementation is a faithful
RFC 2104/RFC 4231 HMAC: keys longer than the 64-byte block are hashed
first (hmac_sha256.c:26-30, strict `>` matching the RFC), shorter keys
are zero-padded to 64 bytes (:38-40), ipad and opad are applied by
full-block XOR (:46, :62), and the two-pass structure matches the RFC.

## Rejected candidates (disproved)

- R1 (brief suspect): key longer than block size. hmac_sha256.c:26
  uses `ksize > HMAC_SHA256_BLOCKSIZE` — exactly 64-byte keys are used
  verbatim (RFC: "if the length of K is B: the key is used as-is" via
  zero-padding of 0 bytes), 65+-byte keys are replaced by
  SHA256(key) (32 bytes). Verified by repro-edge key-64/key-65 cases
  and by the differential fuzz (ksize 63/64/65/66 and 127/128/129 all
  match python). RFC 4231 cases 6/7 (131-byte keys) pass in-tree.
  Rejected.
- R2 (brief suspect): ipad/opad construction. k_ipad aliases
  ctx->k_opad (hmac_sha256.c:23); the block is memcpy'd+zero-padded,
  XORed with IPAD, fed to sha256_update, then converted in place to
  opad by XOR with IPAD^OPAD (:62). This is only sound because
  sha256_update does not retain the input pointer — verified by
  reading sha256.c add() (:179-212): input is memcpy'd into ctx->buf
  or consumed immediately by Transform. xor_block's uint64_t XOR with
  0x3636.../0x5C5C... constants XORs every byte with 0x36/0x5C
  independently of host endianness (the key bytes were memcpy'd into
  the uint64_t array, so byte order inside each word is irrelevant to
  a uniform-byte pad). Correctness confirmed end-to-end by 12/12 RFC
  4231 vectors and the 5000-case differential fuzz. Rejected.
- R3 (brief suspect): key length 0. hmac_sha256.c:38 guards the
  memcpy with `if (ksize != 0)`, so hmac_sha256(hmac, NULL, 0, ...)
  performs no memcpy(NULL) at all and memset pads the full 64 bytes —
  HMAC with an all-zero-block key, which is exactly what HMAC of an
  empty key must be. Verified: (NULL, 0, NULL, 0) and (NULL, 0,
  "abc", 3) match python hmac.new(b"", ...) reference values, clean
  under ASan+UBSan. Rejected.
- R4: `memset((char *)k_ipad + ksize, 0, HMAC_SHA256_BLOCKSIZE -
  ksize)` (hmac_sha256.c:40) underflow: ksize <= 64 on all paths (the
  >64 branch resets ksize to sizeof(struct sha256) == 32), so the
  length is in [0, 32+... no underflow possible]. Rejected.
- R5: alignment/strict-aliasing of the uint64_t block: ctx->k_opad is
  a uint64_t[8] struct member, correctly aligned for uint64_t;
  memcpy into it from arbitrary `const void *k` and subsequent
  uint64_t stores give it effective type uint64_t — no aliasing
  violation; BLOCK_U64S = 64/8 = 8 exact, no tail bytes. Rejected.
- R6: hmac_sha256_done (hmac_sha256.c:76-92) reuses ctx->sha after
  sha256_done invalidated it (sha256.c:21 bytes = (size_t)-1):
  hmac_sha256_done calls sha256_init (:88) before any further update,
  so the check_sha256 assert path is never tripped in normal use.
  Rejected.
- R7: hmac_sha256_update(ctx, NULL, 0) / zero-length data: sha256's
  add() performs no memory access when len == 0 (all branches skip),
  so NULL/0 updates are safe; exercised in repro-edge (incremental
  case with NULL,0 and "",0 updates) and by the fuzzer (dsize 0
  cases). Rejected.
- R8: update after done / double done: only assert-guarded
  (check_sha256, compiled out under NDEBUG). Documented precondition:
  hmac_sha256.h:82-83 "Note that @ctx is *destroyed* by this, and
  must be reinitialized." Rejected.
- R9: struct hmac_sha256_ctx member-wise copy ("pass a copy instead",
  hmac_sha256.h:83): sha256_ctx and k_opad are plain arrays with no
  internal pointers; a struct copy is a valid independent context.
  Exercised implicitly by api-rfc4231's two-phase test pattern.
  Rejected.
- R10: caller aliasing (k pointing into ctx->k_opad, or d overlapping
  hmac output): no documentation permits it; caller precondition
  violation. Rejected.
- R11: key material left in ctx->k_opad after hmac_sha256_done (not
  zeroized): speculative hardening, excluded by AGENTS.md; the
  struct's lifetime is caller-owned stack/heap memory. Noted, not
  reported.
- R12: dead `#else` branch (hmac_sha256.c:105-160, RFC2104 MD5-example
  mapping using bzero/bcopy): never compiled (`#if 1`); also its
  bcopy(key, k_ipad, key_len) would overflow the 65-byte pads for
  key_len > 64 — but it is dead code, and per AGENTS.md unreachable
  paths are out of scope. Noted for cleanup; rejected.

## Auditor-added test files

None. No defects were found, so no regression tests were added; the
module's own api-rfc4231.c plus the temporary /tmp reproducers cover
the brief's suspects. Reproducers remain in /tmp/hmac-asan/
(repro-edge.c, repro-diff.c) if a permanent edge-case test (empty
key, key length exactly 64, key length 65) is desired — the current
in-tree test has no empty-key or exact-blocksize-key case, a coverage
gap only.

No production files were modified (hmac_sha256.h, hmac_sha256.c,
_info untouched; verified by git status — no new or changed paths
under ccan/crypto/hmac_sha256).
