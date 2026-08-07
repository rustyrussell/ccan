# Audit: ccan/strset

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: strset.h (167 lines), strset.c (309 lines), _info and
ccan/strset/test/ only. Deps per _info: ccan/ilog, ccan/likely,
ccan/short_types, ccan/str, ccan/typesafe_cb. ilog not yet audited;
strset uses it only as `ilog32_nz((u8)str[b] ^ bytes[b]) - 1` with a
guaranteed-nonzero argument (strset.c:121) — matches ilog32_nz's
contract. The other four deps were previously audited; no misuse found.
ccan/strmap is "based on ccan/strset.c" but is a separate copy, not a
dependency — out of scope (see note under F1).

## Mechanical checks

- ccanlint (before auditor additions): **53/59**, every check PASS;
  the only partial credit is tests_coverage (+1/6, "556 of 902 lines
  covered"). tests_pass, tests_pass_valgrind, tests_pass_valgrind_noleaks,
  examples_compile all PASS.
- After adding the auditor test: ccanlint **54/56 FAIL** — exactly as
  designed, because test/run-deep-recursion.c dies with SIGSEGV against
  the current code (F1).
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes
  ccan/strset/strset.c directly; linked with ccan/tap/tap.c; per-test
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0),
  LP64: **5/5 pass cleanly, zero sanitizer diagnostics** —
  run (36 ok), run-hibit (2500 ok), run-iterate-const (3 ok),
  run-order (3500 ok), run-prefix (118 ok). Total 6157 ok.
- -m32: skipped — module includes errno.h, and the 32-bit build fails on
  missing `asm/errno.h` (per audit instructions).
- Reproducers/probes in /tmp/strset-asan/ (temporary, not committed):
  repro-fuzz.c, repro-fuzz2.c (model-based fuzzers), repro-prefix.c
  (targeted prefix probe), repro-depth.c, repro-depth2.c,
  repro-depth3.c (recursion/stack probes), rlimit-probe.c,
  tap-depth-bigstack.c.

## Findings

## F1 — CONFIRMED (FIXED in b5dcf30e + 19b73d88 + 51c4da92): strset_iterate_() and strset_clear() recurse once per tree level; an adversarially deep tree (built via documented API calls alone) overflows any fixed stack

- Location: ccan/strset/strset.c:231-241 (`static bool iterate()`,
  reached from strset_iterate_() at strset.c:243-251) and
  strset.c:293-302 (`static void clear()`, reached from
  strset_clear() at strset.c:304-309). Both are plain recursive tree
  walks with one stack frame per internal node on the spine.
- Reachable path: insert keys, then call the documented
  strset_iterate() / strset_clear(). Tree depth is bounded only by
  8 x (longest key length) and by the number of keys; a "staircase"
  key set — s_k = (k/8) bytes of 0xFF followed by byte 0xFF<<(k%8)
  (or 0x01) — forces a chain of depth D-1 from D keys totalling only
  ~D^2/16 bytes (8 split positions consumed per key byte, the maximum
  the critbit invariant allows). All keys are ordinary C strings; no
  documented precondition is violated.
- Caller preconditions: none. strset.h documents no depth, key-length,
  or key-distribution limits; strset_add/get/del/prefix are themselves
  iterative, so only iteration and clear are affected.
- Concrete consequence: SIGSEGV (stack exhaustion) — a denial of
  service in any program that iterates or clears a strset whose keys
  an attacker can influence (the module's advertised use is ordered
  storage of arbitrary strings such as words/paths). Measured on this
  host (x86-64, clang 18):
  - frame size ~70 bytes at -O0, ~16 bytes at -O2 (for clear(); at
    -O1/-O2 iterate()'s second recursive call is a tail call, so a
    right-leaning chain only crashes clear() — at -O0 both crash);
  - a 128 KiB stack (the musl default thread stack size) overflows at
    depth ~1900 (-O0) / ~7400 (-O2); 256 KiB at ~3700 / ~15700;
  - depth 7400 costs the attacker ~3.4 MB of keys (7400 staircase
    strings, avg ~460 bytes). On a default 8 MiB main stack the same
    attack needs depth ~500k, i.e. ~16 GB of keys — impractical there;
    the realistic exposure is threads/small-stack contexts.
- Observed output:
  - /tmp/strset-asan/repro-depth3 (clang -O2, `ulimit -s 256`,
    depth 24000 staircase): prints "PHASE iterate done 24000" then
    `Segmentation fault (core dumped)` inside strset_clear()
    (rc=139). Same binary at -O0 dies inside strset_iterate_().
  - Bisected thresholds (depth vs `ulimit -s`):
    -O2: 128K->~7414, 256K->~15710; -O0: 128K->~1882, 256K->~3726.
  - Auditor TAP test under clang ASan:
    `AddressSanitizer: stack-overflow ... (pc ... sp 0x7ffdebc57fd0)`.
- Regression test: ccan/strset/test/run-deep-recursion.c (alarm(60)-
  bounded, 3 subtests). It lowers RLIMIT_STACK to 128 KiB in-process
  (verified on this host that Linux enforces the lowered limit on
  subsequent main-stack growth), builds a depth-12000 staircase
  (~9 MB of keys, ~1.2 s plain, ~9 s under valgrind), then iterates
  and clears. Current code: SIGSEGV at -O0 (in strset_iterate_, after
  subtest 1) and at -O2 (in strset_clear, after subtest 2); ccanlint
  reports FAIL. A big-stack variant (same test with a 64 MiB rlimit)
  passes 3/3, confirming the tree itself is well-formed and the test
  must pass once the walks are iterative. Note: under valgrind the
  lowered rlimit is not enforced, so the test passes there even
  unfixed — the plain tests_pass run is the one that catches it.
- Repair direction: convert iterate() and clear() to explicit-stack
  (or parent-pointer-free iterative) traversals. A simple option for
  clear() is a loop that frees leaf-parenting nodes from the bottom
  using the child pointers themselves as the work list; for iterate()
  a small dynamic stack of struct strset (bounded by depth) preserves
  the documented in-order, early-exit-on-false semantics.
- Note for the orchestrator: ccan/strmap (not yet audited) is a copy
  of this code and has the identical recursive iterate()
  (ccan/strmap/strmap.c:181-189) and clear() (strmap.c:238-242);
  expect the same finding there.

## Rejected candidates (disproved)

- R1: Delete-during-iterate corrupts the walk. Documented
  precondition: "You should not alter the set within the @handle
  function!" (strset.h:115). Out of scope.
- R2: Empty-string magic node (byte_num == (size_t)-1, string stored
  at child[0], child[1] never initialized). Verified every path:
  add/get/del of "" as sole member, with other members, as root and
  as a child; duplicate "" (EEXIST); del of non-empty member when the
  walk hits the magic node (ENOENT, strset.c:190-193); prefix ""
  returning the whole set including ""; iterate/clear over the magic
  node. child[1] is never read anywhere (closest/del/iterate/prefix
  all use child[0] and break; clear skips recursion for the magic
  node) — ccanlint tests_pass_valgrind_noleaks is clean. Covered by
  run.c, run-prefix.c, both fuzzers (empty strings in the alphabet)
  and the targeted prefix probe. No defect.
- R3: strset_prefix() top-tracking (`if (c) top = n`, strset.c:280).
  c != 0 iff byte_num < len (prefix bytes are nonzero), so top is the
  subtree below the deepest node splitting inside the prefix; the
  final strstarts() leaf check (strset.c:284) rejects absent prefixes.
  Correctness argued via the critbit LCA property (any member of top's
  subtree differing from the prefix within its length would be
  separated from the verified leaf by a node with byte_num < len,
  contradicting the strictly-increasing split positions), and verified
  empirically: /tmp/strset-asan/repro-prefix.c checks every prefix of
  every member plus near-misses on a set containing "", high-bit and
  shared-prefix keys (PREFIX OK), and both fuzzers cross-check prefix
  counts against a sorted-array model. No defect.
- R4: strset_del() parent/direction tracking, including the
  sew-the-empty-string-back path (strset.c:187-199): parent is
  assigned before descending, so when the magic node is collapsed in
  place, parent/direction still refer to the real parent node; the
  stored pointer is saved into `ret` before the parent node is freed
  (strset.c:216-226). Del-heavy fuzzing (including "" members) under
  ASan+UBSan clean. No defect.
- R5: strset_iterate macro double evaluation (strset.h:136-140): arg
  appears once under __typeof__ (unevaluated) inside
  typesafe_cb_preargs and once as the value argument; set and handle
  once each. No side-effect hazard.
- R6: bit_num computation (strset.c:121): the differ-loop guarantees
  str[byte_num] != member[byte_num], so the xor is nonzero, the u8
  cast is value-preserving, ilog32_nz result is 1..8, bit_num 0..7;
  the assert is correct. High-bit bytes (0x80-0xFF) exercised by
  run-hibit.c and the fuzzers. No defect.
- R7: Insertion-walk break on the magic node (strset.c:148):
  (size_t)-1 > any real byte_num, so the walk always breaks above the
  magic node; the empty string always sorts to child[0] (direction 0
  at every split since len == 0), which is where every other walk
  expects it. No defect.
- R8: OOM paths: strset_add frees newn when set_string fails
  (strset.c:136-139); the magic-node malloc failure leaves nothing
  allocated; errno set on all failure paths. Reading only (no
  allocation-failure injection without failtest); no defect found.
- R9: Doc nit (not a defect): strset_prefix's doc says "You can use
  strset_iterate(), strset_test() or strset_empty()" (strset.h:151) —
  there is no strset_test(); presumably strset_get() was meant.
- R10: -m32 concerns: none identifiable — all indexing is size_t, bit
  math is u8; -m32 build itself unavailable on this host (missing
  32-bit asm headers via errno.h), skipped per instructions.
- R11: Unbounded-recursion variants for strset_add/get/del/prefix:
  all four are iterative loops (closest() strset.c:36-58, insertion
  walk strset.c:142-159, del walk strset.c:183-208, prefix walk
  strset.c:266-282). Only iterate/clear recurse — that is F1.

## Auditor-added test files (temporary; keep or remove later)

- ccan/strset/test/run-deep-recursion.c — proves F1. Currently dies
  with SIGSEGV (stack overflow) in plain builds at both -O0 and -O2
  and under ASan (stack-overflow report); passes under valgrind
  (rlimit not enforced there) and passes everywhere once iterate()/
  clear() are iterative. alarm(60)-bounded; frees all keys after
  clear so tests_pass_valgrind_noleaks stays clean post-fix.
- ccanlint with the file present: 54/56 FAIL, solely because
  run-deep-recursion.c crashes against the current code.

No production files were modified (strset.h, strset.c, _info
untouched; verified by git status — the only new path under
ccan/strset is the test above).
