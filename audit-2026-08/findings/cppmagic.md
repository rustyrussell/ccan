# Audit: ccan/cppmagic

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: cppmagic.h (191 lines, header-only; all defects are
compile/preprocessor-time), _info and ccan/cppmagic/test/ only.
No dependencies (_info "depends" is empty; the test links ccan/tap).
Because the module is pure preprocessor metaprogramming, all probes
are compile/expansion probes (`gcc -E`, stringification harnesses)
rather than runtime sanitizers; there is no runtime code to be
memory-unsafe.

## Mechanical checks

- ccanlint (before auditor additions): **28/30**. Every check passes
  except examples_exist (+0/2: "No Example: section" in _info and
  cppmagic.h). tests_pass and tests_pass_valgrind PASS.
- After adding the auditor test: ccanlint **26/32 FAIL** — exactly as
  designed, because test/run-map-empty-arg.c fails 4/4 against the
  current code (F1).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  -g, -I.; run.c includes only headers; linked with ccan/tap/tap.c;
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:
  detect_leaks=0), LP64: **1/1 pass cleanly, 42/42 ok, zero sanitizer
  diagnostics** (expected — assertions are on string literals produced
  by the preprocessor; the sanitizers have nothing to observe).
- -m32: not applicable (no runtime code, no word-size dependence).
- Probes in /tmp/cppmagic-asan/ (temporary, not committed):
  strict.c, empty.c, dollar.c, emptyexp.c, emptylit.c, misc.c,
  odd2map.c, parencond.c, depth*.c, res.c, code3000.c.

## Findings

## F1 — CONFIRMED (documented in e3155f93): an argument that is empty or expands to no tokens silently truncates CPPMAGIC_MAP / CPPMAGIC_2MAP / CPPMAGIC_JOIN (and fools CPPMAGIC_NONEMPTY / ISEMPTY), dropping all following arguments

- Location: ccan/cppmagic/cppmagic.h:71-73 (`_CPPMAGIC_EOA` /
  CPPMAGIC_NONEMPTY) as used by the iteration termination tests at
  cppmagic.h:146 (_CPPMAGIC_MAP), :165 (_CPPMAGIC_2MAP) and :183
  (_CPPMAGIC_JOIN).
- Mechanism: after emitting `m_(a_)`, _CPPMAGIC_MAP decides whether to
  recurse via CPPMAGIC_NONEMPTY(__VA_ARGS__), which expands to
  `CPPMAGIC_1ST(_CPPMAGIC_EOA <args>)()`.  If the first remaining
  argument expands to no tokens, `_CPPMAGIC_EOA` ends up followed by
  `()` and expands to 0, so NONEMPTY reports "no more arguments" and
  the iteration *terminates* — it does not skip the empty element, it
  drops it and every element after it, with no diagnostic.
- Reachable path (observed, /tmp/cppmagic-asan/emptyexp.c + emptylit.c,
  gcc 13 default mode):
  ```
  #define EMPTY
  #define M(x) [x]
  CPPMAGIC_NONEMPTY(EMPTY)        => 0          (documented: '1')
  CPPMAGIC_MAP(M, a, EMPTY, b)    => [a]        (documented: [a] , [] , [b])
  CPPMAGIC_JOIN(;, a, EMPTY, b)   => a          (documented: a ; ; b)
  CPPMAGIC_MAP(M, a, , b)         => [a]        (textually empty arg)
  ```
- Preconditions: none documented are violated.  cppmagic.h:140-141
  documents `CPPMAGIC_MAP(@m, @a1, @a2, ... @an)` as expanding to
  `@m(@a1) , @m(@a2) , ... , @m(@an)` with no restriction that
  arguments must expand to at least one token; cppmagic.h:67-69
  documents `CPPMAGIC_NONEMPTY(@a)` as expanding to '1'.  Arguments
  that conditionally expand to nothing (e.g. a feature flag that is
  `/* nothing */` when disabled) are a natural metaprogramming input.
- Concrete consequence: silently wrong expansion in *code* contexts,
  not just stringify contexts — struct fields, initializers or
  statements corresponding to the empty argument and all later
  arguments vanish from the generated code with no compile error.
  E.g. `CPPMAGIC_MAP(FIELD, int x, COND_FEATURE, int y)` emits only
  `FIELD(int x)` when COND_FEATURE is empty.
- Regression test: ccan/cppmagic/test/run-map-empty-arg.c
  (alarm(10)-bounded, 4 subtests).  Currently fails 4/4:
  `not ok 1..4` with the outputs shown above.
- Repair direction: this limitation is inherent to the
  EOA-probe emptiness-detection technique (an argument that expands to
  no tokens is indistinguishable from no argument pre-C23), so the
  realistic minimal repair is to document the precondition on
  CPPMAGIC_MAP / CPPMAGIC_2MAP / CPPMAGIC_JOIN / CPPMAGIC_NONEMPTY /
  CPPMAGIC_ISEMPTY: "no argument may be empty or expand to no tokens;
  such an argument terminates the iteration and drops itself and all
  following arguments".  (A behavioral fix would require a
  __VA_OPT__-based rewrite limited to C23.)  If the documentation
  repair is chosen, the regression test should be adjusted to assert
  the documented truncation instead of the current contract.

## F2 — LIKELY (FIXED in 0421034f) (portability): the header relies on two extensions rejected by strict ISO C modes — a `$` pp-token in _CPPMAGIC_PROBE (cppmagic.h:49) and zero-argument variadic macro invocations throughout the public API

- Location: ccan/cppmagic/cppmagic.h:49 (`#define _CPPMAGIC_PROBE()
  $, 1` — `$` is not in the pre-C23 basic source character set and is
  still an extension as an identifier character in C23), and every
  documented empty-list use of the variadic macros
  (CPPMAGIC_NONEMPTY(), CPPMAGIC_ISEMPTY(), CPPMAGIC_MAP(m),
  CPPMAGIC_JOIN(d), plus the internal _CPPMAGIC_ISPROBE(..., 0) call
  chain), which C99/C11/C17 6.10.3p4 make a constraint violation
  ("there shall be more arguments ... than there are parameters,
  excluding the ...").
- Observed output (/tmp/cppmagic-asan/strict.c, empty.c):
  - `clang -std=c99/c11/c17/c23 -pedantic-errors`:
    ```
    cppmagic.h:49:28: error: '$' in identifier
      [-Werror,-Wdollar-in-identifier-extension]
    error: must specify at least one argument for '...' parameter of
      variadic macro [-Werror,-Wgnu-zero-variadic-macro-arguments]
    ```
    (both errors in every std mode, including c23).
  - `gcc -std=c99/c11/c17 -pedantic-errors`:
    `error: ISO C99 requires at least one argument for the "..." in a
    variadic macro` on any documented empty-argument use (gcc accepts
    the `$` token silently even with -pedantic-errors — verified with
    /tmp/cppmagic-asan/dollar.c).
- Why LIKELY rather than CONFIRMED: the failure is mechanically
  reproduced, but the module documents no strict-ISO conformance
  claim, and both constructs are accepted by default (gnu*) modes of
  gcc and clang, so this only bites users compiling with
  -pedantic-errors / -Werror=pedantic or on preprocessors without
  these extensions.  The `$` is gratuitous, though: any rarely-used
  identifier token would serve the probe role equally well.
- Repair direction: replace `$` in _CPPMAGIC_PROBE with a plain
  identifier unlikely to collide (e.g. `_CPPMAGIC_PROBE_SENTINEL`);
  the empty-variadic-argument uses are unavoidable for the documented
  API (CPPMAGIC_ISEMPTY() etc.) and can only be silenced by requiring
  C23 or documenting the GNU-extension requirement.

## Rejected candidates (disproved)

- R1: Recursion-depth limit.  Suspect: CPPMAGIC_EVAL is documented as
  forcing expansion "up to 1024 times", so MAP/JOIN should fail around
  ~512 elements.  Probed (/tmp/cppmagic-asan/depth*.c): MAP expands
  correctly through N=2049 elements; at N>=~2050 the EVAL pass budget
  is exhausted and unexpanded internal tokens
  (`_CPPMAGIC_IF_1 (, _CPPMAGIC_MAP_ CPPMAGIC_NOTHING ()()(M, ...)`) are
  emitted.  In any *code* context this is a loud compile error —
  verified at N=3000 (/tmp/cppmagic-asan/code3000.c): `error: expected
  expression before ',' token` at cppmagic.h:147 — never silent
  miscompilation (in the worst case, a pre-C99 implicit-declaration
  mode would still fail at link time on the undefined `_CPPMAGIC_IF_1`
  symbol).  Silent truncation only occurs inside CPPMAGIC_STRINGIFY,
  where the consumer (a strcmp in the run.c style) necessarily sees
  the residue.  Documented-budget, loud failure: rejected.
- R2: CPPMAGIC_2MAP with an odd number of arguments.  Documented
  contract is pairs (@a1,@b1,...).  Probed
  (/tmp/cppmagic-asan/odd2map.c): loud preprocessor error `macro
  "_CPPMAGIC_2MAP" requires 4 arguments, but only 2 given`.  Caller
  precondition, loud failure: rejected.
- R3: CPPMAGIC_IFELSE((0)) — parenthesized condition.  Probed
  (/tmp/cppmagic-asan/parencond.c): loud error `pasting
  "_CPPMAGIC_ISZERO_" and "(" does not give a valid preprocessing
  token` at cppmagic.h:52.  The contract (@cond "is '0'") implies a
  single token; failure is loud: rejected.  Same for CPPMAGIC_ISZERO
  with multi-token or punctuation-leading arguments — either correct
  (e.g. ISZERO(1+1) => 0, ISZERO() => 0, ISZERO(0x0) => 0) or a loud
  paste error.
- R4: Token-paste pitfalls in CPPMAGIC_GLUE2: pastes that would form
  invalid tokens (e.g. GLUE2(1, x)) are rejected by the preprocessor
  with a loud "does not give a valid preprocessing token" error on
  both gcc and clang; valid pastes (GLUE2(x, 1) => x1) work.  No
  silent wrong-token path exists — invalid ## results are a
  constraint violation diagnosed by both compilers: rejected.
- R5: Double evaluation / name capture: not applicable — macro
  arguments are substituted by the preprocessor, not evaluated, and
  the `a_`/`b_`/`m_`/`d_`/`cond_` parameter naming plus textual
  substitution prevents capture of user tokens; user arguments named
  e.g. `a_` substitute correctly (verified by inspection of the
  substitution rules and the run.c suite).
- R6: Deferred-expansion edge cases (CPPMAGIC_DEFER1/DEFER2,
  TESTRECURSE, nested/chained MAPs): covered by run.c tests 25-27 and
  39-42, all passing under both gcc and clang; generating a *nested*
  _CPPMAGIC_MAP from inside a MAP callback is blocked by standard
  blue-paint non-recursion and fails loudly (unexpanded identifier),
  which is the documented quasi-recursion design (_info: "(quasi)
  recursion"): rejected.
- R7: CPPMAGIC_EVAL documentation accuracy: "up to 1024 times" matches
  the EVAL1..EVAL1024 tree (cppmagic.h:107-128); measured budget
  (~2049 MAP iterations, R1) is consistent with 2 DEFER2 passes per
  iteration.  No discrepancy: rejected.
- R8: _CPPMAGIC_PROBE's `$` colliding with user tokens (i.e. a user
  argument expanding to `$`): only reachable in the F2 extension mode
  and only flips ISZERO(x) to 1 for the literal input `$`; folded into
  F2's repair (use an identifier sentinel instead, which has the same
  theoretical collision profile but is strictly more portable).

## Auditor-added test files (temporary; keep or remove later)

- ccan/cppmagic/test/run-map-empty-arg.c — proves F1.  Currently
  fails 4/4 in plain and ASan builds (`CPPMAGIC_NONEMPTY(EMPTY) => 0`,
  `MAP(TESTMAP, a, EMPTY, b) => [a]`, `JOIN(;, a, EMPTY, b) => a`,
  `MAP(TESTMAP, a, , b) => [a]`); alarm(10)-bounded.  Note: if the
  orchestrator chooses the documentation repair for F1, this test must
  be adjusted to assert the documented truncation behavior.
- ccanlint with the file present: 26/32 FAIL, solely because
  run-map-empty-arg.c fails against the current code.

No production files were modified (cppmagic.h, _info untouched;
verified by git status — the only new path under ccan/cppmagic is the
test above).
