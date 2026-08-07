# Audit: ccan/crypto/shachain

Date: 2026-08-05. Auditor: kimi-code (per AGENTS.md procedure).
Scope: shachain.h (138 lines), shachain.c (126 lines), _info, design.txt,
ccan/crypto/shachain/test/ (4 tests). Dependencies: ccan/ilog,
ccan/crypto/sha256 — treated as given (one cross-module note in C1 (FIXED in 43cfe853)).

Key documented contract (shachain.h):
- shachain_from_seed(seed, index, hash): "index of value to generate
  (0 == seed)"; "no way to derive the result from that generated for
  any *greater* index" (shachain.h:16-20).
- SHACHAIN_BITS is an explicitly supported compile-time override
  (shachain.h:9-11 `#ifndef SHACHAIN_BITS`; commit 4bb69fe6 "shachain:
  allow overriding of number of bits"; test/run-8bit.c exercises
  SHACHAIN_BITS=8). `known[]` has SHACHAIN_BITS + 1 slots (shachain.h:58).
- shachain_next_index: "what's the next index I can add to the
  shachain?" (shachain.h:70-76).
- shachain_add_hash: "You can only add shachain_next_index(@chain)"
  (shachain.h:85) — the only documented add precondition.
- shachain_init: "Alternately, ensure that it's all zero"
  (shachain.h:65).

## Mechanical checks

- ccanlint (before auditor additions): **53/57**, every check PASS;
  shortfall is only partial credit on tests_coverage (+2/6).
- After adding the auditor regression test: ccanlint **47/51 FAIL** —
  exactly as designed: tests_pass loses +4/4 because
  run-exhaust-wrap fails against the current code (it compiles fine).
- Existing tests under clang 18.1.3 ASan+UBSan (`-fno-sanitize=function`,
  `-fno-sanitize-recover=all`, -g, -I.; each test #includes shachain.c
  per convention; linked with sha256.c, ilog.c, tap.c; per-test
  `timeout 60`, ASAN_OPTIONS=abort_on_error=1:symbolize=0:detect_leaks=0),
  LP64:
  - run.c: **2749/2749 clean** (top-of-chain indices near 2^64-1).
  - run-8bit.c (SHACHAIN_BITS=8): **66559/66559 clean**, full chain
    255..0 including the index-0/seed slot.
  - run-badhash.c: **1000/1000 clean** (corrupted-hash rejection).
  - run-can_derive.c: **65536/65536 clean** (can_derive vs design.txt
    naive implementation, all 8-bit from/to pairs).
  Zero sanitizer diagnostics anywhere.
- -m32: contrary to the audit-brief default this module's chain
  (sha256.c, ilog.c, tap.c) does not pull in errno.h, so a
  clang -m32 ASan+UBSan build of run.c succeeded: **2749/2749 clean**.
- Cross-check against the documented design (Lightning BOLT 3,
  Appendix D, fetched from lightning/bolts master): reproducer
  /tmp/shachain-asan/repro-bolt3-vectors.c (built with
  -DSHACHAIN_BITS=48, the BOLT index width) passes **87/87**:
  all 5 `generate_from_seed` vectors byte-exact (seeds 00.., FF..,
  01..; I = 0xFFFFFFFFFFFF, 0xAAAAAAAAAAA, 0x555555555555, 1), the
  8-step `insert_secret correct sequence` storage vector with
  get_hash re-derivation of every prior secret at each step, and the
  `insert_secret #1 incorrect` corruption-detection vector (first bad
  secret at ctz-0 index is undetectable — accepted, as the header
  documents — the next one is rejected). The implementation's
  high-to-low bit walk and change_bit() byte/bit layout match BOLT 3
  exactly.
- Probes/reproducers in /tmp/shachain-asan/ (temporary, not
  committed): repro-bolt3-vectors.c, repro-wrap-oob.c,
  repro-wrap-sym, shadow/ (one-line repair used to validate the
  regression test).

## Findings

## F1 — CONFIRMED (FIXED in b4316b44): post-exhaustion `shachain_add_hash` writes/reads past `known[]` when SHACHAIN_BITS < 64

- Location: ccan/crypto/shachain/shachain.c:89
  (`pos = count_trailing_zeroes(index);`), used unbounded at
  shachain.c:93-100 (consistency loop reads `chain->known[i]` for
  i < pos) and shachain.c:102-103 (`chain->known[pos].index/.hash`
  write). Root enabler: shachain.c:71
  (`return chain->min_index - 1;`) wraps next_index to UINT64_MAX
  after index 0 has been added, and `count_trailing_zeroes()`
  (shachain.c:13-26) operates on the full uint64_t domain, returning
  up to 63 for nonzero indices — while `known[]` has only
  SHACHAIN_BITS + 1 entries.
- Reachable path (only possible when SHACHAIN_BITS < 64; see
  disproof for the 64-bit default in R1): build with e.g.
  -DSHACHAIN_BITS=8, then obey the documented rule. Add 255..0 —
  the whole 8-bit chain — each index taken from
  shachain_next_index(). After the index-0 add, next_index returns
  0 - 1 = UINT64_MAX, which is still "the next index I can add" per
  the header, and the assert at shachain.c:87 passes. Continuing
  with consistent hashes (shachain_from_seed is total over uint64_t,
  so every consistency check passes), 511 further adds reach
  index 0xFFFFFFFFFFFFFE00, whose count_trailing_zeroes() is
  9 > SHACHAIN_BITS: the loop at shachain.c:93 reads known[0..8]
  (in bounds), then shachain.c:102 writes known[9] — one slot
  (struct {uint64_t; sha256} = 40 bytes) past the array, and `known`
  is the last member of struct shachain, so the write lands past the
  user-allocated object. Later indices (ctz 10..63) also read known[]
  out of bounds in the check loop.
- Caller preconditions: the only documented add precondition
  (shachain.h:85 — add exactly shachain_next_index()) is followed at
  every step. Nothing documents that next_index becomes invalid once
  index 0 has been added, and nothing rejects indices >=
  2^SHACHAIN_BITS. Aggravating factor: shachain.h:65 documents
  all-zero as an alternative to shachain_init(), but in a
  SHACHAIN_BITS < 64 build a zeroed struct has min_index = 0, so its
  very first next_index is UINT64_MAX — out of domain from the start
  (shachain_init sets 2^BITS instead). Same root cause, same fix.
- Concrete consequence: out-of-bounds write of attacker-influenced
  (well, hash-derived) 40-byte records past struct shachain, plus OOB
  reads. With SHACHAIN_BITS=48 (the Lightning configuration) this
  needs 2^48 + 2^49 adds — not practical; with small overrides
  (8/16 bits, the tested/supported configurations) it is trivially
  reachable.
- Observed output (clang 18 ASan, -DSHACHAIN_BITS=8,
  /tmp/shachain-asan/repro-wrap-oob):
  ```
  ERROR: AddressSanitizer: stack-buffer-overflow
  WRITE of size 8 ... in frame ...
      [160, 536) 'chain'  <== Memory access at offset 536 overflows this variable
  ```
  Offset 536 = 16-byte header + 9 * 40-byte entries: exactly
  `&chain.known[9].index`, i.e. the shachain.c:102 write with
  pos == 9. The TAP regression test trips the same ASan report; in a
  plain gcc build it instead detects the clobber via a canary placed
  immediately after the struct:
  ```
  not ok 4 - w.canary == CANARY
  ```
- Regression test: ccan/crypto/shachain/test/run-exhaust-wrap.c
  (alarm(10)-bounded, 4 subtests, SHACHAIN_BITS=8; includes shachain.c
  per run-test convention). Fails against current code (subtest 4);
  passes 4/4 against /tmp/shachain-asan/shadow with the repair below
  applied, and the existing four tests still pass against the shadow.
- Repair direction: reject out-of-domain indices in
  shachain_add_hash before using pos, e.g. right after shachain.c:89:
  ```c
  if (pos > SHACHAIN_BITS)
      return false;
  ```
  (validated: run-exhaust-wrap 4/4, existing suite unchanged).
  A complete repair should also reject non-zero bits above
  SHACHAIN_BITS (e.g. `SHACHAIN_BITS < 64 && (index >> SHACHAIN_BITS)`
  — guarding the shift against the BITS == 64 case) so that the
  documented all-zero init in reduced-bits builds cannot start an
  out-of-domain chain, and/or document that next_index is undefined
  once 0 has been added.

## Rejected candidates (disproved)

- R1: Same wrap for the default SHACHAIN_BITS=64. Unreachable: it
  requires 2^64 successful adds before next_index wraps. Even then no
  OOB occurs: count_trailing_zeroes() <= 64 and known[] has 65 slots,
  so pos is always in bounds (that "+ 1" in known[SHACHAIN_BITS + 1]
  is exactly the index-0 slot, exercised by run-8bit.c). Rejected.
- R2: Derivation bit order vs design.txt. design.txt's pseudocode
  walks "for bit in 0 to 63" (low to high); the code walks high to
  low (ilog64(branches)-1 down to 0). The authoritative design is
  Lightning BOLT 3 ("for B in 47 down to 0"), and the code matches it
  byte-for-byte on all Appendix D vectors (87/87 probe above);
  design.txt explicitly defers to "the implementation for more
  optimized variants". Rejected — doc pseudocode simplification, not
  a code defect.
- R3: can_derive() mask logic (shachain.c:28-39), including from == 0
  ("always derive from seed") and 1 << ctz with ctz up to 63.
  Exhaustively validated against the naive per-bit implementation
  from design.txt by run-can_derive.c (65536 pairs, ASan/UBSan
  clean). Note the mask never needs to reject to < from: matching all
  bits >= ctz(from) forces to >= from since from's low bits are 0.
  Rejected.
- R4: derive() with from == to: branches == 0, ilog64(0) == 0 per
  ilog.h:50-53, loop `for (i = -1; i >= 0;)` never runs, returns a
  copy of from_hash — the correct R(from). Covered by run.c's j == i
  get_hash cases. Rejected.
- R5: shachain_get_hash() returning values for indices never added.
  In-protocol impossible: adds must decrement by exactly 1 (asserted),
  so any index derivable from the stored set was added. Also verified
  negatively by run.c/run-8bit.c (`!shachain_get_hash(chain, i-1)` at
  every step). Rejected.
- R6: assert(index == shachain_next_index(chain)) (shachain.c:87)
  compiled out under NDEBUG, making out-of-order adds silently
  accepted/corrupting. Out-of-order adds violate the documented
  precondition (shachain.h:85). Rejected.
- R7: shachain_init() arithmetic (shachain.c:78):
  `(UINT64_MAX >> (64 - SHACHAIN_BITS)) + 1`. BITS=64: shift of 0,
  +1 wraps to 0 — unsigned wraparound, defined; BITS<64: exact.
  SHACHAIN_BITS > 64 or 0 would be a compile-time misconfiguration,
  not a runtime defect. Rejected.
- R8: count_trailing_zeroes() non-builtin fallback (shachain.c:17-24)
  vs __builtin_ctzll path: both return SHACHAIN_BITS for 0; the
  fallback loop's `1ULL << i` stays below 1<<64 because i <
  SHACHAIN_BITS <= 64. Both paths exercised (reduce_features
  ccanlint pass builds without HAVE_BUILTIN_CTZLL; tests pass).
  Rejected.
- R9: add_hash consistency check only covers known[i], i < pos
  (the shachain.c:92 FIXME). Sufficient: known[j] with j > pos hold
  larger indices not derivable from the new index, so nothing can be
  checked against them; a wrong hash that passes all i < pos checks
  is genuinely consistent with everything derivable. Corruption
  detection demonstrated by run-badhash.c (1000/1000) and by the
  BOLT 3 "incorrect" vectors. Rejected.
- R10: num_valid bookkeeping (shachain.c:104-105): in-protocol,
  buckets 0..pos-1 are always populated before an index with ctz pos
  is added (strict decrement visits them), so the consistency loop
  never reads an uninitialized entry; pos+1 > num_valid only grows.
  Exhaustively covered by run-8bit.c. Rejected.
- R11: shachain_from_seed(seed, i, &h) with h aliasing seed: derive()
  copies *from_hash first, and sha256() one-shot reads all input
  before writing the digest, so in-place use is safe; not a
  documented usage anyway. Rejected.
- R12: change_bit() (shachain.c:8-11): index <= 63 keeps
  index/CHAR_BIT <= 7 within the 32-byte hash; `1 << (index % 8)` is
  a small int shift. Matches BOLT 3's flip(B) definition
  ("flip(10) in e3b0... is e3b4...") via the vectors. Rejected.

## Cross-module note (not a shachain defect)

- C1: ccan/ilog/ilog.h:134 — inside `#ifdef builtin_ilog64_nz` the
  header defines `ilog32` (again) where `ilog64` was presumably
  intended. Verified identical in upstream ccan (rustyrussell/ccan
  master), so it is an upstream typo, not local damage. Behaviorally
  harmless here: ilog64() falls back to the extern function in
  ilog.c (same result — all vector tests pass), costing only the
  macro fast path; the ilog32 redefinition is token-identical to the
  one at ilog.h:126, so it compiles cleanly. Flagged for the future
  ccan/ilog audit.

## Auditor-added test files (temporary; keep or remove later)

- ccan/crypto/shachain/test/run-exhaust-wrap.c — proves F1.
  SHACHAIN_BITS=8, alarm(10)-bounded, 4 subtests: fresh next_index,
  full 256-add exhaustion, complete re-derivation, then continued
  documented-rule adds with a canary after the struct. Against
  current code: subtest 4 fails in a plain build (canary clobbered)
  and ASan reports a stack-buffer-overflow WRITE at
  &chain.known[9].index (shachain.c:102). Passes 4/4 against the
  /tmp/shachain-asan/shadow tree with the repair-direction guard
  applied; the four existing tests are unaffected by that guard.
- ccanlint with the test present: 47/51 FAIL, solely because
  run-exhaust-wrap fails against the current code (tests_pass -4/4).

No production files were modified (shachain.h, shachain.c, _info,
design.txt untouched; verified by git status — the only new path
under ccan/crypto/shachain is test/run-exhaust-wrap.c).
