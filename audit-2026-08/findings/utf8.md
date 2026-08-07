# Audit: ccan/utf8

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: utf8.h (54 lines, state struct + two prototypes), utf8.c (178
lines, incremental decoder utf8_decode + utf8_encode), _info and
ccan/utf8/test/ only. No dependencies (_info "depends" is empty).

## Mechanical checks

- ccanlint (before auditor additions): **41/47**, every check PASS;
  partial credit only on tests_coverage (+1/6) and examples_exist
  (+1/2). tests_pass and tests_pass_valgrind PASS.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/utf8/utf8.c directly; linked with ccan/tap/tap.c only; per-test
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0),
  LP64: **3/3 pass cleanly, zero sanitizer diagnostics** —
  run-decode (2190905 ok), run-encode (1114113 ok),
  run-encode-decode (1112061 ok). Total 4417079 ok.
- -m32: skipped; module includes errno.h, and 32-bit system headers are
  unavailable on this host (known limitation, same as earlier modules).
  The code has no word-size-sensitive arithmetic (all accumulation into
  uint32_t c, max value 0x1FFFFF).
- Differential fuzzing (/tmp/utf8-asan/repro-fuzz.c, temporary):
  2000000 random byte strings (length 1-12, alphabet biased toward
  continuation/lead/ASCII bytes) decoded bytewise by utf8_decode
  (continuing after errors, exercising state reuse) and compared
  against a strict RFC 3629 reference decoder, under ASan+UBSan:
  **clean** — every reference-valid string was accepted with identical
  codepoint sequence and complete final state; every reference-invalid
  string produced at least one nonzero errno (or an incomplete tail);
  every errno observed was one of the documented EINVAL/EFBIG/ERANGE.
- Exhaustive differential (/tmp/utf8-asan/repro-exhaust.c, temporary):
  **5439744 sequences, zero mismatches** — all 256 1-byte strings, all
  65536 2-byte strings, all 3-byte strings with lead 0xC0-0xFF
  (3,932,160), and all 4-byte strings with lead 0xE0-0xFF, every second
  byte, and sampled third/fourth bytes. Whole-string validity verdict
  and decoded codepoints matched the reference everywhere; ASan+UBSan
  clean.
- Edge probes (/tmp/utf8-asan/repro-edge.c, temporary): errno class for
  overlong NUL, overlong ASCII, surrogates (plain and overlong-4-byte),
  >U+10FFFF (F4 90.. and F5 leads), and state reuse after an error —
  all behave sanely (details under rejected candidates).

## Findings

None. No CONFIRMED or LIKELY defects retained.

## Rejected candidates (disproved)

- R1: Continuation-byte acceptance window (utf8.c:87). Suspicion: the
  decoder checks only `(c & 0xC0) == 0x80` per continuation byte, so it
  might accept restricted second bytes (E0 A0.., ED 80..9F, F0 90..,
  F4 80..8F) that a table-driven validator constrains at byte 2.
  Disproved: the range restrictions are enforced at completion instead
  — overlong forms via the `c >> min_bits == 0` EFBIG check
  (utf8.c:107-124; min_bits 7/11/16 for total_len 2/3/4, correct since
  min encodable values are 0x80/0x800/0x10000), surrogates via
  `(c & 0xFFFFF800) == 0xD800` gated on total_len == 3 (utf8.c:102-104;
  a 3-byte sequence caps c at 0xFFFF so the mask test is exact), and
  >U+10FFFF via `c > 0x10FFFF` (utf8.c:99; max accumulable c is
  0x1FFFFF, no uint32 overflow). Verified exhaustively against the
  reference (see above): every restricted-second-byte case is rejected.
  Rejected.
- R2: State machine reuse across sequences (utf8.c:61, 94, 131). The
  documented contract ("@utf8_state can be reused without
  initializeation", utf8.h:33) holds because every true-return path
  leaves used_len == total_len: finished_decoding is only reached from
  the `used_len == total_len` test (utf8.c:94) or the ASCII path which
  sets both to 1 (utf8.c:62,68); bad_encoding sets total_len = used_len
  (utf8.c:131). UTF8_STATE_INIT {0,0,0} also satisfies
  used_len == total_len for the first call. Reuse verified by the fuzz
  (decoding continued after every error class and later valid
  characters decoded correctly) and by repro-edge.c ('B' decoded
  cleanly immediately after an error). Rejected.
- R3: Overlong NUL (C0 80, E0 80 80) yields ERANGE, not EFBIG
  (utf8.c:99 takes the c == 0 branch before the minimal-encoding
  branch), while a literal NUL byte yields EINVAL (utf8.c:66-67) and
  the docs say "EINVAL: bad encoding (including a NUL character)".
  Observed (repro-edge.c): C0 80 -> ERANGE, E0 80 80 -> ERANGE,
  00 -> EINVAL. The documentation does not specify precedence between
  "encoding of invalid character" (ERANGE) and "not a minimal encoding"
  (EFBIG) for inputs that are both; every path returns a nonzero errno,
  so every caller checking `errno != 0` (the documented usage in _info)
  rejects correctly. Doc-level ambiguity at most, no incorrect accept;
  rejected.
- R4: Overlong 4-byte surrogate (F0 8D A0 80 = U+D800) yields EFBIG
  instead of ERANGE because the surrogate branch is gated on
  total_len == 3. Observed (repro-edge.c): EFBIG. Same reasoning as R3
  — both are nonzero error errnos and the docs do not rank them;
  rejected.
- R5: `utf8_state->c` left holding a partial/stale codepoint on error
  returns (e.g. cp=0xd800 after the surrogate error in repro-edge.c).
  The documented extraction contract is "You can extract the character
  from @utf8_state->c" alongside the errno protocol (utf8.h:34-41);
  reading c after a nonzero errno is caller error, and the example in
  _info does not. Rejected.
- R6: used_len semantics after bad_encoding (utf8.c:131): after a bad
  continuation at byte k of a sequence, used_len counts only the bytes
  of the failing sequence before the current one (the current byte is
  not counted). The _info example's error span `i - used_len .. i`
  therefore covers exactly the failing sequence plus the offending
  byte — consistent. For a bad *first* byte the span is one byte wider
  than the actual bad byte; cosmetic, in example code only. Rejected.
- R7: Sign issues from the plain `char c` parameter (utf8.c:59): every
  classification and accumulation site casts to (unsigned char)
  (utf8.c:64,71,75,79,87,91); the only signed uses are `c == 0`
  (utf8.c:66, exact) and `utf8_state->c = c` (utf8.c:69), which is only
  reached after the high bit is known clear, so no sign extension.
  Rejected.
- R8: Integer overflow in `c <<= 6` (utf8.c:90): c is uint32_t and
  bounded by 7 bits of lead payload plus 3 continuation shifts, max
  0x1FFFFF; shift of an unsigned type is defined. Verified clean under
  UBSan across the exhaustive sweep. Rejected.
- R9: Lead bytes 0xF5-0xF7 accepted as 4-byte leads (the
  `(c & 0xF8) == 0xF0` test at utf8.c:79 spans 0xF0-0xF7), and
  0xC0/0xC1 accepted as 2-byte leads. Both families are rejected at
  completion (c > 0x10FFFF -> ERANGE; c <= 0x7F -> EFBIG), confirmed by
  the exhaustive 2-byte sweep and repro-edge.c (F5 80 80 80 -> ERANGE).
  Only 0xF8-0xFF and stray continuations take the immediate EINVAL
  path. All rejected outputs; rejected.
- R10: `default: abort()` in the min_bits switch (utf8.c:120-121):
  reachable only if total_len is set to a value outside {1,2,3,4},
  which utf8_decode never does; requires a caller-fabricated state,
  violating the "initialized UTF8 state" precondition (utf8.h:29).
  Rejected.
- R11: utf8_encode boundary classes (utf8.c:136-177): point 0 ->
  ERANGE; surrogates -> ERANGE; point > 0x10FFFF -> ERANGE; the
  `(point >> 7) == 0` / `>> 11` / `>> 16` ladder partitions
  [1, 0xFFFFFFFF] exactly; all shifts of uint32_t are in range.
  Exhaustively covered by run-encode.c (0..0x110000) under ASan+UBSan
  and round-tripped by run-encode-decode.c for every scalar value.
  Rejected.
- R12: utf8_encode truncating dest writes for huge points: the
  point > 0x10FFFF check (utf8.c:167) precedes the 4-byte store, so
  `point >> 18` <= 4 and dest[0] = 0xF0|.. is always a valid lead;
  dest is always written within UTF8_MAX_LEN=4 bytes per the
  documented buffer size (utf8.h:9,53). Rejected.
- R13: -m32 portability: untestable on this host (errno.h pulls in
  missing 32-bit headers); the code has no word-size-sensitive
  arithmetic beyond uint32_t/uint16_t fields and size_t return.
  Rejected.

## Auditor-added test files

None. No defect survived disproof, so no regression test was added; no
files under ccan/utf8 were created or modified (verified by
`git status --short -- ccan/utf8`: clean). Temporary reproducers live
only in /tmp/utf8-asan/ (repro-fuzz.c, repro-exhaust.c, repro-edge.c)
and are not committed.

No production files were modified (utf8.h, utf8.c, _info untouched).
