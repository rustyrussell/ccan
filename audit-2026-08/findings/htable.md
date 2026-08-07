# Audit: ccan/htable

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: htable.h, htable.c, htable_type.h, _info and ccan/htable/test/
only.  tools/ (speed.c, density.c, hsearchspeed.c, stringspeed.c) are
benchmarks/visualizers, not linked into the module; excluded from defect
scope (they include htable.c directly and add no production code paths).
Deps ccan/compiler and ccan/str already audited; their documented
behavior taken as given (htable only uses stringify/UNNEEDED/COLD —
no misuse found).

Mechanical checks:
- ccanlint: 68/73.  The only check losing points is tests_coverage
  (+1/6; "1611 of 2780 lines covered" across module+headers).  All other
  checks pass, including tests_pass, tests_pass_valgrind and
  tests_pass_valgrind_noleaks.
- Existing tests under clang 18 ASan+UBSan (`-fno-sanitize=function`,
  each test #includes htable.c directly; linked with tap.c, str.c,
  str/debug.c; per-test `timeout 60`), LP64:
  - 10/10 pass cleanly, no sanitizer diagnostics:
    run (43 ok), run-allocator (12), run-clash (64262), run-copy (1536),
    run-debug (36; the CCAN_HTABLE_DEBUG variant), run-extra (assert-
    based, rc=0), run-size (1024), run-type (36), run-type-int (29),
    run-zero-hash-first-entry (6).
- Same 9 TAP tests also built and run under `-m32` ASan+UBSan (ILP32):
  all pass cleanly, no sanitizer diagnostics.
- Extra debug-knob variant: plain run.c compiled with
  -DCCAN_HTABLE_DEBUG crashes — test-harness artifact, not a module
  defect; see R1.
- Reproducers live in /tmp/htable-asan/repro-*.c (temporary, not
  committed).

Auditor-added test files (temporary; keep or remove later):
- ccan/htable/test/run-init-sized-alloc-fail.c — proves F1 (currently
  fails: tests 3-5; designed to fail *without* tripping ASan so it can
  stay in the suite pre-fix).
- ccan/htable/test/run-init-sized-huge.c — proves F2 on 32-bit
  (currently fails under -m32: "requested 0, correct 4294967296";
  passes on 64-bit both before and after the fix).

## Findings

## F1 — CONFIRMED (FIXED in af8ff95e): htable_init_sized() allocation failure leaves ht->bits != 0 with a singleton table; next htable_add() writes out of bounds

- Location: ccan/htable/htable.c:101-120 `htable_init_sized()`.  The
  sizing loop sets `ht->bits` (line 108) *before* the allocation; on
  allocation failure (lines 114-117) only `ht->table` is restored to
  `&ht->common_bits` — `ht->bits` keeps the computed value (1..30).
- Reachable path:
  1. Caller uses the documented htable_set_allocator() API (or plain
     calloc under memory pressure with a large `expect`), so
     `htable_alloc(ht, sizeof(size_t) << ht->bits)` at htable.c:113
     returns NULL.
  2. htable_init_sized() returns false.  htable.h:70-72 documents:
     "If this returns false, @ht is still usable, but may need to do
     reallocation upon an add."
  3. Caller adds an element (documented contract).  In htable_add_
     (htable.c:390) the growth check `ht->elems+1 + ht->deleted >
     ht_max(ht)` is false because ht_max() is large for the leftover
     bits value, so no double_table() repair happens.  ht_add()
     (htable.c:290) computes `hash_bucket(ht, h) = h & ((1 << bits)-1)`
     — up to 2^30-1 — and probes/writes `ht->table[i]` where
     ht->table == &ht->common_bits, a single uintptr_t inside the
     struct.  Any bucket index >= 1 writes past the field; index 1
     overwrites ht->table itself, larger indices land past the struct.
- Preconditions: none violated — custom allocators are a documented
  feature (htable.h:305-310) and the post-failure usability is
  explicitly documented.
- Concrete consequence: out-of-bounds read/write on the first add after
  a failed htable_init_sized(); silent corruption of the htable struct
  (bucket 1 clobbers ht->table → later wild pointer dereference) or of
  adjacent stack/heap memory.  Memory-corruption severity, reachable
  with a trivially small (16 KB) failing allocation.
- Reproducer: /tmp/htable-asan/repro-init-sized-fail.c (custom
  allocator fails allocations > 64 bytes; init_sized(expect=1000) fails
  with bits=11; one htable_add).  Observed under clang 18 ASan+UBSan:
  ```
  ==ERROR: AddressSanitizer: stack-buffer-overflow on address ... 
  READ of size 8 ... 
    This frame has 2 object(s):
      [32, 104) 'ht' (line 38) <== Memory access at offset 112 overflows this variable
  ```
  (read in ht_add's probe loop at htable.c:297; the write at line 301
  would follow).  Regression test
  ccan/htable/test/run-init-sized-alloc-fail.c fails as designed:
  `not ok 3 - ht.bits == 0`, `not ok 4 - htable unusable after failed
  htable_init_sized (bits=11)`.
- Repair direction: on the failure path reset the sizing state, e.g.
  set `ht->bits = 0;` alongside `ht->table = &ht->common_bits;`
  (equivalently re-run htable_init, which preserves rehash/priv).  With
  bits == 0 the invariant "table == &common_bits ⟹ bits == 0" that
  every other path maintains is restored, and the first add falls into
  double_table() as for a plain htable_init() table.

## F2 — CONFIRMED (FIXED in d3da4521) (32-bit only): htable_init_sized() allocation size wraps to 0 at the bits==30 cap; returns true with a 0-byte table, first add writes OOB

- Location: ccan/htable/htable.c:113
  `ht->table = htable_alloc(ht, sizeof(size_t) << ht->bits);`
  combined with the sizing cap at htable.c:108-111
  (`if (ht->bits == 30) break;`).
- Reachable path (ILP32): caller passes `expect > ht_max(bits=29)` =
  469,762,048 (e.g. `htable_init_sized(&ht, hash, NULL, 469762049)`).
  The loop stops at bits == 30; the size computation
  `(size_t)4 << 30` = 2^32 wraps (well-defined unsigned wraparound) to
  0.  calloc(0, 1) *succeeds* (glibc returns a unique minimal chunk),
  so htable_init_sized() returns true — asserting (htable.h:72-73) "it
  will not need to reallocate within @size htable_adds" — and the first
  htable_add() probes/writes `table[h & (2^30-1)]` into a zero-byte
  allocation.  No allocation failure is required on this path, unlike
  F1.
- Preconditions: none violated; `expect` is an unbounded size_t hint
  and the bits==30 cap is the module's own "Don't go insane with
  sizing" guard (htable.c:107) — the guard itself triggers the wrap on
  32-bit.
- Concrete consequence: heap-buffer-overflow (read then write) on the
  first add, on any 32-bit target, from a single legal API call.
  Corruption severity.  (The same wrap exists in double_table() at
  htable.c:313 for bits 29→30 and 30→31 on 32-bit, but reaching it by
  growth needs hundreds of millions of live elements — impractical in a
  32-bit address space; init_sized is the reachable vector.)
- Reproducer: /tmp/htable-asan/repro-init-sized-32bit-wrap.c built
  `-m32 -fsanitize=address,undefined`.  Observed:
  ```
  ==ERROR: AddressSanitizer: heap-buffer-overflow on address 0xe890079c ...
  READ of size 4 at 0xe890079c thread T0
  0xe890079c is located 11 bytes after 1-byte region [0xe8900790,0xe8900791)
  allocated by thread T0 here: ... calloc ... htable_init_sized ...
  ```
  Regression test ccan/htable/test/run-init-sized-huge.c (intercepts
  the allocator and compares the requested size against the
  overflow-free 64-bit computation) fails under -m32 as designed:
  `not ok 2 - alloc size for bits==30: requested 0, correct 4294967296`;
  passes on LP64.
- Repair direction: make the sizing loop overflow-aware instead of the
  fixed cap of 30, e.g. break/fail when
  `(sizeof(size_t) << ht->bits) >> ht->bits != sizeof(size_t)` (or cap
  bits at `CHAR_BIT * sizeof(size_t) - 1 - 3` on ILP32, i.e. 28), and
  apply the same guard to the double_table() size computation at
  htable.c:313.

## F3 — LIKELY (FIXED in ba8c4ad2) (UB at extreme scale): `1 << ht->bits` signed int shifts once bits reaches 31+

- Location: five sites use `int` literals against a table that can in
  principle grow past 2^30 buckets:
  - htable.c:156 `hash_bucket()`: `h & ((1 << ht->bits)-1)`
  - htable.c:169 `htable_val()`: `i->off = (i->off + 1) & ((1 << ht->bits)-1)`
  - htable.c:185 `htable_nextval_()`: same expression
  - htable.c:299 `ht_add()`: `i = (i + 1) & ((1 << ht->bits)-1)`
  - htable.c:354 `rehash_table()`: `(i + start) & ((1 << ht->bits)-1)`
  (All other sites correctly use `(size_t)1 << ht->bits`.)
- Path: htable_init_sized() caps bits at 30, but double_table()
  (htable.c:318, `ht->bits++`) has no cap.  At bits == 31,
  `1 << 31` shifts into the sign bit of a 32-bit int — UB per C11
  6.5.7p4 (and `(1<<31)-1` then signed-overflows; UBSan
  -fsanitize=shift,signed-integer-overflow would fire).  At bits == 32
  the practical x86 behavior (shift count masked to 5 bits) yields a
  zero mask: hash_bucket() always returns 0 and ht_add()'s probe loop
  `i = (i+1) & 0` never advances — an infinite loop once bucket 0 is
  occupied.
- Why LIKELY, not CONFIRMED: bits == 31 requires > ht_max(30) =
  939,524,096 live elements and a successful 16 GB table allocation;
  bits == 32 needs ~1.9 G elements and 32 GB.  Reachable only on a
  large 64-bit host; not reproducible within test resources (churn and
  growth tests were run to millions of elements only).  No sanitizer
  output observed; classification rests on static UB.
- Repair direction: replace `1 << ht->bits` with `(size_t)1 <<
  ht->bits` (or reuse hash_bucket()) at the five sites; optionally cap
  double_table() at bits < sizeof(size_t)*CHAR_BIT - 1 for symmetry
  with ht_max().

## Rejected candidates (disproved)

- R1: plain run.c compiled with -DCCAN_HTABLE_DEBUG crashes (UBSan
  misaligned load + ASan SEGV in the test's own hash() at run.c:15).
  Test-harness artifact: run.c deliberately inserts a bogus pointer
  (`~(uintptr_t)&val[...]`) and its hash function dereferences every
  element when CCAN_HTABLE_DEBUG makes htable_check() rehash them.
  run-debug.c is the supported debug variant and guards exactly this
  via `bad_pointer`; it passes 36/36 under ASan+UBSan.  Not a module
  defect.
- R2: rehash/resize invariants under adversarial hashes (all keys in 4
  hash values → maximal clash chains, deleted-marker reuse, wrap-around
  probe, rehash_table cleanup path, iteration interleaved with
  htable_delval).  Disproved: /tmp/htable-asan/repro-churn.c — 3000
  rounds with CCAN_HTABLE_DEBUG (every public op runs htable_check())
  and 2,000,000 rounds without it, under ASan+UBSan: clean, element
  membership model exactly matches.  Probing always terminates because
  elems+deleted <= 7/8 of capacity is enforced before every ht_add.
- R3: deleted-marker/perfect-bit collisions with hash 0 or all-ones.
  Disproved by construction and by test: stored entries satisfy
  e > HTABLE_DELETED because make_hval() results <= 1 are caught and
  repaired via update_common_fix_invalid() (htable.c:301-303), which
  exposes another real pointer bit; the perfect bit only ever marks
  entries in their ideal bucket and is cleared from the match key after
  the first probe step (htable.c:170).  run-zero-hash-first-entry.c,
  run-clash.c (64262 clash pairs) and run.c's bogus-pointer sections
  pass under ASan+UBSan, 32- and 64-bit.
- R4: OOM in double_table() mid-add.  Disproved:
  /tmp/htable-asan/repro-oom-double.c — allocator capped at 64 KB;
  growth stops at bits=13, htable_add_ returns false with the old table
  restored (htable.c:314-317), htable_check() passes, all 7168 elements
  still found, deletes still work, no ASan/LSan diagnostics.
- R5: OOM/half-init in htable_copy_.  Disproved by inspection: the
  allocation (htable.c:131) precedes any state change; on failure *dst
  is untouched.  Copying an empty table allocates one bucket and
  memcpy's the common_bits field as the single (zero) entry — a
  consistent bits==0 heap table; subsequent adds double it normally
  (covered by run-copy.c under ASan).
- R6: fixup_table_common() reconstructing a pointer with wrong
  maskdiff bits when an entry goes invalid (htable.c:256-258).
  Disproved by inspection: the wrongly-valued bits are exactly those in
  maskdiff, which unset_another_common_bit() excludes from its search
  (`common_mask & ~*maskdiff`, htable.c:228); the selected bit is a
  genuine set bit of the real pointer.  The restart loop is bounded
  (<= 64 restarts).  Exercised heavily by run.c/run-debug.c
  bogus-pointer sections and the churn test.
- R7: htable_firstval/nextval non-termination with a probe path full
  of deleted markers.  Disproved: the probe stops at the first 0
  bucket; capacity is kept <= 87.5% and rehash_table() purges deleted
  markers once they exceed 12.5%, so a 0 bucket always exists.
  rehash_table()'s scan for the first empty bucket (htable.c:351) is
  likewise guaranteed to terminate.
- R8: htable_type.h macro pitfalls (double evaluation, silent type
  mismatch).  Disproved: keyof/hashfn arguments are evaluated exactly
  once in each generated inline; HTABLE_KTYPE uses typeof in an
  unevaluated context; key/elem confusion is a compile error, not
  silent.  run-type.c and run-type-int.c (value keys via the
  HTABLE_KTYPE override) pass.  The NODUPS duplicate check being
  assert()-based (htable_type.h:187) means NDEBUG builds accept
  duplicate keys — a documented-pattern debug check, no memory-safety
  consequence; style-level.
- R9: doc nits in htable_type.h (`bool <name>_delval` documented at
  line 47 vs void definition at line 139) and htable.h htable_copy
  pseudo-code (`htable_add(dst, v)` missing the hash argument).
  Documentation-level only, no code defect.
- R10: htable_init_sized() returning true when expect exceeds
  ht_max(30) (>= ~940M on LP64), breaking the "will not need to
  reallocate within @size adds" promise.  Consequence is one
  unexpected-but-correct reallocation; no correctness or safety impact.
  Rejected as benign (distinct from F2, where the same cap causes
  memory corruption on 32-bit).
- R11: alignment/strict-aliasing and endianness.  All pointer
  bit-fiddling goes through uintptr_t values, never through misaligned
  accesses; no endian-dependent logic.  32-bit (-m32) runs of the full
  suite are clean.
- R12: htable_set_allocator() switching allocators while tables are
  live → mismatched alloc/free pairs.  Requires violating the implied
  caller precondition (the API is global setup, used by run-allocator.c
  exactly that way); caller misuse.
- R13: htable_add() of (void*)1 or NULL under NDEBUG, hash mismatch in
  htable_del, iteration concurrent with htable_add.  All violate
  documented preconditions (htable.h:153, 158, 165-175); the
  assert()s document intent.  Out of scope per AGENTS.md.
