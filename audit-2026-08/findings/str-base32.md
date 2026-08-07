# Audit: ccan/str/base32

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: base32.h (77 lines), base32.c (160 lines, RFC 4648 base32
encode/decode), _info and ccan/str/base32/test/ only. Dep ccan/endian
(beint64_t, cpu_to_be64, be64_to_cpu) used per its documented contract;
no in-tree callers of the module outside its own tests.

## Mechanical checks

- ccanlint (before auditor additions): **43/48**, every check passes;
  the 5 lost points are the missing `Example:` sections in _info
  (examples_* checks). tests_pass and tests_pass_valgrind PASS.
- After adding the auditor test: ccanlint **38/43 FAIL** — exactly as
  designed, because test/run-encode-oversized-dest.c fails 6/6 against
  the current code (F1).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/str/base32/base32.c directly; linked with ccan/tap/tap.c only;
  per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **2/2 pass cleanly, zero sanitizer diagnostics** — run (48 ok),
  run-lower (48 ok). Total 96 ok.
- -m32: skipped; fails on missing 32-bit system headers via errno.h
  (known host limitation, same as earlier modules).
- Differential round-trip fuzz (/tmp/base32-asan/repro-fuzz.c,
  temporary): 200000 random buffers (lengths 0-39) — exact-size encode,
  data_size == input length, exact-size decode, byte-for-byte compare,
  plus one-byte-short destsize/bufsize rejection — all pass, ASan+UBSan
  clean. One-short undersizing never writes out of bounds and is always
  reported by a false return.
- Reproducers/probes in /tmp/base32-asan/ (temporary, not committed):
  repro-encode-fits.c, repro-decode-lenient.c, repro-fuzz.c.

## Findings

## F1 — CONFIRMED (FIXED in f2b445b3): base32_encode() requires destsize to be exactly base32_str_size(bufsize), contradicting the documented "fits"/"max size" contract, and fails spuriously for larger buffers

- Location: ccan/str/base32/base32.c:156, function base32_encode:
  ```c
  if (destsize != 1)
      return false;
  ```
- Documented contract (base32.h:30-33): "@destsize: the max size of
  the string" and "Returns true if the string, including terminator,
  fits in @destsize". Both phrases promise that any destsize >= the
  required length (as computed by the companion base32_str_size(),
  base32.h:44-54) is acceptable.
- Reachable path: caller sizes dest generously (e.g. a fixed stack
  buffer, or a buffer reused for several encodes) and calls
  base32_encode("f", 1, dest, 100). No documented precondition is
  violated — the 9-byte encoding including terminator fits in 100
  bytes.
- Concrete incorrect consequences, observed
  (/tmp/base32-asan/repro-encode-fits.c, plain and ASan builds):
  ```
  encode("f",1,dest,100) -> 0, dest[0..9]=MY======<0xAA> term@8=0
  encode("",0,dest,5)   -> 0
  ```
  1. Spurious failure: the function returns false even though the
     string fits, so a caller following the documented contract
     misdiagnoses success as "malformed"/failure.
  2. On that false return, dest has been partially written (the
     8-character quantum) but is NOT NUL-terminated — the terminating
     `*dest = '\0'` at base32.c:158 is only reached on the exact-size
     path. (Not memory-unsafe — every write is bounded by the
     destsize >= 8 check per quantum — but a caller that logs dest on
     failure reads unterminated data.)
  Note the asymmetry with base32_decode(), whose doc (base32.h:15-16)
  deliberately says "the string wasn't the right length for @bufsize"
  (exact match), so strictness there is documented; the encode doc
  says the opposite.
- Regression test: ccan/str/base32/test/run-encode-oversized-dest.c
  (alarm(10)-bounded, 6 subtests: oversized dest, destsize ==
  base32_str_size()+1, empty input with oversized dest). Currently
  fails 6/6 in both plain gcc and clang ASan builds:
  `not ok 1..6`, `# Looks like you failed 6 tests of 6.`
- Repair direction (minimal, code side): change base32.c:156 to
  `if (destsize < 1) return false;` so any destsize >= the needed
  length succeeds; the per-quantum `destsize < 8` guard already keeps
  every write in bounds, and base32_str_size() remains the exact
  minimum. (The alternative repair is to reword the base32.h doc to
  state an exact-size requirement like decode's; the code repair
  matches the doc's plain wording and costs one character.)

## Rejected candidates (disproved)

- R1: Lenient decode of RFC-invalid padding. decode_8_chars
  (base32.c:93-97) only rejects num_pad 2 and 5, so 7- or 8-pad quanta
  decode to 0 bytes ("========" and "A=======" return true with
  bufsize 0); padding in a non-final quantum is accepted
  ("MY======MZXW6YTB" -> "ffooba", 6 bytes, bufsize 6 -> true);
  non-zero pad bits are ignored ("MZ======" -> 'f', same as
  "MY======"). Observed via /tmp/base32-asan/repro-decode-lenient.c.
  Disproved as a reportable defect: (a) the doc only promises false
  "if there are any characters which aren't valid encodings or the
  string wasn't the right length for @bufsize" — every character here
  is in the alphabet, and bufsize matching is caller-supplied;
  (b) RFC 4648 explicitly makes pad-bit rejection optional ("MAY");
  (c) no memory-safety consequence: each quantum's output is
  (40 - 5*num_pad)/8 <= 5 bytes and is checked against the remaining
  bufsize before memcpy, and total decode output never exceeds
  base32_data_size() (per-quantum output <= 5, and data_size's
  trailing-pad subtraction (padding*5+7)/8 exactly matches decode's
  reduction for the valid pads 0/1/3/4/6, while its cap at 6 pads
  makes it over-allocate for the invalid 7/8-pad cases — always the
  safe direction; e.g. "========": data_size 1 vs decode output 0, so
  the documented data_size-then-decode pattern just returns false).
  Lenient acceptance of malformed input, at most a design choice.
  Rejected.
- R2: size_t overflow in base32_str_size ((bytes + 4) / 5 * 8 + 1,
  base32.c:49-52) and base32_data_size ((strlen + 7) / 8 * 5,
  base32.c:54-58): (bytes + 4) wraps for bytes > SIZE_MAX - 4, and the
  * 8 can wrap for bytes near SIZE_MAX * 5/8. Disproved: even with a
  wrong, too-small result, base32_encode bounds every 8-byte quantum
  write by `if (destsize < 8) return false` (base32.c:148-149), so the
  only consequence is a false return, never an out-of-bounds write;
  and inputs anywhere near SIZE_MAX bytes cannot exist. No reachable
  incorrect consequence. Rejected.
- R3: Decode of truncated input (suspect from the audit brief): slen
  not a multiple of 8 falls out of the while loop and returns
  `slen == 0 && bufsize == 0` == false (base32.c:116). Verified:
  "MY=====" (7 chars) and "MY======A" (9 chars) both return false, no
  sanitizer diagnostic. Bytes already written for preceding complete
  quanta stay within bufsize (checked per quantum). Rejected.
- R4: Invalid character rejection: memchr(base32_chars, c[i], 32)
  fails for any non-alphabet, non-pad char and returns false before
  any state is committed for that quantum ("M$======" -> false,
  verified). Custom alphabets (base32_chars is a documented mutable
  pointer, base32.h:69-75) are honored symmetrically by encode and
  decode; run-lower.c exercises this. Rejected.
- R5: Integer/sign/shift issues: `int bytes` in [0,5] compared against
  size_t bufsize — value-preserving usual conversions; the 8 x 5-bit
  accumulation is in uint64_t (40 bits max, no shift UB); `int bits`
  encode loop terminates at <= 0 after exactly ceil(bits/5) chars;
  memchr length 32 covers exactly the value characters. Rejected.
- R6: Alignment/aliasing/endianness: all byte transfers between the
  beint64_t accumulator and caller buffers go through memcpy
  (base32.c:112, 126) — no alignment or strict-aliasing UB. The +3
  offset is correct: decode accumulates the 40-bit value right-aligned
  in acc, and big-endian memory layout of the 64-bit word places the
  first data byte at offset 3; encode mirrors it. Verified by the RFC
  4648 test vectors (run.c), the lowercase variant, and the 200k-round
  differential fuzz on this little-endian host; the construction is
  endian-neutral via cpu_to_be64/be64_to_cpu. Rejected.
- R7: Macro pitfalls / double evaluation: the module exposes plain
  functions only, no macros; base32.h has no inline code. Rejected.
- R8: padlen() (base32.c:31-47) abort() on default: only called from
  encode_8_chars with bytes in [1,4] when bytes != 5 (guarded by
  assert(bytes > 0 && bytes <= 5) and the base32_encode loop
  invariant bufsize > 0 => bytes >= 1). Unreachable. Rejected.
- R9: -m32 concerns: none testable (32-bit headers unavailable); the
  only word-size-sensitive arithmetic is the R2 size_t overflow
  analysis, which is safe on 32-bit by the same per-quantum bounds
  check. Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/str/base32/test/run-encode-oversized-dest.c — proves F1.
  Currently fails 6/6 in both plain gcc and clang ASan builds
  (deterministic false return for oversized dest). alarm(10)-bounded.
  After repairing F1 it must pass in both builds.
- ccanlint with this file present: 38/43 FAIL, solely because
  run-encode-oversized-dest.c fails against the current code.

No production files were modified (base32.h, base32.c, _info
untouched; verified by git status — the only new path under
ccan/str/base32 is the test above).
