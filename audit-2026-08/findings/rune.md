# Audit: ccan/rune

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope note: the audit brief described this module as "UTF-8 rune
encode/decode (rune_encode/rune_decode)" with UTF-8 validation suspects
(overlong encodings, surrogates, >U+10FFFF, truncation). The actual
ccan/rune module (~1330 LOC: rune.h 400, rune.c 496, coding.c 426,
internal.h 8) is Rusty Russell's *authorization* rune library
(Macaroon-like cookies: SHA-256-chained restrictions, base64/string
encode/decode). There is no rune_encode/rune_decode and no UTF-8
codec or validation anywhere in the module; those suspects do not
apply. The module was audited for what it is, including its untrusted-
input decode paths (rune_from_base64n, rune_from_string,
rune_restr_from_string).

Scope: rune.h, rune.c, coding.c, internal.h, _info, ccan/rune/test/.
Deps ccan/base64, ccan/crypto/sha256, ccan/endian, ccan/mem,
ccan/short_types, ccan/str/hex, ccan/tal/str, ccan/tal,
ccan/typesafe_cb — used per their documented contracts. Verified
specifically: base64_decode_using_maps returns ssize_t -1 on error
(base64.h:94), so `size_t blen; ... if (blen == -1)` (coding.c:352)
works via conversion; hex_decode never reads past the first NUL of a
short string (hex.c:29-39), so rune_from_string on short input is safe.

## Mechanical checks

- ccanlint (before auditor additions): **55/60**, every check passes
  except tests_coverage (+1/6). tests_pass, tests_pass_valgrind,
  examples_compile, examples_run all PASS.
- After adding the three auditor tests: ccanlint **50/56 FAIL** —
  exactly as designed, because the new regression tests fail against
  the current code (F1-F3).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/rune/rune.c and ccan/rune/coding.c directly; linked with
  ccan/tap, ccan/tal, ccan/tal/str, ccan/tal/grab_file, ccan/str,
  ccan/str/hex, ccan/base64, ccan/crypto/sha256, ccan/mem, ccan/take,
  ccan/utf8, ccan/noerr, ccan/list; per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **3/3 pass cleanly, zero sanitizer diagnostics** — run (355 ok),
  run-altern-escape (9 ok), run-alt-lexicographic-order (121 ok).
  Total 485 ok.
- -m32: skipped; fails on missing 32-bit system headers via errno.h
  (known host limitation, same as earlier modules).
- Decoder fuzzing (/tmp/rune-asan/repro-fuzz.c, temporary): 3 x 400000
  iterations (two independent seeds plus a third) of random counted
  buffers into rune_from_base64n, mutated valid runes into
  rune_from_base64 / rune_from_string / rune_restr_from_string
  (mutations drawn from "=!|&\\.:~^{}#<>/?-_" etc.), with successful
  decodes further exercised through rune_to_base64,
  rune_is_derived_anyversion and rune_eq, all under ASan+UBSan:
  **clean**, no diagnostics, no decode crashes.
- Reproducers/probes in /tmp/rune-asan/ (temporary, not committed):
  repro-dup-uaf.c, repro-fieldname-punct.c, repro-lexo-nul.c,
  repro-fuzz.c.

## Findings

## F1 — CONFIRMED (FIXED in cc63dceb): rune_dup() shallow-copies unique_id/version; freeing the original rune leaves the "copy" with dangling pointers (heap-use-after-free)

- Location: ccan/rune/rune.c:65-79, function rune_dup, concretely
  rune.c:72:
  ```c
  dup = tal_dup(ctx, struct rune, rune);
  ```
  tal_dup copies the struct by value; the `unique_id` and `version`
  members (rune.h:12-14) are pointers to tal-owned strings that are
  children of the *original* rune (tal_strdup'd to it by rune_new
  rune.c:54, rune_derive_start rune.c:90-92 and extract_unique_id
  rune.c:181-184). rune_dup deep-copies `restrs` (rune.c:73-77) but
  never re-dups the two strings, so `dup->unique_id`/`dup->version`
  alias the original's children.
- Reachable path: documented primary usage — rune_new with a version,
  rune_derive_start, then rune_dup (rune.h:99-106 "Copy a rune"),
  then tal_free of the original (the caller owns it and is done with
  it; nothing in the docs says the copy borrows the original's
  strings). Any later use of the copy that touches unique_id/version —
  rune_eq (rune.c:476-479 via runestr_eq rune.c:464-472), or
  rune_is_derived's version compare (coding.c:126 via runestr_eq) —
  reads freed memory. Note rune_derive_start itself is safe (it
  re-steals/re-dups both fields, rune.c:88-96); only direct rune_dup
  of a rune with unique_id/version set is affected.
- Why the preconditions are not violated: rune_dup is documented
  simply as "Copy a rune... otherwise copies" (rune.h:99-105); every
  sibling dup in the module (rune_altern_dup rune.c:115-127,
  rune_restr_dup rune.c:129-146) deep-copies all tal children, so the
  inferred contract is an independent copy whose lifetime is governed
  by @ctx alone ("Freeing @ctx will free the returned rune" idiom used
  throughout the header).
- Concrete incorrect consequences, observed:
  1. Heap-use-after-free read. /tmp/rune-asan/repro-dup-uaf.c, clang 18
     ASan (dup a rune with unique_id "uid1", version "1", free the
     original, then rune_eq(dup, dup)):
     ```
     ==ERROR: AddressSanitizer: heap-use-after-free
     READ of size 5
     #0 strcmp
     #1 runestr_eq /home/kimi/ccan/./ccan/rune/rune.c:469:10
     #2 rune_eq /home/kimi/ccan/./ccan/rune/rune.c:476:7
     freed by ... del_tree_inner ccan/tal/tal.c:463 (tal_free(rune))
     ```
  2. Deterministic ownership violation in plain builds: the regression
     test shows `tal_parent(dup->unique_id) == rune` and
     `tal_parent(dup->version) == rune` (`not ok 4`, `not ok 5`), i.e.
     the copy's fields are freed with the original. Once the heap
     chunks are reused, the copy's unique_id/version silently compare
     different/print garbage — in an authorization library, a wrong
     version comparison in rune_is_derived ("Version mismatch"
     coding.c:126-127) or a wrong unique_id shown to a blacklist check
     is a security-relevant logic error, not just a memory bug.
- Regression test: ccan/rune/test/run-dup-unique-id.c
  (alarm(10)-bounded, 9 subtests). Currently fails 2/9 in a plain
  build (ownership) and aborts under ASan at runestr_eq (rune.c:469)
  when the copy is used after freeing the original; after repair it
  must pass both ways.
- Repair direction: in rune_dup, after the tal_dup, deep-copy the two
  strings, e.g.
  `dup->unique_id = rune->unique_id ? tal_strdup(dup, rune->unique_id) : NULL;`
  `dup->version = rune->version ? tal_strdup(dup, rune->version) : NULL;`

## F2 — CONFIRMED (FIXED in 51257cd9): fieldnames containing '.' or '-' are documented as valid and accepted by rune_altern_new, but the parser rejects them — encoded runes cannot be decoded back (roundtrip failure)

- Location: ccan/rune/coding.c:206-213, function
  rune_altern_fieldname_len:
  ```c
  if (cispunct(alternstr[i]) && alternstr[i] != '_')
      return i;
  ```
  This stops the fieldname at ANY punctuation character except '_'.
  '.' and '-' are punctuation but are not condition characters, so a
  fieldname containing them is truncated there and the '.'/'-' is then
  rejected by rune_condition_is_valid (coding.c:187-204), failing the
  whole parse (coding.c:232-233).
- Reachable path: rune.h:111-113 documents for rune_altern_new:
  "@fieldname: the UTF-8 field for the altern.  You can only have
  alphanumerics, '.', '-' and '_' here."  rune_altern_new performs no
  validation (rune.c:103-113) and the encoder (rune_altern_encode,
  coding.c:81-103) writes the fieldname verbatim, so
  rune_altern_new(NULL, "a.b-c", RUNE_COND_EQUAL, "7") +
  rune_add_restr + rune_to_string/rune_to_base64 all succeed and
  produce `...:a.b-c=7`, which rune_from_string / rune_from_base64 /
  rune_restr_from_string then reject as malformed.
- Observed output (/tmp/rune-asan/repro-fieldname-punct.c):
  ```
  encoded: f0a89058ea08b458569c9a13ab7128c13af423574ace7a3943be6191a5aca435:a.b-c=7
  decoded: NULL (ROUNDTRIP FAILURE)
  restr_from_string('a.b-c=7'): NULL
  ```
- Concrete incorrect consequence: a rune built entirely through
  documented APIs with a documented-valid fieldname cannot survive its
  own serialization roundtrip (data loss / interop failure); a peer
  implementation following the documented charset produces runes this
  module cannot parse. Git history shows d1eabfd6 "runes: allow
  underscores in field names, as per Python runes 0.6" special-cased
  '_' in exactly this function, i.e. the parser charset was meant to
  track the documented charset and only '_' was fixed.
- Regression test: ccan/rune/test/run-fieldname-punct.c
  (alarm(10)-bounded, 7 subtests). Currently fails 5/7 in both plain
  and ASan builds; after repair it must pass.
- Repair direction: make the two sides agree. Either (a) stop
  rune_altern_fieldname_len only at actual condition characters plus
  the structural separators (e.g. return i when
  rune_condition_is_valid(alternstr[i]), keeping '|', '&', '\\'
  out of fieldnames since the encoder cannot escape them), which makes
  "a.b-c=7" parse as documented; or (b) correct the documentation
  (rune.h:112) to "alphanumerics and '_'" and add validation to
  rune_altern_new so it fails loudly instead of producing unparseable
  runes. Option (a) matches the header as written; check the Python
  runes reference charset before choosing.

## F3 — LIKELY (FIXED in fb4dbfa6): lexo_order() uses strncmp, so counted field values with an embedded NUL compare wrong for RUNE_COND_LEXO_BEFORE/LEXO_AFTER

- Location: ccan/rune/rune.c:286-296, function lexo_order:
  ```c
  int ret = strncmp(fieldval_str, alt, fieldval_strlen);
  if (ret == 0 && strlen(alt) > fieldval_strlen)
      ret = -1;
  ```
- Reachable path: rune_alt_single_str() is a counted API
  (rune.h:224-227, explicit fieldval_strlen) and every other string
  condition is implemented with NUL-safe counted helpers — memeqstr,
  memstarts_str, memends_str, memmem (rune.c:326, :331, :336, :341,
  :346) — so the module's design treats the field value as arbitrary
  counted bytes, not a C string. lexo_order is the only comparison
  that stops at a NUL: for fieldval = {'a','b','c',0,'X'} (5 bytes)
  and altern value "abc", strncmp returns 0 at the coincident NULs and
  the strlen(alt) > fieldval_strlen guard (3 > 5) does not fire, so
  lexo_order reports "equal" where byte-wise the altern value is a
  proper prefix and fieldval sorts strictly AFTER it.
- Observed output (/tmp/rune-asan/repro-lexo-nul.c):
  ```
  LEXO_AFTER 'abc' for fieldval 'abc\0X': f is equal to or ordered before abc (should PASS)
  ```
  i.e. RUNE_COND_LEXO_AFTER wrongly fails; RUNE_COND_LEXO_BEFORE
  wrongly passes for the mirror-image case. Since this module exists
  to make authorization decisions, a mis-ordered comparison can grant
  or deny incorrectly. The Python reference compares whole strings and
  gets the byte-wise answer.
- Why LIKELY rather than CONFIRMED: no documentation states explicitly
  that field values passed to rune_alt_single_str may contain embedded
  NULs; the contract is inferred from the counted-length parameter and
  the uniform use of NUL-safe mem* helpers for every other condition.
  The altern value itself can never contain a NUL (it is a C string
  from parsing or rune_altern_new), so the divergence needs a NUL in
  the caller-supplied field value only.
- Regression test: ccan/rune/test/run-lexo-embedded-nul.c
  (alarm(10)-bounded, 3 subtests). Currently fails 1/3 in both plain
  and ASan builds; after repair it must pass.
- Repair direction: compare counted bytes with a prefix rule, e.g.
  `int ret = memcmp(fieldval_str, alt, min(fieldval_strlen, strlen(alt)));`
  and if ret == 0 order by length (shorter sorts first), matching
  strcmp semantics extended to counted strings.

## Rejected candidates (disproved)

- R1: UTF-8 validation suspects from the brief (overlong encodings,
  surrogates, >U+10FFFF, truncated sequences): the module contains no
  UTF-8 codec and performs no UTF-8 validation; rune.h:114-115 merely
  states callers supply UTF-8 ("Any UTF-8 value is allowed" is a
  permissive statement, not a validation promise). Nothing to attack.
  Rejected.
- R2: rune_sha256_endmarker padding length (rune.c:35-45):
  `1 + ((128 - 8 - (bytes % 64) - 1) % 64)` in [1,64] reads at most
  the 64-byte pad array; the formula equals the standard
  `(56 - bytes) mod 64 + 1` for all bytes % 64 in [0,63] (checked
  algebraically: 119 % 64 = 55). sizedesc uses the pre-update byte
  count, as required. All-zero-sized arithmetic is unsigned. Rejected.
- R3: rune_from_base64n (coding.c:341-370): allocation is
  base64_decoded_length(len)+1, blen <= decoded length, data[blen]
  in bounds; blen < 32 rejected; embedded-NUL check
  `strlen(data+32) != blen - 32` is sound because data[blen] was just
  NUL-terminated; blen == -1 comparison works (ssize_t -1 -> SIZE_MAX
  on assignment, -1 converts back to SIZE_MAX in the comparison).
  Covered by 1.2M fuzz iterations. Rejected.
- R4: rune_from_string short-input over-read (coding.c:402-410):
  hex_decode(str, 64, ...) stops at the first non-hex char, which the
  terminating NUL always is (hex.c:30-31), so it never reads past the
  NUL; after a successful 64-hex-char decode, str[64] is within the
  string (either a real char or the NUL). Rejected.
- R5: rune_altern_decode buffer handling (coding.c:216-257): the
  worst-case value allocation `tal_arr(alt, char, *len + 1)` is an
  upper bound (each output byte consumes >= 1 input byte; escapes
  consume 2); pull_invalid's *data = NULL is never dereferenced
  afterwards (every failure path returns immediately and callers
  propagate); `while (*len && pull_char(...))` cannot fail in the
  condition. Trailing-backslash and NUL-containing counted input
  exercised by the fuzz. Rejected.
- R6: rune_meets_criteria_ / rune_restr_test dereferencing alterns[0]
  of an empty restriction (rune.c:416, rune.c:237-249): only reachable
  by adding a zero-altern restriction through rune_add_restr, whose
  documentation says "@restr: the (non-empty) restriction"
  (rune.h:179); the decoder always produces >= 1 altern per
  restriction (rune_restr_decode is a do/while, coding.c:268-276).
  Documented precondition; rejected.
- R7: rune_new secret_len > 55 only assert-guarded (rune.c:51): the
  header documents "@secret_len: ... (must be 55 bytes or less)"
  (rune.h:53); moreover the behavior in NDEBUG builds stays
  well-defined (sha256_update + endmarker are correct for any length).
  Documented precondition; rejected.
- R8: rune_derive_start with '-' in unique_id (rune.c:21 assert) or
  version set but unique_id NULL (rune.c:98 assert): both are
  documented preconditions — "@unique_id cannot contain '-'"
  (rune.h:86) and "if [version is] non-zero, you *must* set unique_id"
  (rune.h:83-84). Rejected.
- R9: rune_eq comparing shactx.buf bytes (rune.c:485-487): every path
  that mutates shactx ends with rune_sha256_endmarker, so
  shactx.bytes is always a multiple of 64 and the memcmp length is 0;
  from_string's `shactx.bytes = 64` seed (coding.c:289) preserves
  this. Rejected.
- R10: to_wbuf growth loop (coding.c:68-69): wbuf.len starts at
  64/128, never 0, so the doubling loop terminates; off + len cannot
  overflow for any realistically constructible rune (lengths originate
  from strlen of C strings). rune_to_base64 sizes ret with
  base64_encoded_length(wbuf.off)+1 while encoding wbuf.off-1 bytes —
  strictly generous; ret_len is bounded accordingly and ret[ret_len]
  is in bounds. Rejected.
- R11: integer_compare_valid strtol edge cases (rune.c:263-284):
  leading whitespace/'+' accepted, matching Python int(); trailing
  junk rejected via *p; ERANGE rejected. long vs s64 truncation only
  on LLP64/ILP32 targets (portability nit, no reachable consequence on
  supported LP64; speculative). Rejected.
- R12: extract_unique_id splitting at the first '-' (rune.c:179-184):
  matches the encoder (unique_id asserted '-'-free) and the Python
  runes id-version joining convention; versions containing '-'
  round-trip correctly since only the FIRST '-' splits. Rejected.
- R13: fieldnames containing '|', '&' or '\\' via rune_altern_new
  produce corrupt encodings (rune_altern_encode writes the fieldname
  unescaped, coding.c:89): outside the documented fieldname charset
  (rune.h:112), and F2 already covers the doc-vs-parser mismatch for
  the in-charset characters. Rejected as precondition violation.
- R14: rune_restr_test error aggregation (rune.c:223-250): errs array
  sized tal_count(restr->alterns) which is >= 1 on all reachable paths
  (see R6); error strings are copied into ctx before errs is freed.
  Rejected.
- R15: rune_is_derived_anyversion compares only shactx.s
  (coding.c:152), not bytes/buf: both states are block-aligned
  multiples of 64 (R9), so s fully determines the chaining state; the
  sha256-state-as-MAC construction is the documented Python-runes
  design, not this audit's to second-guess. Rejected.
- R16: -m32 concerns: none testable (32-bit headers unavailable); the
  code has no word-size-sensitive arithmetic beyond s64/long (R11).
  Rejected.

## Auditor-added test files (temporary; keep or remove later)

- ccan/rune/test/run-dup-unique-id.c — proves F1. Fails 2/9 in a
  plain build (dup does not own its unique_id/version) and aborts
  under ASan (heap-use-after-free READ at runestr_eq rune.c:469 via
  rune_eq rune.c:476) when the copy is used after freeing the
  original; alarm(10)-bounded. After repairing F1 it must pass in
  both plain and ASan builds.
- ccan/rune/test/run-fieldname-punct.c — proves F2. Fails 5/7 in both
  plain and ASan builds (string and base64 roundtrips of a
  documented-valid rune return NULL; rune_restr_from_string rejects
  "a.b-c=7"); alarm(10)-bounded. After repairing F2 it must pass.
- ccan/rune/test/run-lexo-embedded-nul.c — covers F3. Fails 1/3 in
  both builds (LEXO_AFTER wrongly fails for fieldval "abc\0X" vs
  "abc"); alarm(10)-bounded. After repairing F3 it must pass.
- ccanlint with these files present: 50/56 FAIL, solely because the
  new tests fail against the current code.

No production files were modified (rune.h, rune.c, coding.c,
internal.h, _info untouched; verified by git status — the only new
paths under ccan/rune are the three tests above).
