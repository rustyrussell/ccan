# Audit: ccan/json_escape

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: json_escape.h (51 lines), json_escape.c (212 lines), _info,
ccan/json_escape/test/ (run.c, run-partial.c, run-take.c). Deps
ccan/tal and ccan/tal/str used per their documented contracts;
verified specifically that tal_dup_ consumes a taken source pointer by
resizing and tal_steal'ing it to the new context (tal.c:895-903), which
matters for R1 below.

The audit brief's suspects were: \u surrogate pair handling, invalid
escape rejection, UTF-8 validation on unescape (overlongs / surrogate
halves), buffer sizing arithmetic. The module's documented policy
settles most of this up front: "By policy, we don't handle \u.  Use
UTF-8." (json_escape.c:148) and the header says of both unescape entry
points "Be very careful here!  Can fail!  Doesn't handle \u: use UTF-8
please." (json_escape.h:44-49). There is no \u decoding and no UTF-8
validation anywhere in the module, so there is no surrogate-pair or
overlong-encoding code path to attack; see R2-R4.

## Mechanical checks

- ccanlint: **42/48**, every check PASS; shortfall is partial credit on
  tests_coverage (+1/6) and examples_exist (+1/2) only.
- All 3 run-* tests under clang 18 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function
  -fno-sanitize-recover=all -g -I.`; each test #includes
  ccan/json_escape/json_escape.c directly; linked with ccan/tap,
  ccan/tal, ccan/tal/str, ccan/str, ccan/take, ccan/list; per-test
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0),
  LP64: **3/3 pass cleanly, zero sanitizer diagnostics** — run (8 ok),
  run-partial (21 ok), run-take (5 ok). Total 34 ok.
- -m32: skipped; fails on missing 32-bit system headers via errno.h
  (known host limitation, same as earlier modules). The module has no
  word-size-sensitive arithmetic beyond size_t length math.
- Fuzzing/probing under the same sanitizer build
  (/tmp/jsesc-asan/repro-fuzz.c, repro-fuzz2.c, temporary):
  - 500k iterations of the C-string roundtrip
    unescape(escape(x)) == x over random non-NUL bytes restricted to
    the set escape() does not turn into \uXXXX: **0 failures**.
  - 100k iterations of the counted roundtrip
    unescape_len(escape_len(buf,len)) with embedded NULs, comparing
    tal_count and memcmp: **0 failures**.
  - 200k iterations of random counted buffers (raw bytes 0-255,
    including backslashes) into json_escape_unescape_len: no crash, no
    sanitizer diagnostic; invalid escapes and trailing backslash all
    yield NULL.
  - 100k iterations of json_partial_escape over take()'n random
    buffers, plus 19 hand-picked edge strings ("\\", "\\u", "\\u1" ...
    "\\u12345", "\\\uABCD\\", "\x01\x02\x7f", ...): no OOB read, no
    crash.
- Reproducers/probes in /tmp/jsesc-asan/ (temporary, not committed):
  repro-leak.c, repro-leak2.c, repro-fuzz.c, repro-fuzz2.c.

## Findings

None. Every candidate was disproved; see below. The module is two
small, symmetric loops over counted bytes with generous worst-case
allocations, and the brief's suspects are mostly settled by the
documented "we don't handle \u" policy.

## Rejected candidates (disproved)

- R1: unescape fast path leaks a taken input (json_escape.c:152-153).
  When `is_taken(esc) && !memchr(esc, '\\', len)` the function returns
  `tal_strndup(ctx, esc, len)` and never explicitly frees or steals
  esc — suspicious because the slow path carefully does
  `if (taken(esc)) tal_free(esc)` (json_escape.c:198-199).
  Disproved: tal_strndup -> tal_dup_arr -> tal_dup_, and tal_dup_
  explicitly consumes a taken source: `if (taken(p)) { tal_resize_(...);
  tal_steal(ctx, p); return p; }` (tal.c:895-903), i.e. the original
  allocation is resized and reparented to ctx, not leaked.
  /tmp/jsesc-asan/repro-leak2.c confirms: p = tal_strdup(ctx, "abc")
  is a child of ctx before the call, tal_first(ctx) == NULL after
  json_escape_unescape_len(NULL, take(p), 3) — ownership moved with
  the result. Rejected.
- R2: \u surrogate pair handling. There is none: json_partial_escape
  passes a well-formed `\uXXXX` through verbatim as 6 bytes
  (json_escape.c:98-107) without interpreting it, and unescape rejects
  'u' as an unknown escape (json_escape.c:187-191). Both behaviors are
  the documented policy ("we don't handle \u. Use UTF-8.",
  json_escape.c:148, json_escape.h:44-49). No decoding exists, so no
  surrogate-pair defect can exist. Rejected.
- R3: invalid escape rejection. unescape accepts exactly
  n b f t r / \ " (json_escape.c:166-186) and errors (returns NULL,
  freeing taken input) on everything else, including a trailing
  backslash (`if (++i == len) goto error;` json_escape.c:164) — the
  error label sits inside the switch but the goto into it is legal C
  and both error paths free unesc and the taken esc. Verified by the
  200k-iteration raw-byte fuzz and edge strings: every malformed
  escape returns NULL, none crash. "Can fail" is documented
  (json_escape.h:44). Rejected.
- R4: UTF-8 validation on unescape (overlongs / surrogate halves).
  unescape performs no UTF-8 validation and documents none; it only
  deletes escape introducers, so it cannot turn valid UTF-8 into
  invalid UTF-8 (it never alters bytes >= 0x80). json_escape's input
  contract is "escape a valid UTF-8 string" (json_escape.h:14) — a
  caller precondition. Rejected.
- R5: buffer sizing arithmetic. escape() allocates `len * 6 + 1`
  (json_escape.c:63), the true worst case (each input byte becomes at
  most 6 output bytes via `\uXXXX`; snprintf(esc->s + n, 7, ...) at
  json_escape.c:114 writes at most 6 chars + NUL, and at the last
  iteration n = 6(len-1) so 6len+1 bytes fit exactly including the
  NUL). Overflow of len*6 needs len > SIZE_MAX/6, unreachable for
  strlen-derived lengths (same class as earlier modules' rejected
  sizing candidates). unescape allocates len+1, an upper bound (output
  is never longer than input), and tal_resize's failure path is
  handled (json_escape.c:196-197). 800k fuzz iterations under
  ASan+UBSan produced no diagnostic. Rejected.
- R6: partial-escape \u lookahead over-read (json_escape.c:86-102).
  Reads str[i+1]...str[i+5]; the furthest read str[i+5] only happens
  when str[i+4] is a hex digit, and cisxdigit('\0') is false, so the
  scan can never pass the terminating NUL of the C string (len comes
  from strlen, json_escape.c:134); str[len] itself is in bounds.
  Trailing "\\", "\\u", "\\u1"..."\\u123" exercised under ASan:
  clean. Rejected.
- R7: escape() emits \u00XX for bytes < 0x20 (other than n b f t r)
  and 0x7F, which the module's own unescape then cannot parse —
  roundtrip NULL for e.g. "\x01". Both halves are documented: the
  escape side produces *valid JSON* as documented ("Allocates and
  returns a valid JSON string", json_escape.h:18) and the unescape
  side is documented "Can fail! Doesn't handle \u" (json_escape.h:44).
  A documented asymmetry, not a defect. Observed in the edge-string
  probe ('\x01\x02\x7f' -> '' -> NULL). Rejected.
- R8: snprintf(esc->s + n, 7, "\\u%04X", str[i]) sign/extension
  (json_escape.c:113-114): only bytes with (unsigned)str[i] < ' ' or
  str[i] == 127 reach it, i.e. values 0-31 and 127 — positive under
  both signed and unsigned char; output is always exactly 6 chars, so
  truncation is impossible. Bytes >= 0x80 fail the control-char test
  under both char signednesses ((unsigned)(negative int) is huge) and
  are copied verbatim, which is correct for UTF-8. Rejected.
- R9: struct json_escape { char s[1]; } written past index 0
  (json_escape.h:9, writes throughout escape/unescape): the classic
  struct hack, used module-wide and across CCAN; allocations are sized
  accordingly and ASan+UBSan (which includes array-bounds coverage
  here) reports nothing. Rejected.
- R10: escape() fast path steals a taken char array without relabeling
  it as struct json_escape (json_escape.c:53-59): cosmetic
  tal-label/valgrind nit only; tal_count(str) > len guarantees room
  for the terminating NUL write at esc->s[len]. No functional
  consequence. Rejected.
- R11: json_escape_needed / escape() control-char test
  `(unsigned)str[i] < ' '` (json_escape.c:35, :113): correct under
  both char signednesses (see R8); matches the JSON spec's
  must-escape set (U+0000-U+001F, '"', '\\'; 0x7F escaping is
  permitted). Rejected.
- R12: unescape fast path when is_taken(esc) but a backslash exists:
  falls through to the slow path, which consumes taken(esc) and frees
  it (json_escape.c:198-199); when not taken, esc is only read. Both
  taken-consumption points use the standard is_taken()/taken() idiom
  correctly (is_taken peeks, taken consumes). Rejected.

## Auditor-added test files

None. No defect survived disproof, so no regression tests were added;
ccan/json_escape/ is untouched (verified by git status — no changes
under the module). Temporary probes live only in /tmp/jsesc-asan/.
