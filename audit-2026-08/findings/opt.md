# Audit: ccan/opt

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: opt.h, opt.c, parse.c, usage.c, helpers.c, private.h, _info and
ccan/opt/test/ only.  Deps ccan/cast, ccan/compiler, ccan/typesafe_cb:
cast/typesafe_cb not yet audited; opt uses them only for const-casting
(cast_const in opt_set_charp) and callback type-checking macros — no
misuse per their docs found.

Mechanical checks:
- ccanlint: 91/97.  The only check losing points is tests_coverage
  (+0/6; "2742 of 10148 lines covered" — the failmsg() exit paths in
  check_opt and similar).  All other checks pass, including tests_pass
  (+14/14), tests_pass_valgrind (+14/14) and examples_compile (+10/10).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  -g, -I.; each test #includes the module .c files directly; linked
  with test/utils.c and ccan/tap/tap.c; per-test `timeout 60`), LP64:
  14/14 pass cleanly, zero sanitizer diagnostics:
  run (215 ok), run-add_desc (32), run-checkopt (14),
  run-consume_words (27), run-correct-reporting (14), run-early (37),
  run-early_incomplete (8), run-helpers (500), run-iter (74),
  run-no-options (7), run-set_alloc (222), run-unregister (15),
  run-usage (51), run-userbits (28).  Total 1245 ok.
- compile_ok-const-arg.c is ccanlint-only.
- Not run under -m32: the module has no word-size-dependent sizing
  arithmetic (unlike htable); all offsets are bounded by string lengths.
- Note: on this host ASan's default symbolizer hangs after printing the
  first ERROR line; sanitizer transcripts below were captured with
  `ASAN_OPTIONS=abort_on_error=1:symbolize=0` and symbolicated with
  addr2line.  An ASan run without abort_on_error continues past the
  first report and then loops inside the buggy code (see F1).
- Reproducers live in /tmp/opt-asan/repro-*.c (temporary, not
  committed).

Auditor-added test files (temporary; keep or remove later):
- ccan/opt/test/run-early-incomplete-dash.c — proves F1.  Currently
  fails as designed: plain build reports `not ok 2 - v == false`;
  ASan build aborts with a global-buffer-overflow at parse.c:173.
  alarm(10)-bounded; no production files modified.

## Findings

## F1 — CONFIRMED (FIXED in 6c3faecc): bare "-" argument in opt_early_parse_incomplete() reads 1 byte past the string and lets non-option operands fire short-option callbacks

- Location: ccan/opt/parse.c:126-131 (unknown_ok short-option path
  increments `*offset`) and parse.c:173 (the
  `if (*offset && !argv[arg][*offset + 1]) *offset = 0;` check).
- Reachable path:
  1. Caller uses the documented opt_early_parse_incomplete()
     (opt.h:300-320; "ignores unknown options") and any argv element is
     exactly "-" — the conventional non-option operand (stdin
     placeholder), not an unknown option.
  2. parse_one() short-option path looks up `argv[arg][*offset + 1]`,
     which for "-" is the terminating NUL (parse.c:126).  No registered
     short option can be NUL (check_opt rejects it), so the lookup
     fails; the unknown_ok branch does `(*offset)++` (parse.c:129),
     moving offset *past* the NUL.
  3. At parse.c:173 the code reads `argv[arg][*offset + 1]` = index 2
     of a 2-byte string — one byte out of bounds.  If that byte is
     nonzero, offset is not reset, and the next parse_one() call looks
     up that out-of-bounds byte as a short-option character
     (parse.c:126), incrementing further — the scan walks forward until
     it happens to hit a zero byte, firing callbacks for any matching
     registered early short option along the way.
  4. With the normal execve layout, argv strings are packed
     contiguously ("prog\0-\0v\0..."), so the byte read past "-" is the
     first character of the *next* argv element (or of the first
     environment string for the last element) — essentially always
     nonzero in practice, so step 3 is the common case, not the
     exception.
- Preconditions: none violated — opt.h:319-320 documents unknown
  options being ignored; a bare "-" is not an option at all.
- Concrete consequences:
  (a) Undefined behavior: 1-byte out-of-bounds read past the string
      object (ASan: heap-buffer-overflow / global-buffer-overflow at
      parse.c:173).
  (b) Wrong parse results: characters of a following *non-option*
      operand are interpreted as short-option letters inside "-";
      registered early short-option callbacks fire for arguments the
      user never supplied as options (e.g. argv {"prog","-","v"} sets
      verbose).  An attacker who controls operand-only arguments can
      thus toggle early options (typically debug/log paths) in programs
      using opt_early_parse_incomplete(), e.g. plugin loaders.
- Reproducers:
  - /tmp/opt-asan/repro-dash-oob.c (heap `strdup("-")`, ASan):
    ```
    ==ERROR: AddressSanitizer: heap-buffer-overflow
    READ of size 1 ... 0 bytes after 2-byte region  (strdup("-"))
    #0 parse.c:173  #1 opt.c:249  #2 opt.c:266 (opt_early_parse_incomplete)
    ```
  - /tmp/opt-asan/repro-dash-callback.c (packed "prog\0-\0v\0"
    storage, no sanitizer needed):
    `v=1 (expected 0: "v" was a non-option argument)` — the -v callback
    fired on the operand.
  - The same crash was rediscovered independently by random fuzzing
    (/tmp/opt-asan/repro-fuzz.c first iteration, parse.c:173).
- Regression test: ccan/opt/test/run-early-incomplete-dash.c.
  Plain clang build: `not ok 2 - v == false` (tests 1,3,4 pass).
  ASan build: global-buffer-overflow at parse.c:173 on the
  string-literal "-" (test 3).
- Repair direction: treat a lone "-" as a non-option operand, matching
  getopt: in the non-POSIXLY_CORRECT scan (parse.c:94-97) skip elements
  with `argv[arg][1] == '\0'`, and extend the post-scan check
  (parse.c:100) to return 0 for them.  As a side benefit this also
  fixes the secondary divergence that plain opt_parse() currently
  rejects "-" with "unrecognized option" (/tmp/opt-asan/repro-dash-parse.c:
  `prog: -: unrecognized option`; getopt treats it as an operand).  A
  narrower alternative is to not increment *offset at parse.c:129 when
  the looked-up character was the terminating NUL, but that leaves the
  getopt divergence in place.

## F2 — CONFIRMED (FIXED in dae5f5f4; doc sentence removed, helpers marked NO_NULL_ARGS) (documentation/implementation mismatch): opt.h promises "Sets to 1 on arg == NULL" for the integer helpers; they dereference NULL and crash

- Location: documentation at ccan/opt/opt.h:450
  (`/* Set an integer value, various forms.  Sets to 1 on arg == NULL. */`
  covering opt_set_intval, opt_set_uintval, opt_set_longval,
  opt_set_ulongval at opt.h:451-458).  Implementation:
  ccan/opt/helpers.c:98 `*l = strtol(arg, &endp, 0);` in
  opt_set_longval() (reached directly, or via opt_set_intval at
  helpers.c:68, opt_set_uintval at helpers.c:82, opt_set_ulongval at
  helpers.c:111).  helpers.c:63-64 carries the telltale
  `/* FIXME: set to 1 on arg == NULL ? */`.
- Reachable path: direct public-API call, e.g.
  `opt_set_intval(NULL, &i)` — NULL is an input the header explicitly
  documents a behavior for, so this is not a precondition violation.
  The module's own parser never produces a NULL optarg (parse.c:158-160
  rejects a missing argument before invoking the callback), so the
  defect only bites programs that call the helpers directly relying on
  the documented NULL semantics.
- Concrete consequence: NULL pointer passed to strtol() — UB and, in
  practice, an immediate segfault instead of setting the value to 1.
- Reproducer: /tmp/opt-asan/repro-null-arg.c.  Observed under clang 18
  ASan+UBSan:
  ```
  UndefinedBehaviorSanitizer: undefined-behavior helpers.c:98:14
    (applying zero offset / null pointer to strtol)
  AddressSanitizer: SEGV on unknown address 0x000000000000
  ```
- History: the claim dates to the module's first commit (d7d5abe9) and
  was never implemented; the FIXME shows the gap is known-but-open.
- Repair direction: either implement the documented semantics
  (`if (!arg) { *l = 1; return NULL; }` in opt_set_longval, with the
  truncation/range checks in the wrappers left to pass 1 through), or
  delete the sentence from opt.h:450.  Note the float helpers and the
  _si/_bi suffix helpers make no such NULL promise, so whichever choice
  is taken, keep the docs consistent.

## Rejected candidates (disproved)

- R1: opt_parse() rejects a bare "-" as "unrecognized option"
  (getopt treats lone "-" as a non-option operand).  Verified with
  /tmp/opt-asan/repro-dash-parse.c.  The error path itself is safe (the
  NUL lookup fails and parse_err returns before the offset machinery);
  it is a behavioral divergence only, folded into F1's repair
  direction.  No getopt compatibility is promised in opt.h/_info.
- R2: opt_set_uintval() rejects values in (INT_MAX, UINT_MAX]
  (e.g. "3000000000") because it routes through opt_set_intval, while
  opt_set_uintval_si/bi accept the full range.
  /tmp/opt-asan/repro-uint.c: `err=value '3000000000' does not fit into
  an integer` vs uintval_si accepting it.  Deliberate code structure;
  the header documents no full-range promise; the suite asserts 2^32
  fails.  Inconsistency is a feature request, not a defect.
- R3: check_opt() compares the *full* type at opt.c:143 and opt.c:147,
  so OPT_EARLY|OPT_HASARG entries are not counted in opt_num_short_arg
  and OPT_EARLY|OPT_NOARG entries escape the "does not take arguments"
  failmsg.  Disproved as a defect: opt_num_short/opt_num_short_arg/
  opt_num_long are write-only in production (grep: only test
  run-checkopt.c resets them), and the missed failmsg only means a
  malformed registration isn't diagnosed — parsing/usage still behave
  (names with ' '/= are "ignored for parsing, only used for printing
  the usage", opt.h:59-61).  Cosmetic.
- R4: get_columns() (usage.c:26-46): `COLUMNS=-1` or a value
  overflowing int yields a huge width.  Disproved: width only feeds
  word-wrap limits; all buffer math is driven by actual string lengths.
  Exercised in /tmp/opt-asan/repro-fuzz.c section 3 — clean.  Cosmetic
  (wrapping disabled).
- R5: OOM in add_opt() (opt.c:157-159, realloc NULL overwrites
  opt_table, then `opt_table[opt_count++]` writes to NULL) and in
  early_parse() (opt.c:241, unchecked alloc).  Disproved per
  documentation: opt.h:339 "simply crashes if they fail".
- R6: usage.c buffer management under adversarial input: 1 MB option
  names, descriptions containing "%s %n", tabs, newlines,
  whitespace-only and empty descriptions, plus 20000 random argv soups
  (alphabet "-=abcfx019mkMgGE \n", lengths 0-11, 1-7 elements) through
  opt_parse, opt_early_parse_incomplete and opt_usage — alarm(50)-
  bounded, ASan+UBSan clean, no hangs (/tmp/opt-asan/repro-fuzz.c,
  0 diagnostics after excluding the known F1 input).  The add_str_len/
  add_indent growth invariant (len < max after every append, so
  usage.c:234 `ret[len] = '\0'` is in bounds) holds; overflow of
  `*max * 2 + slen + 1` needs ~2^63 bytes of option text — impractical.
- R7: consume_words()/add_desc() termination: whenever consume_words
  returns l > 0, add_desc advances p by prefix + l > 0; l == 0 ends the
  loop; the inner scan loop strictly increases len or breaks.  Random
  whitespace/newline descriptions in the fuzz run never hung.  Doc nit
  found while disproving: opt.h:390-391 promises a line beginning with
  space *or tab* is output literally, but the strspn set at usage.c:58
  is " \n" — a tab at the very start of a description wraps instead.
  Formatting only.
- R8: Numeric helper edge semantics: opt_inc_intval/opt_dec_intval
  signed overflow after 2^31 repeats (caller precondition);
  opt_set_floatval accepting "nan" and explicit INF (deliberate,
  helpers.c:131-134 comment); strtod denormal underflow (e.g. "1e-320")
  reported as "out of range" via ERANGE; strtol base-0 octal gotcha
  ("08" → "not a number").  All standard strto* semantics or deliberate
  design; the overflow/suffix bounds in set_llong_with_suffix
  (helpers.c:311-313) are correct for both signs at LLONG_MIN/MAX and
  for 1000^6/1024^6 multipliers (verified by reasoning and by
  run-helpers.c's 500 checks under UBSan).
- R9: argc == 0 (argv == {NULL}) makes parse_one() read argv[1] past
  the pointer array (parse.c:94).  Violates the implied main() contract
  (opt.h:244-245 "@argc: pointer to argc"); out of scope.
- R10: opt_free_table() leaves opt_argv0 stale (opt.c:269-274); calling
  opt_usage_exit_fail() after the caller freed its own argv would print
  a dangling pointer.  Requires the caller to free argv and keep
  calling opt — caller misuse.
- R11: helpers.c uses strcasecmp() with only <string.h> included
  (helpers.c:39-44).  Declared via glibc/musl <string.h> under default
  feature-test macros; ccanlint objects_build passes.  Portability nit
  only.
- R12: Dead state: opt_register_table() stores the subtable length in
  u.tlen (opt.c:215-217) and nothing reads it; same for the
  opt_num_* counters (R3).  No consequence.
- R13: opt_find_long(arg, NULL) aliasing (parse.c:57-63 passes the same
  &o for the optarg and o out-params of opt_find_long_extra).  Works
  today only because the `*optarg = NULL` write (parse.c:41) is
  overwritten by the first_lopt assignment before any use, and the
  `*optarg = arg + *len + 1` write (parse.c:48) happens on the
  returning path.  Fragile, but no incorrect behavior reachable.
- R14: opt_usage()'s show-callback buffer contract (usage.c:152-167):
  a show callback that returns true without nul-terminating and without
  filling all OPT_SHOW_LEN bytes yields an unterminated buf.  That is
  exactly the documented caller contract (opt.h:50-54); all in-module
  opt_show_* helpers comply (snprintf/strncpy with explicit
  termination; opt_show_charp fills the buffer completely when it
  omits the NUL).  Caller precondition.
- R15: Long-option matching has no abbreviation feature:
  opt_find_long_extra (parse.c:34-55) requires all *len* characters to
  match plus a following '=' or NUL, so "--foo" never matches
  "--foobar" and there is no ambiguity surface.  Verified by inspection
  and by the run.c/run-iter.c coverage under ASan.
- R16: "%" in user-controlled strings: argv text and descriptions only
  ever appear as arguments to %s/%.*s (parse_err parse.c:21,
  opt_log_stderr opt.c:281, arg_bad helpers.c:20, opt_invalid_argument
  opt.c:301) or are memcpy'd (usage.c add_str*).  No format-string
  exposure; fuzz descriptions containing "%s %n" confirm.

No other findings.  Everything else in the audit plan (early/full
parse interplay, subtable flattening, unregister memmove bounds,
"--" terminator, "--opt=arg" splitting, short-option grouping and
argument swallowing, terminator-after-operand scanning) checked out
clean under the sanitizer suite and the fuzz run.
