# Audit: ccan/structeq

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: structeq.h (46 lines, header-only), _info and ccan/structeq/test/
only. Dependencies: ccan/build_assert and ccan/cppmagic (both audited
separately / documented behavior taken as given; structeq uses
BUILD_ASSERT and CPPMAGIC_GLUE2/JOIN/MAP exactly as documented — no
misuse found).

Module shape: STRUCTEQ_DEF(sname, padbytes, members...) generates a
static inline `<sname>_eq(const struct sname *, const struct sname *)`.
BUILD_ASSERT validates the padding claim
(sum of member sizeofs + padbytes == sizeof, or, for negative padbytes,
sum - padbytes >= sizeof). If sum of member sizeofs == sizeof(struct)
it returns `memcmp(_a, _b, sizeof(*_a)) == 0`; otherwise it compares
member-by-member (`memcmp(&_a->m, &_b->m, sizeof(_a->m)) == 0 && ...`).
Top-level padding bytes are therefore never compared — that is the
module's advertised purpose and it works as documented.

## Mechanical checks

- ccanlint: **39/41, every check PASS** (the 2 missing points are
  examples_exist/examples_compile at +1/2 — the header has no Example:
  section; _info has one). tests_pass +3/3, tests_pass_valgrind +3/3,
  tests_coverage PASS.
- All 3 run tests under clang 18 ASan+UBSan
  (`-fno-sanitize=function -fno-sanitize-recover=all`, -g, -I., linked
  with ccan/tap/tap.c — the tests do not #include any module .c;
  ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0, per-test
  `timeout 60`): **3/3 pass, 9 subtests ok, zero sanitizer
  diagnostics** (run, run-with-padding, run-with-unknown-padding).
- -m32 (clang, same sanitizers): builds and passes, 3/3 ok — this
  header-only module pulls in no errno.h/asm headers, so the usual
  -m32 header failure does not apply.
- compile_fail matrix (gcc 13.3.0 and clang 18.1.3,
  `-c -Wall -Werror -I.`): all 5 compile_fail tests compile without
  FAIL and are rejected with -DFAIL under BOTH compilers, each for its
  intended reason:
  - compile_fail-different.c — incompatible pointer type on the _b
    parameter (the generated function's prototype does the type check).
  - compile_fail-expect-padding.c, -unexpceted-padding.c (sic, upstream
    filename), -expect-any-padding.c, -unexpected-negative-padding.c —
    all via the BUILD_ASSERT static assertion with the exact expected
    expression in clang's diagnostic.
- Reproducers/probes in /tmp/structeq-asan/ (temporary, not committed):
  probe-nested.c, probe-unsigned-pad.c, probe-bitfield.c.

## Findings

## F1 — LIKELY (documented in 6e830d68): members that are themselves structs/unions with internal padding are memcmp'd whole, reintroducing the exact false-negative the module exists to prevent — and the documentation does not warn about it

- Location: ccan/structeq/structeq.h:33-35 (whole-struct memcmp path)
  and structeq.h:44 (`STRUCTEQ_MEMBER_CMP_(m) memcmp(&_a->m, &_b->m,
  sizeof(_a->m)) == 0` — member-wise path). Both compare a member's
  full object representation; padding *inside* a nested struct/union
  member is invisible to the macro's accounting because sizeof(member)
  includes that padding.
- Reachable path / reproducer (/tmp/structeq-asan/probe-nested.c):
  ```c
  struct inner { char c; int i; };          /* 3 bytes padding */
  struct outer { struct inner s; int x; };  /* no top-level padding */
  STRUCTEQ_DEF(outer, 0, s, x);             /* whole-struct memcmp path */
  struct outer2 { char tag; struct inner s; int x; };
  STRUCTEQ_DEF(outer2, 3, tag, s, x);       /* member-wise path */
  ```
  Fill a and b with identical member values, copy, then flip one
  padding byte inside b.s. Observed output (identical under clang 18
  ASan+UBSan and gcc 13 `-Wall -Wextra -Werror`, both rc=0, no
  sanitizer diagnostics):
  ```
  outer: UNEQUAL despite identical members (false negative)
  outer2: UNEQUAL despite identical members (false negative)
  ```
  Both code paths are affected. Compiles with zero warnings — the
  BUILD_ASSERT accounting is self-consistent (sum of member sizeofs
  genuinely equals sizeof(outer)) so nothing warns.
- Documented contract: _info — "structeq - bitwise comparison of
  structs... takes into account padding in the structure"; header —
  "@...: name of every member of the structure". A nested struct
  member satisfies "every member" literally, and nothing states that
  members must be padding-free. At the same time the first _info line
  ("bitwise comparison") and the flat member-list design make clear
  (to an implementer, not to a reader of the docs) that padding is
  only accounted for at the top level: @padbytes is defined as
  sizeof(struct) minus the sum of member sizeofs, which cannot see
  inside members. The two readings conflict exactly here.
- Concrete consequence: two structs with identical member values
  compare unequal whenever indeterminate padding bytes inside a nested
  member differ (e.g. after independent initialization, passing
  through a network/disk round trip, or a partially-initialized
  member). This is the classic memcmp-on-padded-struct false negative,
  silent, in a module whose entire selling point is eliminating it.
  No UB is involved (padding bytes read via memcmp are unsigned char
  reads — unspecified values, not traps), so sanitizers stay quiet and
  the failure is data-dependent.
- LIKELY rather than CONFIRMED: the documented mechanism is member
  sizeof accounting, which is definitionally top-level; one can argue
  a padded member type is outside the intended contract, the same
  documented-precondition gray zone as container_of F2. But nothing in
  _info or the header says so, and the failure is silent, hence
  retained as LIKELY.
- Repair direction: documentation note in the header and _info:
  "members are compared bitwise; members that are themselves structs
  or unions containing padding can compare unequal despite equal
  contents — flatten such members into the member list (or use scalar/
  array members only)". A code fix is not feasible within the macro's
  flat member-list design.

## Rejected candidates (disproved)

- R1: Padding bytes compared by memcmp → false negatives (the prime
  suspect). Disproved for top-level padding: the whole-struct memcmp
  path (structeq.h:33-35) is only taken when the sum of member sizeofs
  equals sizeof(struct), i.e. provably no top-level padding exists;
  otherwise the member-wise path skips padding entirely. Verified by
  run-with-padding.c and run-with-unknown-padding.c passing under
  ASan+UBSan and valgrind (ccanlint tests_pass_valgrind +3/3 —
  valgrind would flag any uninitialized padding read). Only the
  *nested* member case survives — that is F1.
- R2: Alignment / strict-aliasing violation. Disproved by reading:
  every access goes through memcmp (const void *, byte-wise, no
  alignment requirement, may alias anything). No type-punning anywhere
  in the expansion.
- R3: Double evaluation / name capture. Disproved by reading: the
  macro generates a static inline function; _a and _b are its
  parameters (evaluated once at the call site), member names are
  identifiers used inside sizeof (unevaluated) and the comparisons.
  Member identifiers cannot collide with the _a/_b parameters because
  members are only ever accessed as `_a->m`/`_b->m`.
- R4: Unsigned-wrapped negative padbytes silently defeating the
  `(padbytes) < 0` test (e.g. `sizeof(char) - sizeof(int)` wraps to a
  huge size_t). Disproved by probe-unsigned-pad.c under both
  compilers: the BUILD_ASSERT then *fails loudly* — algebraically it
  must: with real padding p > 0, the second clause needs
  sum + (2^64 - k) == sizeof, i.e. k == sum - sizeof == -p < 0, but
  k > 0; with no padding it needs padbytes == 0. A wrapped value can
  never satisfy either clause. Fails safe (loud static assertion on
  both gcc and clang).
- R5: Bitfield member. Disproved by probe-bitfield.c:
  `sizeof((_a)->m)` is a hard constraint violation ("sizeof applied
  to a bit-field" / "invalid application of 'sizeof' to bit-field") on
  both compilers. Loud, not silent.
- R6: Float members (+0.0 vs -0.0, NaN != NaN compare unequal
  bitwise). True, but that is the documented contract — _info line 1
  is "bitwise comparison of structs" and the header calls the result
  "a single memcmp() call". Documented behavior; rejected.
- R7: Missing member disguised as padding (e.g. listing 2 of 3 ints
  with padbytes=4 passes the assert and compares only the listed
  members). True, but explicitly documented: "Since it can't tell the
  difference between padding and a missing member, @padbytes can be
  used to assert..." — the caller attests the padding amount.
  Documented limitation; rejected.
- R8: NULL arguments → memcmp(NULL, ...) UB. Violates the evident
  precondition (arguments are pointers to the structs being compared);
  the generated function's const struct sname * prototype documents
  it. Rejected.
- R9: Member listed twice, with real padding exactly equal to the
  duplicated member's size, making sum == sizeof spuriously and taking
  the whole-struct memcmp path over genuine padding (e.g.
  struct {short s; char c;} with STRUCTEQ_DEF(x, 0, s, c, c)). True in
  principle, but requires violating "@...: name of every member"
  (each member exactly once is the only sensible reading) — contrived
  documented-precondition violation. Rejected.
- R10: Non-constant padbytes / zero members / block-scope use /
  flexible-array member. All fail loudly at compile time (static
  assertion requires an ICE; CPPMAGIC_MAP needs at least one member —
  and standard C structs have at least one; `static inline` at block
  scope is a constraint violation; sizeof on a flexible array member
  is a constraint violation). Loud failures; rejected.
- R11: Indeterminate-value UB from memcmp over padding in the F1
  scenario. Disproved: memcmp reads bytes as unsigned char, which has
  no trap representations; indeterminate unsigned char reads yield
  unspecified values, not UB. ASan/UBSan clean on probe-nested.c.
  (MSan/valgrind memcmp interceptors can flag it, but that is a
  property of any memcmp on padded data, folded into F1.)

## Auditor-added test files

None. The single retained finding is LIKELY, not CONFIRMED, so no
failing regression test was added to ccan/structeq/test/ (the
demonstrating probe-nested.c lives in /tmp/structeq-asan/ and is
reproduced verbatim in F1 above; the orchestrator can promote it to a
TAP test if F1 is upgraded).

No production files were modified (structeq.h, _info, and existing
tests untouched).
