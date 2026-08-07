# Audit: ccan/alignof

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: alignof.h (20 lines, header-only; single macro ALIGNOF at
alignof.h:12-18), _info and ccan/alignof/test/ (run.c only). No
dependencies ("depends" returns nothing). config.h here:
HAVE_ALIGNOF=1, so the `__alignof__(t)` branch (alignof.h:14) is
active; the struct-padding fallback (alignof.h:17) is dead code on this
host and on every compiler the configurator (tools/configurator/
configurator.c:118-120) accepts that implements `__alignof__`.

Note on the suspect list: **ALIGNOF_POW2 does not exist in this tree.**
`git grep ALIGNOF_POW2` across the whole repository and
`git log --all -S ALIGNOF_POW2 -- ccan/alignof` both come up empty; it
was added to upstream CCAN after this tree's HEAD (e7e57cbf). The
module here exports only ALIGNOF.

## Mechanical checks

- ccanlint (before any auditor additions): **34/35**. The single lost
  point is the "Example in module header" check
  ("alignof.h: No Example: section") — a documentation-coverage nit;
  the _info file carries a full Example section. Not a correctness
  defect; recorded for completeness only.
- test/run.c (15 subtests) under clang 18.1.3 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function
  -fno-sanitize-recover=all -g -I.`, linked with ccan/tap/tap.c,
  `timeout 60`,
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0):
  **15/15 pass, rc=0, zero sanitizer diagnostics.**
- Same under gcc 13 ASan+UBSan (`-fno-sanitize-recover=all`):
  15/15 pass, rc=0, clean.
- -m32: not attempted; module has no word-size-dependent logic beyond
  what __alignof__ itself reports, and the test's premise (alignment of
  natural accesses) is portable by construction.
- Reproducers/probes in /tmp/alignof-asan/ (temporary, not committed):
  probe-expr.c, probe-vla.c, probe-vla-fb.c, probe-fallback.c,
  probe-ice.c.

## Findings

None. No CONFIRMED or LIKELY defects retained.

## Rejected candidates (disproved)

- R1 (suspect: alignment-of-expression vs type): `ALIGNOF(x)` where x
  is an expression, not a type. With HAVE_ALIGNOF=1 the GNU
  `__alignof__(expr)` extension accepts it (verified: compiles clean
  under both `gcc -Wall -Wextra` and `clang -Wall -Wextra`,
  probe-expr.c) and yields the expression's type alignment; the
  fallback branch would be a hard compile error (`struct { char c;
  x _h; }` is not a declaration). Divergent branch behavior, but the
  documented contract is "@t: the type to test" (alignof.h:8) —
  passing an expression is a documented-precondition violation that
  happens to work on one branch and fail loudly on the other. No silent
  wrong answer. Rejected.
- R2 (suspect: VLA side effects): `ALIGNOF(int[n++])` — does the size
  expression get evaluated? Disproved by probe-vla.c: both gcc and
  clang leave n untouched (`n_after=4`, i.e. n++ never executed) and
  return 4. `__alignof__` on a VLA type is an unevaluated query on both
  compilers. The fallback branch with a VLA type is a loud constraint
  violation (probe-vla-fb.c: "expected identifier or '(' before '['
  token"), and struct members cannot be VLAs anyway. Rejected.
- R3 (suspect: _Alignof availability gating): the macro uses the GNU
  `__alignof__`, gated on HAVE_ALIGNOF, which the configurator probes
  by compiling and running exactly `__alignof__(double) > 0`
  (configurator.c:118-120) — the gate tests the precise construct the
  macro uses. The fallback does not require C11 `_Alignof`. No gating
  mismatch. Rejected.
- R4 (suspect: POW2 check on 0/1): ALIGNOF_POW2 absent from this tree
  (see header note). Nothing to check. Rejected as not present.
- R5: the !HAVE_ALIGNOF fallback
  `((char *)(&((struct { char c; t _h; } *)0)->_h) - (char *)0)`
  (alignof.h:17) is formally UB — member designator applied to a null
  pointer (C17 6.5.2.3 / 6.5.3.2p4), the classic pre-C11 offsetof
  idiom. Confirmed diagnosable: forcing the fallback into a runtime
  evaluation (probe-fallback.c) under clang UBSan gives
  "runtime error: member access within null pointer", rc=134. BUT it is
  unreachable on every compiler CCAN targets: the configurator only
  sets HAVE_ALIGNOF=0 for a compiler that cannot even compile
  `__alignof__(double)`, and no such compiler runs UBSan; when folded
  as a constant expression neither gcc nor clang diagnoses it. Its
  values are also correct where testable (char=1 short=2 int=4
  double=8 ptr=8, matching offsetof cross-check, probe-fallback.c under
  gcc UBSan rc=0). Same disposition as container_of R10:
  unreachable-on-supported-compilers. Rejected; noted for completeness.
- R6: fallback measures *effective* alignment (struct member padding)
  rather than `__alignof__`'s preferred alignment (e.g. i386 double: 8
  vs 4). Per the documented contract "This returns a safe alignment
  for the given type" (alignof.h:10), the fallback value is the safe
  one — matching `t`'s alignment as it actually occurs in aggregates
  and from malloc'd storage; the test comment itself documents this
  subtlety (test/run.c:6-13). No incorrect consequence. Rejected.
- R7: result type signedness — fallback yields ptrdiff_t, `__alignof__`
  yields size_t; both are used in `x % ALIGNOF(t)` after the caller's
  cast to unsigned long (test/run.c) or in size contexts. Usual
  arithmetic conversions keep positive values exact; no reachable
  incorrect consequence. Speculative. Rejected.
- R8: ALIGNOF as an integer constant expression — verified working on
  both compilers (probe-ice.c: `char buf[ALIGNOF(double)]` at block
  scope and `enum { A = ALIGNOF(int) }` compile under
  `-Wall -Wextra -Werror` and run correctly). Not a defect. Rejected.
- R9: ALIGNOF on incomplete types, void, function types, or types
  containing unparenthesized commas — all are loud compile errors or
  documented-precondition violations ("the type to test"); no silent
  misbehavior. Rejected.
- R10: macro double evaluation / name capture — ALIGNOF(t) uses its
  argument once on the active branch, and a type name cannot have side
  effects (R2 covers the only runtime-evaluable case, VLAs). No
  capture surface (no local identifiers introduced on the
  `__alignof__` branch). Rejected.

## Auditor-added test files

None. No confirmed defect to regress; adding tests would only restate
the existing run.c coverage. ccan/alignof is untouched
(verified: `git status --porcelain -- ccan/alignof` is empty).

No production files were modified.
