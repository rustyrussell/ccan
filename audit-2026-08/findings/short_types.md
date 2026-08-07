# Audit: ccan/short_types

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: short_types.h (35 lines, header-only typedefs: u64/s64/u32/s32/
u16/s16/u8/s8, plus be64..le16 gated on CCAN_ENDIAN_H), _info and
ccan/short_types/test/ only. No dependencies; testdepends on ccan/endian
(documented behavior taken as given — beint64_t etc. are plain typedefs
of uint64_t/32/16 per endian.h:139-144, NOT struct wrappers).

## Mechanical checks

- ccanlint (before any auditor additions): **33/34**. The only lost
  point is examples_exist (+1/2: "short_types.h: No Example: section") —
  a documentation-completeness nit, not a defect. All other checks pass,
  including tests_compile, tests_pass (+2/2), tests_pass_valgrind (+2/2)
  and, notably, examples_compile (+2/2) despite the broken _info example
  (see F1 — stringify() hides the error from the compiler).
- Tests under clang 18.1.3 ASan+UBSan
  (`-fsanitize=address,undefined -fno-sanitize=function -g -I.`,
  `ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0`,
  per-test `timeout 60`), LP64: **2/2 pass, zero sanitizer
  diagnostics** — run.c 16 ok, run-endian.c 6 ok (22 subtests total).
  run-endian.c includes ccan/endian/endian.h only as a header; linked
  with ccan/tap/tap.c.
- -m32 (clang): builds and runs fine, 16/16 ok (this module does not
  include errno.h, so the missing-32-bit-asm-headers issue does not
  apply).
- Reproducers/probes in /tmp/short_types-asan/ (temporary, not
  committed): info-example.c, probe-order1.c, probe-order2.c,
  probe-alias.c.

## Findings

## F1 — CONFIRMED (FIXED in 172ae94c): _info example's EVALUATE macro stringifies the undeclared token `sht` instead of its `short` parameter; the example compiles cleanly and prints garbage type names

- Location: ccan/short_types/_info:43-44:
  ```c
  #define EVALUATE(psx, short, pt, st, t)				\
  	evaluate(sizeof(psx), stringify(psx), stringify(sht), pt, st, t)
  ```
  The second macro parameter is named `short`, but the body references
  `sht`. `stringify(sht)` → `#x` stringifies the *literal tokens*
  without ever evaluating `sht` as an identifier, so the compiler sees
  no undeclared identifier: the example compiles with zero diagnostics
  (this is why ccanlint examples_compile passes) and every invocation
  passes the constant string `"sht"` where the type name was meant.
- Reachable path: anyone copy-pasting the documented Example: section
  verbatim (its only purpose). Reproducer
  (/tmp/short_types-asan/info-example.c): the example extracted
  character-for-character from _info; `cc -I.` compiles it clean.
- Observed output (reproducer run):
  ```
  Comparing size of type vs size of name:
  	signed ht: POSIX 700%, short 300%
  	signed ht: POSIX 600%, short 300%
  	... (all 8 lines identical "signed ht")
  ```
  Intended output is "unsigned 8: POSIX 800%, short 25%" etc. Two
  distinct corruptions: (a) the name column is always the literal "ht"
  (`sht+1`) with spurious "signed" (`sht[0]=='s'`), and (b) the
  "short %" column and the totals use strlen("sht")=3 instead of the
  real 2-character short names, skewing the joke conclusion numbers
  (20% instead of the intended 25%-based figure).
- Preconditions: none — this is the module's own documentation.
- Consequence: the module's showcase example produces visibly wrong
  output; readers copying it inherit a broken macro. Documentation
  defect only — no production code is affected (short_types.h itself is
  untouched by this).
- Minimal repair direction: rename the parameter, i.e.
  `#define EVALUATE(psx, sht, pt, st, t)` (keep the body unchanged).
  As a side benefit this drops the C++ keyword `short` as a macro
  parameter name (legal in C, which is why it compiles today).
  No TAP regression test was added: the defect lives in _info
  documentation text, not in compilable module code, so a test in
  ccan/short_types/test/ cannot exercise it; the reproducer in
  /tmp/short_types-asan/info-example.c is the evidence.

## Rejected candidates (disproved)

- R1: "Endian-typed structs' conversion helpers" (the assigned suspect).
  Disproved by reading: ccan/endian/endian.h:139-144 defines
  beint64_t/leint64_t etc. as plain `typedef uint64_t ENDIAN_TYPE ...`,
  not structs (ENDIAN_TYPE is empty outside sparse `__CHECKER__`
  builds). short_types' be64/le64 are therefore the *same type* as
  u64; the conversion helpers (cpu_to_be64/be64_to_cpu) belong to
  ccan/endian, which is a dependency whose documented behavior is taken
  as given. No struct, no helper duplication in short_types.
- R2: Strict-aliasing violation on be64/le64 loads via cast instead of
  memcpy. Disproved: because be64 IS uint64_t (R1), casting `u64 *`
  ↔ `be64 *` involves identical types — no aliasing question arises at
  all. probe-alias.c loads the same 8-byte buffer via memcpy, via
  `*(u64 *)&be64_var`, and converts both ways: all values agree
  (be=0x0102030405060708, le/cast/memcpy=0x807060504030201) under clang
  ASan+UBSan, rc=0. (The `__attribute__((bitwise))` sparse annotation
  only exists under `__CHECKER__` and affects nothing at compile/run
  time.)
- R3: Include-order dependence of the be/le typedefs. Disproved:
  short_types.h:19 gates its block on `CCAN_ENDIAN_H`, and endian.h:348
  carries the reciprocal block gated on `CCAN_SHORT_TYPES_H`, so
  whichever header is included first, the second one defines
  be64/be32/be16/le64/le32/le16. probe-order1.c (short_types→endian)
  and probe-order2.c (endian→short_types) both compile clean and
  round-trip 0x0102030405060708 through cpu_to_be64/be64_to_cpu and
  cpu_to_le64/le64_to_cpu correctly, ASan+UBSan clean.
- R4: Double typedef when both reciprocal blocks are reachable.
  Disproved: each block is guarded by the *other* header's include
  guard, so each block can fire at most once and only in the opposite
  include order (R3); and a repeated typedef of the same type is legal
  C11 anyway. No reachable redefinition error.
- R5: Signedness/size of the base typedefs. The module's own run.c
  (16 subtests: all eight sizes, `(uN)-1 > 0`, `(sN)-1 < 0`) passes
  under clang ASan+UBSan on LP64 and under -m32. The typedefs are direct
  aliases of the stdint.h types; nothing to get wrong.
- R6: be/le typedefs missing when ccan/endian/endian.h is not included.
  True by design and documented: _info states the be/le names are
  provided "If ccan/endian/endian.h is included". Not a defect.
- R7: The `__CHECKER__`/sparse ENDIAN_CAST forcing comment
  (endian.h:130-137 mentions short_types). Build-time annotation
  plumbing for sparse only; no effect on gcc/clang compilation or
  runtime. Not a correctness issue in this module.
- R8: ccanlint's lost point (no Example: section in short_types.h).
  Documentation-completeness nit; AGENTS.md excludes style issues.

## Auditor-added test files

None. The single confirmed finding (F1) is in _info documentation text
and cannot be exercised by a TAP test under ccan/short_types/test/;
evidence is the /tmp reproducer and its transcript above.

No production files were modified (short_types.h and _info untouched;
verified by `git status --short ccan/short_types/` — clean).
