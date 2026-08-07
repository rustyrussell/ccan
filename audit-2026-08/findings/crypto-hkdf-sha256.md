# Audit: ccan/crypto/hkdf_sha256

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: hkdf_sha256.h (22 lines), hkdf_sha256.c (97 lines), _info,
ccan/crypto/hkdf_sha256/test/ (api-rfc5869.c only). ~119 LOC implementing
RFC 5869 HKDF-SHA256 extract+expand in a single function. Dependency
ccan/crypto/hmac_sha256 (and transitively ccan/crypto/sha256) treated as
given; verified specifically for this audit: hmac_sha256_init guards its
key memcpy with `if (ksize != 0)` (hmac_sha256.c:38), and sha256's add()
never dereferences p when len == 0 (sha256.c:182-209: bufsize < 64 always,
`while (len >= 64)` and `if (len)` both false), so NULL/0-length salt,
key and info propagate safely through the whole chain. testdepends
ccan/str/hex used by the api test only.

Documented contract (_info): "This code implements the hkdf described in
RFC5869." Header (hkdf_sha256.h:10): "@okm_size: the number of bytes
pointed to by @okm (must be less than 255*32)". RFC 5869 section 2.3:
"L  length of output keying material in octets (<= 255*HashLen)" — i.e.
L may be as large as 255*32 = 8160 inclusive.

## Mechanical checks

- ccanlint (before auditor additions): **34/36**, every check PASS;
  shortfall is only examples_exist (+0/2, no Example: section in _info
  or the header).
- After adding the auditor regression test: ccanlint **32/38 FAIL** —
  exactly as designed: tests_pass FAIL (+1/2) because run-max-okm aborts
  on the hkdf_sha256.c:16 assertion against the current code;
  tests_compile passes for it.
- Existing test under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; linked with hkdf_sha256.c,
  hmac_sha256.c, sha256.c, str/hex/hex.c, tap.c; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  api-rfc5869.c: **3/3 clean**, zero sanitizer diagnostics (RFC 5869
  Appendix A test cases 1-3 all verified byte-exact, including the
  zero-length salt/info case).
- -m32: **available for this module** (no errno.h chain): api-rfc5869
  builds and passes 3/3 under clang -m32.
- Independent cross-check: full 8160-octet HKDF output for RFC Test
  Case 1 parameters regenerated with Python hmac/hashlib
  (/tmp/hkdf-asan/ref-8160.bin); the module's output at L=8160 (NDEBUG
  build, bypassing the assert) matches all 8160 bytes
  (/tmp/hkdf-asan/repro-max-l.c: "output matches independent RFC5869
  reference").
- Reproducers/probes in /tmp/hkdf-asan/ (temporary, not committed):
  repro-max-l.c (+diag.c), repro-null-empty.c, repro-null2.c,
  ref-8160.bin, shadow/ (patched copy used to validate the regression
  test). Note: two early iterations of repro-max-l.c contained auditor
  harness bugs (a 21-pair and then a 23-pair IKM hex literal, the
  latter overflowing the 22-byte ikm array — ASan stack-buffer-overflow
  in the *reproducer's* fromhex, not the module); fixed before any
  conclusion was drawn.

## Findings

## F1 — CONFIRMED (FIXED in 344ba97c): the okm_size cap is off by one vs RFC 5869 — L = 255*HashLen = 8160 is legal but assert-rejected

- Location: ccan/crypto/hkdf_sha256/hkdf_sha256.c:16, function
  hkdf_sha256:
  ```c
  assert(okm_size < 255 * sizeof(t));
  ```
  plus the matching bound in the header documentation
  (hkdf_sha256.h:10, "must be less than 255*32").
- Documented contract: _info states the module "implements the hkdf
  described in RFC5869"; RFC 5869 section 2.3 allows
  L <= 255*HashLen = 8160 (quoted in the function's own comment block,
  hkdf_sha256.c:56-57). The assert and the header both implement
  L < 8160, rejecting the single largest legal value.
- Reachable path: any caller requesting the RFC-maximal 8160 octets of
  OKM, e.g. a protocol or test harness that derives
  255*HashLen bytes as the RFC permits.
- Why the implementation itself is correct at L=8160 (traced and
  measured): T(1) is computed before the loop with c=1; loop iteration
  k copies T(k) and computes T(k+1) with c=k+1 while the remaining
  okm_size > 32. For okm_size=8160 the loop runs 254 iterations, the
  final memcpy emits T(255), and the counter peaks at exactly 255 — no
  unsigned char overflow. Verified empirically: with -DNDEBUG the
  module's 8160-byte output matches an independent Python hmac/hashlib
  HKDF-SHA256 byte-for-byte, including T(255) =
  76a3f78bcffe95fecf91923c22ad6ee64d48a6d1b981d7e523d5c0f22154ee88.
- Observed output (assert-enabled build, clang 18 ASan+UBSan,
  /tmp/hkdf-asan/repro-max-l-assert and the regression test):
  ```
  hkdf_sha256.c:16: ... Assertion `okm_size < 255 * sizeof(t)' failed.
  Aborted (core dumped)   (exit 134)
  ```
- Caller preconditions: none violated — L=8160 is legal per the RFC the
  module claims to implement; only the module's own (mis-stated) header
  bound forbids it, and that bound is part of the defect.
- Concrete incorrect consequence: spurious assertion abort in debug
  builds for a specification-legal request; the module cannot produce
  the full RFC 5869 output range. (In NDEBUG builds the call silently
  succeeds and is correct, so behavior differs between build modes.)
  Severity is low — one octet-range of L at the extreme — but the
  bound is objectively wrong against the cited specification.
- Side note (not a separate finding): above the cap (okm_size >= 8161)
  in NDEBUG builds the counter c wraps 255 -> 0 and the extra blocks
  are computed with counter byte 0x00, silently wrong; the assert is
  the only guard. That input violates even a corrected `<=` bound, so
  it remains a documented-precondition violation, but it is why the
  assert bound matters.
- Regression test: ccan/crypto/hkdf_sha256/test/run-max-okm.c
  (alarm(10)-bounded, 2 subtests: at L=8160 the first 42 octets must
  equal RFC 5869 Test Case 1's OKM — HKDF-Expand is prefix-closed —
  and the last 32 octets must equal the independently computed T(255)).
  Currently aborts on the assert; validated against a shadow copy with
  only `<` changed to `<=`: 2/2 pass under ASan+UBSan.
- Repair direction: change the assert to
  `assert(okm_size <= 255 * sizeof(t));` and the header to
  "must be at most 255*32" (hkdf_sha256.h:10). One character plus the
  doc line; no other code needs to change (loop and counter verified
  correct at the boundary).

## F2 — LIKELY (FIXED in 344ba97c): hkdf_sha256(NULL, 0, ...) passes NULL to memcpy (formal UB, UBSan-observed)

- Location: ccan/crypto/hkdf_sha256/hkdf_sha256.c:96:
  ```c
  memcpy(okm, &t, okm_size);
  ```
- Reachable path: hkdf_sha256(okm=NULL, okm_size=0, ...). Nothing in
  the documented contract forbids a zero-length request — the header
  only bounds okm_size above ("must be less than 255*32"), and RFC 5869
  does not forbid L=0 either; a generic wrapper that computes L from
  parameters can legitimately produce 0, and NULL is the natural okm
  for it.
- Observed output (clang 18 UBSan, /tmp/hkdf-asan/repro-null-empty.c):
  ```
  ccan/crypto/hkdf_sha256/hkdf_sha256.c:96:9: runtime error: null
  pointer passed as argument 1, which is declared to never be null
  /usr/include/string.h:44:28: note: nonnull attribute specified here
  ```
  (aborts under -fno-sanitize-recover=all). C17 7.1.4p1 / 7.24.1 make
  memcpy(NULL, src, 0) formally undefined even though no byte is
  written.
- Why LIKELY rather than CONFIRMED: the only observed consequence is
  the sanitizer diagnostic itself; on all real platforms the call
  writes nothing and behaves correctly, and a zero-length HKDF request
  is an edge usage. No miscompilation is known to result.
- Regression test: none added (the F1 test demonstrates the module's
  sanitizer posture; a plain-build test for F2 would pass vacuously).
  If desired, the repair is checkable by running repro-null-empty.c
  under UBSan.
- Repair direction: return early for the empty case at the top of
  hkdf_sha256, e.g. `if (okm_size == 0) return;` — this also skips the
  wasted T(1) HMAC computation for L=0.

## Rejected candidates (disproved)

- R1: empty/zero-length salt (suspect list). RFC 5869: absent salt =
  HashLen zeros. HMAC pads any key shorter than the 64-byte block with
  zeros, so HMAC(key="") == HMAC(key=32 zeros) — exactly the RFC
  default. RFC Test Case 3 (zero-length salt/info) passes byte-exact in
  the api test, and a strictly-NULL salt/info reproducer
  (/tmp/hkdf-asan/repro-null2.c) matches the RFC Test Case 3 OKM and is
  ASan+UBSan-clean (hmac_sha256_init guards `if (ksize != 0)`,
  hmac_sha256.c:38; sha256 add() never touches p when len==0).
  Rejected.
- R2: round counter overflow within the cap (suspect list). c is
  unsigned char; at the corrected maximum L=8160 it peaks at exactly
  255 (traced; full 8160-byte output matches the independent reference,
  so all 255 counters 0x01..0xff were emitted correctly). No overflow
  is reachable within the legal range. Wrap at 255->0 requires
  okm_size >= 8161, a documented-precondition violation (see F1 side
  note). Rejected.
- R3: info-length limits (suspect list). isize is size_t, passed
  unmodified to hmac_sha256_update/sha256_update; there is no per-round
  length arithmetic in the module that could overflow, and info is
  re-hashed from the caller's buffer each round (no internal
  accumulation). Arbitrary info lengths are the dependency's domain and
  are handled there. Rejected.
- R4: loop boundary handling for non-multiples and exact multiples of
  32 (`while (okm_size > sizeof(t))` + trailing memcpy): covered by the
  api test (L=42, L=82 = 2*32+18) and by the full-length L=8160 match
  (254 whole blocks + one whole final block); L=8159 (= 254*32+31)
  traced: 254 iterations, final 31-byte memcpy of T(255), counter peaks
  at 255. Correct in all cases. Rejected.
- R5: sizeof(struct hmac_sha256) as the block size: struct hmac_sha256
  is { struct sha256 sha; } and struct sha256 is a 32-byte union
  (hmac_sha256.h:15-17, sha256.h:22-27), so sizeof(t) == 32 == HashLen;
  `255 * sizeof(t)` is the right constant. Rejected.
- R6: okm_size == 0 with non-NULL okm: loop skipped, memcpy of 0 bytes
  to a valid pointer — defined; only the NULL-okm variant is F2.
  Rejected.
- R7: alignment/aliasing: okm is written only via memcpy; all HMAC/SHA
  state is stack structs; no casts of caller buffers. Rejected.
- R8: -m32 concerns: none — the module built and passed 3/3 under
  clang -m32 on this host; no word-size-sensitive arithmetic (only
  size_t lengths and an 8-bit counter). Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/crypto/hkdf_sha256/test/run-max-okm.c — proves F1. Aborts on
  the hkdf_sha256.c:16 assertion against the current code (exit 134,
  plain and ASan builds); alarm(10)-bounded, 2 subtests. Passes 2/2
  under ASan+UBSan against a shadow copy with the repair-direction
  change (`<` -> `<=`). #includes ccan/crypto/hkdf_sha256/hkdf_sha256.c
  per run-test convention (ccanlint only links module objects into api
  tests); when building manually, link hmac_sha256.c, sha256.c,
  str/hex/hex.c and tap.c.
- ccanlint with this file present: 32/38 FAIL, solely because the new
  test fails against the current code (tests_pass +1/2).

No production files were modified (hkdf_sha256.h, hkdf_sha256.c, _info
untouched; verified by git status — the only new path under
ccan/crypto/hkdf_sha256 is the test above).
