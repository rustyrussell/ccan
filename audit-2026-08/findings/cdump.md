# Audit: ccan/cdump

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).

Scope: cdump.h (104 lines), cdump.c (699 lines), _info,
ccan/cdump/test/ (8 pre-existing run tests), ccan/cdump/tools/.
Dependencies ccan/tal, ccan/tal/str, ccan/strmap used per their
documented contracts (tal_abort on OOM is tal's documented default;
strmap_add/del/iterate used as documented — no misuse found).
Suspects from the brief: size/length arithmetic on untrusted input,
recursion depth on nested types, enum/pointer handling, buffer overrun
in dump paths. The recursion-depth suspect confirmed twice (F1 crash
at EOF, F2 unbounded tok_take_expr recursion); a tokenizer EOF bug
(F3) and one UB nit (F4) also confirmed; "buffer overrun in dump
paths" disproved (string_of_toks bound proof, R4).

Documented contract (cdump.h:61-103, _info): cdump_extract(ctx, code,
problems) parses "simple, well-formed C code" from a NUL-terminated
string; "If there is a parse error, it will return NULL and allocate a
problem string for human consumption." The _info example and the
shipped tool ccan/cdump/tools/cdump-enumstr.c both feed it whole
arbitrary files (grab_file from argv/stdin), i.e. the input is
untrusted at the tool boundary, and a parse failure must be a clean
NULL+problems, not a crash.

## Mechanical checks

- ccanlint (before auditor additions): **54/59 PASS**, every check
  passes; shortfall is partial credit on tests_coverage and
  examples-style checks only.
- After adding the four auditor regression tests: ccanlint **45/51
  FAIL** — exactly as designed: tests_pass drops to +9/12 because
  run-eof-builtin-type and run-expr-recursion crash (SIGSEGV) and
  run-trailing-ident fails 6/10 against the current code
  (tests_compile passes for all four; run-unterminated-comment passes
  in ccanlint's unsanitized build — it only fires under UBSan, F4).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  -g, -I.; each test #includes ccan/cdump/cdump.c directly; linked
  with tal.c, tal/str/str.c, strmap.c, take.c, list.c, str.c, tap.c;
  per-test `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0), LP64:
  **8/8 pass cleanly, zero sanitizer diagnostics, 383/383 subtests** —
  run 94, run-arraysize 20, run-attributes 37, run-CDUMP 111,
  run-enum-comma 12, run-forward-decl 16, run-multiline 30,
  run-qualifiers 63.
- -m32: skipped; dependency chain pulls in errno.h (missing 32-bit
  system headers, known host limitation).
- Reproducers in /tmp/cdump-asan/ (temporary, not committed):
  repro-nullderef.c (F1), repro-recursion.c + repro-recursion-plain +
  repro-recursion-rlimit.c (F2), repro-eof.c (F3), repro-edge.c (F4),
  repro-comment-join.c (R1 disproof), repro-tokenize-quadratic.c +
  repro-tokenize-plain (R2 disproof), check-rlimit.c.

## Findings

## F1 — CONFIRMED (FIXED in 5a467007): NULL dereference in string_of_toks via tok_take_type when a builtin type word is the last token before EOF

- Location: ccan/cdump/cdump.c:349-351, tok_take_type:
  ```c
  /* Did we get some? */
  if (ps->toks != types) {
      name = string_of_toks(NULL, types, tok_peek(&ps->toks));
  ```
  with the dereference at cdump.c:155, string_of_toks:
  `str = p = tal_arr(ctx, char, until->p - first->p + 1);`
- Reachable path: cdump_extract → tok_take_conglom member loop
  (cdump.c:464) → tok_take_type, on input where a builtin type word
  (int/long/short/double/float/char/signed/unsigned) is immediately
  followed by EOF inside a struct/union body, e.g. `"struct s { int "`
  (note trailing space so `int` is flushed as a token — without it the
  F3 tokenizer bug drops `int` and masks this crash). The builtin-type
  loop consumes `int`, then `tok_peek(&ps->toks)` hits the terminator
  token and returns NULL (cdump.c:97-106), and string_of_toks
  dereferences `until->p` with until == NULL.
- Preconditions: none violated — malformed/truncated input is exactly
  what the documented problem-string contract exists for.
- Consequence: SIGSEGV instead of NULL+problems. Observed (clang 18
  ASan+UBSan):
  ```
  ccan/cdump/cdump.c:155:12: runtime error: member access within null pointer of type 'const struct token'
  AddressSanitizer: SEGV on unknown address 0x000000000000 ... READ
  ```
  and a plain `gcc -O0` build segfaults identically. Regression test
  test/run-eof-builtin-type.c (alarm(10)) crashes on current code;
  also covers `"struct s { unsigned "` and `"union u { const long "`.
- Note: tok_take_type is the only caller that passes a tok_peek()
  result straight into string_of_toks; the other string_of_toks
  callers pass raw token-array pointers (tok_take_expr_str:
  ps->toks - 1; qualifiers: quals + num_quals; tok_take_until: a
  non-NULL t), so this is the sole NULL-until site.
- Minimal repair direction: in tok_take_type's builtin branch, check
  the tok_peek result before calling string_of_toks, e.g.
  `const struct token *end = tok_peek(&ps->toks); if (!end) {
  complain(ps, "EOF after type name"); return false; }`.

## F2 — CONFIRMED (FIXED in 03676073): unbounded recursion in tok_take_expr → stack overflow on nested parens/brackets

- Location: ccan/cdump/cdump.c:278-291, tok_take_expr — recurses once
  per nested `(` (line 281-283) or `[` (line 284-286) with no depth
  limit:
  ```c
  if (tok_take_if(&ps->toks, "(")) {
      if (!tok_take_expr(ps, ")"))
          return false;
  ```
- Reachable paths: array sizes (tok_take_array → tok_take_expr_str,
  cdump.c:311), CDUMP(...) notes (tok_maybe_take_cdump_note,
  cdump.c:384), and __attribute__((...)) (tok_ignore_attribute,
  cdump.c:404) — all fed directly by the input. Input
  `struct s { int a[` + N×"(" + `1` + N×")" + `]; };` recurses N deep.
- Preconditions: the input is well-formed C at any nesting depth; the
  _info tool reads whole files/stdin, so depth is attacker-controlled.
- Consequence: SIGSEGV. Observed: gcc -O2 build, default 8MB stack,
  depth 500,000 (a ~1MB input) → SIGSEGV; gdb backtrace shows
  tok_is → tok_take_expr (cdump.c:280/282) recurring, rsp at the stack
  guard page (0x7fffff7ff000). With RLIMIT_STACK lowered to 1MB, depth
  60,000 suffices (~32 bytes/frame at -O0); clang 18 ASan reports
  `AddressSanitizer: stack-overflow` in the same configuration.
  Regression test test/run-expr-recursion.c (setrlimit 1MB, depth
  60000, alarm(60)) crashes on current code under both gcc and
  ASan+UBSan builds.
- Minimal repair direction: thread a depth counter through
  parse_state (or a new parameter) in tok_take_expr and fail with
  complain() past a bound (e.g. 256 nesting levels — far beyond any
  real header).

## F3 — CONFIRMED (FIXED in f50f2b17): tokenize() drops a trailing identifier token; truncated definitions parse as silent success

- Location: ccan/cdump/cdump.c:37-87, tokenize. An identifier token is
  only flushed when a non-identifier character follows it
  (whitespace/punctuation/comment branches, cdump.c:44-77). When the
  input ends exactly on an identifier, the loop exits with
  `tok_start != -1U` still pending and the final token is never added;
  only the terminator is appended (cdump.c:86).
- Reachable path: any input whose last character ends an identifier.
  Concretely: `cdump_extract(NULL, "struct", &problems)` returns a
  non-NULL defs with all three maps empty and `problems == NULL`;
  same for `"enum"` and `"union"`. With a trailing space the same
  inputs correctly fail with `Line 1: '': Invalid struct/union name`.
- Contract violated: cdump.h:67-69 — "If there is a parse error, it
  will return NULL and allocate a problem string". A lone `struct` is
  a truncated definition, i.e. a parse error, yet it is reported as a
  successful parse of an empty file. (It also makes error reporting
  input-dependent: `"foo"` is silently ignored while `"foo "` produces
  "Ignoring unknown statement...".)
- Consequence: silent acceptance of truncated input; a consumer of the
  tool sees "no definitions" instead of an error. No memory-safety
  impact (it accidentally masks F1 for the no-trailing-space form).
- Observed: /tmp/cdump-asan/repro-eof.c — `input=struct →
  defs=0x508... problems=(null), structs_empty=1 ...`. Regression test
  test/run-trailing-ident.c (alarm(10)) fails 6/10 on current code
  (the three bare-keyword cases) and passes the sanity cases
  (`"struct s { int x; }; struct t"` already fails gracefully both
  before and after a fix; `"enum foo { BAR };"` unaffected).
- Minimal repair direction: after the for loop in tokenize, before
  appending the terminator:
  `if (tok_start != -1U) add_token(&toks, code+tok_start, i - tok_start);`

## F4 — CONFIRMED (FIXED in f50f2b17) (UB, no behavioral consequence observed): NULL + 2 pointer arithmetic on unterminated comment

- Location: ccan/cdump/cdump.c:50-53, tokenize:
  ```c
  const char *end = strstr(code+i+2, "*/");
  len = (end + 2) - (code + i);
  if (!end)
      len = strlen(code + i);
  ```
  `end + 2` is computed before the NULL check; on an unterminated
  `/*` comment (input `"/*"` or `"... /* unterminated"`) this is
  arithmetic on a null pointer, UB per C17 6.5.6.
- Observed: clang 18 UBSan on input `"/*"`:
  `ccan/cdump/cdump.c:51:15: runtime error: applying non-zero offset 2
  to null pointer`. The computed len is always discarded (the `!end`
  branch recomputes it correctly), so output is correct today; the
  practical exposure is the UB itself (and an abort under
  -fno-sanitize-recover). Regression test
  test/run-unterminated-comment.c passes in plain builds but emits the
  UBSan diagnostic on current code.
- Minimal repair direction: move the computation after the check:
  `if (!end) len = strlen(code + i); else len = (end + 2) - (code + i);`

## Rejected candidates

- R1 — comment-adjacent tokens get joined in reconstructed strings
  ("unsigned/*c*/int" → "unsignedint", qualifiers dropped between
  comments). REJECTED by disproof: the tokenizer never emits tokens
  for comments at all (the "erased token" skip in tok_peek is
  vestigial — only the terminator has len == 0), so gaps between real
  tokens are always ≥ 1 source char and string_of_toks inserts the
  space. /tmp/cdump-asan/repro-comment-join.c: `"unsigned/*c*/int"` →
  `"unsigned int"`, `"const /*c*/ volatile"` → `"const volatile"`,
  `"1/*c*/2"` → `"1 2"`, `"2/*c*/3"` array size → `"2 3"`. All
  correct.
- R2 — O(n²) tokenize: add_token (cdump.c:11-17) does
  tal_resize(toks, n+1) per token. REJECTED as a practical defect:
  under glibc, repeated +16-byte realloc of a heap-top buffer extends
  in place — measured linear (100k tokens 4ms, 400k tokens 10ms,
  gcc -O2). The quadratic only materializes under ASan's always-copy
  realloc (100k tokens 31s) or a hypothetical copying allocator; the
  same growth-at-top pattern holds inside cdump_extract because
  tokenize runs to completion before any other allocation.
- R3 — 32-bit truncation in tokenize counters (`unsigned int i, len,
  tok_start`, cdump.c:33; to_eol's size_t return truncated into len).
  REJECTED: requires a >4GiB input string before `i` can wrap; not
  reachable at any practical scale for this parser, and below 4GiB all
  arithmetic is exact.
- R4 — buffer overrun in string_of_toks space insertion. REJECTED by
  bound argument + sanitizers: a space is inserted only when
  `first->p + first->len != next->p`, i.e. only across a source gap of
  ≥ 1 char, so bytes written ≤ sum(token lens) + sum(gaps) =
  `until->p - first->p`, which plus the NUL fits the
  `until->p - first->p + 1` allocation (cdump.c:155). 383 subtests of
  existing coverage (which include comments inside array sizes and
  qualifiers) are ASan-clean.
- R5 — undefined struct/union/enum types are removed from the
  definition maps (remove_undefined, cdump.c:627-638) while member
  types still point at them with u.members/u.enum_vals == NULL.
  REJECTED: this is intended, test-asserted behavior —
  test/run.c:113-116 checks `p->u.members == NULL` and
  `!strmap_get(&defs->structs, "unknown")`; a CDUMP_STRUCT with NULL
  members is the module's representation of "declared but not
  defined".
- R6 — complain() at EOF compares `p < ps->toks[0].p` with
  toks[0].p == NULL and passes NULL to `%.*s` (cdump.c:259-268).
  REJECTED: exercised by the F1/F3 reproducers ("struct x" at EOF
  prints `Line 1: '': ...` cleanly under ASan+UBSan); precision-0
  `%.*s` never dereferences the pointer on glibc and the NULL
  relational compare is not instrumented. No observable consequence.
- R7 — enum values containing commas inside parentheses
  (`enum e { V = (1, 2) };`) terminate the value early and fail the
  parse. REJECTED: graceful failure (NULL + problem string), and
  _info restricts the module to "simple, well-formed C code"; no
  safety impact.

## Auditor-added tests (in ccan/cdump/test/, alarm-bounded)

- run-eof-builtin-type.c — F1: `"struct s { int "` etc. must return
  NULL+problems, not SIGSEGV. Crashes on current code (gcc and
  ASan+UBSan).
- run-expr-recursion.c — F2: 60000-deep nested parens in an array size
  with RLIMIT_STACK=1MB must fail gracefully. Crashes (stack-overflow)
  on current code.
- run-trailing-ident.c — F3: bare `"struct"`/`"enum"`/`"union"` must
  be parse errors. Fails 6/10 on current code.
- run-unterminated-comment.c — F4: `"/*"` inputs parse successfully;
  emits the cdump.c:51 UBSan diagnostic on current code (passes in
  unsanitized builds).
