# Audit: ccan/str/hex

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: hex.h (73 lines, two static inlines), hex.c (66 lines), _info,
ccan/str/hex/test/run.c. No dependencies (_info "depends" is empty).
The module is exactly what the brief describes: hex encode/decode
helpers. Documented contract (hex.h:8-42): hex_decode returns false on
any non-[0-9a-fA-F] character or when the string length does not match
@bufsize exactly; hex_encode returns true iff the NUL-terminated output
fits in @destsize; hex_str_size(bytes) = 2*bytes+1; hex_data_size(len)
= len/2, correct "with or without NUL" for even-length hex strings.

## Mechanical checks

- ccanlint (no auditor additions needed): **46/47**, every check PASS
  except tests_coverage (99/102 lines; the unreachable `abort()` at
  hex.c:48 and the early-return branches are not all hit by run.c).
  tests_pass, tests_pass_valgrind, examples_compile, examples_run all
  PASS. Note: examples_run passes vacuously for the _info example —
  it has no expected-output marker, and the example returns 0 even
  though its hex_encode call fails (see F1).
- Existing test under clang 18.1.3 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; run.c #includes hex.c itself,
  linked with ccan/tap/tap.c only; `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **33/33 pass, zero sanitizer diagnostics**.
- -m32: builds and passes (33/33, ASan+UBSan) — the module includes no
  errno.h, so the usual 32-bit header limitation does not apply. No
  word-size-sensitive arithmetic in the module.
- Differential fuzzer (/tmp/hex-asan/repro-fuzz.c, 400000 iterations,
  ASan+UBSan): random binary buffers encoded at exact size (must
  succeed, lowercase, strlen == 2n) and one byte short (must fail),
  round-tripped through hex_decode; random strings (valid mixed-case,
  invalid, high-bit/negative-char bytes, odd lengths, random bufsize)
  checked against an independent nibble oracle for both the boolean
  verdict and every decoded byte: **zero mismatches, zero sanitizer
  diagnostics**.
- Edge probes (/tmp/hex-asan/repro-edges.c, ASan+UBSan): empty
  encode/decode, exact-fit rejection of oversized buffers, odd lengths
  1/3/5, invalid char at tail/middle, embedded NUL in counted input,
  high-bit bytes, size-helper corner values: **all pass**.
- Reproducers/probes in /tmp/hex-asan/ (temporary, not committed):
  repro-info-example.c, repro-fuzz.c, repro-edges.c.

## Findings

## F1 — CONFIRMED: the _info Example: section calls hex_encode with buf/dest swapped; the module's primary documentation demonstrates a call that always fails

- Location: ccan/str/hex/_info:21 (inside the Example: section,
  _info:13-26):
  ```c
  char str[hex_str_size(strlen(argv[i]))];

  hex_encode(str, sizeof(str), argv[i], strlen(argv[i]));
  printf("%s ", str);
  ```
  hex_encode's signature is hex_encode(const void *buf, size_t bufsize,
  char *dest, size_t destsize) (hex.h:42). The example passes the
  destination VLA `str` as @buf and argv[i] as @dest — the two pointer
  arguments (and their sizes) are swapped.
- Reachable path: this is the module's _info Example — the usage
  documentation rendered by ccanlint and on the CCAN website, and the
  code a user copy-pastes to learn the API.
- Observed output (/tmp/hex-asan/repro-info-example.c, clang 18
  ASan+UBSan, arg = "hello"):
  ```
  as-written in _info: hex_encode returns 0 (example ignores it)
  arg order fixed:      hex_encode returns 1, str="68656c6c6f"
  ```
  As written, destsize = strlen(argv[i]) = n is always less than
  hex_str_size(bufsize) = hex_str_size(2n+1) = 4n+3, so hex_encode
  returns false at the size check (hex.c:55-56) for every non-empty
  argument; the example ignores the return value and then does
  `printf("%s ", str)` on the never-written, uninitialized VLA —
  undefined behavior in the example itself (prints stack garbage; the
  string may be unterminated).
- Concrete incorrect consequence: the documented usage of the module
  cannot work — any program transcribing the example encodes nothing
  and prints uninitialized memory. No memory-safety impact on the
  library itself (the destsize check bails out before any read or
  write), but the primary documentation is wrong.
- Why CONFIRMED: the defect is objectively demonstrated above; it does
  not depend on any caller precondition.
- Regression test: none added — a TAP test cannot exercise an _info
  doc comment, and ccanlint's examples_compile/examples_run already
  "pass" this example (it compiles and exits 0 despite doing nothing).
  Evidence is the /tmp reproducer above. After repair, transcribing the
  example must print the hex encoding of each argument.
- Repair direction: swap the arguments at _info:21 to
  `hex_encode(argv[i], strlen(argv[i]), str, sizeof(str));`
  (optionally check the return value, as the hex.h examples do).

## Rejected candidates (disproved)

- R1 (brief suspect): invalid-character rejection — char_to_hex
  (hex.c:7-22) rejects everything outside 0-9/a-f/A-F, including
  high-bit bytes (c is a signed char; negative values fail all three
  range tests) and embedded NULs in counted input. Verified by the
  400k-iteration oracle fuzz and repro-edges.c. Rejected.
- R2 (brief suspect): odd-length decode — the loop `while (slen > 1)`
  (hex.c:29) leaves slen == 1 for odd input and the final
  `return slen == 0 && bufsize == 0` (hex.c:39) rejects it for every
  bufsize; str[1] is never read when slen == 1. Verified for slen
  1/3/5 in repro-edges.c and by the fuzz. Rejected.
- R3 (brief suspect): buffer sizing — hex_str_size(n) = 2n+1 and
  hex_data_size(len) = len/2 agree for both strlen and sizeof of
  even-length hex strings (sizeof includes the NUL: (2n+1)/2 == n);
  hex_encode's `destsize < hex_str_size(bufsize)` guard (hex.c:55)
  rejects every undersized destination (run.c tests all 23 undersizes;
  fuzz rechecks exact and exact-minus-one). hex_decode requires the
  exact fit documented at hex.h:15-16 ("the string wasn't the right
  length for @bufsize") — an oversized buffer is rejected, which is
  the documented behavior, not a bug. Rejected.
- R4 (brief suspect): case handling — decode accepts both cases
  (documented hex.h:15), encode emits lowercase (run.c:27 locks this
  in). Consistent; rejected.
- R5: hex_str_size / hex_encode size arithmetic overflow —
  2*bytes+1 wraps for bytes > (SIZE_MAX-1)/2, after which the destsize
  check could pass and the loop would write 2*bufsize bytes. Requires
  a @bufsize near SIZE_MAX/2 with @buf actually spanning 2*bufsize
  readable bytes (hex_encode reads buf[0..bufsize)) — impossible for
  any real buffer; a caller passing a fabricated bufsize already
  violates the documented "@bufsize: the length of @buf" precondition.
  Speculative; rejected.
- R6: hex_decode partially writes @buf before failing on a later bad
  pair (hex.c:30-37 validates and writes pair-by-pair). The contract
  (hex.h:15-16) only promises the false return; nothing documents
  @buf contents on failure, and the in-tree consumer (ccan/rune,
  verified in the rune audit R4) checks the return value. Callers
  using @buf after a false return violate the inferred contract.
  Rejected.
- R7: hex_decode short-string over-read (slen larger than the actual
  NUL-terminated string): hex_decode is a counted-length API
  ("@slen: the length of @str", hex.h:11); the terminating NUL is
  itself an invalid hex char, so the loop stops at it without reading
  past — this is exactly what ccan/rune relies on (rune audit R4
  re-verified here by the fuzz's counted random strings). Rejected.
- R8: hexchar() abort() (hex.c:48) — unreachable: both call sites pass
  c >> 4 or c & 0xF from an unsigned char, always in [0,15]. Not a
  defect (defensive). Rejected.
- R9: EBCDIC/non-contiguous-letter portability — char_to_hex and
  hexchar assume 'a'..'f' and 'A'..'F' are contiguous (only '0'..'9'
  contiguity is C-guaranteed). No EBCDIC target exists for CCAN; the
  same idiom pervades the tree. Speculative portability; rejected.
- R10: aliasing/overlap between buf and dest in hex_encode, or NULL
  pointers with zero sizes — undocumented caller preconditions;
  hex_decode(NULL, 0, NULL, 0) and hex_encode(x, 0, dest, 1) are
  well-defined and verified in repro-edges.c. Rejected.
- R11: hex.h:16 typo ("of the string wasn't") and unused #includes
  (assert.h, stdio.h in hex.c) — style only, explicitly out of scope
  per AGENTS.md. Not reported.

## Auditor-added test files

None. The single finding (F1) is a documentation defect in _info that
no TAP test can reach; evidence is the /tmp reproducer
(/tmp/hex-asan/repro-info-example.c). The existing run.c plus the
/tmp fuzzer and edge probes cover the code paths exhaustively, so no
temporary regression tests were needed. ccanlint therefore still
scores 46/47 with the tree as-is.

No production files were modified (hex.h, hex.c, _info untouched;
verified by git status — no new or changed paths under ccan/str/hex).
